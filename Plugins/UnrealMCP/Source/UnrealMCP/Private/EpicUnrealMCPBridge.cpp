#include "EpicUnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "UnrealMCPSettings.h"

static TAutoConsoleVariable<FString> CVarMCPHost(
    TEXT("unreal.mcp.host"),
    TEXT(""),
    TEXT("Optional host override for the MCP server. Empty uses Project Settings."),
    ECVF_Default
);

static TAutoConsoleVariable<int32> CVarMCPPort(
    TEXT("unreal.mcp.port"),
    0,
    TEXT("Optional port override for the MCP server. 0 uses Project Settings."),
    ECVF_Default
);

#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UEpicUnrealMCPBridge::UEpicUnrealMCPBridge()
    : bIsRunning(false)
    , ListenerSocket(nullptr)
    , ConnectionSocket(nullptr)
    , ServerThread(nullptr)
    , Runnable(nullptr)
{
    EditorCommands = MakeShared<FEpicUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FEpicUnrealMCPBlueprintCommands>();
    BlueprintGraphCommands = MakeShared<FEpicUnrealMCPBlueprintGraphCommands>();

    const UUnrealMCPSettings* Settings = GetDefault<UUnrealMCPSettings>();
    FString HostStr = Settings ? Settings->Host : TEXT(MCP_SERVER_HOST);
    const FString CVarHost = CVarMCPHost.GetValueOnAnyThread();
    if (!CVarHost.IsEmpty())
    {
        HostStr = CVarHost;
    }
    if (HostStr.IsEmpty())
    {
        HostStr = TEXT(MCP_SERVER_HOST);
    }
    if (!FIPv4Address::Parse(HostStr, ServerAddress))
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Invalid unreal.mcp.host '%s', falling back to 127.0.0.1"), *HostStr);
        FIPv4Address::Parse(TEXT(MCP_SERVER_HOST), ServerAddress);
    }

    FIPv4Address LoopbackAddress;
    FIPv4Address::Parse(TEXT(MCP_SERVER_HOST), LoopbackAddress);
    if (ServerAddress != LoopbackAddress)
    {
        if (!Settings || !Settings->bAllowRemoteConnections)
        {
            UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Remote binding '%s' rejected. Set bAllowRemoteConnections=true in Project Settings > Plugins > Unreal MCP to allow non-localhost connections."), *HostStr);
            FIPv4Address::Parse(TEXT(MCP_SERVER_HOST), ServerAddress);
            UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Falling back to 127.0.0.1 for safety."));
        }
    }

    int32 ConfiguredPort = Settings ? Settings->Port : MCP_SERVER_PORT;
    const int32 CVarPort = CVarMCPPort.GetValueOnAnyThread();
    if (CVarPort > 0)
    {
        ConfiguredPort = CVarPort;
    }
    if (ConfiguredPort < 1 || ConfiguredPort > 65535)
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Invalid MCP port %d, falling back to %d"), ConfiguredPort, MCP_SERVER_PORT);
        ConfiguredPort = MCP_SERVER_PORT;
    }
    Port = static_cast<uint16>(ConfiguredPort);
}

UEpicUnrealMCPBridge::~UEpicUnrealMCPBridge()
{
    StopServer();
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintGraphCommands.Reset();
}

void UEpicUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Log, TEXT("EpicUnrealMCPBridge: Initializing"));
    // Defer actor index rebuild to first command — editor world may not be ready yet
    StartServer();
}

void UEpicUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("EpicUnrealMCPBridge: Shutting down"));
    StopServer();
}

void UEpicUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Server is already running"));
        return;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    FSocket* NewListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false);
    if (!NewListenerSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        SocketSubsystem->DestroySocket(NewListenerSocket);
        return;
    }

    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to start listening"));
        SocketSubsystem->DestroySocket(NewListenerSocket);
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Log, TEXT("EpicUnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    Runnable = MakeUnique<FMCPServerRunnable>(this, ListenerSocket);
    ServerThread = FRunnableThread::Create(
        Runnable.Get(),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create server thread"));
        Runnable.Reset();
        StopServer();
        return;
    }
}

void UEpicUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    if (Runnable.IsValid())
    {
        Runnable->Stop();
    }

    if (ListenerSocket)
    {
        ListenerSocket->Close();
    }
    if (ConnectionSocket)
    {
        ConnectionSocket->Close();
    }

    if (ServerThread)
    {
        ServerThread->WaitForCompletion();
        delete ServerThread;
        ServerThread = nullptr;
    }

    Runnable.Reset();

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

    if (ConnectionSocket)
    {
        if (SocketSubsystem)
        {
            SocketSubsystem->DestroySocket(ConnectionSocket);
        }
        ConnectionSocket = nullptr;
    }

    if (ListenerSocket)
    {
        if (SocketSubsystem)
        {
            SocketSubsystem->DestroySocket(ListenerSocket);
        }
        ListenerSocket = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("EpicUnrealMCPBridge: Server stopped"));
}

void UEpicUnrealMCPBridge::EnsureActorIndexInitialized()
{
    if (ActorIndex.NameIndex.IsEmpty() && ActorIndex.McpIdIndex.IsEmpty())
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (World)
        {
            ActorIndex.RebuildFromWorld(World);
        }
    }
}

namespace
{
    // Command routing: 0=ping, 1=EditorCommands, 2=BlueprintCommands, 3=BlueprintGraphCommands
    int32 RouteCommand(const FString& CommandType)
    {
        static const TMap<FString, int32> Router = {
            {TEXT("ping"), 0},
            {TEXT("get_actors_in_level"), 1},
            {TEXT("find_actors_by_name"), 1},
            {TEXT("spawn_actor"), 1},
            {TEXT("delete_actor"), 1},
            {TEXT("set_actor_transform"), 1},
            {TEXT("spawn_blueprint_actor"), 2},
            {TEXT("find_actor_by_mcp_id"), 1},
            {TEXT("set_actor_transform_by_mcp_id"), 1},
            {TEXT("delete_actor_by_mcp_id"), 1},
            {TEXT("apply_scene_delta"), 1},
            {TEXT("clone_actor"), 1},
            {TEXT("create_nav_mesh_volume"), 1},
            {TEXT("create_patrol_route"), 1},
            {TEXT("create_spline_from_points"), 1},
            {TEXT("set_ai_behavior"), 1},
            {TEXT("create_draft_proxy"), 1},
            {TEXT("update_draft_proxy"), 1},
            {TEXT("delete_draft_proxy"), 1},
            {TEXT("spawn_instance_set"), 1},
            {TEXT("update_instance_set"), 1},
            {TEXT("delete_instance_set"), 1},
            {TEXT("get_instance_set_state"), 1},
            {TEXT("list_instance_sets"), 1},
            {TEXT("create_blueprint"), 2},
            {TEXT("add_component_to_blueprint"), 2},
            {TEXT("set_physics_properties"), 2},
            {TEXT("compile_blueprint"), 2},
            {TEXT("set_static_mesh_properties"), 2},
            {TEXT("set_mesh_material_color"), 2},
            {TEXT("get_available_materials"), 2},
            {TEXT("apply_material_to_actor"), 2},
            {TEXT("apply_material_to_blueprint"), 2},
            {TEXT("get_actor_material_info"), 2},
            {TEXT("get_blueprint_material_info"), 2},
            {TEXT("read_blueprint_content"), 2},
            {TEXT("analyze_blueprint_graph"), 2},
            {TEXT("get_blueprint_variable_details"), 2},
            {TEXT("get_blueprint_function_details"), 2},
            {TEXT("add_blueprint_node"), 3},
            {TEXT("connect_nodes"), 3},
            {TEXT("create_variable"), 3},
            {TEXT("set_blueprint_variable_properties"), 3},
            {TEXT("add_event_node"), 3},
            {TEXT("delete_node"), 3},
            {TEXT("set_node_property"), 3},
            {TEXT("create_function"), 3},
            {TEXT("add_function_input"), 3},
            {TEXT("add_function_output"), 3},
            {TEXT("delete_function"), 3},
            {TEXT("rename_function"), 3},
        };
        const int32* Found = Router.Find(CommandType);
        return Found ? *Found : -1;
    }
}

FString UEpicUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Log, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);

    const int32 Route = RouteCommand(CommandType);

    auto ExecuteOnCurrentThread = [this, &CommandType, &Params, Route]() -> FString
    {
        EnsureActorIndexInitialized();

        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

        try
        {
            TSharedPtr<FJsonObject> ResultJson;

            switch (Route)
            {
            case 0: // ping
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
                break;
            case 1: // EditorCommands
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
                break;
            case 2: // BlueprintCommands
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
                break;
            case 3: // BlueprintGraphCommands
                ResultJson = BlueprintGraphCommands->HandleCommand(CommandType, Params);
                break;
            default:
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));

                FString UnknownCommandResult;
                TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
                    TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&UnknownCommandResult);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                return UnknownCommandResult;
            }

            bool bSuccess = true;
            FString ErrorMessage;

            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess && ResultJson->HasField(TEXT("error")))
                {
                    ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                }
            }

            if (bSuccess)
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }

        FString ResultString;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        return ResultString;
    };

    if (IsInGameThread())
    {
        return ExecuteOnCurrentThread();
    }

    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    AsyncTask(ENamedThreads::GameThread, [ExecuteOnCurrentThread, Promise = MoveTemp(Promise)]() mutable
    {
        Promise.SetValue(ExecuteOnCurrentThread());
    });

    return Future.Get();
}
