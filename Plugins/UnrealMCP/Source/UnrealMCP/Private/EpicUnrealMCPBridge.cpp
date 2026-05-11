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
#include "Containers/Ticker.h"
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
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPMaterialCommands.h"
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

namespace
{
double GetDeferredEditorCommandDelaySeconds(const FString& CommandType)
{
    static const TSet<FString> SlowDeferredCommands = {
        TEXT("create_level"),
        TEXT("save_level"),
        TEXT("load_level"),
        TEXT("duplicate_level"),
        TEXT("rename_level"),
        TEXT("delete_level"),
        TEXT("add_sublevel"),
        TEXT("remove_sublevel"),
        TEXT("set_sublevel_visible"),
        TEXT("set_sublevel_loaded"),
        TEXT("create_streaming_volume"),
        TEXT("set_level_streaming_settings"),
        TEXT("enable_world_partition"),
        TEXT("set_world_partition_grid"),
        TEXT("load_world_partition_cell"),
        TEXT("unload_world_partition_cell"),
        TEXT("create_data_layer"),
        TEXT("add_actors_to_data_layer"),
        TEXT("remove_actors_from_data_layer"),
        TEXT("set_data_layer_enabled"),
        TEXT("create_hlod_layer"),
        TEXT("build_hlod"),
        TEXT("rebuild_hlod"),
        TEXT("set_one_file_per_actor"),
        TEXT("set_level_bounds"),
        TEXT("set_world_origin_rebasing"),
        TEXT("start_pie"),
        TEXT("stop_pie"),
        TEXT("start_standalone_game"),
        TEXT("start_simulate"),
        TEXT("undo"),
        TEXT("redo"),
        TEXT("save_all"),
        TEXT("save_asset"),
        TEXT("create_utility_widget"),
        TEXT("create_utility_blueprint"),
        TEXT("create_collapsed_graph"),
        TEXT("create_macro_graph"),
        TEXT("create_macro_instance"),
        TEXT("create_timeline"),
        TEXT("edit_timeline"),
        TEXT("import_fbx_mesh"),
        TEXT("import_texture"),
        TEXT("import_audio"),
        TEXT("import_gltf"),
        TEXT("import_obj"),
        TEXT("import_usd"),
        TEXT("import_mp3"),
        TEXT("import_alembic"),
        TEXT("import_datasmith"),
        TEXT("reimport_asset"),
        TEXT("export_asset"),
        TEXT("take_screenshot"),
        TEXT("export_level"),
        TEXT("save_import_preset"),
        TEXT("load_import_preset"),
        TEXT("create_input_action"),
        TEXT("create_input_mapping_context"),
        TEXT("add_enhanced_input_mapping"),
        TEXT("remove_enhanced_input_mapping"),
        TEXT("configure_enhanced_input_action"),
        TEXT("configure_enhanced_input_mapping"),
        TEXT("add_runtime_mapping_context"),
        TEXT("remove_runtime_mapping_context"),
        TEXT("setup_enhanced_input_binding"),
        TEXT("setup_rebind_ui"),
        TEXT("rebind_enhanced_input_key"),
        TEXT("configure_local_multiplayer_input"),
        TEXT("create_widget_blueprint"),
        TEXT("add_widget_to_widget_blueprint"),
        TEXT("remove_widget_from_widget_blueprint"),
        TEXT("set_widget_slot_properties"),
        TEXT("bind_widget_button_on_clicked"),
        TEXT("bind_widget_property"),
        TEXT("create_widget_animation"),
        TEXT("compile_widget_blueprint"),
        TEXT("create_ui_template")
    };

    if (SlowDeferredCommands.Contains(CommandType))
    {
        return 0.5;
    }

    static const TSet<FString> AssetLifecycleCommands = {
        TEXT("create_blueprint"),
        TEXT("compile_blueprint"),
        TEXT("create_folder"),
        TEXT("delete_folder"),
        TEXT("move_asset"),
        TEXT("copy_asset"),
        TEXT("duplicate_asset"),
        TEXT("rename_asset"),
        TEXT("delete_asset"),
        TEXT("unload_asset"),
        TEXT("save_assets"),
        TEXT("set_asset_metadata"),
        TEXT("tag_asset"),
        TEXT("fixup_redirectors"),
        TEXT("set_blueprint_parent_class"),
        TEXT("set_blueprint_class_settings"),
        TEXT("set_blueprint_class_defaults"),
        TEXT("set_component_defaults"),
        TEXT("edit_construction_script"),
        TEXT("create_event_dispatcher"),
        TEXT("bind_event_dispatcher"),
        TEXT("create_enum"),
        TEXT("create_struct"),
        TEXT("edit_enum"),
        TEXT("edit_struct"),
        TEXT("create_blueprint_interface"),
        TEXT("implement_interface"),
        TEXT("create_function_library"),
        TEXT("create_macro_library"),
        TEXT("add_comment_node"),
        TEXT("add_reroute_node"),
        TEXT("create_collapsed_graph"),
        TEXT("create_macro_graph"),
        TEXT("create_macro_instance"),
        TEXT("create_timeline"),
        TEXT("edit_timeline"),
        TEXT("bulk_rename"),
        TEXT("bulk_move"),
        TEXT("bulk_delete"),
        TEXT("create_primary_asset_label"),
        TEXT("delete_primary_asset_label"),
        TEXT("set_asset_manager_settings"),
        TEXT("add_primary_asset_bundle"),
        // Gameplay Framework commands that modify assets
        TEXT("create_gamemode_blueprint"),
        TEXT("create_gamemode_cpp_class"),
        TEXT("create_gamestate"),
        TEXT("create_playerstate"),
        TEXT("create_playercontroller"),
        TEXT("create_aicontroller"),
        TEXT("create_pawn"),
        TEXT("create_character"),
        TEXT("set_hud_class"),
        TEXT("set_spectator_pawn"),
        TEXT("set_camera_manager"),
        TEXT("setup_camera_component"),
        TEXT("setup_spring_arm"),
        TEXT("create_savegame_class"),
        TEXT("create_gameinstance"),
        TEXT("create_gameinstance_subsystem"),
        TEXT("create_world_subsystem"),
        TEXT("create_localplayer_subsystem"),
        TEXT("setup_gameplay_tags"),
        TEXT("add_gameplay_tag"),
        TEXT("place_player_start"),
        TEXT("create_widget_blueprint"),
        TEXT("add_widget_to_widget_blueprint"),
        TEXT("remove_widget_from_widget_blueprint"),
        TEXT("set_widget_text"),
        TEXT("set_widget_font"),
        TEXT("set_widget_color"),
        TEXT("set_widget_brush"),
        TEXT("set_widget_style"),
        TEXT("bind_widget_button_on_clicked"),
        TEXT("bind_widget_property"),
        TEXT("create_widget_animation"),
        TEXT("compile_widget_blueprint"),
        TEXT("create_ui_template")
    };

    return AssetLifecycleCommands.Contains(CommandType) ? 0.25 : 0.0;
}

FCriticalSection GDeferredEditorCommandMutex;
double GLastDeferredEditorCommandEndSeconds = 0.0;
}

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
    MaterialCommands = MakeShared<FEpicUnrealMCPMaterialCommands>();
    ProjectEditorCommands = MakeShared<FEpicUnrealMCPProjectEditorCommands>();
    ContentBrowserCommands = MakeShared<FEpicUnrealMCPContentBrowserCommands>();
    AssetImportCommands = MakeShared<FEpicUnrealMCPAssetImportCommands>();
    MeshEditingCommands = MakeShared<FEpicUnrealMCPMeshEditingCommands>();
    EnhancedInputCommands = MakeShared<FEpicUnrealMCPEnhancedInputCommands>();
    GameplayFrameworkCommands = MakeShared<FEpicUnrealMCPGameplayFrameworkCommands>();
    UMGCommands = MakeShared<FEpicUnrealMCPUMGCommands>();
    RenderingCommands = MakeShared<FEpicUnrealMCPRenderingCommands>();
    LightingAtmosphereCommands = MakeShared<FEpicUnrealMCPLightingAtmosphereCommands>();
    DataTableCommands = MakeShared<FEpicUnrealMCPDataTableCommands>();
    AudioCommands = MakeShared<FEpicUnrealMCPAudioCommands>();
    SequencerCommands = MakeShared<FEpicUnrealMCPSequencerCommands>();
    VroidCommands = MakeShared<FEpicUnrealMCPVroidCommands>();

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
    MaterialCommands.Reset();
    ProjectEditorCommands.Reset();
    ContentBrowserCommands.Reset();
    AssetImportCommands.Reset();
    MeshEditingCommands.Reset();
    EnhancedInputCommands.Reset();
    GameplayFrameworkCommands.Reset();
    UMGCommands.Reset();
    RenderingCommands.Reset();
    LightingAtmosphereCommands.Reset();
    DataTableCommands.Reset();
    AudioCommands.Reset();
    SequencerCommands.Reset();
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
    // Command routing: 0=ping, 1=EditorCommands, 2=BlueprintCommands, 3=BlueprintGraphCommands, 4=MaterialCommands, 5=ProjectEditorCommands, 6=ContentBrowserCommands, 7=AssetImportCommands, 8=MeshEditingCommands, 9=EnhancedInputCommands, 10=GameplayFrameworkCommands, 11=UMGCommands
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
            {TEXT("create_behavior_tree"), 1},
            {TEXT("create_blackboard"), 1},
            {TEXT("create_nav_modifier_volume"), 1},
            {TEXT("create_nav_link_proxy"), 1},
            {TEXT("set_actor_collision_preset"), 1},
            {TEXT("set_actor_physics"), 1},
            {TEXT("create_physical_material"), 1},
            {TEXT("spawn_radial_force"), 1},
            {TEXT("spawn_physics_constraint"), 1},
            {TEXT("compile_all_blueprints"), 1},
            {TEXT("run_map_check"), 1},
            {TEXT("find_broken_references"), 1},
            {TEXT("create_draft_proxy"), 1},
            {TEXT("update_draft_proxy"), 1},
            {TEXT("delete_draft_proxy"), 1},
            {TEXT("spawn_instance_set"), 1},
            {TEXT("update_instance_set"), 1},
            {TEXT("delete_instance_set"), 1},
            {TEXT("get_instance_set_state"), 1},
            {TEXT("list_instance_sets"), 1},
            {TEXT("request_cognitive_processing"), 1},
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
            {TEXT("set_blueprint_parent_class"), 2},
            {TEXT("set_blueprint_class_settings"), 2},
            {TEXT("set_blueprint_class_defaults"), 2},
            {TEXT("set_component_defaults"), 2},
            {TEXT("edit_construction_script"), 2},
            {TEXT("create_event_dispatcher"), 2},
            {TEXT("bind_event_dispatcher"), 2},
            {TEXT("create_enum"), 2},
            {TEXT("create_struct"), 2},
            {TEXT("edit_enum"), 2},
            {TEXT("edit_struct"), 2},
            {TEXT("create_blueprint_interface"), 2},
            {TEXT("implement_interface"), 2},
            {TEXT("create_function_library"), 2},
            {TEXT("create_macro_library"), 2},
            {TEXT("add_comment_node"), 2},
            {TEXT("add_reroute_node"), 2},
            {TEXT("format_graph"), 2},
            {TEXT("create_collapsed_graph"), 2},
            {TEXT("create_macro_graph"), 2},
            {TEXT("create_macro_instance"), 2},
            {TEXT("create_timeline"), 2},
            {TEXT("edit_timeline"), 2},
            {TEXT("set_blueprint_breakpoint"), 2},
            {TEXT("set_blueprint_watch"), 2},
            {TEXT("clear_blueprint_watches"), 2},
            {TEXT("step_blueprint_debugger"), 2},
            {TEXT("get_blueprint_debug_info"), 2},
            {TEXT("blueprint_diff"), 2},
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
            {TEXT("create_material"), 4},
            {TEXT("analyze_material_graph"), 4},
            {TEXT("add_material_node"), 4},
            {TEXT("connect_material_nodes"), 4},
            {TEXT("create_material_instance"), 4},
            {TEXT("create_dynamic_material_instance"), 4},
            {TEXT("batch_update_material_parameters"), 4},
            {TEXT("set_material_scalar_parameter"), 4},
            {TEXT("set_material_vector_parameter"), 4},
            {TEXT("set_material_texture_parameter"), 4},
            {TEXT("set_material_static_switch_parameter"), 4},
            {TEXT("create_material_parameter_collection"), 4},
            {TEXT("edit_material_parameter_collection"), 4},
            {TEXT("create_advanced_material"), 4},
            // Project / Editor Commands (5)
            {TEXT("get_project_settings"), 5},
            {TEXT("set_project_setting"), 5},
            {TEXT("set_default_map"), 5},
            {TEXT("set_game_default_map"), 5},
            {TEXT("set_editor_startup_map"), 5},
            {TEXT("set_project_description"), 5},
            {TEXT("set_maps_and_modes"), 5},
            {TEXT("list_plugins"), 5},
            {TEXT("set_plugin_enabled"), 5},
            {TEXT("set_engine_scalability"), 5},
            {TEXT("set_rendering_setting"), 5},
            {TEXT("set_physics_setting"), 5},
            {TEXT("set_input_setting"), 5},
            {TEXT("set_collision_setting"), 5},
            {TEXT("set_ai_setting"), 5},
            {TEXT("set_navigation_setting"), 5},
            {TEXT("set_packaging_setting"), 5},
            {TEXT("get_world_settings"), 5},
            {TEXT("set_world_setting"), 5},
            {TEXT("create_level"), 5},
            {TEXT("save_level"), 5},
            {TEXT("load_level"), 5},
            {TEXT("duplicate_level"), 5},
            {TEXT("rename_level"), 5},
            {TEXT("delete_level"), 5},
            {TEXT("get_current_level"), 5},
            {TEXT("list_levels"), 5},
            {TEXT("get_persistent_level"), 5},
            {TEXT("add_sublevel"), 5},
            {TEXT("remove_sublevel"), 5},
            {TEXT("set_sublevel_visible"), 5},
            {TEXT("set_sublevel_loaded"), 5},
            {TEXT("create_streaming_volume"), 5},
            {TEXT("set_level_streaming_settings"), 5},
            {TEXT("enable_world_partition"), 5},
            {TEXT("set_world_partition_grid"), 5},
            {TEXT("get_world_partition_cells"), 5},
            {TEXT("load_world_partition_cell"), 5},
            {TEXT("unload_world_partition_cell"), 5},
            {TEXT("create_data_layer"), 5},
            {TEXT("add_actors_to_data_layer"), 5},
            {TEXT("remove_actors_from_data_layer"), 5},
            {TEXT("set_data_layer_enabled"), 5},
            {TEXT("create_hlod_layer"), 5},
            {TEXT("build_hlod"), 5},
            {TEXT("rebuild_hlod"), 5},
            {TEXT("set_one_file_per_actor"), 5},
            {TEXT("set_level_bounds"), 5},
            {TEXT("set_world_origin_rebasing"), 5},
            {TEXT("undo"), 5},
            {TEXT("redo"), 5},
            {TEXT("get_dirty_assets"), 5},
            {TEXT("save_all"), 5},
            {TEXT("save_asset"), 5},
            {TEXT("get_editor_log"), 5},
            {TEXT("create_utility_widget"), 5},
            {TEXT("create_utility_blueprint"), 5},
            {TEXT("execute_python_script"), 5},
            {TEXT("execute_commandlet"), 5},
            {TEXT("start_pie"), 5},
            {TEXT("stop_pie"), 5},
            {TEXT("get_play_state"), 5},
            {TEXT("start_standalone_game"), 5},
            {TEXT("start_simulate"), 5},
            {TEXT("get_camera_position"), 5},
            {TEXT("set_camera_position"), 5},
            {TEXT("viewport_action"), 5},
            {TEXT("take_screenshot"), 5},
            {TEXT("export_level"), 7},
            // Content Browser Commands (6)
            {TEXT("create_folder"), 6},
            {TEXT("delete_folder"), 6},
            {TEXT("list_assets"), 6},
            {TEXT("search_assets"), 6},
            {TEXT("resolve_asset_path"), 6},
            {TEXT("move_asset"), 6},
            {TEXT("copy_asset"), 6},
            {TEXT("duplicate_asset"), 6},
            {TEXT("rename_asset"), 6},
            {TEXT("delete_asset"), 6},
            {TEXT("load_asset"), 6},
            {TEXT("unload_asset"), 6},
            {TEXT("save_assets"), 6},
            {TEXT("get_asset_metadata"), 6},
            {TEXT("set_asset_metadata"), 6},
            {TEXT("tag_asset"), 6},
            {TEXT("find_redirectors"), 6},
            {TEXT("fixup_redirectors"), 6},
            {TEXT("find_unused_assets"), 6},
            {TEXT("get_asset_references"), 6},
            {TEXT("get_asset_dependencies"), 6},
            {TEXT("get_asset_reference_graph"), 6},
            {TEXT("audit_assets"), 6},
            {TEXT("asset_registry_search"), 6},
            {TEXT("bulk_rename"), 6},
            {TEXT("bulk_move"), 6},
            {TEXT("bulk_delete"), 6},
            {TEXT("create_primary_asset_label"), 6},
            {TEXT("delete_primary_asset_label"), 6},
            {TEXT("list_primary_asset_labels"), 6},
            {TEXT("get_asset_manager_settings"), 6},
            {TEXT("set_asset_manager_settings"), 6},
            {TEXT("add_primary_asset_bundle"), 6},
            // Asset Import/Export Commands (7)
            {TEXT("import_fbx_mesh"), 7},
            {TEXT("import_texture"), 7},
            {TEXT("import_audio"), 7},
            {TEXT("import_gltf"), 7},
            {TEXT("import_obj"), 7},
            {TEXT("import_usd"), 7},
            {TEXT("import_mp3"), 7},
            {TEXT("import_alembic"), 7},
            {TEXT("import_datasmith"), 7},
            {TEXT("reimport_asset"), 7},
            {TEXT("save_import_preset"), 7},
            {TEXT("load_import_preset"), 7},
            {TEXT("export_asset"), 7},

            // Mesh Editing Commands (8)
            {TEXT("get_static_mesh_details"), 8},
            {TEXT("set_nanite_settings"), 8},
            {TEXT("set_lightmap_settings"), 8},
            {TEXT("edit_mesh_bounds"), 8},
            {TEXT("generate_collision"), 8},
            {TEXT("set_collision_complexity"), 8},
            {TEXT("add_simple_collision"), 8},
            {TEXT("remove_collisions"), 8},
            {TEXT("set_lod_group"), 8},
            {TEXT("add_socket"), 8},
            {TEXT("remove_socket"), 8},
            {TEXT("update_socket"), 8},
            {TEXT("mesh_boolean"), 8},
            {TEXT("mesh_remesh"), 8},
            {TEXT("mesh_simplify"), 8},
            {TEXT("mesh_uv_unwrap"), 8},
            {TEXT("mesh_voxel_remesh"), 8},
            {TEXT("mesh_uv_layout"), 8},
            {TEXT("set_pivot"), 8},
            {TEXT("mesh_merge"), 8},
            {TEXT("set_vertex_colors"), 8},
            {TEXT("mesh_bake"), 8},
            {TEXT("poly_edit"), 8},
            {TEXT("modeling_tool_execute"), 8},
            {TEXT("generate_lods"), 8},
            {TEXT("generate_lightmap_uvs"), 8},
            {TEXT("import_ucx_collision"), 8},
            
            // UV Operations (UStaticMeshEditorSubsystem alternatives)
            {TEXT("generate_box_uv_channel"), 8},
            {TEXT("generate_planar_uv_channel"), 8},
            {TEXT("generate_cylindrical_uv_channel"), 8},
            {TEXT("add_uv_channel"), 8},
            {TEXT("remove_uv_channel"), 8},
            
            // LOD Operations (UStaticMeshEditorSubsystem alternatives)
            {TEXT("set_lods"), 8},
            {TEXT("remove_lods"), 8},
            
            // Mesh Merge Operations (UStaticMeshEditorSubsystem alternatives)
            {TEXT("join_static_mesh_actors"), 8},
            {TEXT("merge_static_mesh_actors"), 8},
            {TEXT("create_proxy_mesh_actor"), 8},
            
            // Other utilities
            {TEXT("set_generate_lightmap_uvs"), 8},
            {TEXT("has_vertex_colors"), 8},

            // Enhanced Input Commands (9)
            {TEXT("create_input_action"), 9},
            {TEXT("create_input_mapping_context"), 9},
            {TEXT("add_enhanced_input_mapping"), 9},
            {TEXT("remove_enhanced_input_mapping"), 9},
            {TEXT("configure_enhanced_input_action"), 9},
            {TEXT("configure_enhanced_input_mapping"), 9},
            {TEXT("list_enhanced_input_assets"), 9},
            {TEXT("get_enhanced_input_debug_info"), 9},
            {TEXT("add_runtime_mapping_context"), 9},
            {TEXT("remove_runtime_mapping_context"), 9},
            {TEXT("setup_enhanced_input_binding"), 9},
            {TEXT("setup_rebind_ui"), 9},
            {TEXT("rebind_enhanced_input_key"), 9},
            {TEXT("configure_local_multiplayer_input"), 9},

            // Gameplay Framework Commands (10)
            {TEXT("create_gamemode_blueprint"), 10},
            {TEXT("create_gamemode_cpp_class"), 10},
            {TEXT("set_default_gamemode"), 10},
            {TEXT("create_gamestate"), 10},
            {TEXT("create_playerstate"), 10},
            {TEXT("create_playercontroller"), 10},
            {TEXT("create_aicontroller"), 10},
            {TEXT("create_pawn"), 10},
            {TEXT("create_character"), 10},
            {TEXT("set_default_pawn"), 10},
            {TEXT("set_hud_class"), 10},
            {TEXT("set_spectator_pawn"), 10},
            {TEXT("place_player_start"), 10},
            {TEXT("set_spawn_rules"), 10},
            {TEXT("set_possess_rules"), 10},
            {TEXT("set_camera_manager"), 10},
            {TEXT("setup_camera_component"), 10},
            {TEXT("setup_spring_arm"), 10},
            {TEXT("create_savegame_class"), 10},
            {TEXT("create_gameinstance"), 10},
            {TEXT("create_gameinstance_subsystem"), 10},
            {TEXT("create_world_subsystem"), 10},
            {TEXT("create_localplayer_subsystem"), 10},
            {TEXT("setup_gameplay_tags"), 10},
            {TEXT("add_gameplay_tag"), 10},
            {TEXT("create_gameplay_tag_query"), 10},
            {TEXT("save_game_to_slot"), 10},
            {TEXT("load_game_from_slot"), 10},
            {TEXT("delete_save_slot"), 10},
            {TEXT("has_save_game"), 10},

            // UI / UMG / Common UI Commands (11)
            {TEXT("create_widget_blueprint"), 11},
            {TEXT("add_widget_to_widget_blueprint"), 11},
            {TEXT("remove_widget_from_widget_blueprint"), 11},
            {TEXT("set_widget_slot_properties"), 11},
            {TEXT("set_widget_text"), 11},
            {TEXT("set_widget_font"), 11},
            {TEXT("set_widget_color"), 11},
            {TEXT("set_widget_brush"), 11},
            {TEXT("set_widget_style"), 11},
            {TEXT("bind_widget_button_on_clicked"), 11},
            {TEXT("bind_widget_property"), 11},
            {TEXT("create_widget_animation"), 11},
            {TEXT("compile_widget_blueprint"), 11},
            {TEXT("inspect_widget_blueprint"), 11},
            {TEXT("add_widget_to_viewport"), 11},
            {TEXT("remove_widget_from_parent"), 11},
            {TEXT("click_widget_button"), 11},
            {TEXT("set_ui_input_mode"), 11},
            {TEXT("set_mouse_cursor_visible"), 11},
            {TEXT("create_ui_template"), 11},
            {TEXT("create_widget_instance"), 11},

            // Rendering Commands (12)
            {TEXT("set_global_illumination"), 12},
            {TEXT("set_lumen_enabled"), 12},
            {TEXT("set_lumen_scene_detail"), 12},
            {TEXT("set_lumen_reflection_quality"), 12},
            {TEXT("set_hardware_ray_tracing"), 12},
            {TEXT("set_path_tracing"), 12},
            {TEXT("set_virtual_shadow_maps"), 12},
            {TEXT("set_shadow_quality"), 12},
            {TEXT("set_anti_aliasing"), 12},
            {TEXT("set_tsr_settings"), 12},
            {TEXT("set_upscaler"), 12},
            {TEXT("set_nanite_visualization"), 12},
            {TEXT("get_shader_compile_status"), 12},
            {TEXT("set_post_process_volume"), 12},
            {TEXT("spawn_camera_actor"), 12},
            {TEXT("spawn_cine_camera_actor"), 12},
            {TEXT("set_camera_properties"), 12},
            {TEXT("spawn_post_process_volume"), 12},

            // Lighting / Atmosphere Commands (13)
            {TEXT("set_light_intensity"), 13},
            {TEXT("set_light_color"), 13},
            {TEXT("set_light_temperature"), 13},
            {TEXT("set_light_mobility"), 13},
            {TEXT("set_light_shadow_enabled"), 13},
            {TEXT("set_light_shadow_bias"), 13},
            {TEXT("set_light_contact_shadows"), 13},
            {TEXT("set_light_volumetric_scattering"), 13},
            {TEXT("set_light_attenuation_radius"), 13},
            {TEXT("set_light_cone_angles"), 13},
            {TEXT("set_light_source_radius"), 13},
            {TEXT("set_light_ies_profile"), 13},
            {TEXT("set_light_channel"), 13},
            {TEXT("set_rect_light_properties"), 13},
            {TEXT("set_sky_light_properties"), 13},
            {TEXT("set_sky_atmosphere_properties"), 13},
            {TEXT("set_height_fog_properties"), 13},
            {TEXT("set_volumetric_fog"), 13},
            {TEXT("set_directional_light_as_sun"), 13},
            {TEXT("set_sun_position"), 13},
            {TEXT("create_hdri_backdrop"), 13},
            {TEXT("create_reflection_capture"), 13},
            {TEXT("set_reflection_capture_settings"), 13},
            {TEXT("build_reflection_captures"), 13},
            {TEXT("create_lightmass_importance_volume"), 13},
            {TEXT("build_lighting"), 13},
            {TEXT("set_lighting_scenario"), 13},
            {TEXT("set_megaliights"), 13},

            // Data Table Commands (14)
            {TEXT("create_data_table"), 14},
            {TEXT("import_csv_to_data_table"), 14},
            {TEXT("add_data_table_row"), 14},
            {TEXT("delete_data_table_row"), 14},
            {TEXT("update_data_table_row"), 14},
            {TEXT("export_data_table_csv"), 14},
            {TEXT("export_data_table_json"), 14},

            // Audio Commands (15)
            {TEXT("create_sound_cue"), 15},
            {TEXT("add_audio_component"), 15},
            {TEXT("set_sound_attenuation"), 15},
            {TEXT("create_sound_class"), 15},
            {TEXT("create_sound_mix"), 15},
            {TEXT("spawn_ambient_sound"), 15},

            // Sequencer Commands (16)
            {TEXT("create_level_sequence"), 16},
            {TEXT("add_actor_binding"), 16},
            {TEXT("add_transform_track"), 16},
            {TEXT("add_camera_cut_track"), 16},
            {TEXT("add_event_track"), 16},
            {TEXT("add_keyframe"), 16},
            {TEXT("set_playback_range"), 16},
            {TEXT("set_frame_rate"), 16},

            // VRM / Avatar Commands (17)
            {TEXT("vroid_check_plugin"), 17},
            {TEXT("vroid_import_vrm"), 17},
            {TEXT("vroid_spawn_avatar"), 17},
            {TEXT("vroid_validate_avatar_asset"), 17}
        };
        const int32* Found = Router.Find(CommandType);
        return Found ? *Found : -1;
    }
}

FString UEpicUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Log, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);

    const int32 Route = RouteCommand(CommandType);

    auto ExecuteOnCurrentThread = [this, CommandType, Params, Route]() -> FString
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
            case 4: // MaterialCommands
                ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
                break;
            case 5: // ProjectEditorCommands
                ResultJson = ProjectEditorCommands->HandleCommand(CommandType, Params);
                break;
            case 6: // ContentBrowserCommands
                ResultJson = ContentBrowserCommands->HandleCommand(CommandType, Params);
                break;
            case 7: // AssetImportCommands
                ResultJson = AssetImportCommands->HandleCommand(CommandType, Params);
                break;
            case 8: // MeshEditingCommands
                ResultJson = MeshEditingCommands->HandleCommand(CommandType, Params);
                break;
            case 9: // EnhancedInputCommands
                ResultJson = EnhancedInputCommands->HandleCommand(CommandType, Params);
                break;
            case 10: // GameplayFrameworkCommands
                ResultJson = GameplayFrameworkCommands->HandleCommand(CommandType, Params);
                break;
            case 11: // UMGCommands
                ResultJson = UMGCommands->HandleCommand(CommandType, Params);
                break;
            case 12: // RenderingCommands
                ResultJson = RenderingCommands->HandleCommand(CommandType, Params);
                break;
            case 13: // LightingAtmosphereCommands
                ResultJson = LightingAtmosphereCommands->HandleCommand(CommandType, Params);
                break;
            case 14: // DataTableCommands
                ResultJson = DataTableCommands->HandleCommand(CommandType, Params);
                break;
            case 15: // AudioCommands
                ResultJson = AudioCommands->HandleCommand(CommandType, Params);
                break;
            case 16: // SequencerCommands
                ResultJson = SequencerCommands->HandleCommand(CommandType, Params);
                break;
            case 17: // VroidCommands
                ResultJson = VroidCommands->HandleCommand(CommandType, Params);
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

    const double DeferredEditorCommandDelaySeconds = GetDeferredEditorCommandDelaySeconds(CommandType);
    if (DeferredEditorCommandDelaySeconds > 0.0)
    {
        FScopeLock DeferredCommandLock(&GDeferredEditorCommandMutex);
        const double SecondsUntilSafeDispatch =
            (GLastDeferredEditorCommandEndSeconds + DeferredEditorCommandDelaySeconds) - FPlatformTime::Seconds();
        if (SecondsUntilSafeDispatch > 0.0)
        {
            FPlatformProcess::Sleep(static_cast<float>(SecondsUntilSafeDispatch));
        }

        TSharedRef<TPromise<FString>, ESPMode::ThreadSafe> Promise =
            MakeShared<TPromise<FString>, ESPMode::ThreadSafe>();
        TFuture<FString> Future = Promise->GetFuture();

        FTSTicker::GetCoreTicker().AddTicker(
            TEXT("UnrealMCP.DeferredEditorCommand"),
            0.0f,
            [ExecuteOnCurrentThread, Promise](float) mutable
            {
                Promise->SetValue(ExecuteOnCurrentThread());
                return false;
            }
        );

        FString Result = Future.Get();
        GLastDeferredEditorCommandEndSeconds = FPlatformTime::Seconds();
        return Result;
    }

    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    AsyncTask(ENamedThreads::GameThread, [ExecuteOnCurrentThread, Promise = MoveTemp(Promise)]() mutable
    {
        Promise.SetValue(ExecuteOnCurrentThread());
    });

    return Future.Get();
}
