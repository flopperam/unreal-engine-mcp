#include "Commands/EpicUnrealMCPVroidCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

// UE5.7 Core Headers
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Factories/Factory.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"

// Engine / Editor
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "EngineUtils.h"

// Plugin detection
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

FEpicUnrealMCPVroidCommands::FEpicUnrealMCPVroidCommands()
{
}

// ---------------------------------------------------------------------------
// Command Router
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPVroidCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPVroidCommands::*)(const TSharedPtr<FJsonObject>&);
	static const TMap<FString, Handler> Dispatch = {
		{TEXT("vroid_check_plugin"), &FEpicUnrealMCPVroidCommands::HandleVroidCheckPlugin},
		{TEXT("vroid_import_vrm"), &FEpicUnrealMCPVroidCommands::HandleVroidImportVrm},
		{TEXT("vroid_spawn_avatar"), &FEpicUnrealMCPVroidCommands::HandleVroidSpawnAvatar},
		{TEXT("vroid_validate_avatar_asset"), &FEpicUnrealMCPVroidCommands::HandleVroidValidateAvatarAsset},
	};

	const Handler* H = Dispatch.Find(CommandType);
	if (H)
	{
		return (this->*(*H))(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown VRM/avatar command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Plugin Detection
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPVroidCommands::HandleVroidCheckPlugin(
	const TSharedPtr<FJsonObject>& Params)
{
	const bool bAvailable = IsVrmPluginAvailable();
	const FString PluginName = GetVrmPluginName();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("available"), bAvailable);
	ResultObj->SetStringField(TEXT("plugin_name"), PluginName);

	if (!bAvailable)
	{
		ResultObj->SetStringField(TEXT("action"), TEXT("Install VRM4U from https://github.com/ruyo/VRM4U or Marketplace"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("data"), ResultObj);
	return Response;
}

bool FEpicUnrealMCPVroidCommands::IsVrmPluginAvailable()
{
	// Check common VRM plugin module names
	const TArray<FString> CandidateModules = {
		TEXT("VRM4U"),
		TEXT("VRM4UImporter"),
		TEXT("VRMImporter"),
		TEXT("VrmAssetList"),
	};

	for (const FString& ModuleName : CandidateModules)
	{
		if (FModuleManager::Get().IsModuleLoaded(*ModuleName))
		{
			return true;
		}
	}

	// Also check plugin descriptor
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VRM4U")))
	{
		return Plugin->IsEnabled();
	}

	return false;
}

FString FEpicUnrealMCPVroidCommands::GetVrmPluginName()
{
	const TArray<FString> CandidateModules = {
		TEXT("VRM4U"),
		TEXT("VRM4UImporter"),
		TEXT("VRMImporter"),
		TEXT("VrmAssetList"),
	};

	for (const FString& ModuleName : CandidateModules)
	{
		if (FModuleManager::Get().IsModuleLoaded(*ModuleName))
		{
			return ModuleName;
		}
	}

	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VRM4U")))
	{
		return Plugin->GetName();
	}

	return TEXT("none");
}

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPVroidCommands::HandleVroidImportVrm(
	const TSharedPtr<FJsonObject>& Params)
{
	// Validate plugin is available
	if (!IsVrmPluginAvailable())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("VRM importer plugin (VRM4U) is not installed or enabled. "
			     "Install from https://github.com/ruyo/VRM4U"));
	}

	// Validate required parameters
	FString SourcePath;
	if (!Params->TryGetStringField(TEXT("source_path"), SourcePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("source_path is required"));
	}

	FString DestinationPath;
	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("destination_path is required"));
	}

	// Optional asset name (defaults to filename without extension)
	FString AssetName;
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
	{
		AssetName = FPaths::GetBaseFilename(SourcePath);
	}

	// Validate source file exists
	if (!FPaths::FileExists(SourcePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("VRM file not found: %s"), *SourcePath));
	}

	// Validate destination path
	if (!DestinationPath.StartsWith(TEXT("/Game/")))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("destination_path must start with /Game/"));
	}

	// Create and run import task
	UAssetImportTask* Task = CreateVrmImportTask(SourcePath, DestinationPath, AssetName);
	if (!Task)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create VRM import task"));
	}

	FString Error;
	TArray<UObject*> ImportedObjects = ProcessVrmImportTask(Task, Error);
	if (ImportedObjects.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("VRM import failed: %s"), *Error));
	}

	// Find primary skeletal mesh
	USkeletalMesh* SkeletalMesh = FindSkeletalMeshInImports(ImportedObjects);
	if (!SkeletalMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Import succeeded but no SkeletalMesh was found. "
			     "The VRM file may be invalid or the importer plugin may need configuration."));
	}

	// Build result with asset paths
	TArray<TSharedPtr<FJsonValue>> ImportedAssets;
	for (UObject* Obj : ImportedObjects)
	{
		if (Obj)
		{
			TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
			AssetObj->SetStringField(TEXT("name"), Obj->GetName());
			AssetObj->SetStringField(TEXT("class"), Obj->GetClass()->GetName());
			FString PackagePath = Obj->GetPathName();
			AssetObj->SetStringField(TEXT("path"), PackagePath);
			ImportedAssets.Add(MakeShared<FJsonValueObject>(AssetObj));
		}
	}

	FString SkeletalMeshPath = SkeletalMesh->GetPathName();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("skeletal_mesh"), SkeletalMeshPath);
	ResultObj->SetStringField(TEXT("asset_name"), AssetName);
	ResultObj->SetArrayField(TEXT("imported_assets"), ImportedAssets);
	ResultObj->SetNumberField(TEXT("asset_count"), ImportedAssets.Num());

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("data"), ResultObj);
	return Response;
}

UAssetImportTask* FEpicUnrealMCPVroidCommands::CreateVrmImportTask(
	const FString& SourcePath,
	const FString& DestinationPath,
	const FString& AssetName)
{
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = SourcePath;
	Task->DestinationPath = DestinationPath;

	if (!AssetName.IsEmpty())
	{
		Task->DestinationName = AssetName;
	}

	// Let the engine auto-detect the factory from the .vrm extension.
	// VRM4U registers its factory when the plugin is enabled.
	Task->Factory = nullptr;
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = true;

	return Task;
}

TArray<UObject*> FEpicUnrealMCPVroidCommands::ProcessVrmImportTask(UAssetImportTask* Task, FString& OutError)
{
	if (!Task)
	{
		OutError = TEXT("Null import task");
		return TArray<UObject*>();
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Run import on GameThread (we are already on GameThread from bridge)
	AssetTools.ImportAssetTasks(TArray<UAssetImportTask*>{ Task });

	TArray<UObject*> Result;
	for (UObject* ImportedObject : Task->GetObjects())
	{
		if (ImportedObject)
		{
			Result.Add(ImportedObject);
		}
	}

	if (Result.IsEmpty() && Task->ImportedObjectPaths.Num() > 0)
	{
		// Fallback: try to load from result path
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Task->ImportedObjectPaths[0]));
		if (AssetData.IsValid())
		{
			UObject* LoadedObject = AssetData.GetAsset();
			if (LoadedObject)
			{
				Result.Add(LoadedObject);
			}
		}
	}

	if (Result.IsEmpty())
	{
		OutError = TEXT("No objects were imported. The VRM4U importer may not support this file, or the plugin may need configuration.");
	}

	return Result;
}

USkeletalMesh* FEpicUnrealMCPVroidCommands::FindSkeletalMeshInImports(const TArray<UObject*>& ImportedObjects)
{
	return FindImportedAsset<USkeletalMesh>(ImportedObjects);
}

template<typename T>
T* FEpicUnrealMCPVroidCommands::FindImportedAsset(const TArray<UObject*>& ImportedObjects)
{
	for (UObject* Obj : ImportedObjects)
	{
		if (Obj && Obj->IsA<T>())
		{
			return Cast<T>(Obj);
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Spawn
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPVroidCommands::HandleVroidSpawnAvatar(
	const TSharedPtr<FJsonObject>& Params)
{
	FString SkeletalMeshPath;
	if (!Params->TryGetStringField(TEXT("skeletal_mesh_path"), SkeletalMeshPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("skeletal_mesh_path is required"));
	}

	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
	{
		ActorName = TEXT("VrmAvatar");
	}

	// Optional transform
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	FString ParamError;

	FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError);
	FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), Rotation, ParamError);
	FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("scale"), Scale, ParamError);

	// Load the skeletal mesh
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
	if (!SkeletalMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load SkeletalMesh at path: %s"), *SkeletalMeshPath));
	}

	// Get editor world
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
	}

	// Spawn actor with skeletal mesh component
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*ActorName);
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Rotation, Location, Scale), SpawnParams);
	if (!Actor)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn avatar actor"));
	}

	// Add skeletal mesh component
	USkeletalMeshComponent* MeshComp = NewObject<USkeletalMeshComponent>(Actor, USkeletalMeshComponent::StaticClass());
	if (MeshComp)
	{
		MeshComp->SetSkeletalMesh(SkeletalMesh);
		MeshComp->RegisterComponent();
		Actor->SetRootComponent(MeshComp);
		MeshComp->SetWorldTransform(FTransform(Rotation, Location, Scale));
	}

	// Mark package dirty
	Actor->MarkPackageDirty();

	// Focus viewport if requested
	bool bFocusViewport = true;
	Params->TryGetBoolField(TEXT("focus_viewport"), bFocusViewport);
	if (bFocusViewport)
	{
		GEditor->MoveViewportCamerasToActor({ Actor }, true);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("actor_name"), Actor->GetName());
	ResultObj->SetStringField(TEXT("actor_path"), Actor->GetPathName());
	ResultObj->SetStringField(TEXT("skeletal_mesh"), SkeletalMeshPath);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("data"), ResultObj);
	return Response;
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPVroidCommands::HandleVroidValidateAvatarAsset(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (!AssetData.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	UObject* Asset = AssetData.GetAsset();
	if (!Asset)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset);
	if (!SkeletalMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Asset is not a SkeletalMesh"));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("asset_name"), Asset->GetName());
	ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
	ResultObj->SetBoolField(TEXT("is_skeletal_mesh"), true);

	// Check materials
	TArray<TSharedPtr<FJsonValue>> Materials;
	for (const FSkeletalMaterial& Mat : SkeletalMesh->GetMaterials())
	{
		if (Mat.MaterialInterface)
		{
			Materials.Add(MakeShared<FJsonValueString>(Mat.MaterialInterface->GetPathName()));
		}
	}
	ResultObj->SetArrayField(TEXT("materials"), Materials);
	ResultObj->SetNumberField(TEXT("material_count"), Materials.Num());

	// Check skeleton
	bool bHasSkeleton = SkeletalMesh->GetSkeleton() != nullptr;
	ResultObj->SetBoolField(TEXT("has_skeleton"), bHasSkeleton);
	if (bHasSkeleton)
	{
		ResultObj->SetStringField(TEXT("skeleton_path"), SkeletalMesh->GetSkeleton()->GetPathName());
	}

	// Check physics asset
	bool bHasPhysicsAsset = SkeletalMesh->GetPhysicsAsset() != nullptr;
	ResultObj->SetBoolField(TEXT("has_physics_asset"), bHasPhysicsAsset);
	if (bHasPhysicsAsset)
	{
		ResultObj->SetStringField(TEXT("physics_asset_path"), SkeletalMesh->GetPhysicsAsset()->GetPathName());
	}

	// Overall health
	bool bIsHealthy = bHasSkeleton && Materials.Num() > 0;
	ResultObj->SetBoolField(TEXT("is_healthy"), bIsHealthy);

	TArray<FString> Warnings;
	if (!bHasSkeleton)
	{
		Warnings.Add(TEXT("Missing Skeleton"));
	}
	if (Materials.IsEmpty())
	{
		Warnings.Add(TEXT("No materials assigned"));
	}
	if (!bHasPhysicsAsset)
	{
		Warnings.Add(TEXT("Missing PhysicsAsset (optional)"));
	}

	TArray<TSharedPtr<FJsonValue>> WarningValues;
	for (const FString& W : Warnings)
	{
		WarningValues.Add(MakeShared<FJsonValueString>(W));
	}
	ResultObj->SetArrayField(TEXT("warnings"), WarningValues);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("data"), ResultObj);
	return Response;
}
