#include "MCPClientHandler.h"
#include "EpicUnrealMCPBridge.h"
#include "UnrealMCPSettings.h"
#include "Commands/ProceduralMeshBuilder.h"

namespace
{
double JsonArrayNumberOrDefault(const TArray<TSharedPtr<FJsonValue>>& Array, int32 Index, double DefaultValue)
{
    return Array.IsValidIndex(Index) ? Array[Index]->AsNumber() : DefaultValue;
}

FVector ReadVectorParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FVector& DefaultValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (Params->TryGetArrayField(FieldName, Array) && Array && Array->Num() >= 3)
    {
        return FVector(
            JsonArrayNumberOrDefault(*Array, 0, DefaultValue.X),
            JsonArrayNumberOrDefault(*Array, 1, DefaultValue.Y),
            JsonArrayNumberOrDefault(*Array, 2, DefaultValue.Z)
        );
    }

    const TSharedPtr<FJsonObject>* Object = nullptr;
    if (Params->TryGetObjectField(FieldName, Object) && Object && Object->IsValid())
    {
        double X = DefaultValue.X;
        double Y = DefaultValue.Y;
        double Z = DefaultValue.Z;
        (*Object)->TryGetNumberField(TEXT("x"), X);
        (*Object)->TryGetNumberField(TEXT("y"), Y);
        (*Object)->TryGetNumberField(TEXT("z"), Z);
        return FVector(X, Y, Z);
    }

    return DefaultValue;
}

FRotator ReadRotatorParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FRotator& DefaultValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (Params->TryGetArrayField(FieldName, Array) && Array && Array->Num() >= 3)
    {
        return FRotator(
            JsonArrayNumberOrDefault(*Array, 0, DefaultValue.Pitch),
            JsonArrayNumberOrDefault(*Array, 1, DefaultValue.Yaw),
            JsonArrayNumberOrDefault(*Array, 2, DefaultValue.Roll)
        );
    }

    const TSharedPtr<FJsonObject>* Object = nullptr;
    if (Params->TryGetObjectField(FieldName, Object) && Object && Object->IsValid())
    {
        double Pitch = DefaultValue.Pitch;
        double Yaw = DefaultValue.Yaw;
        double Roll = DefaultValue.Roll;
        (*Object)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*Object)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*Object)->TryGetNumberField(TEXT("roll"), Roll);
        return FRotator(Pitch, Yaw, Roll);
    }

    return DefaultValue;
}

TSharedPtr<FJsonObject> MakeProceduralMeshError(const FString& Code, const FString& Message, const FString& McpId, double RequestIdNumber)
{
    TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
    Error->SetBoolField(TEXT("success"), false);
    Error->SetStringField(TEXT("error_code"), Code);
    Error->SetStringField(TEXT("error"), Message);
    Error->SetStringField(TEXT("mcp_id"), McpId);
    Error->SetNumberField(TEXT("request_id"), RequestIdNumber);
    Error->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
    return Error;
}
}

FMCPClientHandler::FMCPClientHandler(UEpicUnrealMCPBridge* InBridge, FSocket* InClientSocket)
    : Bridge(InBridge)
    , ClientSocket(InClientSocket)
    , bRunning(true)
    , bFinished(false)
{
}

FMCPClientHandler::~FMCPClientHandler()
{
    if (ClientSocket)
    {
        ClientSocket->Close();
        if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
        {
            SocketSubsystem->DestroySocket(ClientSocket);
        }
        ClientSocket = nullptr;
    }
}

bool FMCPClientHandler::Init()
{
    return true;
}

uint32 FMCPClientHandler::Run()
{
    UE_LOG(LogTemp, Log, TEXT("MCPClientHandler: Client thread starting"));

    if (!ClientSocket)
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPClientHandler: No client socket; exiting"));
        bFinished = true;
        return 0;
    }

    ClientSocket->SetNonBlocking(false);
    ClientSocket->SetNoDelay(true);
    int32 SocketBufferSize = 65536;
    ClientSocket->SetSendBufferSize(SocketBufferSize, SocketBufferSize);
    ClientSocket->SetReceiveBufferSize(SocketBufferSize, SocketBufferSize);

    // Receive loop with newline-delimited framing
    const int32 ChunkSize = 65536;
    uint8 Buffer[ChunkSize];
    FString Accumulator;

    while (bRunning)
    {
        int32 BytesRead = 0;
        if (!ClientSocket->Recv(Buffer, ChunkSize - 1, BytesRead))
        {
            int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
            if (LastError == SE_EWOULDBLOCK)
            {
                ClientSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(50));
                continue;
            }
            else if (LastError == SE_EINTR)
            {
                UE_LOG(LogTemp, Verbose, TEXT("MCPClientHandler: Recv interrupted, continuing..."));
                continue;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("MCPClientHandler: Client disconnected or error. Code: %d"), LastError);
                break;
            }
        }

        if (BytesRead == 0)
        {
            UE_LOG(LogTemp, Log, TEXT("MCPClientHandler: Client disconnected (zero bytes)"));
            break;
        }

        Buffer[BytesRead] = '\0';
        Accumulator.Append(UTF8_TO_TCHAR(Buffer));

        int32 NewlineIndex;
        while (Accumulator.FindChar('\n', NewlineIndex))
        {
            FString Line = Accumulator.Left(NewlineIndex);
            Accumulator.RemoveAt(0, NewlineIndex + 1);

            Line.TrimStartAndEndInline();
            if (!Line.IsEmpty())
            {
                ProcessMessage(Line);
            }
        }

        // Prevent unbounded growth of accumulator from malformed input
        constexpr int32 MAX_REQUEST_SIZE = 1048576; // 1MB
        if (Accumulator.Len() > MAX_REQUEST_SIZE)
        {
            UE_LOG(LogTemp, Warning, TEXT("MCPClientHandler: Accumulator exceeded 1MB without newline; closing connection."));
            Accumulator.Empty();
            break;
        }
    }

    if (ClientSocket)
    {
        ClientSocket->Close();
        if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
        {
            SocketSubsystem->DestroySocket(ClientSocket);
        }
        ClientSocket = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("MCPClientHandler: Client thread stopping"));
    bFinished = true;
    return 0;
}

void FMCPClientHandler::Stop()
{
    bRunning = false;
    if (ClientSocket)
    {
        ClientSocket->Close();
    }
}

void FMCPClientHandler::Exit()
{
    // Socket cleanup is handled in destructor and Run() exit path
}

bool FMCPClientHandler::IsFinished() const
{
    return bFinished;
}

FSocket* FMCPClientHandler::GetClientSocket() const
{
    return ClientSocket;
}

void FMCPClientHandler::ProcessMessage(const FString& Message)
{
    auto SendJsonError = [this](const FString& ErrorCode, const FString& ErrorMessage)
    {
        if (!ClientSocket)
        {
            return;
        }

        TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
        ErrorObj->SetBoolField(TEXT("success"), false);
        ErrorObj->SetStringField(TEXT("error_code"), ErrorCode);
        ErrorObj->SetStringField(TEXT("error"), ErrorMessage);
        FString ErrorStr;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ErrorStr);
        FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
        ErrorStr.AppendChar('\n');
        FTCHARToUTF8 UTF8Error(*ErrorStr);
        const uint8* Data = (const uint8*)UTF8Error.Get();
        int32 TotalSize = UTF8Error.Length();
        int32 Sent = 0;
        while (Sent < TotalSize)
        {
            int32 ChunkSent = 0;
            if (!ClientSocket->Send(Data + Sent, TotalSize - Sent, ChunkSent))
            {
                return;
            }
            Sent += ChunkSent;
        }
    };

    TSharedPtr<FJsonObject> JsonMessage;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

    if (!FJsonSerializer::Deserialize(Reader, JsonMessage) || !JsonMessage.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPClientHandler: Failed to parse JSON message"));
        SendJsonError(TEXT("INVALID_JSON"), TEXT("Failed to parse request as valid JSON"));
        return;
    }

    FString CommandType;
    if (!JsonMessage->TryGetStringField(TEXT("command"), CommandType))
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPClientHandler: Missing 'command' field"));
        SendJsonError(TEXT("MISSING_COMMAND"), TEXT("Request must include a 'command' field"));
        return;
    }

    const UUnrealMCPSettings* Settings = GetDefault<UUnrealMCPSettings>();
    if (Settings && !Settings->AuthToken.IsEmpty())
    {
        FString ProvidedToken;
        if (!JsonMessage->TryGetStringField(TEXT("auth_token"), ProvidedToken) || ProvidedToken != Settings->AuthToken)
        {
            UE_LOG(LogTemp, Warning, TEXT("MCPClientHandler: Authentication failed for command '%s'"), *CommandType);
            SendJsonError(TEXT("AUTH_FAILED"), TEXT("Invalid or missing auth_token"));
            return;
        }
    }

    TSharedPtr<FJsonObject> Params;
    const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
    if (JsonMessage->TryGetObjectField(TEXT("params"), ParamsPtr))
    {
        Params = *ParamsPtr;
    }
    else
    {
        Params = MakeShareable(new FJsonObject());
    }

    if (CommandType == TEXT("upsert_procedural_mesh"))
    {
        ProcessUpsertProceduralMesh(Params);
        return;
    }

    FString Response = Bridge->ExecuteCommand(CommandType, Params);
    Response.AppendChar('\n');
    SendJsonResponseString(Response);
}

void FMCPClientHandler::SendJsonResponse(const TSharedPtr<FJsonObject>& ResponseObj)
{
    FString Response;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Response);
    FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);
    Response.AppendChar('\n');
    SendJsonResponseString(Response);
}

void FMCPClientHandler::SendJsonResponseString(const FString& Response)
{
    if (!ClientSocket)
    {
        return;
    }
    FTCHARToUTF8 UTF8Response(*Response);
    const uint8* DataToSend = (const uint8*)UTF8Response.Get();
    int32 TotalDataSize = UTF8Response.Length();
    int32 TotalBytesSent = 0;

    while (TotalBytesSent < TotalDataSize)
    {
        int32 BytesSent = 0;
        if (!ClientSocket->Send(DataToSend + TotalBytesSent, TotalDataSize - TotalBytesSent, BytesSent))
        {
            UE_LOG(LogTemp, Error, TEXT("MCPClientHandler: Failed to send response after %d/%d bytes"), TotalBytesSent, TotalDataSize);
            return;
        }
        TotalBytesSent += BytesSent;
    }
}

void FMCPClientHandler::ProcessUpsertProceduralMesh(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Parse metadata
    int32 BinarySize = 0;
    FString McpId;
    FString ActorName;
    FString MaterialPath;
    double RequestIdNumber = 0.0;
    bool bFocusViewport = true;
    bool bEnableNanite = false;

    Params->TryGetNumberField(TEXT("binary_size"), BinarySize);
    Params->TryGetStringField(TEXT("mcp_id"), McpId);
    Params->TryGetNumberField(TEXT("request_id"), RequestIdNumber);
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        ActorName = McpId;
    }
    Params->TryGetStringField(TEXT("material_path"), MaterialPath);
    Params->TryGetBoolField(TEXT("focus_viewport"), bFocusViewport);
    Params->TryGetBoolField(TEXT("enable_nanite"), bEnableNanite);

    if (BinarySize <= 0)
    {
        SendJsonResponse(MakeProceduralMeshError(TEXT("INVALID_BINARY_SIZE"), TEXT("Invalid binary_size"), McpId, RequestIdNumber));
        return;
    }

    if (McpId.IsEmpty())
    {
        SendJsonResponse(MakeProceduralMeshError(TEXT("MISSING_MCP_ID"), TEXT("mcp_id is required for upsert_procedural_mesh"), McpId, RequestIdNumber));
        return;
    }

    // 2. Send ready response
    {
        TSharedPtr<FJsonObject> Ready = MakeShared<FJsonObject>();
        Ready->SetStringField(TEXT("status"), TEXT("ready"));
        SendJsonResponse(Ready);
    }

    // 3. Receive binary payload
    TArray<uint8> RawBuffer;
    RawBuffer.SetNumUninitialized(BinarySize);

    int32 TotalReceived = 0;
    double RecvStart = FPlatformTime::Seconds();

    while (TotalReceived < BinarySize)
    {
        int32 Received = 0;
        if (!ClientSocket->Recv(RawBuffer.GetData() + TotalReceived, BinarySize - TotalReceived, Received, ESocketReceiveFlags::None))
        {
            SendJsonResponse(MakeProceduralMeshError(TEXT("BINARY_RECEIVE_FAILED"), TEXT("Failed to receive binary payload"), McpId, RequestIdNumber));
            return;
        }
        if (Received == 0)
        {
            SendJsonResponse(MakeProceduralMeshError(TEXT("BINARY_CONNECTION_CLOSED"), TEXT("Connection closed during binary reception"), McpId, RequestIdNumber));
            return;
        }
        TotalReceived += Received;
    }

    double RecvTimeMs = (FPlatformTime::Seconds() - RecvStart) * 1000.0;
    UE_LOG(LogTemp, Log, TEXT("MCPClientHandler: Received %d bytes in %.2f ms"), BinarySize, RecvTimeMs);

    // 4. Parse payload
    FProceduralMeshPayload Payload;
    Payload.McpId = McpId;
    Payload.ActorName = ActorName;
    Payload.MaterialPath = MaterialPath;
    Payload.Location = ReadVectorParam(Params, TEXT("location"), FVector::ZeroVector);
    Payload.Rotation = ReadRotatorParam(Params, TEXT("rotation"), FRotator::ZeroRotator);
    Payload.Scale = ReadVectorParam(Params, TEXT("scale"), FVector::OneVector);
    Payload.bFocusViewport = bFocusViewport;
    Payload.bEnableNanite = bEnableNanite;

    FString ErrorCode;
    FString ErrorMessage;
    if (!FProceduralMeshBuilder::ParseBinaryPayload(RawBuffer, Payload, ErrorCode, ErrorMessage))
    {
        SendJsonResponse(MakeProceduralMeshError(
            ErrorCode.IsEmpty() ? TEXT("PARSE_FAILED") : ErrorCode,
            ErrorMessage.IsEmpty() ? TEXT("Failed to parse binary payload") : ErrorMessage,
            McpId,
            Payload.RequestId != 0 ? static_cast<double>(Payload.RequestId) : RequestIdNumber));
        return;
    }
    RequestIdNumber = static_cast<double>(Payload.RequestId);

    // 5. Build mesh on GameThread
    FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(false);
    TSharedPtr<FJsonObject> ResultJson;

    double BuildStart = FPlatformTime::Seconds();

    AsyncTask(ENamedThreads::GameThread, [&Payload, &ResultJson, CompletionEvent]()
    {
        ResultJson = FProceduralMeshBuilder::BuildAndSpawnOnGameThread(Payload);
        CompletionEvent->Trigger();
    });

    CompletionEvent->Wait();
    FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);

    double BuildTimeMs = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
    UE_LOG(LogTemp, Log, TEXT("MCPClientHandler: request_id=%lld mcp_id=%s bytes=%d verts=%u triangles=%u transfer=%.2fms build=%.2fms"),
        static_cast<int64>(Payload.RequestId), *McpId, BinarySize, Payload.VertexCount, Payload.IndexCount / 3, RecvTimeMs, BuildTimeMs);

    if (ResultJson.IsValid())
    {
        if (!ResultJson->HasTypedField<EJson::String>(TEXT("mcp_id")))
        {
            ResultJson->SetStringField(TEXT("mcp_id"), McpId);
        }
        if (!ResultJson->HasField(TEXT("request_id")))
        {
            ResultJson->SetNumberField(TEXT("request_id"), RequestIdNumber);
        }
        ResultJson->SetNumberField(TEXT("bytes"), BinarySize);
        ResultJson->SetNumberField(TEXT("transfer_time_ms"), RecvTimeMs);
        ResultJson->SetNumberField(TEXT("build_time_ms"), BuildTimeMs);
        SendJsonResponse(ResultJson);
    }
    else
    {
        SendJsonResponse(MakeProceduralMeshError(TEXT("BUILD_FAILED"), TEXT("Failed to build procedural mesh on GameThread"), McpId, RequestIdNumber));
    }
}
