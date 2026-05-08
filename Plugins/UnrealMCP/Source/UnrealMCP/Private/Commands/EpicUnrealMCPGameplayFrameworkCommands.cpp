#include "Commands/EpicUnrealMCPGameplayFrameworkCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SaveGame.h"
#include "Engine/GameInstance.h"
#include "GameMapsSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/SceneComponent.h"
#include "UObject/UnrealType.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Editor.h"

namespace
{
const TCHAR* GameMapsSection = TEXT("/Script/EngineSettings.GameMapsSettings");

FString GetDefaultEngineConfigPath()
{
    FString ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini"));
    FPaths::MakeStandardFilename(ConfigPath);
    return FConfigCacheIni::NormalizeConfigIniPath(ConfigPath);
}

FString GetDefaultGameplayTagsConfigPath()
{
    FString ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultGameplayTags.ini"));
    FPaths::MakeStandardFilename(ConfigPath);
    return FConfigCacheIni::NormalizeConfigIniPath(ConfigPath);
}

bool WriteIniStringPreservingFormatting(const FString& ConfigPath, const TCHAR* Section, const TCHAR* Key, const FString& Value)
{
    FString ConfigContent;
    FFileHelper::LoadFileToString(ConfigContent, *ConfigPath);

    const FString SectionHeader = FString::Printf(TEXT("[%s]"), Section);
    const FString PropertyPrefix = FString::Printf(TEXT("%s="), Key);
    const FString NewLine = FString::Printf(TEXT("%s=%s"), Key, *Value);

    int32 SectionIndex = ConfigContent.Find(SectionHeader);
    if (SectionIndex == INDEX_NONE)
    {
        if (!ConfigContent.IsEmpty() && !ConfigContent.EndsWith(TEXT("\n")))
        {
            ConfigContent.Append(TEXT("\n"));
        }
        ConfigContent.Append(FString::Printf(TEXT("\n[%s]\n%s\n"), Section, *NewLine));
        return FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
    }

    int32 SectionEndIndex = ConfigContent.Find(TEXT("\n["), ESearchCase::IgnoreCase, ESearchDir::FromStart, SectionIndex + SectionHeader.Len());
    if (SectionEndIndex == INDEX_NONE)
    {
        SectionEndIndex = ConfigContent.Len();
    }

    const int32 PropertyIndex = ConfigContent.Find(PropertyPrefix, ESearchCase::IgnoreCase, ESearchDir::FromStart, SectionIndex + SectionHeader.Len());
    if (PropertyIndex != INDEX_NONE && PropertyIndex < SectionEndIndex)
    {
        int32 LineEndIndex = ConfigContent.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, PropertyIndex);
        if (LineEndIndex == INDEX_NONE)
        {
            LineEndIndex = ConfigContent.Len();
        }

        ConfigContent.RemoveAt(PropertyIndex, LineEndIndex - PropertyIndex, EAllowShrinking::No);
        ConfigContent.InsertAt(PropertyIndex, NewLine);
    }
    else
    {
        ConfigContent.InsertAt(SectionIndex + SectionHeader.Len(), FString::Printf(TEXT("\n%s"), *NewLine));
    }

    return FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
}

bool SetGameMapsConfigString(const TCHAR* Key, const FString& Value)
{
    const FString ConfigPath = GetDefaultEngineConfigPath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ConfigPath), true);

    GConfig->SetString(GameMapsSection, Key, *Value, *ConfigPath);
    GConfig->SetString(GameMapsSection, Key, *Value, GEngineIni);
    GConfig->Flush(false, GEngineIni);

    const bool bWroteDefaultConfig = WriteIniStringPreservingFormatting(ConfigPath, GameMapsSection, Key, Value);
    if (bWroteDefaultConfig)
    {
        GConfig->LoadFile(ConfigPath);
        GConfig->SetString(GameMapsSection, Key, *Value, *ConfigPath);
    }

    if (UGameMapsSettings* Settings = GetMutableDefault<UGameMapsSettings>())
    {
        Settings->ReloadConfig();
    }

    return bWroteDefaultConfig;
}

FString NormalizeGameClassPath(FString ClassPath)
{
    ClassPath.TrimStartAndEndInline();
    if (ClassPath.IsEmpty())
    {
        return ClassPath;
    }

    if (ClassPath.StartsWith(TEXT("Class'")) && ClassPath.EndsWith(TEXT("'")))
    {
        ClassPath = ClassPath.Mid(6, ClassPath.Len() - 7);
    }

    if (ClassPath.StartsWith(TEXT("/Script/")))
    {
        return ClassPath;
    }

    if (!ClassPath.StartsWith(TEXT("/")))
    {
        ClassPath = TEXT("/Game/") + ClassPath;
    }

    if (!ClassPath.StartsWith(TEXT("/Game/")))
    {
        return ClassPath;
    }

    if (ClassPath.EndsWith(TEXT("_C")))
    {
        return ClassPath;
    }

    if (ClassPath.Contains(TEXT(".")))
    {
        return ClassPath + TEXT("_C");
    }

    const FString AssetName = FPackageName::GetShortName(ClassPath);
    return FString::Printf(TEXT("%s.%s_C"), *ClassPath, *AssetName);
}

bool LoadGameplayClass(const FString& InputPath, UClass* RequiredParentClass, FString& OutNormalizedClassPath, UClass*& OutClass, FString& OutError)
{
    OutClass = nullptr;
    OutError.Reset();
    OutNormalizedClassPath = NormalizeGameClassPath(InputPath);

    if (OutNormalizedClassPath.IsEmpty())
    {
        OutError = TEXT("Class path is empty");
        return false;
    }

    OutClass = StaticLoadClass(RequiredParentClass, nullptr, *OutNormalizedClassPath);
    if (!OutClass)
    {
        OutError = FString::Printf(
            TEXT("Could not load class '%s'. For Blueprint assets, pass the asset path such as /Game/Folder/BP_Name or the generated class path /Game/Folder/BP_Name.BP_Name_C."),
            *OutNormalizedClassPath);
        return false;
    }

    if (!OutClass->IsChildOf(RequiredParentClass))
    {
        OutError = FString::Printf(TEXT("Class '%s' is not derived from '%s'"), *OutNormalizedClassPath, *RequiredParentClass->GetName());
        OutClass = nullptr;
        return false;
    }

    return true;
}

bool GetConfiguredDefaultGameModeClassPath(FString& OutClassPath)
{
    OutClassPath.Reset();
    GConfig->GetString(GameMapsSection, TEXT("GlobalDefaultGameMode"), OutClassPath, GEngineIni);
    if (!OutClassPath.IsEmpty())
    {
        return true;
    }

    FConfigFile DefaultEngineConfig;
    DefaultEngineConfig.Read(GetDefaultEngineConfigPath());
    return DefaultEngineConfig.GetString(GameMapsSection, TEXT("GlobalDefaultGameMode"), OutClassPath) && !OutClassPath.IsEmpty();
}

bool ResolveTargetGameModeClass(const TSharedPtr<FJsonObject>& Params, bool bRequireTarget, FString& OutClassPath, UClass*& OutClass, FString& OutError)
{
    OutClass = nullptr;
    OutClassPath.Reset();
    OutError.Reset();

    FString GameModePath;
    const bool bHasExplicitTarget = Params.IsValid() && Params->TryGetStringField(TEXT("gamemode_path"), GameModePath);
    if (!bHasExplicitTarget && !GetConfiguredDefaultGameModeClassPath(GameModePath))
    {
        if (bRequireTarget)
        {
            OutError = TEXT("No default GameMode is configured. Run set_default_gamemode first or pass 'gamemode_path'.");
        }
        return false;
    }

    if (!LoadGameplayClass(GameModePath, AGameModeBase::StaticClass(), OutClassPath, OutClass, OutError))
    {
        return false;
    }

    return true;
}

bool SetBlueprintClassDefaultClassProperty(
    UClass* TargetClass,
    const FName PropertyName,
    UClass* NewClass,
    UClass* RequiredNewParentClass,
    const TCHAR* TargetLabel,
    FString& OutError)
{
    OutError.Reset();

    if (!TargetClass)
    {
        OutError = FString::Printf(TEXT("%s class is null"), TargetLabel);
        return false;
    }

    UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(TargetClass);
    if (!Blueprint)
    {
        OutError = FString::Printf(
            TEXT("%s class '%s' is native, so this command cannot persist Blueprint defaults. Use a Blueprint-derived class as the target."),
            TargetLabel,
            *TargetClass->GetPathName());
        return false;
    }

    if (!NewClass || !NewClass->IsChildOf(RequiredNewParentClass))
    {
        OutError = FString::Printf(TEXT("New class is not derived from '%s'"), *RequiredNewParentClass->GetName());
        return false;
    }

    UObject* ClassDefaultObject = TargetClass->GetDefaultObject();
    FClassProperty* ClassProperty = FindFProperty<FClassProperty>(TargetClass, PropertyName);
    if (!ClassDefaultObject || !ClassProperty)
    {
        OutError = FString::Printf(TEXT("Property '%s' was not found on '%s'"), *PropertyName.ToString(), *TargetClass->GetName());
        return false;
    }

    Blueprint->Modify();
    ClassDefaultObject->Modify();
    ClassDefaultObject->PreEditChange(ClassProperty);
    ClassProperty->SetObjectPropertyValue_InContainer(ClassDefaultObject, NewClass);
    FPropertyChangedEvent PropertyChangedEvent(ClassProperty, EPropertyChangeType::ValueSet);
    ClassDefaultObject->PostEditChangeProperty(PropertyChangedEvent);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    if (UPackage* Package = Blueprint->GetOutermost())
    {
        Package->SetDirtyFlag(true);
    }

    return true;
}

bool SetDefaultGameModeClassProperty(
    const TSharedPtr<FJsonObject>& Params,
    const FName PropertyName,
    UClass* NewClass,
    UClass* RequiredNewParentClass,
    bool bRequireTarget,
    FString& OutGameModeClassPath,
    FString& OutError)
{
    UClass* GameModeClass = nullptr;
    if (!ResolveTargetGameModeClass(Params, bRequireTarget, OutGameModeClassPath, GameModeClass, OutError))
    {
        return false;
    }

    return SetBlueprintClassDefaultClassProperty(
        GameModeClass,
        PropertyName,
        NewClass,
        RequiredNewParentClass,
        TEXT("GameMode"),
        OutError);
}

FString GetPrimaryProjectModuleName()
{
    const FString ProjectName = FApp::GetProjectName();
    return ProjectName.IsEmpty() ? FPaths::GetBaseFilename(FPaths::GetProjectFilePath()) : ProjectName;
}

FString GetPrimaryProjectModuleSourceDir()
{
    const FString ModuleName = GetPrimaryProjectModuleName();
    FString SourceDir = FPaths::ProjectDir() / TEXT("Source") / ModuleName;
    FPaths::NormalizeDirectoryName(SourceDir);
    IFileManager::Get().MakeDirectory(*SourceDir, true);
    return SourceDir;
}

FString GetPrimaryProjectModuleApiMacro()
{
    FString ModuleName = GetPrimaryProjectModuleName().ToUpper();
    ModuleName.ReplaceInline(TEXT("-"), TEXT("_"));
    ModuleName.ReplaceInline(TEXT(" "), TEXT("_"));
    return ModuleName + TEXT("_API");
}

void AddNodeToSCS(USimpleConstructionScript* SCS, USCS_Node* NewNode)
{
    if (!SCS || !NewNode)
    {
        return;
    }

    const TArray<USCS_Node*>& RootNodes = SCS->GetRootNodes();
    if (RootNodes.Num() > 0 && RootNodes[0])
    {
        RootNodes[0]->AddChildNode(NewNode);
    }
    else
    {
        SCS->AddNode(NewNode);
    }
}

bool AddGameplayTagToSettings(const FString& TagName, const FString& Comment, bool& bOutAlreadyExisted)
{
    bOutAlreadyExisted = false;

    if (TagName.IsEmpty())
    {
        return false;
    }

    UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
    if (!Settings)
    {
        return false;
    }

    const FName TagFName(*TagName);
    for (const FGameplayTagTableRow& Row : Settings->GameplayTagList)
    {
        if (Row.Tag == TagFName)
        {
            bOutAlreadyExisted = true;
            return true;
        }
    }

    Settings->ImportTagsFromConfig = true;
    Settings->GameplayTagList.Add(FGameplayTagTableRow(TagFName, Comment));
    Settings->SortTags();

    const FString ConfigPath = GetDefaultGameplayTagsConfigPath();
    const bool bSaved = Settings->TryUpdateDefaultConfigFile(ConfigPath);
    GConfig->LoadFile(ConfigPath);

#if WITH_EDITOR
    UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();
#endif

    return bSaved;
}
}

FEpicUnrealMCPGameplayFrameworkCommands::FEpicUnrealMCPGameplayFrameworkCommands()
{
}

// ============================================================================
// Helper Functions
// ============================================================================

UBlueprint* FEpicUnrealMCPGameplayFrameworkCommands::CreateBlueprintWithParent(
    const FString& BlueprintName, 
    const FString& PackagePath, 
    UClass* ParentClass)
{
    if (!ParentClass)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateBlueprintWithParent: ParentClass is null"));
        return nullptr;
    }

    // Ensure package path ends with /
    FString NormalizedPath = PackagePath;
    if (!NormalizedPath.EndsWith(TEXT("/")))
    {
        NormalizedPath += TEXT("/");
    }

    FString FullPackageName = NormalizedPath + BlueprintName;
    FString ObjectPath = FString::Printf(TEXT("%s.%s"), *FullPackageName, *BlueprintName);

    if (UPackage* ExistingPackage = FindPackage(nullptr, *FullPackageName))
    {
        if (UBlueprint* ExistingBlueprint = FindObject<UBlueprint>(ExistingPackage, *BlueprintName))
        {
            UClass* ExistingClass = ExistingBlueprint->GeneratedClass ? ExistingBlueprint->GeneratedClass : ExistingBlueprint->ParentClass;
            if (!ExistingClass || !ExistingClass->IsChildOf(ParentClass))
            {
                UE_LOG(LogTemp, Error, TEXT("Existing blueprint %s does not derive from %s"), *ObjectPath, *ParentClass->GetName());
                return nullptr;
            }
            return ExistingBlueprint;
        }
    }

    if (FPackageName::DoesPackageExist(FullPackageName))
    {
        if (UBlueprint* ExistingBlueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath))
        {
            UClass* ExistingClass = ExistingBlueprint->GeneratedClass ? ExistingBlueprint->GeneratedClass : ExistingBlueprint->ParentClass;
            if (!ExistingClass || !ExistingClass->IsChildOf(ParentClass))
            {
                UE_LOG(LogTemp, Error, TEXT("Existing blueprint %s does not derive from %s"), *ObjectPath, *ParentClass->GetName());
                return nullptr;
            }
            return ExistingBlueprint;
        }

        UE_LOG(LogTemp, Error, TEXT("Package exists on disk but does not contain a blueprint asset: %s"), *FullPackageName);
        return nullptr;
    }

    // Create the package
    UPackage* Package = CreatePackage(*FullPackageName);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create package: %s"), *FullPackageName);
        return nullptr;
    }

    // Use FKismetEditorUtilities to create blueprint (safe API)
    UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
        ParentClass,
        Package,
        FName(*BlueprintName),
        EBlueprintType::BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        NAME_None
    );

    if (NewBlueprint)
    {
        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(NewBlueprint);
        // Mark the package dirty
        Package->MarkPackageDirty();
        
        UE_LOG(LogTemp, Log, TEXT("Created blueprint: %s with parent: %s"), 
            *BlueprintName, *ParentClass->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create blueprint: %s"), *BlueprintName);
    }

    return NewBlueprint;
}

bool FEpicUnrealMCPGameplayFrameworkCommands::SetProjectSetting(
    const FString& SectionName, 
    const FString& PropertyName, 
    const FString& Value)
{
    FString ConfigPath = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
    FString ConfigContent;
    
    if (FFileHelper::LoadFileToString(ConfigContent, *ConfigPath))
    {
        // Find the section
        FString SectionHeader = FString::Printf(TEXT("[%s]"), *SectionName);
        int32 SectionIndex = ConfigContent.Find(SectionHeader);
        
        if (SectionIndex != INDEX_NONE)
        {
            // Find end of section or insert before next section
            int32 InsertIndex = ConfigContent.Find(TEXT("\n["), ESearchCase::IgnoreCase, ESearchDir::FromStart, SectionIndex + 1);
            if (InsertIndex == INDEX_NONE)
            {
                InsertIndex = ConfigContent.Len();
            }
            
            // Check if property already exists
            FString PropertyLine = FString::Printf(TEXT("%s="), *PropertyName);
            int32 PropertyIndex = ConfigContent.Find(PropertyLine, ESearchCase::IgnoreCase, ESearchDir::FromStart, SectionIndex);
            
            if (PropertyIndex != INDEX_NONE && PropertyIndex < InsertIndex)
            {
                // Replace existing property
                int32 EndOfLine = ConfigContent.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, PropertyIndex);
                if (EndOfLine == INDEX_NONE) EndOfLine = ConfigContent.Len();
                
                FString OldLine = ConfigContent.Mid(PropertyIndex, EndOfLine - PropertyIndex);
                FString NewLine = FString::Printf(TEXT("%s=%s"), *PropertyName, *Value);
                ConfigContent.ReplaceInline(*OldLine, *NewLine);
            }
            else
            {
                // Insert new property after section header
                FString NewProperty = FString::Printf(TEXT("\n%s=%s"), *PropertyName, *Value);
                ConfigContent.InsertAt(SectionIndex + SectionHeader.Len(), NewProperty);
            }
            
            return FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
        }
        else
        {
            // Section doesn't exist, append it
            FString NewSection = FString::Printf(TEXT("\n\n[%s]\n%s=%s"), *SectionName, *PropertyName, *Value);
            ConfigContent.Append(NewSection);
            return FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
        }
    }
    
    return false;
}

FString FEpicUnrealMCPGameplayFrameworkCommands::GetProjectSetting(
    const FString& SectionName, 
    const FString& PropertyName)
{
    FString Result;
    GConfig->GetString(*SectionName, *PropertyName, Result, GEngineIni);
    return Result;
}

bool FEpicUnrealMCPGameplayFrameworkCommands::UpdateDefaultEngineIni(
    const FString& SectionName, 
    const FString& PropertyName, 
    const FString& Value)
{
    FString ConfigPath = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
    return SetProjectSetting(SectionName, PropertyName, Value);
}

bool FEpicUnrealMCPGameplayFrameworkCommands::UpdateDefaultGameIni(
    const FString& SectionName, 
    const FString& PropertyName, 
    const FString& Value)
{
    FString ConfigPath = FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini");
    FString ConfigContent;
    
    if (FFileHelper::LoadFileToString(ConfigContent, *ConfigPath))
    {
        FString SectionHeader = FString::Printf(TEXT("[%s]"), *SectionName);
        int32 SectionIndex = ConfigContent.Find(SectionHeader);
        
        if (SectionIndex != INDEX_NONE)
        {
            int32 InsertIndex = ConfigContent.Find(TEXT("\n["), ESearchCase::IgnoreCase, ESearchDir::FromStart, SectionIndex + 1);
            if (InsertIndex == INDEX_NONE)
            {
                InsertIndex = ConfigContent.Len();
            }
            
            FString PropertyLine = FString::Printf(TEXT("%s="), *PropertyName);
            int32 PropertyIndex = ConfigContent.Find(PropertyLine, ESearchCase::IgnoreCase, ESearchDir::FromStart, SectionIndex);
            
            if (PropertyIndex != INDEX_NONE && PropertyIndex < InsertIndex)
            {
                int32 EndOfLine = ConfigContent.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, PropertyIndex);
                if (EndOfLine == INDEX_NONE) EndOfLine = ConfigContent.Len();
                
                FString OldLine = ConfigContent.Mid(PropertyIndex, EndOfLine - PropertyIndex);
                FString NewLine = FString::Printf(TEXT("%s=%s"), *PropertyName, *Value);
                ConfigContent.ReplaceInline(*OldLine, *NewLine);
            }
            else
            {
                FString NewProperty = FString::Printf(TEXT("\n%s=%s"), *PropertyName, *Value);
                ConfigContent.InsertAt(SectionIndex + SectionHeader.Len(), NewProperty);
            }
            
            return FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
        }
        else
        {
            FString NewSection = FString::Printf(TEXT("\n\n[%s]\n%s=%s"), *SectionName, *PropertyName, *Value);
            ConfigContent.Append(NewSection);
            return FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
        }
    }
    
    return false;
}

// ============================================================================
// Command Router
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCommand(
    const FString& CommandType, 
    const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPGameplayFrameworkCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        // GameMode
        {TEXT("create_gamemode_blueprint"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameModeBlueprint},
        {TEXT("create_gamemode_cpp_class"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameModeCPPClass},
        {TEXT("set_default_gamemode"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetDefaultGameMode},
        // GameState
        {TEXT("create_gamestate"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameState},
        // PlayerState
        {TEXT("create_playerstate"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreatePlayerState},
        // Controllers
        {TEXT("create_playercontroller"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreatePlayerController},
        {TEXT("create_aicontroller"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateAIController},
        // Pawn / Character
        {TEXT("create_pawn"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreatePawn},
        {TEXT("create_character"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateCharacter},
        {TEXT("set_default_pawn"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetDefaultPawn},
        // HUD / Spectator
        {TEXT("set_hud_class"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetHUDClass},
        {TEXT("set_spectator_pawn"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetSpectatorPawn},
        // Player Start / Spawn
        {TEXT("place_player_start"), &FEpicUnrealMCPGameplayFrameworkCommands::HandlePlacePlayerStart},
        {TEXT("set_spawn_rules"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetSpawnRules},
        {TEXT("set_possess_rules"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetPossessRules},
        // Camera
        {TEXT("set_camera_manager"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetCameraManager},
        {TEXT("setup_camera_component"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetupCameraComponent},
        {TEXT("setup_spring_arm"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetupSpringArm},
        // SaveGame
        {TEXT("create_savegame_class"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateSaveGameClass},
        // GameInstance
        {TEXT("create_gameinstance"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameInstance},
        // Subsystems
        {TEXT("create_gameinstance_subsystem"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameInstanceSubsystem},
        {TEXT("create_world_subsystem"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateWorldSubsystem},
        {TEXT("create_localplayer_subsystem"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateLocalPlayerSubsystem},
        // Gameplay Tags
        {TEXT("setup_gameplay_tags"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleSetupGameplayTags},
        {TEXT("add_gameplay_tag"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleAddGameplayTag},
        {TEXT("create_gameplay_tag_query"), &FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameplayTagQuery},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown gameplay framework command: %s"), *CommandType));
}

// ============================================================================
// GameMode Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameModeBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/GameModes/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, AGameModeBase::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create GameMode blueprint"));
    }

    // Compile the blueprint
    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("AGameModeBase"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameModeCPPClass(
    const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("name"), ClassName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    const FString SourcePath = GetPrimaryProjectModuleSourceDir();
    const FString ApiMacro = GetPrimaryProjectModuleApiMacro();
    FString HeaderFilePath = SourcePath / ClassName + TEXT(".h");
    FString CPPFilePath = SourcePath / ClassName + TEXT(".cpp");

    // Generate header file content
    FString HeaderContent = FString::Printf(TEXT(
        "#pragma once\n\n"
        "#include \"CoreMinimal.h\"\n"
        "#include \"GameFramework/GameModeBase.h\"\n"
        "#include \"%s.generated.h\"\n\n"
        "UCLASS()\n"
        "class %s A%s : public AGameModeBase\n"
        "{\n"
        "    GENERATED_BODY()\n\n"
        "public:\n"
        "    A%s();\n\n"
        "protected:\n"
        "    virtual void BeginPlay() override;\n"
        "    virtual void Tick(float DeltaTime) override;\n"
        "};\n"
    ), *ClassName, *ApiMacro, *ClassName, *ClassName);

    // Generate cpp file content
    FString CPPContent = FString::Printf(TEXT(
        "#include \"%s.h\"\n\n"
        "A%s::A%s()\n"
        "{\n"
        "    PrimaryActorTick.bCanEverTick = true;\n"
        "}\n\n"
        "void A%s::BeginPlay()\n"
        "{\n"
        "    Super::BeginPlay();\n"
        "}\n\n"
        "void A%s::Tick(float DeltaTime)\n"
        "{\n"
        "    Super::Tick(DeltaTime);\n"
        "}\n"
    ), *ClassName, *ClassName, *ClassName, *ClassName, *ClassName);

    bool bHeaderSaved = FFileHelper::SaveStringToFile(HeaderContent, *HeaderFilePath);
    bool bCPPSaved = FFileHelper::SaveStringToFile(CPPContent, *CPPFilePath);

    if (!bHeaderSaved || !bCPPSaved)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create C++ class files"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), ClassName);
    Result->SetStringField(TEXT("header_path"), HeaderFilePath);
    Result->SetStringField(TEXT("cpp_path"), CPPFilePath);
    Result->SetStringField(TEXT("note"), TEXT("Run Generate Project Files and compile to use the new class"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetDefaultGameMode(
    const TSharedPtr<FJsonObject>& Params)
{
    FString GameModePath;
    if (!Params->TryGetStringField(TEXT("gamemode_path"), GameModePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'gamemode_path' parameter"));
    }

    const FString NormalizedGameModeClass = NormalizeGameClassPath(GameModePath);
    if (!SetGameMapsConfigString(TEXT("GlobalDefaultGameMode"), NormalizedGameModeClass))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Failed to write GlobalDefaultGameMode to DefaultEngine.ini. Check project Config directory permissions and try again."));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("default_gamemode"), NormalizedGameModeClass);
    Result->SetStringField(TEXT("config_file"), TEXT("DefaultEngine.ini"));
    return Result;
}

// ============================================================================
// GameState Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameState(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/GameFramework/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, AGameStateBase::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create GameState blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("AGameStateBase"));
    return Result;
}

// ============================================================================
// PlayerState Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreatePlayerState(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/GameFramework/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, APlayerState::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create PlayerState blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("APlayerState"));
    return Result;
}

// ============================================================================
// Controller Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreatePlayerController(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Controllers/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, APlayerController::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create PlayerController blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("APlayerController"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateAIController(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Controllers/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, AAIController::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create AIController blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("AAIController"));
    return Result;
}

// ============================================================================
// Pawn / Character Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreatePawn(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Pawns/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, APawn::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Pawn blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("APawn"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateCharacter(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Characters/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, ACharacter::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Character blueprint"));
    }

    // Configure movement settings if provided
    bool bConfigureMovement = false;
    if (Params->TryGetBoolField(TEXT("add_movement_component"), bConfigureMovement) && bConfigureMovement)
    {
        USimpleConstructionScript* SCS = NewBlueprint->SimpleConstructionScript;
        if (SCS)
        {
            TArray<USCS_Node*> Nodes = SCS->GetAllNodes();
            for (USCS_Node* Node : Nodes)
            {
                if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<UCharacterMovementComponent>())
                {
                    float MaxWalkSpeed = 600.0f;
                    if (Params->TryGetNumberField(TEXT("max_walk_speed"), MaxWalkSpeed))
                    {
                        UCharacterMovementComponent* MoveComp = Cast<UCharacterMovementComponent>(Node->ComponentTemplate);
                        if (MoveComp)
                        {
                            MoveComp->MaxWalkSpeed = MaxWalkSpeed;
                        }
                    }
                    break;
                }
            }
        }
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("ACharacter"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetDefaultPawn(
    const TSharedPtr<FJsonObject>& Params)
{
    FString PawnPath;
    if (!Params->TryGetStringField(TEXT("pawn_path"), PawnPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pawn_path' parameter"));
    }

    PawnPath = NormalizeGameClassPath(PawnPath);

    FString PawnClassPath;
    FString Error;
    UClass* PawnClass = nullptr;
    if (!LoadGameplayClass(PawnPath, APawn::StaticClass(), PawnClassPath, PawnClass, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString GameModeClassPath;
    if (!SetDefaultGameModeClassProperty(
        Params,
        GET_MEMBER_NAME_CHECKED(AGameModeBase, DefaultPawnClass),
        PawnClass,
        APawn::StaticClass(),
        true,
        GameModeClassPath,
        Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("default_pawn"), PawnClassPath);
    Result->SetStringField(TEXT("gamemode"), GameModeClassPath);
    Result->SetStringField(TEXT("property"), TEXT("DefaultPawnClass"));
    return Result;
}

// ============================================================================
// HUD / Spectator Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetHUDClass(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/HUD/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, AHUD::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create HUD blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("AHUD"));

    FString GameModeClassPath;
    FString Error;
    if (SetDefaultGameModeClassProperty(
        Params,
        GET_MEMBER_NAME_CHECKED(AGameModeBase, HUDClass),
        NewBlueprint->GeneratedClass,
        AHUD::StaticClass(),
        false,
        GameModeClassPath,
        Error))
    {
        Result->SetBoolField(TEXT("applied_to_gamemode"), true);
        Result->SetStringField(TEXT("gamemode"), GameModeClassPath);
        Result->SetStringField(TEXT("property"), TEXT("HUDClass"));
    }
    else if (!Error.IsEmpty())
    {
        Result->SetBoolField(TEXT("applied_to_gamemode"), false);
        Result->SetStringField(TEXT("warning"), Error);
    }
    else
    {
        Result->SetBoolField(TEXT("applied_to_gamemode"), false);
        Result->SetStringField(TEXT("note"), TEXT("No default GameMode is configured; pass 'gamemode_path' or run set_default_gamemode to apply HUDClass."));
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetSpectatorPawn(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Pawns/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, ASpectatorPawn::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Spectator Pawn blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("ASpectatorPawn"));

    FString GameModeClassPath;
    FString Error;
    if (SetDefaultGameModeClassProperty(
        Params,
        GET_MEMBER_NAME_CHECKED(AGameModeBase, SpectatorClass),
        NewBlueprint->GeneratedClass,
        ASpectatorPawn::StaticClass(),
        false,
        GameModeClassPath,
        Error))
    {
        Result->SetBoolField(TEXT("applied_to_gamemode"), true);
        Result->SetStringField(TEXT("gamemode"), GameModeClassPath);
        Result->SetStringField(TEXT("property"), TEXT("SpectatorClass"));
    }
    else if (!Error.IsEmpty())
    {
        Result->SetBoolField(TEXT("applied_to_gamemode"), false);
        Result->SetStringField(TEXT("warning"), Error);
    }
    else
    {
        Result->SetBoolField(TEXT("applied_to_gamemode"), false);
        Result->SetStringField(TEXT("note"), TEXT("No default GameMode is configured; pass 'gamemode_path' or run set_default_gamemode to apply SpectatorClass."));
    }
    return Result;
}

// ============================================================================
// Player Start / Spawn Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandlePlacePlayerStart(
    const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active world found"));
    }

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FString Tag;

    const TSharedPtr<FJsonObject>* LocationObj = nullptr;
    if (Params->TryGetObjectField(TEXT("location"), LocationObj))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*LocationObj, TEXT("value"));
    }

    const TSharedPtr<FJsonObject>* RotationObj = nullptr;
    if (Params->TryGetObjectField(TEXT("rotation"), RotationObj))
    {
        Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(*RotationObj, TEXT("value"));
    }

    Params->TryGetStringField(TEXT("tag"), Tag);

    // Spawn PlayerStart
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    APlayerStart* PlayerStart = World->SpawnActor<APlayerStart>(
        APlayerStart::StaticClass(), 
        Location, 
        Rotation, 
        SpawnParams);

    if (!PlayerStart)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn PlayerStart"));
    }

    PlayerStart->Modify();
    if (!Tag.IsEmpty())
    {
        PlayerStart->Tags.Add(FName(*Tag));
    }

    // Set a custom name if provided
    FString ActorName;
    if (Params->TryGetStringField(TEXT("name"), ActorName))
    {
        FName NewName = FName(*ActorName);
        if (!PlayerStart->Rename(*NewName.ToString(), nullptr, REN_Test))
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not rename PlayerStart to %s, using auto-generated name"), *ActorName);
        }
        else
        {
            PlayerStart->Rename(*NewName.ToString());
        }
    }

    if (ULevel* Level = PlayerStart->GetLevel())
    {
        Level->Modify();
        if (UPackage* Package = Level->GetOutermost())
        {
            Package->SetDirtyFlag(true);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), PlayerStart->GetName());
    Result->SetStringField(TEXT("location"), Location.ToString());
    Result->SetStringField(TEXT("rotation"), Rotation.ToString());
    if (!Tag.IsEmpty())
    {
        Result->SetStringField(TEXT("tag"), Tag);
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetSpawnRules(
    const TSharedPtr<FJsonObject>& Params)
{
    FString SpawnMethod;
    if (!Params->TryGetStringField(TEXT("spawn_method"), SpawnMethod))
    {
        SpawnMethod = TEXT("PlayerStart");
    }

    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    
    if (SpawnMethod.Equals(TEXT("PlayerStart"), ESearchCase::IgnoreCase))
    {
        Settings->SetStringField(TEXT("method"), TEXT("PlayerStart"));
        Settings->SetStringField(TEXT("description"), TEXT("Spawn at nearest available PlayerStart actor"));
    }
    else if (SpawnMethod.Equals(TEXT("Transform"), ESearchCase::IgnoreCase))
    {
        FVector SpawnLocation = FVector::ZeroVector;
        const TSharedPtr<FJsonObject>* LocationObj = nullptr;
        if (Params->TryGetObjectField(TEXT("location"), LocationObj))
        {
            SpawnLocation = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*LocationObj, TEXT("value"));
        }
        Settings->SetStringField(TEXT("method"), TEXT("Transform"));
        Settings->SetStringField(TEXT("location"), SpawnLocation.ToString());
        Settings->SetStringField(TEXT("description"), TEXT("Spawn at specific transform location"));
    }
    else if (SpawnMethod.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
    {
        Settings->SetStringField(TEXT("method"), TEXT("Custom"));
        Settings->SetStringField(TEXT("description"), TEXT("Use custom spawn logic in GameMode"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("spawn_settings"), Settings);
    Result->SetStringField(TEXT("note"), TEXT("Spawn rules are typically implemented in GameMode::ChoosePlayerStart or overridden in blueprint"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetPossessRules(
    const TSharedPtr<FJsonObject>& Params)
{
    FString PossessMethod;
    if (!Params->TryGetStringField(TEXT("possess_method"), PossessMethod))
    {
        PossessMethod = TEXT("Auto");
    }

    bool bAutoPossessPlayer = true;
    Params->TryGetBoolField(TEXT("auto_possess"), bAutoPossessPlayer);

    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    Settings->SetStringField(TEXT("method"), PossessMethod);
    Settings->SetBoolField(TEXT("auto_possess"), bAutoPossessPlayer);

    if (PossessMethod.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
    {
        Settings->SetStringField(TEXT("description"), TEXT("Auto possess on spawn"));
    }
    else if (PossessMethod.Equals(TEXT("Delayed"), ESearchCase::IgnoreCase))
    {
        float DelaySeconds = 0.0f;
        Params->TryGetNumberField(TEXT("delay_seconds"), DelaySeconds);
        Settings->SetNumberField(TEXT("delay_seconds"), DelaySeconds);
        Settings->SetStringField(TEXT("description"), TEXT("Delayed possession after spawn"));
    }
    else if (PossessMethod.Equals(TEXT("Manual"), ESearchCase::IgnoreCase))
    {
        Settings->SetStringField(TEXT("description"), TEXT("Manual possession via PlayerController"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("possess_settings"), Settings);
    Result->SetStringField(TEXT("note"), TEXT("Possession rules are set on Pawn's AutoPossessPlayer property or via GameMode"));
    return Result;
}

// ============================================================================
// Camera Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetCameraManager(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Camera/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, APlayerCameraManager::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create CameraManager blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("APlayerCameraManager"));

    FString Error;
    FString PlayerControllerPath;
    if (Params->TryGetStringField(TEXT("playercontroller_path"), PlayerControllerPath))
    {
        FString PlayerControllerClassPath;
        UClass* PlayerControllerClass = nullptr;
        if (!LoadGameplayClass(PlayerControllerPath, APlayerController::StaticClass(), PlayerControllerClassPath, PlayerControllerClass, Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }

        if (!SetBlueprintClassDefaultClassProperty(
            PlayerControllerClass,
            GET_MEMBER_NAME_CHECKED(APlayerController, PlayerCameraManagerClass),
            NewBlueprint->GeneratedClass,
            APlayerCameraManager::StaticClass(),
            TEXT("PlayerController"),
            Error))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
        }

        Result->SetBoolField(TEXT("applied_to_playercontroller"), true);
        Result->SetStringField(TEXT("playercontroller"), PlayerControllerClassPath);
        Result->SetStringField(TEXT("property"), TEXT("PlayerCameraManagerClass"));
    }
    else
    {
        FString GameModeClassPath;
        UClass* GameModeClass = nullptr;
        if (ResolveTargetGameModeClass(Params, false, GameModeClassPath, GameModeClass, Error))
        {
            AGameModeBase* GameModeCDO = Cast<AGameModeBase>(GameModeClass->GetDefaultObject());
            UClass* PlayerControllerClass = GameModeCDO ? GameModeCDO->PlayerControllerClass : nullptr;
            if (PlayerControllerClass && UBlueprint::GetBlueprintFromClass(PlayerControllerClass) &&
                SetBlueprintClassDefaultClassProperty(
                    PlayerControllerClass,
                    GET_MEMBER_NAME_CHECKED(APlayerController, PlayerCameraManagerClass),
                    NewBlueprint->GeneratedClass,
                    APlayerCameraManager::StaticClass(),
                    TEXT("PlayerController"),
                    Error))
            {
                Result->SetBoolField(TEXT("applied_to_playercontroller"), true);
                Result->SetStringField(TEXT("playercontroller"), PlayerControllerClass->GetPathName());
                Result->SetStringField(TEXT("gamemode"), GameModeClassPath);
                Result->SetStringField(TEXT("property"), TEXT("PlayerCameraManagerClass"));
            }
            else
            {
                Result->SetBoolField(TEXT("applied_to_playercontroller"), false);
                Result->SetStringField(
                    TEXT("note"),
                    TEXT("CameraManager blueprint was created. Pass 'playercontroller_path' to apply PlayerCameraManagerClass to a Blueprint PlayerController."));
            }
        }
        else
        {
            Result->SetBoolField(TEXT("applied_to_playercontroller"), false);
            Result->SetStringField(
                TEXT("note"),
                TEXT("CameraManager blueprint was created. Configure a default GameMode with a Blueprint PlayerController or pass 'playercontroller_path' to apply it."));
        }
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetupCameraComponent(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no construction script"));
    }

    UCameraComponent* CameraComp = nullptr;
    TArray<USCS_Node*> Nodes = SCS->GetAllNodes();
    for (USCS_Node* Node : Nodes)
    {
        if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<UCameraComponent>())
        {
            CameraComp = Cast<UCameraComponent>(Node->ComponentTemplate);
            break;
        }
    }

    if (!CameraComp)
    {
        USCS_Node* NewNode = SCS->CreateNode(UCameraComponent::StaticClass(), TEXT("CameraComponent"));
        if (NewNode)
        {
            AddNodeToSCS(SCS, NewNode);
            CameraComp = Cast<UCameraComponent>(NewNode->ComponentTemplate);
        }
    }

    if (!CameraComp)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create CameraComponent"));
    }

    float FieldOfView = 90.0f;
    if (Params->TryGetNumberField(TEXT("field_of_view"), FieldOfView))
    {
        CameraComp->FieldOfView = FieldOfView;
    }

    bool bUsePawnControlRotation = true;
    if (Params->TryGetBoolField(TEXT("use_pawn_control_rotation"), bUsePawnControlRotation))
    {
        CameraComp->bUsePawnControlRotation = bUsePawnControlRotation;
    }

    FVector CameraOffset = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* OffsetObj = nullptr;
    if (Params->TryGetObjectField(TEXT("offset"), OffsetObj))
    {
        CameraOffset = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*OffsetObj, TEXT("value"));
        CameraComp->SetRelativeLocation(CameraOffset);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint"), BlueprintName);
    Result->SetStringField(TEXT("component"), TEXT("CameraComponent"));
    Result->SetNumberField(TEXT("field_of_view"), CameraComp->FieldOfView);
    Result->SetBoolField(TEXT("use_pawn_control_rotation"), CameraComp->bUsePawnControlRotation);
    Result->SetStringField(TEXT("offset"), CameraComp->GetRelativeLocation().ToString());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetupSpringArm(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no construction script"));
    }

    USpringArmComponent* SpringArm = nullptr;
    TArray<USCS_Node*> Nodes = SCS->GetAllNodes();
    for (USCS_Node* Node : Nodes)
    {
        if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USpringArmComponent>())
        {
            SpringArm = Cast<USpringArmComponent>(Node->ComponentTemplate);
            break;
        }
    }

    if (!SpringArm)
    {
        USCS_Node* NewNode = SCS->CreateNode(USpringArmComponent::StaticClass(), TEXT("SpringArm"));
        if (NewNode)
        {
            AddNodeToSCS(SCS, NewNode);
            SpringArm = Cast<USpringArmComponent>(NewNode->ComponentTemplate);
        }
    }

    if (!SpringArm)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SpringArmComponent"));
    }

    float TargetArmLength = 300.0f;
    if (Params->TryGetNumberField(TEXT("target_arm_length"), TargetArmLength))
    {
        SpringArm->TargetArmLength = TargetArmLength;
    }

    FVector SocketOffset = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* OffsetObj = nullptr;
    if (Params->TryGetObjectField(TEXT("socket_offset"), OffsetObj))
    {
        SocketOffset = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*OffsetObj, TEXT("value"));
        SpringArm->SocketOffset = SocketOffset;
    }

    FVector TargetOffset = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* TargetObj = nullptr;
    if (Params->TryGetObjectField(TEXT("target_offset"), TargetObj))
    {
        TargetOffset = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*TargetObj, TEXT("value"));
        SpringArm->TargetOffset = TargetOffset;
    }

    bool bUsePawnControlRotation = true;
    if (Params->TryGetBoolField(TEXT("use_pawn_control_rotation"), bUsePawnControlRotation))
    {
        SpringArm->bUsePawnControlRotation = bUsePawnControlRotation;
    }

    bool bInheritPitch = true;
    if (Params->TryGetBoolField(TEXT("inherit_pitch"), bInheritPitch))
    {
        SpringArm->bInheritPitch = bInheritPitch;
    }

    bool bInheritYaw = true;
    if (Params->TryGetBoolField(TEXT("inherit_yaw"), bInheritYaw))
    {
        SpringArm->bInheritYaw = bInheritYaw;
    }

    bool bInheritRoll = false;
    if (Params->TryGetBoolField(TEXT("inherit_roll"), bInheritRoll))
    {
        SpringArm->bInheritRoll = bInheritRoll;
    }

    bool bDoCollisionTest = true;
    if (Params->TryGetBoolField(TEXT("do_collision_test"), bDoCollisionTest))
    {
        SpringArm->bDoCollisionTest = bDoCollisionTest;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint"), BlueprintName);
    Result->SetStringField(TEXT("component"), TEXT("SpringArm"));
    Result->SetNumberField(TEXT("target_arm_length"), SpringArm->TargetArmLength);
    Result->SetStringField(TEXT("socket_offset"), SpringArm->SocketOffset.ToString());
    Result->SetStringField(TEXT("target_offset"), SpringArm->TargetOffset.ToString());
    Result->SetBoolField(TEXT("use_pawn_control_rotation"), SpringArm->bUsePawnControlRotation);
    Result->SetBoolField(TEXT("inherit_pitch"), SpringArm->bInheritPitch);
    Result->SetBoolField(TEXT("inherit_yaw"), SpringArm->bInheritYaw);
    Result->SetBoolField(TEXT("inherit_roll"), SpringArm->bInheritRoll);
    Result->SetBoolField(TEXT("do_collision_test"), SpringArm->bDoCollisionTest);
    return Result;
}

// ============================================================================
// SaveGame Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateSaveGameClass(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/SaveGame/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, USaveGame::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SaveGame blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("USaveGame"));
    Result->SetStringField(TEXT("note"), TEXT("Add variables to this blueprint for data you want to save. Use CreateSaveGameObject/SaveGameToSlot in gameplay"));
    return Result;
}

// ============================================================================
// GameInstance Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameInstance(
    const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/GameFramework/");
    
    UBlueprint* NewBlueprint = CreateBlueprintWithParent(BlueprintName, PackagePath, UGameInstance::StaticClass());
    if (!NewBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create GameInstance blueprint"));
    }

    FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), BlueprintName);
    Result->SetStringField(TEXT("path"), PackagePath + BlueprintName);
    Result->SetStringField(TEXT("parent_class"), TEXT("UGameInstance"));
    Result->SetStringField(TEXT("note"), TEXT("Set this in Project Settings > Maps & Modes > Game Instance Class"));
    return Result;
}

// ============================================================================
// Subsystem Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameInstanceSubsystem(
    const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("name"), ClassName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    const FString SourcePath = GetPrimaryProjectModuleSourceDir();
    const FString ApiMacro = GetPrimaryProjectModuleApiMacro();
    FString HeaderFilePath = SourcePath / ClassName + TEXT(".h");
    FString CPPFilePath = SourcePath / ClassName + TEXT(".cpp");

    FString HeaderContent = FString::Printf(TEXT(
        "#pragma once\n\n"
        "#include \"CoreMinimal.h\"\n"
        "#include \"Subsystems/GameInstanceSubsystem.h\"\n"
        "#include \"%s.generated.h\"\n\n"
        "UCLASS()\n"
        "class %s U%s : public UGameInstanceSubsystem\n"
        "{\n"
        "    GENERATED_BODY()\n\n"
        "public:\n"
        "    virtual void Initialize(FSubsystemCollectionBase& Collection) override;\n"
        "    virtual void Deinitialize() override;\n"
        "    UFUNCTION(BlueprintCallable, Category = \"%s\")\n"
        "    void CustomFunction();\n"
        "};\n"
    ), *ClassName, *ApiMacro, *ClassName, *ClassName);

    FString CPPContent = FString::Printf(TEXT(
        "#include \"%s.h\"\n\n"
        "void U%s::Initialize(FSubsystemCollectionBase& Collection)\n"
        "{\n"
        "    Super::Initialize(Collection);\n"
        "}\n\n"
        "void U%s::Deinitialize()\n"
        "{\n"
        "    Super::Deinitialize();\n"
        "}\n\n"
        "void U%s::CustomFunction()\n"
        "{\n"
        "}\n"
    ), *ClassName, *ClassName, *ClassName, *ClassName);

    bool bHeaderSaved = FFileHelper::SaveStringToFile(HeaderContent, *HeaderFilePath);
    bool bCPPSaved = FFileHelper::SaveStringToFile(CPPContent, *CPPFilePath);

    if (!bHeaderSaved || !bCPPSaved)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create subsystem files"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), ClassName);
    Result->SetStringField(TEXT("type"), TEXT("GameInstanceSubsystem"));
    Result->SetStringField(TEXT("header_path"), HeaderFilePath);
    Result->SetStringField(TEXT("cpp_path"), CPPFilePath);
    Result->SetStringField(TEXT("usage"), FString::Printf(TEXT("UGameInstance::GetSubsystem<U%s>()"), *ClassName));
    Result->SetStringField(TEXT("note"), TEXT("Run Generate Project Files and compile to use the new subsystem"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateWorldSubsystem(
    const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("name"), ClassName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    const FString SourcePath = GetPrimaryProjectModuleSourceDir();
    const FString ApiMacro = GetPrimaryProjectModuleApiMacro();
    FString HeaderFilePath = SourcePath / ClassName + TEXT(".h");
    FString CPPFilePath = SourcePath / ClassName + TEXT(".cpp");

    FString HeaderContent = FString::Printf(TEXT(
        "#pragma once\n\n"
        "#include \"CoreMinimal.h\"\n"
        "#include \"Subsystems/WorldSubsystem.h\"\n"
        "#include \"%s.generated.h\"\n\n"
        "UCLASS()\n"
        "class %s U%s : public UWorldSubsystem\n"
        "{\n"
        "    GENERATED_BODY()\n\n"
        "public:\n"
        "    virtual void Initialize(FSubsystemCollectionBase& Collection) override;\n"
        "    virtual void Deinitialize() override;\n"
        "    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;\n"
        "    UFUNCTION(BlueprintCallable, Category = \"%s\")\n"
        "    void CustomFunction();\n"
        "};\n"
    ), *ClassName, *ApiMacro, *ClassName, *ClassName);

    FString CPPContent = FString::Printf(TEXT(
        "#include \"%s.h\"\n\n"
        "void U%s::Initialize(FSubsystemCollectionBase& Collection)\n"
        "{\n"
        "    Super::Initialize(Collection);\n"
        "}\n\n"
        "void U%s::Deinitialize()\n"
        "{\n"
        "    Super::Deinitialize();\n"
        "}\n\n"
        "bool U%s::ShouldCreateSubsystem(UObject* Outer) const\n"
        "{\n"
        "    return true;\n"
        "}\n\n"
        "void U%s::CustomFunction()\n"
        "{\n"
        "}\n"
    ), *ClassName, *ClassName, *ClassName, *ClassName, *ClassName, *ClassName);

    bool bHeaderSaved = FFileHelper::SaveStringToFile(HeaderContent, *HeaderFilePath);
    bool bCPPSaved = FFileHelper::SaveStringToFile(CPPContent, *CPPFilePath);

    if (!bHeaderSaved || !bCPPSaved)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create subsystem files"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), ClassName);
    Result->SetStringField(TEXT("type"), TEXT("WorldSubsystem"));
    Result->SetStringField(TEXT("header_path"), HeaderFilePath);
    Result->SetStringField(TEXT("cpp_path"), CPPFilePath);
    Result->SetStringField(TEXT("usage"), FString::Printf(TEXT("UWorld::GetSubsystem<U%s>()"), *ClassName));
    Result->SetStringField(TEXT("note"), TEXT("Run Generate Project Files and compile to use the new subsystem"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateLocalPlayerSubsystem(
    const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("name"), ClassName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    const FString SourcePath = GetPrimaryProjectModuleSourceDir();
    const FString ApiMacro = GetPrimaryProjectModuleApiMacro();
    FString HeaderFilePath = SourcePath / ClassName + TEXT(".h");
    FString CPPFilePath = SourcePath / ClassName + TEXT(".cpp");

    FString HeaderContent = FString::Printf(TEXT(
        "#pragma once\n\n"
        "#include \"CoreMinimal.h\"\n"
        "#include \"Subsystems/LocalPlayerSubsystem.h\"\n"
        "#include \"%s.generated.h\"\n\n"
        "UCLASS()\n"
        "class %s U%s : public ULocalPlayerSubsystem\n"
        "{\n"
        "    GENERATED_BODY()\n\n"
        "public:\n"
        "    virtual void Initialize(FSubsystemCollectionBase& Collection) override;\n"
        "    virtual void Deinitialize() override;\n"
        "    UFUNCTION(BlueprintCallable, Category = \"%s\")\n"
        "    void CustomFunction();\n"
        "};\n"
    ), *ClassName, *ApiMacro, *ClassName, *ClassName);

    FString CPPContent = FString::Printf(TEXT(
        "#include \"%s.h\"\n\n"
        "void U%s::Initialize(FSubsystemCollectionBase& Collection)\n"
        "{\n"
        "    Super::Initialize(Collection);\n"
        "}\n\n"
        "void U%s::Deinitialize()\n"
        "{\n"
        "    Super::Deinitialize();\n"
        "}\n\n"
        "void U%s::CustomFunction()\n"
        "{\n"
        "}\n"
    ), *ClassName, *ClassName, *ClassName, *ClassName);

    bool bHeaderSaved = FFileHelper::SaveStringToFile(HeaderContent, *HeaderFilePath);
    bool bCPPSaved = FFileHelper::SaveStringToFile(CPPContent, *CPPFilePath);

    if (!bHeaderSaved || !bCPPSaved)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create subsystem files"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("name"), ClassName);
    Result->SetStringField(TEXT("type"), TEXT("LocalPlayerSubsystem"));
    Result->SetStringField(TEXT("header_path"), HeaderFilePath);
    Result->SetStringField(TEXT("cpp_path"), CPPFilePath);
    Result->SetStringField(TEXT("usage"), FString::Printf(TEXT("ULocalPlayer::GetSubsystem<U%s>()"), *ClassName));
    Result->SetStringField(TEXT("note"), TEXT("Run Generate Project Files and compile to use the new subsystem"));
    return Result;
}

// ============================================================================
// Gameplay Tags Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleSetupGameplayTags(
    const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> TagsToRegister;
    const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
    if (Params->TryGetArrayField(TEXT("tags"), TagsArray))
    {
        for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
        {
            TagsToRegister.Add(TagValue->AsString());
        }
    }

    int32 RegisteredCount = 0;
    int32 AlreadyExistingCount = 0;
    for (const FString& TagString : TagsToRegister)
    {
        if (!TagString.IsEmpty())
        {
            bool bAlreadyExisted = false;
            if (!AddGameplayTagToSettings(TagString, TEXT(""), bAlreadyExisted))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Failed to add gameplay tag '%s' to DefaultGameplayTags.ini"), *TagString));
            }

            if (bAlreadyExisted)
            {
                AlreadyExistingCount++;
            }
            else
            {
                RegisteredCount++;
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("registered_count"), RegisteredCount);
    Result->SetNumberField(TEXT("already_existing_count"), AlreadyExistingCount);
    
    TArray<TSharedPtr<FJsonValue>> TagsJson;
    for (const FString& Tag : TagsToRegister)
    {
        TagsJson.Add(MakeShared<FJsonValueString>(Tag));
    }
    Result->SetArrayField(TEXT("tags"), TagsJson);
    
    Result->SetStringField(TEXT("ini_path"), GetDefaultGameplayTagsConfigPath());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleAddGameplayTag(
    const TSharedPtr<FJsonObject>& Params)
{
    FString TagName;
    if (!Params->TryGetStringField(TEXT("tag"), TagName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tag' parameter"));
    }

    FString Comment;
    Params->TryGetStringField(TEXT("comment"), Comment);

    bool bAlreadyExisted = false;
    if (!AddGameplayTagToSettings(TagName, Comment, bAlreadyExisted))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to add gameplay tag '%s' to DefaultGameplayTags.ini"), *TagName));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("tag"), TagName);
    Result->SetBoolField(TEXT("already_existed"), bAlreadyExisted);
    if (!Comment.IsEmpty())
    {
        Result->SetStringField(TEXT("comment"), Comment);
    }
    Result->SetStringField(TEXT("ini_path"), GetDefaultGameplayTagsConfigPath());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPGameplayFrameworkCommands::HandleCreateGameplayTagQuery(
    const TSharedPtr<FJsonObject>& Params)
{
    FString QueryName;
    if (!Params->TryGetStringField(TEXT("name"), QueryName))
    {
        QueryName = TEXT("TagQuery");
    }

    FString QueryType;
    if (!Params->TryGetStringField(TEXT("query_type"), QueryType))
    {
        QueryType = TEXT("Any");
    }

    TArray<FString> Tags;
    const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
    if (Params->TryGetArrayField(TEXT("tags"), TagsArray))
    {
        for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
        {
            Tags.Add(TagValue->AsString());
        }
    }

    TSharedPtr<FJsonObject> QueryDescription = MakeShared<FJsonObject>();
    QueryDescription->SetStringField(TEXT("name"), QueryName);
    QueryDescription->SetStringField(TEXT("type"), QueryType);
    
    TArray<TSharedPtr<FJsonValue>> TagsJson;
    for (const FString& Tag : Tags)
    {
        TagsJson.Add(MakeShared<FJsonValueString>(Tag));
    }
    QueryDescription->SetArrayField(TEXT("tags"), TagsJson);

    FString Description;
    if (QueryType.Equals(TEXT("Any"), ESearchCase::IgnoreCase))
    {
        Description = TEXT("Query matches if ANY of the specified tags are present");
    }
    else if (QueryType.Equals(TEXT("All"), ESearchCase::IgnoreCase))
    {
        Description = TEXT("Query matches if ALL of the specified tags are present");
    }
    else if (QueryType.Equals(TEXT("None"), ESearchCase::IgnoreCase))
    {
        Description = TEXT("Query matches if NONE of the specified tags are present");
    }
    else
    {
        Description = TEXT("Custom tag query");
    }
    QueryDescription->SetStringField(TEXT("description"), Description);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("query"), QueryDescription);
    Result->SetStringField(TEXT("usage_c"), TEXT("FGameplayTagQuery::BuildQuery(QueryType, Tags...)"));
    Result->SetStringField(TEXT("usage_bp"), TEXT("Use MakeGameplayTagQuery node in Blueprints"));
    Result->SetStringField(TEXT("note"), TEXT("Tag queries can be used in C++ with FGameplayTagQuery or in Blueprint with the GameplayTagQuery type"));
    return Result;
}
