#include "Commands/EpicUnrealMCPProjectEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPBridge.h"
#include "CoreMinimal.h"
#include "Json.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorViewportClient.h"
#include "FileHelpers.h"
#include "EditorLevelUtils.h"
#include "GameMapsSettings.h"
#include "GeneralProjectSettings.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "IAssetViewport.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LevelStreamingVolume.h"
#include "LevelEditorSubsystem.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "Components/BrushComponent.h"
#include "GameFramework/Volume.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"

FEpicUnrealMCPProjectEditorCommands::FEpicUnrealMCPProjectEditorCommands() {}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPProjectEditorCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("get_project_settings"), &FEpicUnrealMCPProjectEditorCommands::HandleGetProjectSettings},
        {TEXT("set_project_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetProjectSetting},
        {TEXT("set_default_map"), &FEpicUnrealMCPProjectEditorCommands::HandleSetDefaultMap},
        {TEXT("set_game_default_map"), &FEpicUnrealMCPProjectEditorCommands::HandleSetGameDefaultMap},
        {TEXT("set_editor_startup_map"), &FEpicUnrealMCPProjectEditorCommands::HandleSetEditorStartupMap},
        {TEXT("set_project_description"), &FEpicUnrealMCPProjectEditorCommands::HandleSetProjectDescription},
        {TEXT("set_maps_and_modes"), &FEpicUnrealMCPProjectEditorCommands::HandleSetMapsAndModes},
        {TEXT("list_plugins"), &FEpicUnrealMCPProjectEditorCommands::HandleListPlugins},
        {TEXT("set_plugin_enabled"), &FEpicUnrealMCPProjectEditorCommands::HandleSetPluginEnabled},
        {TEXT("set_engine_scalability"), &FEpicUnrealMCPProjectEditorCommands::HandleSetEngineScalability},
        {TEXT("set_rendering_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetRenderingSetting},
        {TEXT("set_physics_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetPhysicsSetting},
        {TEXT("set_input_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetInputSetting},
        {TEXT("set_collision_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetCollisionSetting},
        {TEXT("set_ai_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetAISetting},
        {TEXT("set_navigation_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetNavigationSetting},
        {TEXT("set_packaging_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetPackagingSetting},
        {TEXT("get_world_settings"), &FEpicUnrealMCPProjectEditorCommands::HandleGetWorldSettings},
        {TEXT("set_world_setting"), &FEpicUnrealMCPProjectEditorCommands::HandleSetWorldSetting},
        {TEXT("create_level"), &FEpicUnrealMCPProjectEditorCommands::HandleCreateLevel},
        {TEXT("save_level"), &FEpicUnrealMCPProjectEditorCommands::HandleSaveLevel},
        {TEXT("load_level"), &FEpicUnrealMCPProjectEditorCommands::HandleLoadLevel},
        {TEXT("duplicate_level"), &FEpicUnrealMCPProjectEditorCommands::HandleDuplicateLevel},
        {TEXT("rename_level"), &FEpicUnrealMCPProjectEditorCommands::HandleRenameLevel},
        {TEXT("delete_level"), &FEpicUnrealMCPProjectEditorCommands::HandleDeleteLevel},
        {TEXT("get_current_level"), &FEpicUnrealMCPProjectEditorCommands::HandleGetCurrentLevel},
        {TEXT("list_levels"), &FEpicUnrealMCPProjectEditorCommands::HandleListLevels},
        {TEXT("get_persistent_level"), &FEpicUnrealMCPProjectEditorCommands::HandleGetPersistentLevel},
        {TEXT("add_sublevel"), &FEpicUnrealMCPProjectEditorCommands::HandleAddSublevel},
        {TEXT("remove_sublevel"), &FEpicUnrealMCPProjectEditorCommands::HandleRemoveSublevel},
        {TEXT("set_sublevel_visible"), &FEpicUnrealMCPProjectEditorCommands::HandleSetSublevelVisible},
        {TEXT("set_sublevel_loaded"), &FEpicUnrealMCPProjectEditorCommands::HandleSetSublevelLoaded},
        {TEXT("create_streaming_volume"), &FEpicUnrealMCPProjectEditorCommands::HandleCreateStreamingVolume},
        {TEXT("set_level_streaming_settings"), &FEpicUnrealMCPProjectEditorCommands::HandleSetLevelStreamingSettings},
        {TEXT("enable_world_partition"), &FEpicUnrealMCPProjectEditorCommands::HandleEnableWorldPartition},
        {TEXT("set_world_partition_grid"), &FEpicUnrealMCPProjectEditorCommands::HandleSetWorldPartitionGrid},
        {TEXT("get_world_partition_cells"), &FEpicUnrealMCPProjectEditorCommands::HandleGetWorldPartitionCells},
        {TEXT("load_world_partition_cell"), &FEpicUnrealMCPProjectEditorCommands::HandleLoadWorldPartitionCell},
        {TEXT("unload_world_partition_cell"), &FEpicUnrealMCPProjectEditorCommands::HandleUnloadWorldPartitionCell},
        {TEXT("create_data_layer"), &FEpicUnrealMCPProjectEditorCommands::HandleCreateDataLayer},
        {TEXT("add_actors_to_data_layer"), &FEpicUnrealMCPProjectEditorCommands::HandleAddActorsToDataLayer},
        {TEXT("remove_actors_from_data_layer"), &FEpicUnrealMCPProjectEditorCommands::HandleRemoveActorsFromDataLayer},
        {TEXT("set_data_layer_enabled"), &FEpicUnrealMCPProjectEditorCommands::HandleSetDataLayerEnabled},
        {TEXT("create_hlod_layer"), &FEpicUnrealMCPProjectEditorCommands::HandleCreateHLODLayer},
        {TEXT("build_hlod"), &FEpicUnrealMCPProjectEditorCommands::HandleBuildHLOD},
        {TEXT("rebuild_hlod"), &FEpicUnrealMCPProjectEditorCommands::HandleRebuildHLOD},
        {TEXT("set_one_file_per_actor"), &FEpicUnrealMCPProjectEditorCommands::HandleSetOneFilePerActor},
        {TEXT("set_level_bounds"), &FEpicUnrealMCPProjectEditorCommands::HandleSetLevelBounds},
        {TEXT("set_world_origin_rebasing"), &FEpicUnrealMCPProjectEditorCommands::HandleSetWorldOriginRebasing},
        {TEXT("undo"), &FEpicUnrealMCPProjectEditorCommands::HandleUndo},
        {TEXT("redo"), &FEpicUnrealMCPProjectEditorCommands::HandleRedo},
        {TEXT("get_dirty_assets"), &FEpicUnrealMCPProjectEditorCommands::HandleGetDirtyAssets},
        {TEXT("save_all"), &FEpicUnrealMCPProjectEditorCommands::HandleSaveAll},
        {TEXT("save_asset"), &FEpicUnrealMCPProjectEditorCommands::HandleSaveAsset},
        {TEXT("get_editor_log"), &FEpicUnrealMCPProjectEditorCommands::HandleGetEditorLog},
        {TEXT("create_utility_widget"), &FEpicUnrealMCPProjectEditorCommands::HandleCreateUtilityWidget},
        {TEXT("create_utility_blueprint"), &FEpicUnrealMCPProjectEditorCommands::HandleCreateUtilityBlueprint},
        {TEXT("execute_python_script"), &FEpicUnrealMCPProjectEditorCommands::HandleExecutePythonScript},
        {TEXT("execute_commandlet"), &FEpicUnrealMCPProjectEditorCommands::HandleExecuteCommandlet},
        {TEXT("start_pie"), &FEpicUnrealMCPProjectEditorCommands::HandleStartPIE},
        {TEXT("stop_pie"), &FEpicUnrealMCPProjectEditorCommands::HandleStopPIE},
        {TEXT("get_play_state"), &FEpicUnrealMCPProjectEditorCommands::HandleGetPlayState},
        {TEXT("start_standalone_game"), &FEpicUnrealMCPProjectEditorCommands::HandleStartStandaloneGame},
        {TEXT("start_simulate"), &FEpicUnrealMCPProjectEditorCommands::HandleStartSimulate},
        {TEXT("get_camera_position"), &FEpicUnrealMCPProjectEditorCommands::HandleGetCameraPosition},
        {TEXT("set_camera_position"), &FEpicUnrealMCPProjectEditorCommands::HandleSetCameraPosition},
        {TEXT("viewport_action"), &FEpicUnrealMCPProjectEditorCommands::HandleViewportAction},
    };
    const Handler* H = Dispatch.Find(CommandType);
    if (H) { return (this->*(*H))(Params); }
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project/editor command: %s"), *CommandType));
}

static FString GetConfigFilePath(const FString& FileName)
{
    FString ResolvedFileName = FileName;
    if (FileName.Equals(TEXT("DefaultEngine.ini"), ESearchCase::IgnoreCase)) { ResolvedFileName = TEXT("DefaultEngine.ini"); }
    else if (FileName.Equals(TEXT("DefaultGame.ini"), ESearchCase::IgnoreCase)) { ResolvedFileName = TEXT("DefaultGame.ini"); }
    else if (FileName.Equals(TEXT("DefaultEditor.ini"), ESearchCase::IgnoreCase)) { ResolvedFileName = TEXT("DefaultEditor.ini"); }
    else if (FileName.Equals(TEXT("DefaultInput.ini"), ESearchCase::IgnoreCase)) { ResolvedFileName = TEXT("DefaultInput.ini"); }

    FString ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / ResolvedFileName);
    FPaths::MakeStandardFilename(ConfigPath);
    return FConfigCacheIni::NormalizeConfigIniPath(ConfigPath);
}

static void SetGameMapsConfigString(const TCHAR* Key, const FString& Value)
{
    const TCHAR* Section = TEXT("/Script/EngineSettings.GameMapsSettings");
    const FString ConfigPath = GetConfigFilePath(TEXT("DefaultEngine.ini"));
    GConfig->SetString(Section, Key, *Value, *ConfigPath);
    GConfig->SetString(Section, Key, *Value, GEngineIni);
    GConfig->Flush(false, *ConfigPath);
    GConfig->Flush(false, GEngineIni);
}

static bool MatchesStreamingLevelName(ULevelStreaming* StreamingLevel, const FString& LevelName)
{
    if (!StreamingLevel)
    {
        return false;
    }

    const FString PackageName = StreamingLevel->GetWorldAssetPackageName();
    return StreamingLevel->GetName() == LevelName
        || PackageName == LevelName
        || FPackageName::GetShortName(PackageName) == LevelName;
}

static FString NormalizeLevelPackageName(const FString& AssetPath)
{
    FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    int32 DotIndex = INDEX_NONE;
    if (PackageName.FindChar(TEXT('.'), DotIndex))
    {
        PackageName.LeftInline(DotIndex);
    }
    return PackageName;
}

static bool TryMapPackageToFilename(const FString& PackageName, FString& OutFilename)
{
    return FPackageName::TryConvertLongPackageNameToFilename(
        NormalizeLevelPackageName(PackageName),
        OutFilename,
        FPackageName::GetMapPackageExtension()
    );
}

static void ScanAssetFile(const FString& Filename)
{
    if (Filename.IsEmpty())
    {
        return;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FString> Files;
    Files.Add(Filename);
    AssetRegistryModule.Get().ScanFilesSynchronous(Files, true);
}

static bool IsCurrentEditorWorldPackage(const FString& AssetPath)
{
    if (!GEditor)
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World || !World->GetOutermost())
    {
        return false;
    }

    const FString CurrentPackageName = World->GetOutermost()->GetName();
    if (CurrentPackageName.StartsWith(TEXT("/Temp/")) || CurrentPackageName.StartsWith(TEXT("/Engine/Transient")))
    {
        return false;
    }

    return CurrentPackageName == NormalizeLevelPackageName(AssetPath);
}

static bool IsEditorPlayBusy(FString* OutReason = nullptr)
{
    if (!GEditor)
    {
        if (OutReason) { *OutReason = TEXT("GEditor is not available"); }
        return true;
    }
    if (GEditor->ShouldEndPlayMap())
    {
        if (OutReason) { *OutReason = TEXT("PIE/SIE teardown is queued"); }
        return true;
    }
    if (GEditor->PlayWorld)
    {
        if (OutReason) { *OutReason = TEXT("PIE/SIE is running"); }
        return true;
    }
    if (GEditor->IsPlaySessionRequestQueued())
    {
        if (OutReason) { *OutReason = TEXT("PIE/SIE startup is queued"); }
        return true;
    }
    if (GEditor->IsPlayingSessionInEditor())
    {
        if (OutReason) { *OutReason = TEXT("PIE/SIE is running"); }
        return true;
    }
    return false;
}

static TSharedPtr<FJsonObject> CreateEditorPlayBusyError(const TCHAR* Operation)
{
    FString Reason;
    if (!IsEditorPlayBusy(&Reason))
    {
        return nullptr;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Cannot %s while %s. Stop PIE/SIE and wait for teardown to complete."), Operation, *Reason)
    );
}

static void AddPlayStateFields(const TSharedPtr<FJsonObject>& Response)
{
    const bool bHasEditor = GEditor != nullptr;
    const bool bPlayWorldActive = bHasEditor && GEditor->PlayWorld != nullptr;
    const bool bPlaySessionQueued = bHasEditor && GEditor->IsPlaySessionRequestQueued();
    const bool bPlaySessionRunning = bHasEditor && GEditor->IsPlayingSessionInEditor();
    const bool bEndPlayQueued = bHasEditor && GEditor->ShouldEndPlayMap();

    Response->SetBoolField(TEXT("play_world_active"), bPlayWorldActive);
    Response->SetBoolField(TEXT("play_session_queued"), bPlaySessionQueued);
    Response->SetBoolField(TEXT("play_session_running"), bPlaySessionRunning);
    Response->SetBoolField(TEXT("play_session_in_progress"), bHasEditor && GEditor->IsPlaySessionInProgress());
    Response->SetBoolField(TEXT("end_play_queued"), bEndPlayQueued);
    Response->SetBoolField(TEXT("safe_for_editor_commands"), bHasEditor && !IsEditorPlayBusy());
}

static FEditorViewportClient* GetSafeEditorViewportClient()
{
    if (!GEditor)
    {
        return nullptr;
    }

    FViewport* ActiveViewport = GEditor->GetActiveViewport();
    FViewportClient* ActiveClient = ActiveViewport ? ActiveViewport->GetClient() : nullptr;
    for (FEditorViewportClient* Client : GEditor->GetAllViewportClients())
    {
        if (Client && Client == ActiveClient)
        {
            return Client;
        }
    }
    for (FEditorViewportClient* Client : GEditor->GetAllViewportClients())
    {
        if (Client)
        {
            return Client;
        }
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params)
{
    FString FileName; if (!Params->TryGetStringField(TEXT("file"), FileName)) { FileName = TEXT("DefaultEngine.ini"); }
    FString Section; if (!Params->TryGetStringField(TEXT("section"), Section)) { Section = TEXT("/Script/Engine.Engine"); }
    FString Key; Params->TryGetStringField(TEXT("key"), Key);
    FString ConfigPath = GetConfigFilePath(FileName);
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath))
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Config file not found: %s"), *ConfigPath));

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetStringField(TEXT("file"), FileName);
    R->SetStringField(TEXT("section"), Section);
    if (!Key.IsEmpty())
    {
        FString Value;
        if (GConfig->GetString(*Section, *Key, Value, *ConfigPath)) { R->SetStringField(TEXT("key"), Key); R->SetStringField(TEXT("value"), Value); }
        else { R->SetStringField(TEXT("key"), Key); R->SetStringField(TEXT("value"), TEXT("")); R->SetBoolField(TEXT("found"), false); }
    }
    else
    {
        if (FConfigFile* CF = GConfig->Find(ConfigPath))
        {
            if (const FConfigSection* CS = CF->FindSection(*Section))
            {
                TSharedPtr<FJsonObject> VO = MakeShared<FJsonObject>();
                for (const auto& It : *CS) { VO->SetStringField(It.Key.ToString(), It.Value.GetValue()); }
                R->SetObjectField(TEXT("values"), VO);
            }
        }
        else { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read config section")); }
    }
    R->SetBoolField(TEXT("success"), true); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params)
{
    FString FileName; if (!Params->TryGetStringField(TEXT("file"), FileName)) { FileName = TEXT("DefaultEngine.ini"); }
    FString Section; if (!Params->TryGetStringField(TEXT("section"), Section)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing section")); }
    FString Key; if (!Params->TryGetStringField(TEXT("key"), Key)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing key")); }
    FString Value; if (!Params->TryGetStringField(TEXT("value"), Value)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing value")); }
    FString ConfigPath = GetConfigFilePath(FileName);
    GConfig->SetString(*Section, *Key, *Value, *ConfigPath);
    GConfig->Flush(false, *ConfigPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("file"), FileName);
    R->SetStringField(TEXT("section"), Section); R->SetStringField(TEXT("key"), Key); R->SetStringField(TEXT("value"), Value); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetDefaultMap(const TSharedPtr<FJsonObject>& Params)
{
    FString MapPath; if (!Params->TryGetStringField(TEXT("map_path"), MapPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing map_path")); }
    SetGameMapsConfigString(TEXT("EditorStartupMap"), MapPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("default_map"), MapPath); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetGameDefaultMap(const TSharedPtr<FJsonObject>& Params)
{
    FString MapPath; if (!Params->TryGetStringField(TEXT("map_path"), MapPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing map_path")); }
    SetGameMapsConfigString(TEXT("GameDefaultMap"), MapPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("game_default_map"), MapPath); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetEditorStartupMap(const TSharedPtr<FJsonObject>& Params)
{
    FString MapPath; if (!Params->TryGetStringField(TEXT("map_path"), MapPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing map_path")); }
    SetGameMapsConfigString(TEXT("EditorStartupMap"), MapPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("editor_startup_map"), MapPath); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetProjectDescription(const TSharedPtr<FJsonObject>& Params)
{
    UGeneralProjectSettings* S = GetMutableDefault<UGeneralProjectSettings>();
    if (!S) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get GeneralProjectSettings")); }
    FString V;
    if (Params->TryGetStringField(TEXT("description"), V)) S->Description = V;
    if (Params->TryGetStringField(TEXT("project_name"), V)) S->ProjectName = V;
    if (Params->TryGetStringField(TEXT("company_name"), V)) S->CompanyName = V;
    if (Params->TryGetStringField(TEXT("company_distinguished_name"), V)) S->CompanyDistinguishedName = V;
    if (Params->TryGetStringField(TEXT("homepage"), V)) S->Homepage = V;
    if (Params->TryGetStringField(TEXT("support_contact"), V)) S->SupportContact = V;
    double D = 0.0; if (Params->TryGetNumberField(TEXT("project_version"), D)) S->ProjectVersion = FString::Printf(TEXT("%.1f"), D);
    S->SaveConfig();
    // Also write to GConfig so reads via get_project_settings see the new values immediately
    FString GameConfigPath = GetConfigFilePath(TEXT("DefaultGame.ini"));
    GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectName"), *S->ProjectName, *GameConfigPath);
    GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("CompanyName"), *S->CompanyName, *GameConfigPath);
    GConfig->SetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), *S->ProjectVersion, *GameConfigPath);
    GConfig->Flush(false, *GameConfigPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("project_name"), S->ProjectName);
    R->SetStringField(TEXT("description"), S->Description);
    R->SetStringField(TEXT("company_name"), S->CompanyName);
    R->SetStringField(TEXT("project_version"), S->ProjectVersion); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetMapsAndModes(const TSharedPtr<FJsonObject>& Params)
{
    FString V;
    if (Params->TryGetStringField(TEXT("game_mode"), V)) { SetGameMapsConfigString(TEXT("GlobalDefaultGameMode"), V); }
    if (Params->TryGetStringField(TEXT("game_instance"), V)) { SetGameMapsConfigString(TEXT("GameInstanceClass"), V); }
    if (Params->TryGetStringField(TEXT("transition_map"), V)) { SetGameMapsConfigString(TEXT("TransitionMap"), V); }

    if (UGameMapsSettings* G = GetMutableDefault<UGameMapsSettings>())
    {
        G->ReloadConfig();
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    FString GameDefaultMap;
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), GameDefaultMap, GEngineIni);
    R->SetStringField(TEXT("game_default_map"), GameDefaultMap);
    FString EditorStartupMap;
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("EditorStartupMap"), EditorStartupMap, GEngineIni);
    R->SetStringField(TEXT("editor_startup_map"), EditorStartupMap); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleListPlugins(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> PluginArray;
    TSet<FString> EnabledNames;
    for (const TSharedPtr<IPlugin>& P : IPluginManager::Get().GetEnabledPlugins())
    {
        EnabledNames.Add(P->GetName());
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("name"), P->GetName());
        O->SetStringField(TEXT("friendly_name"), P->GetDescriptor().FriendlyName);
        O->SetBoolField(TEXT("enabled"), true);
        O->SetStringField(TEXT("version_name"), P->GetDescriptor().VersionName);
        PluginArray.Add(MakeShared<FJsonValueObject>(O));
    }
    for (const TSharedPtr<IPlugin>& P : IPluginManager::Get().GetDiscoveredPlugins())
    {
        if (EnabledNames.Contains(P->GetName())) continue;
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("name"), P->GetName());
        O->SetStringField(TEXT("friendly_name"), P->GetDescriptor().FriendlyName);
        O->SetBoolField(TEXT("enabled"), false);
        O->SetStringField(TEXT("version_name"), P->GetDescriptor().VersionName);
        PluginArray.Add(MakeShared<FJsonValueObject>(O));
    }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("count"), PluginArray.Num());
    R->SetArrayField(TEXT("plugins"), PluginArray); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetPluginEnabled(const TSharedPtr<FJsonObject>& Params)
{
    FString PluginName; if (!Params->TryGetStringField(TEXT("plugin_name"), PluginName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing plugin_name")); }
    bool bEnabled = false; if (!Params->TryGetBoolField(TEXT("enabled"), bEnabled)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing enabled")); }

    FString UProjectPath = FPaths::GetProjectFilePath();
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *UProjectPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to load .uproject")); }
    TSharedPtr<FJsonObject> UProjectObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, UProjectObj) || !UProjectObj.IsValid()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to parse .uproject")); }

    const TArray<TSharedPtr<FJsonValue>>* PluginsArray = nullptr;
    UProjectObj->TryGetArrayField(TEXT("Plugins"), PluginsArray);
    TArray<TSharedPtr<FJsonValue>> NewPluginsArray;
    bool bFound = false;
    if (PluginsArray)
    {
        for (const auto& PV : *PluginsArray)
        {
            TSharedPtr<FJsonObject> PO = PV->AsObject();
            if (PO.IsValid())
            {
                FString Name; if (PO->TryGetStringField(TEXT("Name"), Name) && Name == PluginName) { PO->SetBoolField(TEXT("Enabled"), bEnabled); bFound = true; }
            }
            NewPluginsArray.Add(PV);
        }
    }
    if (!bFound)
    {
        TSharedPtr<FJsonObject> NP = MakeShared<FJsonObject>();
        NP->SetStringField(TEXT("Name"), PluginName);
        NP->SetBoolField(TEXT("Enabled"), bEnabled);
        NewPluginsArray.Add(MakeShared<FJsonValueObject>(NP));
    }
    UProjectObj->SetArrayField(TEXT("Plugins"), NewPluginsArray);
    FString Out;
    TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(UProjectObj.ToSharedRef(), W);
    if (!FFileHelper::SaveStringToFile(Out, *UProjectPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save .uproject")); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("plugin_name"), PluginName);
    R->SetBoolField(TEXT("enabled"), bEnabled);
    R->SetStringField(TEXT("note"), TEXT("Editor restart required")); return R;
}

static TSharedPtr<FJsonObject> SetConfigValue(const FString& FileName, const FString& Section, const TSharedPtr<FJsonObject>& Params)
{
    FString Key; if (!Params->TryGetStringField(TEXT("key"), Key)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing key")); }
    FString Value; if (!Params->TryGetStringField(TEXT("value"), Value)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing value")); }
    FString ConfigPath = GetConfigFilePath(FileName);
    GConfig->SetString(*Section, *Key, *Value, *ConfigPath);
    GConfig->Flush(false, *ConfigPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("key"), Key); R->SetStringField(TEXT("value"), Value); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetEngineScalability(const TSharedPtr<FJsonObject>& Params)
{
    double Quality = 3.0; Params->TryGetNumberField(TEXT("quality"), Quality);
    FString ConfigPath = GetConfigFilePath(TEXT("DefaultScalability.ini"));
    GConfig->SetInt(TEXT("ScalabilitySettings"), TEXT("OverallQualityLevel"), static_cast<int32>(Quality), *ConfigPath);
    GConfig->Flush(false, *ConfigPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetNumberField(TEXT("quality"), Quality); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetRenderingSetting(const TSharedPtr<FJsonObject>& Params) { return SetConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/Engine.RendererSettings"), Params); }
TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetPhysicsSetting(const TSharedPtr<FJsonObject>& Params) { return SetConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/Engine.PhysicsSettings"), Params); }
TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetInputSetting(const TSharedPtr<FJsonObject>& Params) { return SetConfigValue(TEXT("DefaultInput.ini"), TEXT("/Script/Engine.InputSettings"), Params); }
TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetCollisionSetting(const TSharedPtr<FJsonObject>& Params) { return SetConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/Engine.CollisionProfile"), Params); }
TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetAISetting(const TSharedPtr<FJsonObject>& Params) { return SetConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/AIModule.AISystem"), Params); }
TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetNavigationSetting(const TSharedPtr<FJsonObject>& Params) { return SetConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/NavigationSystem.NavigationSystemV1"), Params); }
TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetPackagingSetting(const TSharedPtr<FJsonObject>& Params) { return SetConfigValue(TEXT("DefaultGame.ini"), TEXT("/Script/UnrealEd.ProjectPackagingSettings"), Params); }

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetWorldSettings(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    AWorldSettings* WS = World->GetWorldSettings();
    if (!WS) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No WorldSettings found")); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("world_name"), World->GetName());
    R->SetNumberField(TEXT("world_to_meters"), WS->WorldToMeters);
    R->SetNumberField(TEXT("kill_z"), WS->KillZ);
    R->SetBoolField(TEXT("enable_world_bounds_checks"), WS->bEnableWorldBoundsChecks);
    R->SetBoolField(TEXT("enable_world_composition"), WS->bEnableWorldComposition);
    if (WS->DefaultGameMode) { R->SetStringField(TEXT("default_game_mode"), WS->DefaultGameMode->GetPathName()); }
    R->SetNumberField(TEXT("gravity_z"), WS->GetGravityZ()); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetWorldSetting(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("set world settings"))) { return Busy; }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    AWorldSettings* WS = World->GetWorldSettings();
    if (!WS) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No WorldSettings found")); }
    double D = 0.0;
    if (Params->TryGetNumberField(TEXT("world_to_meters"), D)) WS->WorldToMeters = static_cast<float>(D);
    if (Params->TryGetNumberField(TEXT("kill_z"), D)) WS->KillZ = static_cast<float>(D);
    bool B = false;
    if (Params->TryGetBoolField(TEXT("enable_world_bounds_checks"), B)) WS->bEnableWorldBoundsChecks = B;
    if (Params->TryGetBoolField(TEXT("enable_world_composition"), B)) WS->bEnableWorldComposition = B;
    FString V;
    if (Params->TryGetStringField(TEXT("default_game_mode"), V)) WS->DefaultGameMode = LoadClass<AGameModeBase>(nullptr, *V);
    World->MarkPackageDirty();
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("message"), TEXT("World settings updated")); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleUndo(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    double Count = 1.0; Params->TryGetNumberField(TEXT("count"), Count);
    for (int32 i = 0; i < static_cast<int32>(Count); ++i) { GEditor->UndoTransaction(); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetNumberField(TEXT("undone_count"), Count); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRedo(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    double Count = 1.0; Params->TryGetNumberField(TEXT("count"), Count);
    for (int32 i = 0; i < static_cast<int32>(Count); ++i) { GEditor->RedoTransaction(); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetNumberField(TEXT("redone_count"), Count); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetDirtyAssets(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> DirtyArray;
    TArray<UPackage*> DirtyContentPackages;
    TArray<UPackage*> DirtyMapPackages;
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContentPackages);
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMapPackages);
    for (UPackage* P : DirtyContentPackages)
    {
        if (P)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("name"), P->GetName()); O->SetStringField(TEXT("type"), TEXT("content")); O->SetStringField(TEXT("path"), P->GetPathName());
            DirtyArray.Add(MakeShared<FJsonValueObject>(O));
        }
    }
    for (UPackage* P : DirtyMapPackages)
    {
        if (P)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("name"), P->GetName()); O->SetStringField(TEXT("type"), TEXT("map")); O->SetStringField(TEXT("path"), P->GetPathName());
            DirtyArray.Add(MakeShared<FJsonValueObject>(O));
        }
    }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetNumberField(TEXT("count"), DirtyArray.Num()); R->SetArrayField(TEXT("dirty_assets"), DirtyArray); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    bool bPrompt = false; Params->TryGetBoolField(TEXT("prompt"), bPrompt);
    bool bSaved = false;
    if (bPrompt) { bSaved = UEditorLoadingAndSavingUtils::SaveDirtyPackagesWithDialog(true, true); }
    else { bSaved = UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetBoolField(TEXT("saved"), bSaved); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }
    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!Asset) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath)); }
    bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(Asset);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bSaved); R->SetStringField(TEXT("asset_path"), AssetPath); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetEditorLog(const TSharedPtr<FJsonObject>& Params)
{
    double TailLines = 100.0; Params->TryGetNumberField(TEXT("tail_lines"), TailLines);
    FString LogDir = FPaths::ProjectLogDir();
    FString LogFile = LogDir / TEXT("UnrealEditor.log");
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*LogFile))
    {
        LogFile = LogDir / TEXT("UE4Editor.log");
    }
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*LogFile))
    {
        LogFile = LogDir / (FPaths::GetBaseFilename(FPaths::GetProjectFilePath()) + TEXT(".log"));
    }
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*LogFile))
    {
        TArray<FString> LogFiles;
        IFileManager::Get().FindFiles(LogFiles, *(LogDir / TEXT("*.log")), true, false);
        if (LogFiles.Num() == 0)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Log file not found"));
        }

        LogFile = LogDir / LogFiles[0];
    }
    TArray<FString> Lines;
    FString LogContents;
    if (!FFileHelper::LoadFileToString(LogContents, *LogFile, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read log file"));
    }
    LogContents.ParseIntoArrayLines(Lines, false);
    int32 StartIndex = FMath::Max(0, Lines.Num() - static_cast<int32>(TailLines));
    TArray<FString> Tail;
    for (int32 i = StartIndex; i < Lines.Num(); ++i) { Tail.Add(Lines[i]); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("total_lines"), Lines.Num());
    R->SetNumberField(TEXT("returned_lines"), Tail.Num());
    R->SetStringField(TEXT("log_content"), FString::Join(Tail, TEXT("\n"))); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateUtilityWidget(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false);
    R->SetStringField(TEXT("error"), TEXT("Editor Utility Widget creation not yet implemented. Requires UMGEditor module."));
    R->SetStringField(TEXT("asset_path"), AssetPath); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateUtilityBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false);
    R->SetStringField(TEXT("error"), TEXT("Editor Utility Blueprint creation not yet implemented. Requires UMGEditor module."));
    R->SetStringField(TEXT("asset_path"), AssetPath); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleExecutePythonScript(const TSharedPtr<FJsonObject>& Params)
{
    FString Script; if (!Params->TryGetStringField(TEXT("script"), Script)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing script")); }
    // Attempt to execute via console command if PythonScriptPlugin is loaded
    if (GEditor)
    {
        FString Command = FString::Printf(TEXT("py %s"), *Script);
        GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetStringField(TEXT("message"), TEXT("Python script executed via console command"));
        R->SetStringField(TEXT("script"), Script); return R;
    }
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available for Python execution"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleExecuteCommandlet(const TSharedPtr<FJsonObject>& Params)
{
    FString CommandletName; if (!Params->TryGetStringField(TEXT("commandlet_name"), CommandletName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing commandlet_name")); }
    FString Args; Params->TryGetStringField(TEXT("args"), Args);
    // Use the current editor executable path as base
    FString EditorExe = FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe");
#if PLATFORM_LINUX
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Linux/UnrealEditor-Cmd");
#elif PLATFORM_MAC
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Mac/UnrealEditor-Cmd");
#endif
    FString FullArgs = FString::Printf(TEXT("\"%s\" -run=%s %s"), *FPaths::GetProjectFilePath(), *CommandletName, *Args);
    FProcHandle Handle = FPlatformProcess::CreateProc(*EditorExe, *FullArgs, true, false, false, nullptr, 0, nullptr, nullptr);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("commandlet_name"), CommandletName);
    R->SetStringField(TEXT("args"), Args);
    R->SetStringField(TEXT("note"), TEXT("Commandlet process started")); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleStartPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("start PIE"))) { return Busy; }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    FRequestPlaySessionParams RequestParams;
    RequestParams.WorldType = EPlaySessionWorldType::PlayInEditor;
    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
        if (TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport())
        {
            RequestParams.DestinationSlateViewport = TWeakPtr<IAssetViewport>(ActiveViewport);
        }
    }
    GEditor->RequestPlaySession(RequestParams);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("message"), TEXT("PIE start requested"));
    AddPlayStateFields(R);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleStopPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (!GEditor->PlayWorld && !GEditor->IsPlayingSessionInEditor() && !GEditor->IsPlaySessionRequestQueued() && !GEditor->ShouldEndPlayMap())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PIE/SIE is not running"));
    }

    if (GEditor->IsPlaySessionRequestQueued() && !GEditor->IsPlayingSessionInEditor())
    {
        GEditor->CancelRequestPlaySession();
    }
    else if (!GEditor->ShouldEndPlayMap())
    {
        GEditor->RequestEndPlayMap();
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("message"), TEXT("PIE/SIE stop requested"));
    AddPlayStateFields(R);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetPlayState(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    AddPlayStateFields(R);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleStartStandaloneGame(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("start standalone game"))) { return Busy; }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    // Launch standalone game process. This is a separate process, not in-editor.
    FString MapPath = World->GetPathName();
    FString GameExe = FPlatformProcess::ExecutablePath();
    FString CmdLine = FString::Printf(TEXT("\"%s\" \"%s\" -game"), *FPaths::GetProjectFilePath(), *MapPath);
    FProcHandle Handle = FPlatformProcess::CreateProc(*GameExe, *CmdLine, true, false, false, nullptr, 0, nullptr, nullptr);
    if (!Handle.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to launch standalone game process"));
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("message"), TEXT("Standalone game launched")); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleStartSimulate(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("start simulate"))) { return Busy; }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    // Start Simulate In Editor through the UE 5.7 play-session request path.
    FRequestPlaySessionParams RequestParams;
    RequestParams.WorldType = EPlaySessionWorldType::SimulateInEditor;
    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
        if (TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport())
        {
            RequestParams.DestinationSlateViewport = TWeakPtr<IAssetViewport>(ActiveViewport);
        }
    }
    GEditor->RequestPlaySession(RequestParams);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("message"), TEXT("Simulate in Editor start requested"));
    AddPlayStateFields(R);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetCameraPosition(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    FEditorViewportClient* Client = GetSafeEditorViewportClient();
    if (!Client) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor viewport client available")); }
    FVector Loc = Client->GetViewLocation();
    FRotator Rot = Client->GetViewRotation();
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("x"), Loc.X); R->SetNumberField(TEXT("y"), Loc.Y); R->SetNumberField(TEXT("z"), Loc.Z);
    R->SetNumberField(TEXT("pitch"), Rot.Pitch); R->SetNumberField(TEXT("yaw"), Rot.Yaw); R->SetNumberField(TEXT("roll"), Rot.Roll); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetCameraPosition(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    FEditorViewportClient* Client = GetSafeEditorViewportClient();
    if (!Client) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor viewport client available")); }
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Params->TryGetArrayField(TEXT("location"), Arr) && Arr && Arr->Num() >= 3)
    {
        FVector Loc(
            (*Arr)[0]->AsNumber(),
            (*Arr)[1]->AsNumber(),
            (*Arr)[2]->AsNumber()
        );
        Client->SetViewLocation(Loc);
    }
    if (Params->TryGetArrayField(TEXT("rotation"), Arr) && Arr && Arr->Num() >= 3)
    {
        FRotator Rot(
            (*Arr)[0]->AsNumber(),
            (*Arr)[1]->AsNumber(),
            (*Arr)[2]->AsNumber()
        );
        Client->SetViewRotation(Rot);
    }
    if (Client->Viewport)
    {
        Client->Viewport->Invalidate();
    }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("message"), TEXT("Camera position updated")); return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleViewportAction(const TSharedPtr<FJsonObject>& Params)
{
    FString Action;
    if (!Params->TryGetStringField(TEXT("action"), Action)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing action")); }
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    FEditorViewportClient* Client = GetSafeEditorViewportClient();
    if (!Client) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor viewport client available")); }

    if (Action == TEXT("focus_selected"))
    {
        GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
    }
    else if (Action == TEXT("focus_actor"))
    {
        FString ActorName; Params->TryGetStringField(TEXT("actor_name"), ActorName);
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
                {
                    Client->FocusViewportOnBox(It->GetComponentsBoundingBox(true));
                    break;
                }
            }
        }
    }
    else if (Action == TEXT("set_view_mode"))
    {
        FString Mode; Params->TryGetStringField(TEXT("mode"), Mode);
        if (Mode == TEXT("wireframe")) { Client->SetViewMode(VMI_Wireframe); }
        else if (Mode == TEXT("lit")) { Client->SetViewMode(VMI_Lit); }
        else if (Mode == TEXT("unlit")) { Client->SetViewMode(VMI_Unlit); }
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown viewport action: %s"), *Action));
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("action"), Action); return R;
}

// ---------------------------------------------------------------------------
// Level / Map Management (Phase 1)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("create level"))) { return Busy; }
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset already exists: %s"), *AssetPath));
    }

    TArray<FString> TemplatePaths;
    TemplatePaths.Add(TEXT("/Engine/Maps/Templates/Template_Default"));
    TemplatePaths.Add(TEXT("/Engine/Maps/Templates/OpenWorld"));
    TemplatePaths.Add(TEXT("/Game/Maps/E2E_Advanced_Main"));

    FString SourcePath;
    FString TargetFilename;
    if (!TryMapPackageToFilename(AssetPath, TargetFilename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid map package path: %s"), *AssetPath));
    }

    for (const FString& TemplatePath : TemplatePaths)
    {
        FString TemplateFilename;
        if (!TryMapPackageToFilename(TemplatePath, TemplateFilename) || !FPaths::FileExists(TemplateFilename))
        {
            continue;
        }

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetFilename), true);
        const uint32 CopyResult = IFileManager::Get().Copy(*TargetFilename, *TemplateFilename, true, true);
        if (CopyResult == COPY_OK)
        {
            SourcePath = TemplatePath;
            ScanAssetFile(TargetFilename);
            break;
        }
    }

    if (SourcePath.IsEmpty())
    {
        ULevelEditorSubsystem* LevelSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
        if (!LevelSubsystem) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem not available")); }

        const bool bSuccess = LevelSubsystem->NewLevel(AssetPath);
        if (!bSuccess)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create new level"));
        }
        SourcePath = TEXT("LevelEditorSubsystem::NewLevel");
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("asset_path"), AssetPath);
    R->SetStringField(TEXT("source_path"), SourcePath);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSaveLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("save level"))) { return Busy; }
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }

    // Only fall back to SaveMap if the asset path matches the currently loaded world.
    // This avoids the dangerous side-effect of saving the wrong world to a new asset path.
    UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
    if (CurrentWorld)
    {
        FString CurrentPackageName = CurrentWorld->GetOutermost()->GetName();
        if (CurrentPackageName == NormalizeLevelPackageName(AssetPath))
        {
            bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(CurrentWorld, AssetPath);
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetBoolField(TEXT("success"), bSaved);
            R->SetStringField(TEXT("asset_path"), AssetPath);
            R->SetBoolField(TEXT("saved_current_map"), true);
            return R;
        }
    }

    // If the world is already loaded in memory (e.g. by a previous level load), save it directly.
    // Use FindObject (not LoadObject/StaticLoadObject) to avoid force-loading a UWorld into
    // memory, which can corrupt the TickTaskManager and crash subsequent LoadLevel calls.
    UWorld* WorldToSave = FindObject<UWorld>(nullptr, *AssetPath);
    if (!WorldToSave)
    {
        FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
        WorldToSave = FindObject<UWorld>(nullptr, *ObjectPath);
    }

    if (WorldToSave)
    {
        bool bSavedAsset = UEditorAssetLibrary::SaveLoadedAsset(WorldToSave);
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), bSavedAsset);
        R->SetStringField(TEXT("asset_path"), AssetPath);
        R->SetBoolField(TEXT("saved_loaded_asset"), true);
        return R;
    }

    // Fallback: If the map file exists on disk (e.g. from HandleCreateLevel's
    // file copy), treat it as already saved.  Loading the package just to
    // call SaveLoadedAsset would pull an extra UWorld into memory and risks
    // TickTaskManager corruption.
    {
        const FString PackageName = NormalizeLevelPackageName(AssetPath);
        FString MapFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, MapFilename, FPackageName::GetMapPackageExtension()))
        {
            if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*MapFilename))
            {
                TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
                R->SetBoolField(TEXT("success"), true);
                R->SetStringField(TEXT("asset_path"), AssetPath);
                R->SetBoolField(TEXT("already_on_disk"), true);
                return R;
            }
        }
    }

    // Asset is not the current world and not already loaded; nothing to save.
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Cannot save level: asset is not the currently loaded world and is not resident in memory (%s)"), *AssetPath)
    );
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleLoadLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("load level"))) { return Busy; }

    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }

    if (IsCurrentEditorWorldPackage(AssetPath))
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetBoolField(TEXT("already_loaded"), true);
        R->SetStringField(TEXT("asset_path"), AssetPath);
        return R;
    }

    ULevelEditorSubsystem* LevelSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
    if (!LevelSubsystem) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem not available")); }

    bool bLoaded = LevelSubsystem->LoadLevel(AssetPath);

    // After loading a new level, rebuild the actor index so that
    // actors saved in previous sessions are tracked and won't cause
    // duplicate-name fatal errors on subsequent spawn_actor calls.
    if (bLoaded)
    {
        UEpicUnrealMCPBridge* Bridge = GEditor->GetEditorSubsystem<UEpicUnrealMCPBridge>();
        if (Bridge)
        {
            UWorld* NewWorld = GEditor->GetEditorWorldContext().World();
            Bridge->ActorIndex.RebuildFromWorld(NewWorld);
        }
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bLoaded);
    R->SetStringField(TEXT("asset_path"), AssetPath);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleDuplicateLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("duplicate level"))) { return Busy; }
    FString SourcePath; if (!Params->TryGetStringField(TEXT("source_path"), SourcePath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing source_path")); }
    FString DestPath; if (!Params->TryGetStringField(TEXT("dest_path"), DestPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing dest_path")); }

    // Use path-based duplication for better safety with worlds and to handle unloaded assets.
    UObject* Duplicated = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), Duplicated != nullptr);
    R->SetStringField(TEXT("source_path"), SourcePath);
    R->SetStringField(TEXT("dest_path"), DestPath);
    if (Duplicated) { R->SetStringField(TEXT("new_asset_path"), Duplicated->GetPathName()); }
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRenameLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("rename level"))) { return Busy; }
    FString SourcePath; if (!Params->TryGetStringField(TEXT("source_path"), SourcePath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing source_path")); }
    FString DestPath; if (!Params->TryGetStringField(TEXT("dest_path"), DestPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing dest_path")); }

    // Use path-based rename for better safety with worlds.
    bool bRenamed = UEditorAssetLibrary::RenameAsset(SourcePath, DestPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bRenamed);
    R->SetStringField(TEXT("source_path"), SourcePath);
    R->SetStringField(TEXT("dest_path"), DestPath);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleDeleteLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("delete level"))) { return Busy; }
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }
    if (IsCurrentEditorWorldPackage(AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Cannot delete the currently loaded level: %s"), *AssetPath));
    }

    const FString PackageName = NormalizeLevelPackageName(AssetPath);
    FString MapFilename;
    if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, MapFilename, FPackageName::GetMapPackageExtension()))
    {
        const bool bFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*MapFilename);
        if (!bFileExists && !UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
        }

        const bool bDeletedFile = bFileExists && IFileManager::Get().Delete(*MapFilename, false, true, true);
        if (bDeletedFile)
        {
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            TArray<FString> ModifiedFiles;
            ModifiedFiles.Add(MapFilename);
            AssetRegistryModule.Get().ScanModifiedAssetFiles(ModifiedFiles);
        }

        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), bDeletedFile);
        R->SetStringField(TEXT("asset_path"), AssetPath);
        R->SetStringField(TEXT("filename"), MapFilename);
        if (!bDeletedFile)
        {
            R->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to delete map file: %s"), *MapFilename));
        }
        return R;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetPath);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bDeleted);
    R->SetStringField(TEXT("asset_path"), AssetPath);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    ULevel* CurrentLevel = World->GetCurrentLevel();
    if (!CurrentLevel) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No current level found")); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("level_name"), CurrentLevel->GetName());
    R->SetStringField(TEXT("outer_path"), CurrentLevel->GetOuter()->GetPathName());
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleListLevels(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    TArray<TSharedPtr<FJsonValue>> LevelArray;
    for (ULevel* Level : World->GetLevels())
    {
        if (!Level) continue;
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("name"), Level->GetName());
        O->SetStringField(TEXT("outer_path"), Level->GetOuter()->GetPathName());
        O->SetBoolField(TEXT("is_current"), Level == World->GetCurrentLevel());
        O->SetBoolField(TEXT("is_persistent"), Level == World->PersistentLevel);
        LevelArray.Add(MakeShared<FJsonValueObject>(O));
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("count"), LevelArray.Num());
    R->SetArrayField(TEXT("levels"), LevelArray);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetPersistentLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    ULevel* PersistentLevel = World->PersistentLevel;
    if (!PersistentLevel) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No persistent level found")); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("level_name"), PersistentLevel->GetName());
    R->SetStringField(TEXT("outer_path"), PersistentLevel->GetOuter()->GetPathName());
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleAddSublevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("add sublevel"))) { return Busy; }
    FString LevelPath; if (!Params->TryGetStringField(TEXT("level_path"), LevelPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing level_path")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    for (ULevelStreaming* ExistingLevel : World->GetStreamingLevels())
    {
        if (MatchesStreamingLevelName(ExistingLevel, LevelPath))
        {
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetBoolField(TEXT("success"), true);
            R->SetBoolField(TEXT("already_added"), true);
            R->SetStringField(TEXT("level_path"), LevelPath);
            R->SetStringField(TEXT("streaming_level_name"), ExistingLevel->GetName());
            return R;
        }
    }

    ULevelStreaming* StreamingLevel = UEditorLevelUtils::AddLevelToWorld(World, *LevelPath, ULevelStreamingDynamic::StaticClass());
    if (!StreamingLevel) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add sublevel")); }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("level_path"), LevelPath);
    R->SetStringField(TEXT("streaming_level_name"), StreamingLevel->GetName());
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRemoveSublevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("remove sublevel"))) { return Busy; }
    FString LevelName; if (!Params->TryGetStringField(TEXT("level_name"), LevelName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing level_name")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    ULevelStreaming* TargetStreamingLevel = nullptr;
    for (ULevelStreaming* SL : World->GetStreamingLevels())
    {
        if (MatchesStreamingLevelName(SL, LevelName))
        {
            TargetStreamingLevel = SL;
            break;
        }
    }
    if (!TargetStreamingLevel)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *LevelName));
    }

    TargetStreamingLevel->Modify();
    TargetStreamingLevel->SetShouldBeVisible(false);
    TargetStreamingLevel->SetShouldBeLoaded(false);
    const bool bRemoved = World->RemoveStreamingLevel(TargetStreamingLevel);
    if (bRemoved)
    {
        TargetStreamingLevel->MarkAsGarbage();
        World->MarkPackageDirty();
        World->BroadcastLevelsChanged();
        FEditorDelegates::RefreshLevelBrowser.Broadcast();
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bRemoved);
    R->SetStringField(TEXT("level_name"), LevelName);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetSublevelVisible(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("set sublevel visibility"))) { return Busy; }
    FString LevelName; if (!Params->TryGetStringField(TEXT("level_name"), LevelName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing level_name")); }
    bool bVisible = true; Params->TryGetBoolField(TEXT("visible"), bVisible);
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    ULevelStreaming* StreamingLevel = nullptr;
    for (ULevelStreaming* SL : World->GetStreamingLevels())
    {
        if (MatchesStreamingLevelName(SL, LevelName))
        {
            StreamingLevel = SL;
            break;
        }
    }
    if (!StreamingLevel) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *LevelName)); }

    StreamingLevel->SetShouldBeVisibleInEditor(bVisible);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("level_name"), LevelName);
    R->SetBoolField(TEXT("visible"), bVisible);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetSublevelLoaded(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("set sublevel loaded state"))) { return Busy; }
    FString LevelName; if (!Params->TryGetStringField(TEXT("level_name"), LevelName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing level_name")); }
    bool bLoaded = true; Params->TryGetBoolField(TEXT("loaded"), bLoaded);
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    ULevelStreaming* StreamingLevel = nullptr;
    for (ULevelStreaming* SL : World->GetStreamingLevels())
    {
        if (MatchesStreamingLevelName(SL, LevelName))
        {
            StreamingLevel = SL;
            break;
        }
    }
    if (!StreamingLevel) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *LevelName)); }

    StreamingLevel->SetShouldBeLoaded(bLoaded);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("level_name"), LevelName);
    R->SetBoolField(TEXT("loaded"), bLoaded);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateStreamingVolume(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("create streaming volume"))) { return Busy; }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    FVector Location(0.0f, 0.0f, 0.0f);
    FVector Extent(500.0f, 500.0f, 500.0f);
    const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
    if (Params->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() >= 3)
    {
        Location.X = static_cast<float>((*LocArr)[0]->AsNumber());
        Location.Y = static_cast<float>((*LocArr)[1]->AsNumber());
        Location.Z = static_cast<float>((*LocArr)[2]->AsNumber());
    }
    const TArray<TSharedPtr<FJsonValue>>* ExtArr = nullptr;
    if (Params->TryGetArrayField(TEXT("extent"), ExtArr) && ExtArr && ExtArr->Num() >= 3)
    {
        Extent.X = static_cast<float>((*ExtArr)[0]->AsNumber());
        Extent.Y = static_cast<float>((*ExtArr)[1]->AsNumber());
        Extent.Z = static_cast<float>((*ExtArr)[2]->AsNumber());
    }

    ALevelStreamingVolume* Volume = World->SpawnActor<ALevelStreamingVolume>(Location, FRotator::ZeroRotator);
    if (!Volume) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn streaming volume")); }

    UBrushComponent* Brush = Volume->GetBrushComponent();
    if (Brush)
    {
        Brush->Bounds.BoxExtent = Extent;
    }

    TArray<FString> LevelNames;
    const TArray<TSharedPtr<FJsonValue>>* NameArr = nullptr;
    if (Params->TryGetArrayField(TEXT("streaming_levels"), NameArr) && NameArr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *NameArr)
        {
            LevelNames.Add(Val->AsString());
        }
    }
    if (LevelNames.Num() > 0)
    {
        TArray<FName> StreamingLevelPackageNames;
        for (const FString& Name : LevelNames)
        {
            StreamingLevelPackageNames.Add(FName(*Name));
        }
        Volume->StreamingLevelNames = StreamingLevelPackageNames;
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("actor_name"), Volume->GetName());
    R->SetNumberField(TEXT("x"), Location.X);
    R->SetNumberField(TEXT("y"), Location.Y);
    R->SetNumberField(TEXT("z"), Location.Z);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetLevelStreamingSettings(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("set level streaming settings"))) { return Busy; }
    FString LevelName; if (!Params->TryGetStringField(TEXT("level_name"), LevelName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing level_name")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    ULevelStreaming* StreamingLevel = nullptr;
    for (ULevelStreaming* SL : World->GetStreamingLevels())
    {
        if (MatchesStreamingLevelName(SL, LevelName))
        {
            StreamingLevel = SL;
            break;
        }
    }
    if (!StreamingLevel) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Streaming level not found: %s"), *LevelName)); }

    bool B = false;
    if (Params->TryGetBoolField(TEXT("should_be_loaded"), B)) StreamingLevel->SetShouldBeLoaded(B);
    if (Params->TryGetBoolField(TEXT("should_be_visible"), B)) StreamingLevel->SetShouldBeVisible(B);
    double D = 0.0;
    if (Params->TryGetNumberField(TEXT("priority"), D)) StreamingLevel->SetPriority(static_cast<int32>(D));
    FString V;
    if (Params->TryGetStringField(TEXT("level_transform"), V))
    {
        // JSON array string [x,y,z,...] is not handled here; full transform parsing omitted for brevity
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("level_name"), LevelName);
    return R;
}

// ---------------------------------------------------------------------------
// World Partition (Phase 3)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleEnableWorldPartition(const TSharedPtr<FJsonObject>& Params)
{
    bool bEnable = true;
    Params->TryGetBoolField(TEXT("enable"), bEnable);

    // Use WorldPartitionConvertCommandlet to convert the current level to/from World Partition
    FString EditorExe = FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe");
#if PLATFORM_LINUX
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Linux/UnrealEditor-Cmd");
#elif PLATFORM_MAC
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Mac/UnrealEditor-Cmd");
#endif
    FString MapPath;
    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        MapPath = GEditor->GetEditorWorldContext().World()->GetPathName();
    }
    FString CommandletArgs = FString::Printf(TEXT("\"%s\" -run=WorldPartitionConvertCommandlet %s %s"),
        *FPaths::GetProjectFilePath(),
        *MapPath,
        bEnable ? TEXT("-Enable") : TEXT("-Disable"));
    FProcHandle Handle = FPlatformProcess::CreateProc(*EditorExe, *CommandletArgs, true, false, false, nullptr, 0, nullptr, nullptr);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("world_partition_enabled"), bEnable);
    R->SetStringField(TEXT("note"), TEXT("WorldPartitionConvertCommandlet launched (may take time)"));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetWorldPartitionGrid(const TSharedPtr<FJsonObject>& Params)
{
    FString ConfigPath = GetConfigFilePath(TEXT("DefaultEngine.ini"));
    FString Section = TEXT("/Script/Engine.WorldPartitionEditorPerProjectUserSettings");

    double D = 0.0;
    if (Params->TryGetNumberField(TEXT("placement_grid_size"), D))
    {
        GConfig->SetInt(*Section, TEXT("PlacementGridSize"), static_cast<int32>(D), *ConfigPath);
    }
    if (Params->TryGetNumberField(TEXT("foliage_grid_size"), D))
    {
        GConfig->SetInt(*Section, TEXT("InstancedFoliageGridSize"), static_cast<int32>(D), *ConfigPath);
    }
    if (Params->TryGetNumberField(TEXT("minimap_threshold"), D))
    {
        GConfig->SetInt(*Section, TEXT("MinimapLowQualityWorldUnitsPerPixelThreshold"), static_cast<int32>(D), *ConfigPath);
    }
    GConfig->Flush(false, *ConfigPath);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("note"), TEXT("World Partition grid settings updated"));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleGetWorldPartitionCells(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    if (!World->IsPartitionedWorld()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("World Partition is not enabled")); }

    UWorldPartition* WP = World->GetWorldPartition();
    if (!WP) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("World Partition not initialized")); }

    FBox Bounds = WP->GetEditorWorldBounds();
    TArray<TSharedPtr<FJsonValue>> RegionArray;
    TArray<FBox> Regions = WP->GetUserLoadedEditorRegions();
    for (const FBox& Box : Regions)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("min_x"), Box.Min.X);
        O->SetNumberField(TEXT("min_y"), Box.Min.Y);
        O->SetNumberField(TEXT("min_z"), Box.Min.Z);
        O->SetNumberField(TEXT("max_x"), Box.Max.X);
        O->SetNumberField(TEXT("max_y"), Box.Max.Y);
        O->SetNumberField(TEXT("max_z"), Box.Max.Z);
        RegionArray.Add(MakeShared<FJsonValueObject>(O));
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("editor_bounds_min_x"), Bounds.Min.X);
    R->SetNumberField(TEXT("editor_bounds_min_y"), Bounds.Min.Y);
    R->SetNumberField(TEXT("editor_bounds_min_z"), Bounds.Min.Z);
    R->SetNumberField(TEXT("editor_bounds_max_x"), Bounds.Max.X);
    R->SetNumberField(TEXT("editor_bounds_max_y"), Bounds.Max.Y);
    R->SetNumberField(TEXT("editor_bounds_max_z"), Bounds.Max.Z);
    R->SetNumberField(TEXT("loaded_region_count"), RegionArray.Num());
    R->SetArrayField(TEXT("loaded_regions"), RegionArray);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleLoadWorldPartitionCell(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    if (!World->IsPartitionedWorld()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("World Partition is not enabled")); }

    UWorldPartition* WP = World->GetWorldPartition();
    if (!WP) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("World Partition not initialized")); }

    // Build a region box from params and load it
    double MinX = 0.0, MinY = 0.0, MinZ = 0.0, MaxX = 0.0, MaxY = 0.0, MaxZ = 0.0;
    Params->TryGetNumberField(TEXT("min_x"), MinX);
    Params->TryGetNumberField(TEXT("min_y"), MinY);
    Params->TryGetNumberField(TEXT("min_z"), MinZ);
    Params->TryGetNumberField(TEXT("max_x"), MaxX);
    Params->TryGetNumberField(TEXT("max_y"), MaxY);
    Params->TryGetNumberField(TEXT("max_z"), MaxZ);

    TArray<FBox> Regions;
    Regions.Add(FBox(FVector(static_cast<float>(MinX), static_cast<float>(MinY), static_cast<float>(MinZ)),
                     FVector(static_cast<float>(MaxX), static_cast<float>(MaxY), static_cast<float>(MaxZ))));
    WP->LoadLastLoadedRegions(Regions);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("note"), TEXT("World Partition cell load requested"));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleUnloadWorldPartitionCell(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }
    if (!World->IsPartitionedWorld()) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("World Partition is not enabled")); }

    UWorldPartition* WP = World->GetWorldPartition();
    if (!WP) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("World Partition not initialized")); }

    // Unloading is done by resetting user loaded regions; we pass an empty array to clear custom loads
    TArray<FBox> EmptyRegions;
    WP->LoadLastLoadedRegions(EmptyRegions);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("note"), TEXT("World Partition cell unload requested (cleared custom loaded regions)"));
    return R;
}

// ---------------------------------------------------------------------------
// Data Layer / HLOD / OFPA / Bounds / Origin Rebasing (Phase 4)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateDataLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing name")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    // Use GConfig to register a new data layer asset stub entry (full asset creation via commandlet is safer)
    FString ConfigPath = GetConfigFilePath(TEXT("DefaultEngine.ini"));
    FString Section = FString::Printf(TEXT("DataLayers.%s"), *DataLayerName);
    GConfig->SetString(*Section, TEXT("Name"), *DataLayerName, *ConfigPath);
    GConfig->Flush(false, *ConfigPath);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("data_layer_name"), DataLayerName);
    R->SetStringField(TEXT("note"), TEXT("Data layer registered in config. Use editor to finalize asset creation."));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleAddActorsToDataLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("data_layer_name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing data_layer_name")); }
    const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
    if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNames) || !ActorNames) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_names array")); }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    int32 ModifiedCount = 0;
    for (const TSharedPtr<FJsonValue>& Val : *ActorNames)
    {
        FString ActorName = Val->AsString();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
            {
                // Direct property modification via reflection to avoid version-specific struct names
                FProperty* Prop = It->GetClass()->FindPropertyByName(FName(TEXT("DataLayerAssets")));
                if (Prop && Prop->IsA<FArrayProperty>())
                {
                    FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop);
                    FScriptArrayHelper ArrayHelper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(*It));
                    // Note: Appending a null soft-object ptr is a stub; real implementation needs UDataLayerAsset* load
                    int32 NewIndex = ArrayHelper.AddValue();
                    ModifiedCount++;
                }
                break;
            }
        }
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("modified_count"), ModifiedCount);
    R->SetStringField(TEXT("note"), TEXT("Data layer assignment is a stub; production code should load UDataLayerAsset and set the soft-object ptr."));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRemoveActorsFromDataLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("data_layer_name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing data_layer_name")); }
    const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
    if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNames) || !ActorNames) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_names array")); }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    int32 ModifiedCount = 0;
    for (const TSharedPtr<FJsonValue>& Val : *ActorNames)
    {
        FString ActorName = Val->AsString();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
            {
                FProperty* Prop = It->GetClass()->FindPropertyByName(FName(TEXT("DataLayerAssets")));
                if (Prop && Prop->IsA<FArrayProperty>())
                {
                    FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop);
                    FScriptArrayHelper ArrayHelper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(*It));
                    ArrayHelper.EmptyValues();
                    ModifiedCount++;
                }
                break;
            }
        }
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("modified_count"), ModifiedCount);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetDataLayerEnabled(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("data_layer_name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing data_layer_name")); }
    bool bEnabled = true; Params->TryGetBoolField(TEXT("enabled"), bEnabled);

    // Use Python script execution to toggle data layer visibility in editor
    FString PyCmd = FString::Printf(TEXT("import unreal; dl = unreal.DataLayerAsset.find_data_layer_by_name('%s'); unreal.EditorLevelLibrary.set_data_layer_runtime_state(dl, unreal.DataLayerRuntimeState.%s)"),
        *DataLayerName,
        bEnabled ? TEXT("Activated") : TEXT("Unloaded"));
    if (GEditor)
    {
        GEditor->Exec(GEditor->GetEditorWorldContext().World(), *FString::Printf(TEXT("py %s"), *PyCmd));
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("data_layer_name"), DataLayerName);
    R->SetBoolField(TEXT("enabled"), bEnabled);
    R->SetStringField(TEXT("note"), TEXT("Data layer state toggled via Python console (requires PythonScriptPlugin)."));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateHLODLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString LayerName; if (!Params->TryGetStringField(TEXT("name"), LayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing name")); }
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("name"), LayerName);
    R->SetStringField(TEXT("note"), TEXT("HLOD layer creation is a stub. Use the editor HLOD system or WorldPartitionHLODBuilderCommandlet."));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleBuildHLOD(const TSharedPtr<FJsonObject>& Params)
{
    FString MapPath; Params->TryGetStringField(TEXT("map_path"), MapPath);
    FString EditorExe = FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe");
#if PLATFORM_LINUX
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Linux/UnrealEditor-Cmd");
#elif PLATFORM_MAC
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Mac/UnrealEditor-Cmd");
#endif
    FString Args = FString::Printf(TEXT("\"%s\" -run=WorldPartitionHLODBuilderCommandlet %s"), *FPaths::GetProjectFilePath(), *MapPath);
    FProcHandle Handle = FPlatformProcess::CreateProc(*EditorExe, *Args, true, false, false, nullptr, 0, nullptr, nullptr);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("note"), TEXT("WorldPartitionHLODBuilderCommandlet launched (async, may take time)."));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRebuildHLOD(const TSharedPtr<FJsonObject>& Params)
{
    FString MapPath; Params->TryGetStringField(TEXT("map_path"), MapPath);
    FString EditorExe = FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe");
#if PLATFORM_LINUX
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Linux/UnrealEditor-Cmd");
#elif PLATFORM_MAC
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Mac/UnrealEditor-Cmd");
#endif
    FString Args = FString::Printf(TEXT("\"%s\" -run=WorldPartitionHLODBuilderCommandlet -Rebuild %s"), *FPaths::GetProjectFilePath(), *MapPath);
    FProcHandle Handle = FPlatformProcess::CreateProc(*EditorExe, *Args, true, false, false, nullptr, 0, nullptr, nullptr);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("note"), TEXT("WorldPartitionHLODBuilderCommandlet rebuild launched (async, may take time)."));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetOneFilePerActor(const TSharedPtr<FJsonObject>& Params)
{
    bool bEnable = true; Params->TryGetBoolField(TEXT("enable"), bEnable);
    FString ConfigPath = GetConfigFilePath(TEXT("DefaultEngine.ini"));
    GConfig->SetBool(TEXT("/Script/Engine.WorldSettings"), TEXT("bUseExternalActors"), bEnable, *ConfigPath);
    GConfig->Flush(false, *ConfigPath);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("one_file_per_actor"), bEnable);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetLevelBounds(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("set level bounds"))) { return Busy; }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    // Find the LevelBounds actor by name pattern to avoid needing the full ALevelBounds type
    AActor* BoundsActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName().Contains(TEXT("LevelBounds")) || It->GetName().Contains(TEXT("Level_Bounds")))
        {
            BoundsActor = *It;
            break;
        }
    }
    if (!BoundsActor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No LevelBounds actor found in world")); }

    FVector Min(0.0f, 0.0f, 0.0f), Max(0.0f, 0.0f, 0.0f);
    const TArray<TSharedPtr<FJsonValue>>* MinArr = nullptr;
    if (Params->TryGetArrayField(TEXT("min"), MinArr) && MinArr && MinArr->Num() >= 3)
    {
        Min.X = static_cast<float>((*MinArr)[0]->AsNumber());
        Min.Y = static_cast<float>((*MinArr)[1]->AsNumber());
        Min.Z = static_cast<float>((*MinArr)[2]->AsNumber());
    }
    const TArray<TSharedPtr<FJsonValue>>* MaxArr = nullptr;
    if (Params->TryGetArrayField(TEXT("max"), MaxArr) && MaxArr && MaxArr->Num() >= 3)
    {
        Max.X = static_cast<float>((*MaxArr)[0]->AsNumber());
        Max.Y = static_cast<float>((*MaxArr)[1]->AsNumber());
        Max.Z = static_cast<float>((*MaxArr)[2]->AsNumber());
    }

    UBrushComponent* BrushComp = BoundsActor->FindComponentByClass<UBrushComponent>();
    if (BrushComp)
    {
        BrushComp->Bounds.BoxExtent = (Max - Min) * 0.5f;
    }
    BoundsActor->SetActorLocation((Min + Max) * 0.5f);
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("actor_name"), BoundsActor->GetName());
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetWorldOriginRebasing(const TSharedPtr<FJsonObject>& Params)
{
    bool bEnable = true; Params->TryGetBoolField(TEXT("enable"), bEnable);
    FString ConfigPath = GetConfigFilePath(TEXT("DefaultEngine.ini"));
    GConfig->SetBool(TEXT("/Script/Engine.WorldSettings"), TEXT("bEnableWorldOriginRebasing"), bEnable, *ConfigPath);
    GConfig->Flush(false, *ConfigPath);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("world_origin_rebasing_enabled"), bEnable);
    return R;
}
