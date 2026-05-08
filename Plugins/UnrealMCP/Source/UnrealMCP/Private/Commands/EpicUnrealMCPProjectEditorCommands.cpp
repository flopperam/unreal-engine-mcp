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
#include "AssetToolsModule.h"
#include "IAssetTools.h"
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
#include "Components/BoxComponent.h"
#include "GameFramework/Volume.h"
#include "Engine/LevelBounds.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/GarbageCollection.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "FileHelpers.h"
#include "Factories/WorldFactory.h"
#include "ImageUtils.h"

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
        {TEXT("take_screenshot"), &FEpicUnrealMCPProjectEditorCommands::HandleTakeScreenshot},
        {TEXT("export_level"), &FEpicUnrealMCPProjectEditorCommands::HandleExportLevel},
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

static FString GetEditorCommandletExecutablePath()
{
    FString EditorExe = FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe");
#if PLATFORM_LINUX
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Linux/UnrealEditor-Cmd");
#elif PLATFORM_MAC
    EditorExe = FPaths::EngineDir() / TEXT("Binaries/Mac/UnrealEditor-Cmd");
#endif
    return FPaths::ConvertRelativePathToFull(EditorExe);
}

static FString QuoteCommandletArg(const FString& Arg)
{
    FString Escaped = Arg;
    Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
    return FString::Printf(TEXT("\"%s\""), *Escaped);
}

static FString MakeCommandletLogPath(const FString& CommandletName)
{
    const FString SafeName = CommandletName.Replace(TEXT("."), TEXT("_")).Replace(TEXT(" "), TEXT("_"));
    const FString LogDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / TEXT("MCPCommandlets"));
    IFileManager::Get().MakeDirectory(*LogDir, true);
    return LogDir / FString::Printf(
        TEXT("%s-%s-%u.log"),
        *SafeName,
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")),
        FPlatformProcess::GetCurrentProcessId());
}

static FString ReadCommandletLogTail(const FString& CommandletLogFilename, int32 MaxChars = 32000)
{
    FString LogContent;
    if (!CommandletLogFilename.IsEmpty() && FFileHelper::LoadFileToString(LogContent, *CommandletLogFilename))
    {
        if (LogContent.Len() > MaxChars)
        {
            return LogContent.Right(MaxChars);
        }
        return LogContent;
    }
    return FString();
}

static TSharedPtr<FJsonObject> RunEditorCommandletProcess(
    const FString& CommandletName,
    const FString& CommandletArgs,
    bool bWaitForCompletion,
    double TimeoutSeconds)
{
    const FString EditorExe = GetEditorCommandletExecutablePath();
    if (!FPaths::FileExists(EditorExe))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("UnrealEditor-Cmd executable not found: %s"), *EditorExe));
    }

    const FString ProjectFile = FPaths::GetProjectFilePath();
    if (ProjectFile.IsEmpty() || !FPaths::FileExists(ProjectFile))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Project file path is unavailable; commandlets require a .uproject path"));
    }

    const FString CommandletLogFilename = MakeCommandletLogPath(CommandletName);
    const FString FullArgs = FString::Printf(
        TEXT("%s -run=%s %s -unattended -nop4 -abslog=%s"),
        *QuoteCommandletArg(ProjectFile),
        *CommandletName,
        *CommandletArgs,
        *QuoteCommandletArg(CommandletLogFilename));

    uint32 ProcessId = 0;
    FProcHandle Handle = FPlatformProcess::CreateProc(
        *EditorExe,
        *FullArgs,
        false,
        false,
        false,
        &ProcessId,
        0,
        nullptr,
        nullptr);

    if (!Handle.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to launch commandlet: %s %s"), *EditorExe, *FullArgs));
    }

    bool bTimedOut = false;
    int32 ReturnCode = 0;
    if (bWaitForCompletion)
    {
        const double StartSeconds = FPlatformTime::Seconds();
        while (FPlatformProcess::IsProcRunning(Handle))
        {
            if (TimeoutSeconds > 0.0 && (FPlatformTime::Seconds() - StartSeconds) > TimeoutSeconds)
            {
                bTimedOut = true;
                FPlatformProcess::TerminateProc(Handle, true);
                break;
            }
            FPlatformProcess::Sleep(0.5f);
        }
        FPlatformProcess::GetProcReturnCode(Handle, &ReturnCode);
    }

    FPlatformProcess::CloseProc(Handle);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("commandlet_name"), CommandletName);
    Result->SetStringField(TEXT("editor_exe"), EditorExe);
    Result->SetStringField(TEXT("project_file"), ProjectFile);
    Result->SetStringField(TEXT("args"), FullArgs);
    Result->SetStringField(TEXT("log_path"), CommandletLogFilename);
    Result->SetNumberField(TEXT("process_id"), static_cast<double>(ProcessId));
    Result->SetBoolField(TEXT("waited"), bWaitForCompletion);
    Result->SetBoolField(TEXT("timed_out"), bTimedOut);
    if (bWaitForCompletion)
    {
        Result->SetBoolField(TEXT("commandlet_success"), !bTimedOut && ReturnCode == 0);
        Result->SetNumberField(TEXT("exit_code"), ReturnCode);
        const FString LogTail = ReadCommandletLogTail(CommandletLogFilename);
        if (!LogTail.IsEmpty())
        {
            Result->SetStringField(TEXT("log_tail"), LogTail);
        }
        if (bTimedOut)
        {
            Result->SetStringField(TEXT("error"), TEXT("Commandlet timed out and was terminated"));
        }
        else if (ReturnCode != 0)
        {
            Result->SetStringField(TEXT("error"), TEXT("Commandlet exited with a non-zero exit code; inspect log_tail/log_path"));
        }
    }
    else
    {
        Result->SetStringField(TEXT("note"), TEXT("Commandlet process started asynchronously. Use log_path to inspect progress."));
    }
    return Result;
}

static bool TryGetRegionBoxFromParams(const TSharedPtr<FJsonObject>& Params, FBox& OutBox)
{
    double MinX = 0.0, MinY = 0.0, MinZ = 0.0, MaxX = 0.0, MaxY = 0.0, MaxZ = 0.0;
    const bool bHasAllFields =
        Params->TryGetNumberField(TEXT("min_x"), MinX) &&
        Params->TryGetNumberField(TEXT("min_y"), MinY) &&
        Params->TryGetNumberField(TEXT("min_z"), MinZ) &&
        Params->TryGetNumberField(TEXT("max_x"), MaxX) &&
        Params->TryGetNumberField(TEXT("max_y"), MaxY) &&
        Params->TryGetNumberField(TEXT("max_z"), MaxZ);

    if (!bHasAllFields)
    {
        return false;
    }

    OutBox = FBox(
        FVector(static_cast<double>(MinX), static_cast<double>(MinY), static_cast<double>(MinZ)),
        FVector(static_cast<double>(MaxX), static_cast<double>(MaxY), static_cast<double>(MaxZ)));
    return OutBox.IsValid != 0;
}

static void AddBoxFields(TSharedPtr<FJsonObject> Object, const FString& Prefix, const FBox& Box)
{
    Object->SetNumberField(Prefix + TEXT("_min_x"), Box.Min.X);
    Object->SetNumberField(Prefix + TEXT("_min_y"), Box.Min.Y);
    Object->SetNumberField(Prefix + TEXT("_min_z"), Box.Min.Z);
    Object->SetNumberField(Prefix + TEXT("_max_x"), Box.Max.X);
    Object->SetNumberField(Prefix + TEXT("_max_y"), Box.Max.Y);
    Object->SetNumberField(Prefix + TEXT("_max_z"), Box.Max.Z);
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

static bool IsLevelReferencedByCurrentWorld(const FString& AssetPath)
{
    if (!GEditor)
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return false;
    }

    const FString PackageName = NormalizeLevelPackageName(AssetPath);
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (!StreamingLevel)
        {
            continue;
        }

        if (StreamingLevel->GetWorldAssetPackageName() == PackageName)
        {
            return true;
        }

        if (ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel())
        {
            if (LoadedLevel->GetOutermost() && LoadedLevel->GetOutermost()->GetName() == PackageName)
            {
                return true;
            }
        }
    }

    return false;
}

static FString MakeSafeAssetName(const FString& Name)
{
    FString SafeName = FPaths::MakeValidFileName(Name);
    SafeName.ReplaceInline(TEXT(" "), TEXT("_"));
    return SafeName.IsEmpty() ? TEXT("MCP_DataLayer") : SafeName;
}

static FString GetDataLayerAssetPath(const FString& DataLayerName)
{
    return FString::Printf(TEXT("/Game/DataLayers/%s"), *MakeSafeAssetName(DataLayerName));
}

static UDataLayerAsset* FindDataLayerAssetByName(const FString& DataLayerName)
{
    const FString SafeName = MakeSafeAssetName(DataLayerName);
    const FString DirectObjectPath = FString::Printf(TEXT("/Game/DataLayers/%s.%s"), *SafeName, *SafeName);
    if (UDataLayerAsset* LoadedAsset = LoadObject<UDataLayerAsset>(nullptr, *DirectObjectPath))
    {
        return LoadedAsset;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    TArray<FAssetData> Assets;
    AssetRegistry.GetAssetsByClass(UDataLayerAsset::StaticClass()->GetClassPathName(), Assets, true);
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.AssetName.ToString() == DataLayerName || Asset.AssetName.ToString() == SafeName)
        {
            return Cast<UDataLayerAsset>(Asset.GetAsset());
        }
    }

    return nullptr;
}

static UDataLayerAsset* FindOrCreateDataLayerAsset(const FString& DataLayerName, FString* OutAssetPath = nullptr)
{
    const FString AssetPath = GetDataLayerAssetPath(DataLayerName);
    if (OutAssetPath)
    {
        *OutAssetPath = AssetPath;
    }

    if (UDataLayerAsset* ExistingAsset = FindDataLayerAssetByName(DataLayerName))
    {
        return ExistingAsset;
    }

    UEditorAssetLibrary::MakeDirectory(TEXT("/Game/DataLayers"));
    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return nullptr;
    }

    const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
    UDataLayerAsset* DataLayerAsset = NewObject<UDataLayerAsset>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!DataLayerAsset)
    {
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(DataLayerAsset);
    Package->MarkPackageDirty();
    UEditorAssetLibrary::SaveLoadedAsset(DataLayerAsset, false);
    return DataLayerAsset;
}

static UDataLayerInstance* FindOrCreateDataLayerInstance(const FString& DataLayerName, UDataLayerAsset* DataLayerAsset)
{
    if (!GEditor || !DataLayerAsset)
    {
        return nullptr;
    }

    UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
    if (!DataLayerSubsystem)
    {
        return nullptr;
    }

    if (UDataLayerInstance* ExistingInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerAsset))
    {
        return ExistingInstance;
    }
    if (UDataLayerInstance* ExistingInstance = DataLayerSubsystem->GetDataLayerInstance(FName(*DataLayerName)))
    {
        return ExistingInstance;
    }

    FDataLayerCreationParameters CreationParameters;
    CreationParameters.DataLayerAsset = DataLayerAsset;
    return DataLayerSubsystem->CreateDataLayerInstance(CreationParameters);
}

static TArray<AActor*> ResolveActorsByNames(UWorld* World, const TArray<TSharedPtr<FJsonValue>>& ActorNames)
{
    TArray<AActor*> Actors;
    if (!World)
    {
        return Actors;
    }

    for (const TSharedPtr<FJsonValue>& Val : ActorNames)
    {
        const FString ActorName = Val->AsString();
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
            {
                Actors.Add(*It);
                break;
            }
        }
    }
    return Actors;
}

static bool TryUnloadInactiveLevelPackage(const FString& PackageName, FString& OutError)
{
    UPackage* ExistingPackage = FindPackage(nullptr, *PackageName);
    if (!ExistingPackage)
    {
        return true;
    }

    FText UnloadError;
    if (FEditorFileUtils::AttemptUnloadInactiveWorldPackage(ExistingPackage, UnloadError))
    {
        return true;
    }

    OutError = UnloadError.ToString();
    return false;
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
    else
    {
        bool bPackagesNeededSaving = false;
        bSaved = FEditorFileUtils::SaveDirtyPackages(
            false,  // Do not show the save prompt for MCP automation.
            true,
            true,
            true,   // Fast save avoids checkout dialogs and Slate-heavy save prompts.
            false,
            true,
            &bPackagesNeededSaving,
            FEditorFileUtils::FShouldIgnorePackage::Default,
            true);
    }
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

    FString AssetName = FPaths::GetBaseFilename(AssetPath);
    FString PackagePath = FPaths::GetPath(AssetPath);
    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game/EditorWidgets");
    }

    FString FullPackagePath = PackagePath / AssetName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPackagePath))
    {
        UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullPackagePath);
        if (!ExistingAsset || !ExistingAsset->IsA<UWidgetBlueprint>())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Asset already exists but is not a Widget Blueprint: %s"), *FullPackagePath));
        }

        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetBoolField(TEXT("already_exists"), true);
        R->SetStringField(TEXT("asset_path"), FullPackagePath);
        R->SetStringField(TEXT("asset_name"), AssetName);
        R->SetStringField(TEXT("parent_class"), UUserWidget::StaticClass()->GetName());
        return R;
    }

    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for utility widget"));
    }
    if (FindObject<UBlueprint>(Package, *AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint object already exists in package: %s"), *FullPackagePath));
    }

    UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(
        FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(),
            Package,
            FName(*AssetName),
            BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass(),
            FName("MCP")
        )
    );

    if (!WidgetBP)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
    }

    FAssetRegistryModule::AssetCreated(WidgetBP);
    Package->MarkPackageDirty();
    UEditorAssetLibrary::SaveLoadedAsset(WidgetBP, false);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("asset_path"), FullPackagePath);
    R->SetStringField(TEXT("asset_name"), AssetName);
    R->SetStringField(TEXT("parent_class"), UUserWidget::StaticClass()->GetName());
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateUtilityBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }

    FString AssetName = FPaths::GetBaseFilename(AssetPath);
    FString PackagePath = FPaths::GetPath(AssetPath);
    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game/EditorBlueprints");
    }

    FString FullPackagePath = PackagePath / AssetName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPackagePath))
    {
        UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullPackagePath);
        if (!ExistingAsset || !ExistingAsset->IsA<UBlueprint>())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Asset already exists but is not a Blueprint: %s"), *FullPackagePath));
        }

        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetBoolField(TEXT("already_exists"), true);
        R->SetStringField(TEXT("asset_path"), FullPackagePath);
        R->SetStringField(TEXT("asset_name"), AssetName);
        R->SetStringField(TEXT("blueprint_type"), TEXT("FunctionLibrary"));
        R->SetStringField(TEXT("category"), TEXT("Editor"));
        return R;
    }

    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for utility blueprint"));
    }
    if (FindObject<UBlueprint>(Package, *AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint object already exists in package: %s"), *FullPackagePath));
    }

    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        Package,
        FName(*AssetName),
        BPTYPE_FunctionLibrary,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        FName("MCP")
    );

    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Utility Blueprint"));
    }

    Blueprint->BlueprintCategory = TEXT("Editor");
    FAssetRegistryModule::AssetCreated(Blueprint);
    Package->MarkPackageDirty();
    UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("asset_path"), FullPackagePath);
    R->SetStringField(TEXT("asset_name"), AssetName);
    R->SetStringField(TEXT("blueprint_type"), TEXT("FunctionLibrary"));
    R->SetStringField(TEXT("category"), TEXT("Editor"));
    return R;
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
    bool bWaitForCompletion = false;
    Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
    double TimeoutSeconds = 1800.0;
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
    return RunEditorCommandletProcess(CommandletName, Args, bWaitForCompletion, TimeoutSeconds);
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

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString OutputPath;
    Params->TryGetStringField(TEXT("output_path"), OutputPath);

    if (OutputPath.IsEmpty())
    {
        FDateTime Now = FDateTime::Now();
        FString Timestamp = Now.ToString(TEXT("%Y%m%d_%H%M%S"));
        OutputPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("MCP_Screenshot_%s.png"), *Timestamp);
    }

    // Ensure directory exists
    FString OutputDir = FPaths::GetPath(OutputPath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*OutputDir))
    {
        PlatformFile.CreateDirectoryTree(*OutputDir);
    }

    FEditorViewportClient* Client = GetSafeEditorViewportClient();
    if (!Client)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor viewport client available"));
    }

    FViewport* Viewport = Client->Viewport;
    if (!Viewport)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active viewport available"));
    }

    // Read viewport pixels
    TArray<FColor> Bitmap;
    FIntRect Rect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
    bool bReadSuccess = Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), Rect);
    if (!bReadSuccess || Bitmap.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read viewport pixels"));
    }

    // Flip vertically (Unreal renders top-down, most image formats expect bottom-up)
    const int32 Width = Rect.Width();
    const int32 Height = Rect.Height();
    for (int32 Y = 0; Y < Height / 2; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            Swap(Bitmap[Y * Width + X], Bitmap[(Height - 1 - Y) * Width + X]);
        }
    }

    // Save via FImageUtils (UE 5.7 takes FImageView)
    FImageView ImageView(Bitmap.GetData(), Width, Height);
    FImageUtils::SaveImageByExtension(*OutputPath, ImageView);

    bool bFileExists = FPaths::FileExists(OutputPath);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bFileExists);
    Result->SetStringField(TEXT("output_path"), OutputPath);
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    if (!bFileExists)
    {
        Result->SetStringField(TEXT("error"), TEXT("Screenshot was captured but the file may not have been saved correctly."));
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleExportLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }

    FString OutputPath;
    Params->TryGetStringField(TEXT("output_path"), OutputPath);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active world available"));
    }

    ULevel* CurrentLevel = World->GetCurrentLevel();
    if (!CurrentLevel)
    {
        CurrentLevel = World->PersistentLevel;
    }
    if (!CurrentLevel)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No persistent level available"));
    }

    if (OutputPath.IsEmpty())
    {
        FDateTime Now = FDateTime::Now();
        FString Timestamp = Now.ToString(TEXT("%Y%m%d_%H%M%S"));
        FString LevelName = FPaths::GetBaseFilename(CurrentLevel->GetOutermost()->GetName());
        OutputPath = FPaths::ProjectSavedDir() / TEXT("LevelExports") / FString::Printf(TEXT("%s_%s.json"), *LevelName, *Timestamp);
    }

    FString OutputDir = FPaths::GetPath(OutputPath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*OutputDir))
    {
        PlatformFile.CreateDirectoryTree(*OutputDir);
    }

    TSharedPtr<FJsonObject> LevelJson = MakeShared<FJsonObject>();
    LevelJson->SetStringField(TEXT("level_name"), CurrentLevel->GetOutermost()->GetName());
    LevelJson->SetStringField(TEXT("world_name"), World->GetName());

    TArray<TSharedPtr<FJsonValue>> ActorArray;
    int32 ActorCount = 0;
    FBox LevelBounds(ForceInit);

    for (AActor* Actor : CurrentLevel->Actors)
    {
        if (!Actor) { continue; }
        ActorCount++;

        TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
        ActorJson->SetStringField(TEXT("name"), Actor->GetName());
        ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
        ActorJson->SetStringField(TEXT("label"), Actor->GetActorLabel());

        FVector Loc = Actor->GetActorLocation();
        TSharedPtr<FJsonObject> LocJson = MakeShared<FJsonObject>();
        LocJson->SetNumberField(TEXT("x"), Loc.X);
        LocJson->SetNumberField(TEXT("y"), Loc.Y);
        LocJson->SetNumberField(TEXT("z"), Loc.Z);
        ActorJson->SetObjectField(TEXT("location"), LocJson);

        FRotator Rot = Actor->GetActorRotation();
        TSharedPtr<FJsonObject> RotJson = MakeShared<FJsonObject>();
        RotJson->SetNumberField(TEXT("pitch"), Rot.Pitch);
        RotJson->SetNumberField(TEXT("yaw"), Rot.Yaw);
        RotJson->SetNumberField(TEXT("roll"), Rot.Roll);
        ActorJson->SetObjectField(TEXT("rotation"), RotJson);

        FVector Scale = Actor->GetActorScale3D();
        TSharedPtr<FJsonObject> ScaleJson = MakeShared<FJsonObject>();
        ScaleJson->SetNumberField(TEXT("x"), Scale.X);
        ScaleJson->SetNumberField(TEXT("y"), Scale.Y);
        ScaleJson->SetNumberField(TEXT("z"), Scale.Z);
        ActorJson->SetObjectField(TEXT("scale"), ScaleJson);

        UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>();
        if (SMC && SMC->GetStaticMesh())
        {
            ActorJson->SetStringField(TEXT("static_mesh"), SMC->GetStaticMesh()->GetName());
        }

        ActorArray.Add(MakeShared<FJsonValueObject>(ActorJson));
        LevelBounds += Actor->GetActorLocation();
    }

    LevelJson->SetNumberField(TEXT("actor_count"), ActorCount);
    LevelJson->SetArrayField(TEXT("actors"), ActorArray);

    if (LevelBounds.IsValid)
    {
        TSharedPtr<FJsonObject> BoundsJson = MakeShared<FJsonObject>();
        FVector Min = LevelBounds.Min;
        FVector Max = LevelBounds.Max;
        BoundsJson->SetNumberField(TEXT("min_x"), Min.X);
        BoundsJson->SetNumberField(TEXT("min_y"), Min.Y);
        BoundsJson->SetNumberField(TEXT("min_z"), Min.Z);
        BoundsJson->SetNumberField(TEXT("max_x"), Max.X);
        BoundsJson->SetNumberField(TEXT("max_y"), Max.Y);
        BoundsJson->SetNumberField(TEXT("max_z"), Max.Z);
        LevelJson->SetObjectField(TEXT("approx_bounds"), BoundsJson);
    }

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(LevelJson.ToSharedRef(), Writer);

    if (!FFileHelper::SaveStringToFile(JsonString, *OutputPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to write export file: %s"), *OutputPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("output_path"), OutputPath);
    Result->SetNumberField(TEXT("actor_count"), ActorCount);
    Result->SetStringField(TEXT("format"), TEXT("json"));
    return Result;
}

// ---------------------------------------------------------------------------
// Level / Map Management (Phase 1)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("create level"))) { return Busy; }
    FString AssetPath; if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path")); }
    const FString PackageName = NormalizeLevelPackageName(AssetPath);

    FString TargetFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, TargetFilename, FPackageName::GetMapPackageExtension()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid map package path: %s"), *AssetPath));
    }

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath) || FPlatformFileManager::Get().GetPlatformFile().FileExists(*TargetFilename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset already exists: %s"), *AssetPath));
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
    }

    UWorldFactory* WorldFactory = NewObject<UWorldFactory>();
    WorldFactory->WorldType = EWorldType::Inactive;
    WorldFactory->bInformEngineOfWorld = false;

    const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
    UWorld* NewWorld = Cast<UWorld>(WorldFactory->FactoryCreateNew(
        UWorld::StaticClass(),
        Package,
        FName(*AssetName),
        RF_Public | RF_Standalone,
        nullptr,
        GWarn
    ));
    if (!NewWorld)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create world asset: %s"), *AssetPath));
    }

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetFilename), true);

    Package->MarkPackageDirty();
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GWarn;
    const bool bSaved = UPackage::SavePackage(Package, NewWorld, *TargetFilename, SaveArgs);
    UPackage::WaitForAsyncFileWrites();

    if (!bSaved)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save new level: %s"), *TargetFilename));
    }

    FAssetRegistryModule::AssetCreated(NewWorld);
    ScanAssetFile(TargetFilename);

    FString UnloadWarning;
    const bool bUnloadedPackage = TryUnloadInactiveLevelPackage(PackageName, UnloadWarning);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("asset_path"), AssetPath);
    R->SetStringField(TEXT("filename"), TargetFilename);
    R->SetStringField(TEXT("source_path"), TEXT("UWorldFactory::FactoryCreateNew"));
    R->SetBoolField(TEXT("unloaded_package"), bUnloadedPackage);
    if (!bUnloadedPackage)
    {
        R->SetStringField(TEXT("warning"), FString::Printf(TEXT("Level was saved but its inactive package could not be unloaded: %s"), *UnloadWarning));
    }
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
    const FString PackageName = NormalizeLevelPackageName(AssetPath);

    if (IsCurrentEditorWorldPackage(AssetPath))
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetBoolField(TEXT("already_loaded"), true);
        R->SetStringField(TEXT("asset_path"), AssetPath);
        return R;
    }

    FString MapFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, MapFilename, FPackageName::GetMapPackageExtension()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid map package path: %s"), *AssetPath));
    }

    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*MapFilename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Map file does not exist: %s"), *MapFilename));
    }

    if (UPackage* ExistingPackage = FindPackage(nullptr, *PackageName))
    {
        FText UnloadError;
        const bool bUnloaded = FEditorFileUtils::AttemptUnloadInactiveWorldPackage(ExistingPackage, UnloadError);

        if (!bUnloaded)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("Cannot safely load level because package is already resident and could not be unloaded: %s. Reason: %s"),
                *PackageName,
                *UnloadError.ToString()
            ));
        }
    }

    UWorld* LoadedWorld = UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
    bool bLoaded = LoadedWorld != nullptr;

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
    R->SetStringField(TEXT("map_filename"), MapFilename);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleDuplicateLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("duplicate level"))) { return Busy; }
    FString SourcePath; if (!Params->TryGetStringField(TEXT("source_path"), SourcePath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing source_path")); }
    FString DestPath; if (!Params->TryGetStringField(TEXT("dest_path"), DestPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing dest_path")); }

    const FString SourcePackageName = NormalizeLevelPackageName(SourcePath);
    const FString DestPackageName = NormalizeLevelPackageName(DestPath);

    FString SourceFilename;
    FString DestFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(SourcePackageName, SourceFilename, FPackageName::GetMapPackageExtension()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid source map path: %s"), *SourcePath));
    }
    if (!FPackageName::TryConvertLongPackageNameToFilename(DestPackageName, DestFilename, FPackageName::GetMapPackageExtension()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid destination map path: %s"), *DestPath));
    }
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*SourceFilename))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source map file does not exist: %s"), *SourceFilename));
    }
    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*DestFilename) || UEditorAssetLibrary::DoesAssetExist(DestPackageName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Destination level already exists: %s"), *DestPackageName));
    }

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(DestFilename), true);
    const bool bCopied = IFileManager::Get().Copy(*DestFilename, *SourceFilename, true, true) == COPY_OK;
    if (bCopied)
    {
        ScanAssetFile(SourceFilename);
        ScanAssetFile(DestFilename);
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bCopied);
    R->SetStringField(TEXT("source_path"), SourcePackageName);
    R->SetStringField(TEXT("dest_path"), DestPackageName);
    R->SetStringField(TEXT("source_filename"), SourceFilename);
    R->SetStringField(TEXT("dest_filename"), DestFilename);
    if (bCopied)
    {
        R->SetStringField(TEXT("new_asset_path"), DestPackageName);
        R->SetStringField(TEXT("method"), TEXT("file_copy"));
    }
    else
    {
        R->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to copy map file from %s to %s"), *SourceFilename, *DestFilename));
    }
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRenameLevel(const TSharedPtr<FJsonObject>& Params)
{
    if (TSharedPtr<FJsonObject> Busy = CreateEditorPlayBusyError(TEXT("rename level"))) { return Busy; }
    FString SourcePath; if (!Params->TryGetStringField(TEXT("source_path"), SourcePath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing source_path")); }
    FString DestPath; if (!Params->TryGetStringField(TEXT("dest_path"), DestPath)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing dest_path")); }

    const FString SourcePackageName = NormalizeLevelPackageName(SourcePath);
    const FString DestPackageName = NormalizeLevelPackageName(DestPath);
    if (IsCurrentEditorWorldPackage(SourcePackageName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Cannot rename the currently loaded level: %s. Load a different level first."), *SourcePackageName));
    }

    FString SourceFilename;
    FString DestFilename;
    const bool bHasSourceMapFilename = FPackageName::TryConvertLongPackageNameToFilename(
        SourcePackageName, SourceFilename, FPackageName::GetMapPackageExtension());
    const bool bHasDestMapFilename = FPackageName::TryConvertLongPackageNameToFilename(
        DestPackageName, DestFilename, FPackageName::GetMapPackageExtension());

    const bool bDestAssetExists = UEditorAssetLibrary::DoesAssetExist(DestPackageName);
    const bool bDestFileExists = bHasDestMapFilename && FPlatformFileManager::Get().GetPlatformFile().FileExists(*DestFilename);
    if (bDestAssetExists || bDestFileExists)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Destination level already exists: %s"), *DestPackageName));
    }

    bool bRenamed = false;
    FString RenameMethod;

    if (bHasSourceMapFilename && bHasDestMapFilename)
    {
        FString UnloadError;
        if (!TryUnloadInactiveLevelPackage(SourcePackageName, UnloadError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Cannot unload source level before rename: %s. %s. Load a different map and remove it from streaming levels before retrying."),
                    *SourcePackageName,
                    *UnloadError));
        }

        const bool bSourceFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*SourceFilename);
        if (bSourceFileExists)
        {
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(DestFilename), true);
            bRenamed = IFileManager::Get().Move(*DestFilename, *SourceFilename, false, true, true, true);
            if (bRenamed)
            {
                RenameMethod = TEXT("file_move");
                ScanAssetFile(SourceFilename);
                ScanAssetFile(DestFilename);
            }
        }
    }

    if (!bRenamed && !bHasSourceMapFilename)
    {
        if (UObject* SourceAsset = UEditorAssetLibrary::LoadAsset(SourcePackageName))
        {
            TArray<FAssetRenameData> RenameData;
            RenameData.Emplace(SourceAsset, FPaths::GetPath(DestPackageName), FPackageName::GetLongPackageAssetName(DestPackageName));

            FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
            bRenamed = AssetToolsModule.Get().RenameAssets(RenameData);
            if (bRenamed)
            {
                RenameMethod = TEXT("asset_tools");
                UEditorAssetLibrary::SaveAsset(DestPackageName, false);
            }
        }
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bRenamed);
    R->SetStringField(TEXT("source_path"), SourcePackageName);
    R->SetStringField(TEXT("dest_path"), DestPackageName);
    if (!RenameMethod.IsEmpty())
    {
        R->SetStringField(TEXT("method"), RenameMethod);
    }
    if (!bRenamed)
    {
        R->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to rename level from %s to %s"), *SourcePackageName, *DestPackageName));
        if (bHasSourceMapFilename)
        {
            R->SetStringField(TEXT("source_filename"), SourceFilename);
        }
        if (bHasDestMapFilename)
        {
            R->SetStringField(TEXT("dest_filename"), DestFilename);
        }
    }
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
    if (IsLevelReferencedByCurrentWorld(PackageName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Cannot delete level while it is referenced by the current world's streaming levels: %s. Remove or unload the sublevel first."), *PackageName));
    }

    FString MapFilename;
    const bool bHasMapFilename = FPackageName::TryConvertLongPackageNameToFilename(PackageName, MapFilename, FPackageName::GetMapPackageExtension());
    FString UnloadError;
    if (!TryUnloadInactiveLevelPackage(PackageName, UnloadError))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Cannot unload level package before delete: %s. %s. Load a different map and remove it from streaming levels before retrying."),
                *PackageName,
                *UnloadError));
    }

    const bool bAssetExists = UEditorAssetLibrary::DoesAssetExist(PackageName);
    const bool bFileExists = bHasMapFilename && FPlatformFileManager::Get().GetPlatformFile().FileExists(*MapFilename);
    if (!bAssetExists && !bFileExists)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    bool bDeleted = false;
    FString DeleteMethod;

    if (bFileExists)
    {
        bDeleted = IFileManager::Get().Delete(*MapFilename, false, true, true);
        if (bDeleted)
        {
            DeleteMethod = TEXT("file_delete");
            ScanAssetFile(MapFilename);
        }
    }
    else if (bAssetExists && bHasMapFilename)
    {
        ScanAssetFile(MapFilename);
        bDeleted = true;
        DeleteMethod = TEXT("registry_rescan_missing_file");
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), bDeleted);
    R->SetStringField(TEXT("asset_path"), PackageName);
    if (bHasMapFilename)
    {
        R->SetStringField(TEXT("filename"), MapFilename);
    }
    if (!DeleteMethod.IsEmpty())
    {
        R->SetStringField(TEXT("method"), DeleteMethod);
    }
    if (!bDeleted)
    {
        R->SetStringField(TEXT("error"), bHasMapFilename
            ? FString::Printf(TEXT("Failed to delete map asset or file: %s"), *MapFilename)
            : FString::Printf(TEXT("Failed to delete asset: %s"), *AssetPath));
    }
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
    R->SetBoolField(TEXT("streaming_enabled_in_editor"), WP->IsStreamingEnabledInEditor());
    R->SetBoolField(TEXT("has_loaded_user_regions"), WP->HasLoadedUserCreatedRegions());
    AddBoxFields(R, TEXT("editor_bounds"), Bounds);
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

    FBox RequestedRegion(ForceInit);
    if (!TryGetRegionBoxFromParams(Params, RequestedRegion))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("load_cell requires min_x/min_y/min_z/max_x/max_y/max_z"));
    }

    TArray<FBox> Regions = WP->GetUserLoadedEditorRegions();
    bool bAlreadyLoaded = false;
    for (const FBox& Region : Regions)
    {
        if (Region.Equals(RequestedRegion))
        {
            bAlreadyLoaded = true;
            break;
        }
    }
    if (!bAlreadyLoaded)
    {
        Regions.Add(RequestedRegion);
    }
    WP->LoadLastLoadedRegions(Regions);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("already_loaded"), bAlreadyLoaded);
    R->SetNumberField(TEXT("loaded_region_count"), Regions.Num());
    AddBoxFields(R, TEXT("region"), RequestedRegion);
    R->SetStringField(TEXT("note"), TEXT("World Partition editor region load requested"));
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

    TArray<FBox> Regions = WP->GetUserLoadedEditorRegions();
    const int32 BeforeCount = Regions.Num();
    FBox RequestedRegion(ForceInit);
    bool bClearAll = true;
    Params->TryGetBoolField(TEXT("clear_all"), bClearAll);
    if (TryGetRegionBoxFromParams(Params, RequestedRegion))
    {
        bClearAll = false;
        Regions.RemoveAll([&RequestedRegion](const FBox& Region)
        {
            return Region.Equals(RequestedRegion) || Region.Intersect(RequestedRegion);
        });
    }
    else if (bClearAll)
    {
        Regions.Reset();
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("unload_cell requires region coordinates or clear_all=true"));
    }

    WP->LoadLastLoadedRegions(Regions);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetBoolField(TEXT("cleared_all"), bClearAll);
    R->SetNumberField(TEXT("previous_loaded_region_count"), BeforeCount);
    R->SetNumberField(TEXT("loaded_region_count"), Regions.Num());
    if (!bClearAll && RequestedRegion.IsValid != 0)
    {
        AddBoxFields(R, TEXT("region"), RequestedRegion);
    }
    R->SetStringField(TEXT("note"), TEXT("World Partition editor region unload requested"));
    return R;
}

// ---------------------------------------------------------------------------
// Data Layer / HLOD / OFPA / Bounds / Origin Rebasing (Phase 4)
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateDataLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing name")); }
    if (!GEditor) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GEditor not available")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    FString AssetPath;
    UDataLayerAsset* DataLayerAsset = FindOrCreateDataLayerAsset(DataLayerName, &AssetPath);
    if (!DataLayerAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create DataLayerAsset for '%s'"), *DataLayerName));
    }

    UDataLayerInstance* DataLayerInstance = FindOrCreateDataLayerInstance(DataLayerName, DataLayerAsset);
    if (DataLayerInstance)
    {
        World->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("data_layer_name"), DataLayerName);
    R->SetStringField(TEXT("asset_path"), AssetPath);
    if (DataLayerInstance)
    {
        R->SetStringField(TEXT("instance_name"), DataLayerInstance->GetDataLayerShortName());
    }
    else
    {
        R->SetStringField(TEXT("warning"), TEXT("DataLayerAsset was created, but this world does not expose WorldDataLayers for an editor instance."));
    }
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleAddActorsToDataLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("data_layer_name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing data_layer_name")); }
    const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
    if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNames) || !ActorNames) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_names array")); }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    UDataLayerAsset* DataLayerAsset = FindOrCreateDataLayerAsset(DataLayerName);
    if (!DataLayerAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("DataLayerAsset '%s' not found. Create it first."), *DataLayerName));
    }

    TArray<AActor*> Actors = ResolveActorsByNames(World, *ActorNames);
    UDataLayerInstance* DataLayerInstance = FindOrCreateDataLayerInstance(DataLayerName, DataLayerAsset);
    int32 ModifiedCount = Actors.Num();
    if (DataLayerInstance)
    {
        if (UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>())
        {
            DataLayerSubsystem->AddActorsToDataLayer(Actors, DataLayerInstance);
        }
    }
    else
    {
        for (AActor* Actor : Actors)
        {
            FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(TEXT("DataLayerAssets")));
            if (Prop && Prop->IsA<FArrayProperty>())
            {
                FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop);
                FScriptArrayHelper ArrayHelper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(Actor));
                FProperty* InnerProp = ArrProp->Inner;
                if (InnerProp)
                {
                    const int32 NewIndex = ArrayHelper.AddValue();
                    void* ElementPtr = ArrayHelper.GetRawPtr(NewIndex);
                    TSoftObjectPtr<UDataLayerAsset> TargetPtr(DataLayerAsset);
                    InnerProp->CopyCompleteValue(ElementPtr, &TargetPtr);
                    Actor->Modify();
                }
            }
        }
    }
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("modified_count"), ModifiedCount);
    R->SetStringField(TEXT("data_layer_name"), DataLayerName);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRemoveActorsFromDataLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("data_layer_name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing data_layer_name")); }
    const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
    if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNames) || !ActorNames) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing actor_names array")); }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available")); }

    UDataLayerAsset* DataLayerAsset = FindDataLayerAssetByName(DataLayerName);
    if (!DataLayerAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("DataLayerAsset '%s' not found."), *DataLayerName));
    }

    TArray<AActor*> Actors = ResolveActorsByNames(World, *ActorNames);
    UDataLayerInstance* DataLayerInstance = FindOrCreateDataLayerInstance(DataLayerName, DataLayerAsset);
    int32 ModifiedCount = Actors.Num();
    if (DataLayerInstance)
    {
        if (UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>())
        {
            DataLayerSubsystem->RemoveActorsFromDataLayer(Actors, DataLayerInstance);
        }
    }
    else
    {
        for (AActor* Actor : Actors)
        {
            FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName(TEXT("DataLayerAssets")));
            if (Prop && Prop->IsA<FArrayProperty>())
            {
                FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop);
                FScriptArrayHelper ArrayHelper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(Actor));
                FProperty* InnerProp = ArrProp->Inner;
                if (InnerProp)
                {
                    TSoftObjectPtr<UDataLayerAsset> TargetPtr(DataLayerAsset);
                    for (int32 i = ArrayHelper.Num() - 1; i >= 0; --i)
                    {
                        void* ElementPtr = ArrayHelper.GetRawPtr(i);
                        TSoftObjectPtr<UDataLayerAsset> CurrentPtr;
                        InnerProp->CopyCompleteValue(&CurrentPtr, ElementPtr);
                        if (CurrentPtr == TargetPtr)
                        {
                            ArrayHelper.RemoveValues(i);
                            Actor->Modify();
                        }
                    }
                }
            }
        }
    }
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetNumberField(TEXT("modified_count"), ModifiedCount);
    R->SetStringField(TEXT("data_layer_name"), DataLayerName);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleSetDataLayerEnabled(const TSharedPtr<FJsonObject>& Params)
{
    FString DataLayerName; if (!Params->TryGetStringField(TEXT("data_layer_name"), DataLayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing data_layer_name")); }
    bool bEnabled = true; Params->TryGetBoolField(TEXT("enabled"), bEnabled);

    UDataLayerAsset* DataLayerAsset = FindOrCreateDataLayerAsset(DataLayerName);
    UDataLayerInstance* DataLayerInstance = FindOrCreateDataLayerInstance(DataLayerName, DataLayerAsset);
    if (UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor ? GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>() : nullptr)
    {
        if (DataLayerInstance)
        {
            DataLayerSubsystem->SetDataLayerVisibility(DataLayerInstance, bEnabled);
            DataLayerSubsystem->SetDataLayerIsLoadedInEditor(DataLayerInstance, bEnabled, true);
        }
    }

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("data_layer_name"), DataLayerName);
    R->SetBoolField(TEXT("enabled"), bEnabled);
    if (DataLayerAsset) { R->SetStringField(TEXT("asset_path"), DataLayerAsset->GetPathName()); }
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleCreateHLODLayer(const TSharedPtr<FJsonObject>& Params)
{
    FString LayerName; if (!Params->TryGetStringField(TEXT("name"), LayerName)) { return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing name")); }

    FString PackagePath = FPaths::Combine(TEXT("/Game"), TEXT("HLOD"), LayerName);
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for HLOD layer"));
    }

    UHLODLayer* HLODLayer = NewObject<UHLODLayer>(Package, FName(*LayerName), RF_Public | RF_Standalone);
    if (!HLODLayer)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create HLOD layer object"));
    }

    FAssetRegistryModule::AssetCreated(HLODLayer);
    Package->MarkPackageDirty();

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Package);
    FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("name"), LayerName);
    R->SetStringField(TEXT("asset_path"), PackagePath);
    R->SetStringField(TEXT("note"), TEXT("HLOD layer created. Assign to actors via their HLODLayer property."));
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleBuildHLOD(const TSharedPtr<FJsonObject>& Params)
{
    FString MapPath; Params->TryGetStringField(TEXT("map_path"), MapPath);
    if (MapPath.IsEmpty() && GEditor)
    {
        if (UWorld* World = GEditor->GetEditorWorldContext().World())
        {
            MapPath = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
        }
    }
    if (MapPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("map_path is required when no editor world map is active"));
    }

    bool bWaitForCompletion = false;
    Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
    double TimeoutSeconds = 3600.0;
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);

    FString ExtraArgs;
    Params->TryGetStringField(TEXT("extra_args"), ExtraArgs);

    bool bSetupHLODs = true;
    bool bBuildHLODs = true;
    bool bDumpStats = false;
    Params->TryGetBoolField(TEXT("setup_hlods"), bSetupHLODs);
    Params->TryGetBoolField(TEXT("build_hlods"), bBuildHLODs);
    Params->TryGetBoolField(TEXT("dump_stats"), bDumpStats);

    FString Args = FString::Printf(
        TEXT("%s -AllowCommandletRendering -Builder=WorldPartitionHLODsBuilder -SCCProvider=None"),
        *QuoteCommandletArg(MapPath));
    if (bSetupHLODs) { Args += TEXT(" -SetupHLODs"); }
    if (bBuildHLODs) { Args += TEXT(" -BuildHLODs"); }
    if (bDumpStats) { Args += TEXT(" -DumpStats"); }
    if (!ExtraArgs.IsEmpty()) { Args += TEXT(" ") + ExtraArgs; }

    TSharedPtr<FJsonObject> R = RunEditorCommandletProcess(TEXT("WorldPartitionBuilderCommandlet"), Args, bWaitForCompletion, TimeoutSeconds);
    R->SetStringField(TEXT("builder"), TEXT("WorldPartitionHLODsBuilder"));
    R->SetStringField(TEXT("map_path"), MapPath);
    R->SetBoolField(TEXT("setup_hlods"), bSetupHLODs);
    R->SetBoolField(TEXT("build_hlods"), bBuildHLODs);
    return R;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProjectEditorCommands::HandleRebuildHLOD(const TSharedPtr<FJsonObject>& Params)
{
    FString MapPath; Params->TryGetStringField(TEXT("map_path"), MapPath);
    if (MapPath.IsEmpty() && GEditor)
    {
        if (UWorld* World = GEditor->GetEditorWorldContext().World())
        {
            MapPath = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
        }
    }
    if (MapPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("map_path is required when no editor world map is active"));
    }

    bool bWaitForCompletion = false;
    Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
    double TimeoutSeconds = 3600.0;
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);

    bool bDeleteHLODs = false;
    bool bDumpStats = true;
    Params->TryGetBoolField(TEXT("delete_hlods"), bDeleteHLODs);
    Params->TryGetBoolField(TEXT("dump_stats"), bDumpStats);

    FString ExtraArgs;
    Params->TryGetStringField(TEXT("extra_args"), ExtraArgs);

    FString Args = FString::Printf(
        TEXT("%s -AllowCommandletRendering -Builder=WorldPartitionHLODsBuilder -SCCProvider=None -SetupHLODs -BuildHLODs -RebuildHLODs"),
        *QuoteCommandletArg(MapPath));
    if (bDeleteHLODs) { Args += TEXT(" -DeleteHLODs"); }
    if (bDumpStats) { Args += TEXT(" -DumpStats"); }
    if (!ExtraArgs.IsEmpty()) { Args += TEXT(" ") + ExtraArgs; }

    TSharedPtr<FJsonObject> R = RunEditorCommandletProcess(TEXT("WorldPartitionBuilderCommandlet"), Args, bWaitForCompletion, TimeoutSeconds);
    R->SetStringField(TEXT("builder"), TEXT("WorldPartitionHLODsBuilder"));
    R->SetStringField(TEXT("map_path"), MapPath);
    R->SetBoolField(TEXT("delete_hlods"), bDeleteHLODs);
    R->SetBoolField(TEXT("rebuild_hlods"), true);
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

    ALevelBounds* BoundsActor = nullptr;
    for (TActorIterator<ALevelBounds> It(World); It; ++It)
    {
        BoundsActor = *It;
        break;
    }
    if (!BoundsActor)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = MakeUniqueObjectName(World, ALevelBounds::StaticClass(), TEXT("MCP_LevelBounds"));
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        BoundsActor = World->SpawnActor<ALevelBounds>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    }
    if (!BoundsActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create LevelBounds actor in world"));
    }

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

    BoundsActor->Modify();
    BoundsActor->bAutoUpdateBounds = false;
    const FVector Center = (Min + Max) * 0.5f;
    const FVector Extent = (Max - Min) * 0.5f;
    if (BoundsActor->BoxComponent)
    {
        BoundsActor->BoxComponent->Modify();
        BoundsActor->BoxComponent->SetBoxExtent(Extent.GetAbs());
    }
    BoundsActor->SetActorLocation(Center);
    BoundsActor->MarkLevelBoundsDirty();
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetStringField(TEXT("actor_name"), BoundsActor->GetName());
    R->SetBoolField(TEXT("auto_update_bounds"), BoundsActor->bAutoUpdateBounds);
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
