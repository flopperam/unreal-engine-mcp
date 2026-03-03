#include "EpicUnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Editor.h"
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
// Add Blueprint related includes
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
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPTransactionCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPErrorCodes.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UEpicUnrealMCPBridge::UEpicUnrealMCPBridge()
{
    EditorCommands = MakeShared<FEpicUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FEpicUnrealMCPBlueprintCommands>();
    BlueprintGraphCommands = MakeShared<FEpicUnrealMCPBlueprintGraphCommands>();
    TransactionCommands = MakeShared<FEpicUnrealMCPTransactionCommands>();
}

UEpicUnrealMCPBridge::~UEpicUnrealMCPBridge()
{
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintGraphCommands.Reset();
    TransactionCommands.Reset();
}

// Initialize subsystem
void UEpicUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UEpicUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UEpicUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UEpicUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server stopped"));
}

// Execute a command received from a client
FString UEpicUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);
    
    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            // ---------------------------------------------------------------
            // batch_execute_commands -- special case, handled inline because
            // it needs to call DispatchToHandler repeatedly and build its own
            // response envelope.
            // ---------------------------------------------------------------
            if (CommandType == TEXT("batch_execute_commands"))
            {
                const TArray<TSharedPtr<FJsonValue>>* CommandsArray = nullptr;
                if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("commands"), CommandsArray) || !CommandsArray)
                {
                    ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                    ResponseJson->SetStringField(TEXT("error"), TEXT("Missing 'commands' array parameter"));
                    ResponseJson->SetStringField(TEXT("error_code"), MCPErrorCodes::MISSING_PARAM);
                    ResponseJson->SetStringField(TEXT("protocol_version"), TEXT("1.1.0"));

                    FString ResultString;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                    FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                    Promise.SetValue(ResultString);
                    return;
                }

                bool bStopOnError = false;
                Params->TryGetBoolField(TEXT("stop_on_error"), bStopOnError);

                bool bWrapInTransaction = false;
                Params->TryGetBoolField(TEXT("transaction"), bWrapInTransaction);

                FString TransactionDesc = TEXT("MCP Batch");
                Params->TryGetStringField(TEXT("transaction_description"), TransactionDesc);

                int32 TransactionIndex = -1;
                if (bWrapInTransaction && GEditor)
                {
                    TransactionIndex = GEditor->BeginTransaction(TEXT("MCP"), FText::FromString(TransactionDesc), nullptr);
                }

                TArray<TSharedPtr<FJsonValue>> Results;
                bool bHadError = false;
                int32 SuccessCount = 0;
                int32 ErrorCount = 0;

                for (int32 i = 0; i < CommandsArray->Num(); ++i)
                {
                    const TSharedPtr<FJsonObject>* CmdObj = nullptr;
                    if (!(*CommandsArray)[i]->TryGetObject(CmdObj) || !CmdObj || !(*CmdObj).IsValid())
                    {
                        TSharedPtr<FJsonObject> ErrResult = MakeShared<FJsonObject>();
                        ErrResult->SetNumberField(TEXT("index"), i);
                        ErrResult->SetStringField(TEXT("status"), TEXT("error"));
                        ErrResult->SetStringField(TEXT("error_code"), MCPErrorCodes::INVALID_PARAM);
                        ErrResult->SetStringField(TEXT("error"), TEXT("Invalid command object"));
                        Results.Add(MakeShared<FJsonValueObject>(ErrResult));
                        ErrorCount++;
                        if (bStopOnError) { bHadError = true; break; }
                        continue;
                    }

                    FString SubCommand;
                    (*CmdObj)->TryGetStringField(TEXT("type"), SubCommand);

                    // Block nested batch commands
                    if (SubCommand == TEXT("batch_execute_commands"))
                    {
                        TSharedPtr<FJsonObject> ErrResult = MakeShared<FJsonObject>();
                        ErrResult->SetNumberField(TEXT("index"), i);
                        ErrResult->SetStringField(TEXT("command"), SubCommand);
                        ErrResult->SetStringField(TEXT("status"), TEXT("error"));
                        ErrResult->SetStringField(TEXT("error_code"), MCPErrorCodes::OPERATION_NOT_SUPPORTED);
                        ErrResult->SetStringField(TEXT("error"), TEXT("Nested batch commands are not allowed"));
                        Results.Add(MakeShared<FJsonValueObject>(ErrResult));
                        ErrorCount++;
                        if (bStopOnError) { bHadError = true; break; }
                        continue;
                    }

                    TSharedPtr<FJsonObject> SubParams = MakeShared<FJsonObject>();
                    if ((*CmdObj)->HasField(TEXT("params")))
                    {
                        SubParams = (*CmdObj)->GetObjectField(TEXT("params"));
                    }

                    TSharedPtr<FJsonObject> SubResult = DispatchToHandler(SubCommand, SubParams);

                    TSharedPtr<FJsonObject> IndexedResult = MakeShared<FJsonObject>();
                    IndexedResult->SetNumberField(TEXT("index"), i);
                    IndexedResult->SetStringField(TEXT("command"), SubCommand);

                    bool bSubSuccess = true;
                    if (SubResult.IsValid() && SubResult->HasField(TEXT("success")))
                    {
                        bSubSuccess = SubResult->GetBoolField(TEXT("success"));
                    }

                    if (bSubSuccess)
                    {
                        IndexedResult->SetStringField(TEXT("status"), TEXT("success"));
                        IndexedResult->SetObjectField(TEXT("result"), SubResult);
                        SuccessCount++;
                    }
                    else
                    {
                        IndexedResult->SetStringField(TEXT("status"), TEXT("error"));
                        IndexedResult->SetStringField(TEXT("error"), SubResult.IsValid() ? SubResult->GetStringField(TEXT("error")) : TEXT("Unknown error"));
                        if (SubResult.IsValid() && SubResult->HasField(TEXT("error_code")))
                        {
                            IndexedResult->SetStringField(TEXT("error_code"), SubResult->GetStringField(TEXT("error_code")));
                        }
                        ErrorCount++;
                        bHadError = true;
                    }

                    Results.Add(MakeShared<FJsonValueObject>(IndexedResult));

                    if (bStopOnError && bHadError)
                    {
                        break;
                    }
                }

                // End or rollback transaction
                if (bWrapInTransaction && GEditor)
                {
                    if (bHadError && bStopOnError)
                    {
                        // Rollback: end then undo
                        GEditor->EndTransaction();
                        GEditor->UndoTransaction();
                    }
                    else
                    {
                        GEditor->EndTransaction();
                    }
                }

                ResponseJson->SetStringField(TEXT("status"),
                    ErrorCount == 0 ? TEXT("success") :
                    (SuccessCount > 0 ? TEXT("partial") : TEXT("error")));
                if (ErrorCount > 0)
                {
                    ResponseJson->SetStringField(TEXT("error_code"),
                        SuccessCount > 0 ? MCPErrorCodes::BATCH_PARTIAL_FAILURE : MCPErrorCodes::BATCH_ABORTED);
                }
                ResponseJson->SetArrayField(TEXT("results"), Results);
                ResponseJson->SetNumberField(TEXT("success_count"), SuccessCount);
                ResponseJson->SetNumberField(TEXT("error_count"), ErrorCount);
                ResponseJson->SetNumberField(TEXT("total_count"), CommandsArray->Num());
                ResponseJson->SetStringField(TEXT("protocol_version"), TEXT("1.1.0"));

                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }

            // ---------------------------------------------------------------
            // Regular single-command dispatch
            // ---------------------------------------------------------------
            TSharedPtr<FJsonObject> ResultJson;

            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            // Editor Commands (actor manipulation, lifecycle, levels, viewport, reflection)
            else if (CommandType == TEXT("get_actors_in_level") ||
                     CommandType == TEXT("find_actors_by_name") ||
                     CommandType == TEXT("spawn_actor") ||
                     CommandType == TEXT("delete_actor") ||
                     CommandType == TEXT("set_actor_transform") ||
                     CommandType == TEXT("spawn_blueprint_actor") ||
                     CommandType == TEXT("request_editor_exit") ||
                     CommandType == TEXT("restart_editor") ||
                     // Phase 1: Editor lifecycle
                     CommandType == TEXT("get_editor_state") ||
                     CommandType == TEXT("get_engine_version") ||
                     CommandType == TEXT("save_all") ||
                     CommandType == TEXT("play_in_editor_start") ||
                     CommandType == TEXT("play_in_editor_stop") ||
                     CommandType == TEXT("simulate_start") ||
                     CommandType == TEXT("simulate_stop") ||
                     CommandType == TEXT("pause_game") ||
                     // Phase 1: Level lifecycle
                     CommandType == TEXT("list_levels") ||
                     CommandType == TEXT("open_level") ||
                     CommandType == TEXT("save_level") ||
                     CommandType == TEXT("create_level") ||
                     CommandType == TEXT("duplicate_level") ||
                     CommandType == TEXT("delete_level") ||
                     CommandType == TEXT("set_current_world") ||
                     CommandType == TEXT("add_sublevel") ||
                     CommandType == TEXT("remove_sublevel") ||
                     CommandType == TEXT("toggle_sublevel_visibility") ||
                     // Phase 1: Selection and viewport
                     CommandType == TEXT("get_selected_actors") ||
                     CommandType == TEXT("select_actors") ||
                     CommandType == TEXT("focus_viewport_on_selection") ||
                     CommandType == TEXT("set_viewport_camera") ||
                     CommandType == TEXT("capture_viewport_screenshot") ||
                     CommandType == TEXT("get_viewport_stats") ||
                     // Reflection API
                     CommandType == TEXT("get_object_properties") ||
                     CommandType == TEXT("set_object_properties") ||
                     CommandType == TEXT("call_uobject_function") ||
                     CommandType == TEXT("build_navmesh"))
            {
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") ||
                     CommandType == TEXT("add_component_to_blueprint") ||
                     CommandType == TEXT("set_physics_properties") ||
                     CommandType == TEXT("compile_blueprint") ||
                     CommandType == TEXT("set_static_mesh_properties") ||
                     CommandType == TEXT("set_mesh_material_color") ||
                     CommandType == TEXT("get_available_materials") ||
                     CommandType == TEXT("apply_material_to_actor") ||
                     CommandType == TEXT("apply_material_to_blueprint") ||
                     CommandType == TEXT("get_actor_material_info") ||
                     CommandType == TEXT("get_blueprint_material_info") ||
                     CommandType == TEXT("read_blueprint_content") ||
                     CommandType == TEXT("analyze_blueprint_graph") ||
                     CommandType == TEXT("get_blueprint_variable_details") ||
                     CommandType == TEXT("get_blueprint_function_details"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Graph Commands
            else if (CommandType == TEXT("add_blueprint_node") ||
                     CommandType == TEXT("connect_nodes") ||
                     CommandType == TEXT("create_variable") ||
                     CommandType == TEXT("set_blueprint_variable_properties") ||
                     CommandType == TEXT("add_event_node") ||
                     CommandType == TEXT("delete_node") ||
                     CommandType == TEXT("set_node_property") ||
                     CommandType == TEXT("create_function") ||
                     CommandType == TEXT("add_function_input") ||
                     CommandType == TEXT("add_function_output") ||
                     CommandType == TEXT("delete_function") ||
                     CommandType == TEXT("rename_function"))
            {
                ResultJson = BlueprintGraphCommands->HandleCommand(CommandType, Params);
            }
            // Transaction Commands
            else if (CommandType == TEXT("begin_editor_transaction") ||
                     CommandType == TEXT("end_editor_transaction") ||
                     CommandType == TEXT("rollback_transaction") ||
                     CommandType == TEXT("undo") ||
                     CommandType == TEXT("redo") ||
                     CommandType == TEXT("checkpoint_scene_state"))
            {
                ResultJson = TransactionCommands->HandleCommand(CommandType, Params);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
                ResponseJson->SetStringField(TEXT("error_code"), MCPErrorCodes::UNKNOWN_COMMAND);
                ResponseJson->SetStringField(TEXT("protocol_version"), TEXT("1.1.0"));

                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }

            // Check if the result contains an error
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
                // Set success status and include the result
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                // Set error status and include the error message
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
                // Forward error_code from handler if present
                if (ResultJson->HasField(TEXT("error_code")))
                {
                    ResponseJson->SetStringField(TEXT("error_code"), ResultJson->GetStringField(TEXT("error_code")));
                }
            }

            ResponseJson->SetStringField(TEXT("protocol_version"), TEXT("1.1.0"));
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
            ResponseJson->SetStringField(TEXT("error_code"), MCPErrorCodes::UNKNOWN_ERROR);
            ResponseJson->SetStringField(TEXT("protocol_version"), TEXT("1.1.0"));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    return Future.Get();
}

// ---------------------------------------------------------------------------
// DispatchToHandler -- shared routing logic for single and batch execution
// Must be called on the Game Thread.
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> UEpicUnrealMCPBridge::DispatchToHandler(
    const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("ping"))
    {
        TSharedPtr<FJsonObject> PingResult = MakeShared<FJsonObject>();
        PingResult->SetBoolField(TEXT("success"), true);
        PingResult->SetStringField(TEXT("message"), TEXT("pong"));
        return PingResult;
    }
    // Editor Commands (actor manipulation, lifecycle, levels, viewport, reflection)
    else if (CommandType == TEXT("get_actors_in_level") ||
             CommandType == TEXT("find_actors_by_name") ||
             CommandType == TEXT("spawn_actor") ||
             CommandType == TEXT("delete_actor") ||
             CommandType == TEXT("set_actor_transform") ||
             CommandType == TEXT("spawn_blueprint_actor") ||
             CommandType == TEXT("request_editor_exit") ||
             CommandType == TEXT("restart_editor") ||
             // Phase 1: Editor lifecycle
             CommandType == TEXT("get_editor_state") ||
             CommandType == TEXT("get_engine_version") ||
             CommandType == TEXT("save_all") ||
             CommandType == TEXT("play_in_editor_start") ||
             CommandType == TEXT("play_in_editor_stop") ||
             CommandType == TEXT("simulate_start") ||
             CommandType == TEXT("simulate_stop") ||
             CommandType == TEXT("pause_game") ||
             // Phase 1: Level lifecycle
             CommandType == TEXT("list_levels") ||
             CommandType == TEXT("open_level") ||
             CommandType == TEXT("save_level") ||
             CommandType == TEXT("create_level") ||
             CommandType == TEXT("duplicate_level") ||
             CommandType == TEXT("delete_level") ||
             CommandType == TEXT("set_current_world") ||
             CommandType == TEXT("add_sublevel") ||
             CommandType == TEXT("remove_sublevel") ||
             CommandType == TEXT("toggle_sublevel_visibility") ||
             // Phase 1: Selection and viewport
             CommandType == TEXT("get_selected_actors") ||
             CommandType == TEXT("select_actors") ||
             CommandType == TEXT("focus_viewport_on_selection") ||
             CommandType == TEXT("set_viewport_camera") ||
             CommandType == TEXT("capture_viewport_screenshot") ||
             CommandType == TEXT("get_viewport_stats") ||
             // Reflection API
             CommandType == TEXT("get_object_properties") ||
             CommandType == TEXT("set_object_properties") ||
             CommandType == TEXT("call_uobject_function") ||
             CommandType == TEXT("build_navmesh"))
    {
        return EditorCommands->HandleCommand(CommandType, Params);
    }
    // Blueprint Commands
    else if (CommandType == TEXT("create_blueprint") ||
             CommandType == TEXT("add_component_to_blueprint") ||
             CommandType == TEXT("set_physics_properties") ||
             CommandType == TEXT("compile_blueprint") ||
             CommandType == TEXT("set_static_mesh_properties") ||
             CommandType == TEXT("set_mesh_material_color") ||
             CommandType == TEXT("get_available_materials") ||
             CommandType == TEXT("apply_material_to_actor") ||
             CommandType == TEXT("apply_material_to_blueprint") ||
             CommandType == TEXT("get_actor_material_info") ||
             CommandType == TEXT("get_blueprint_material_info") ||
             CommandType == TEXT("read_blueprint_content") ||
             CommandType == TEXT("analyze_blueprint_graph") ||
             CommandType == TEXT("get_blueprint_variable_details") ||
             CommandType == TEXT("get_blueprint_function_details"))
    {
        return BlueprintCommands->HandleCommand(CommandType, Params);
    }
    // Blueprint Graph Commands
    else if (CommandType == TEXT("add_blueprint_node") ||
             CommandType == TEXT("connect_nodes") ||
             CommandType == TEXT("create_variable") ||
             CommandType == TEXT("set_blueprint_variable_properties") ||
             CommandType == TEXT("add_event_node") ||
             CommandType == TEXT("delete_node") ||
             CommandType == TEXT("set_node_property") ||
             CommandType == TEXT("create_function") ||
             CommandType == TEXT("add_function_input") ||
             CommandType == TEXT("add_function_output") ||
             CommandType == TEXT("delete_function") ||
             CommandType == TEXT("rename_function"))
    {
        return BlueprintGraphCommands->HandleCommand(CommandType, Params);
    }
    // Transaction Commands
    else if (CommandType == TEXT("begin_editor_transaction") ||
             CommandType == TEXT("end_editor_transaction") ||
             CommandType == TEXT("rollback_transaction") ||
             CommandType == TEXT("undo") ||
             CommandType == TEXT("redo") ||
             CommandType == TEXT("checkpoint_scene_state"))
    {
        return TransactionCommands->HandleCommand(CommandType, Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        MCPErrorCodes::UNKNOWN_COMMAND,
        FString::Printf(TEXT("Unknown command: %s"), *CommandType));
}
