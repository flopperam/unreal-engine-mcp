#include "Commands/EpicUnrealMCPAssetImportCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"

// UE5.7 Core Headers
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Factories/Factory.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "RenderingThread.h"
#include "Factories/TextureFactory.h"
#include "Factories/SoundFactory.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

// For export
#include "Exporters/Exporter.h"
#include "AssetRegistry/AssetRegistryModule.h"

FEpicUnrealMCPAssetImportCommands::FEpicUnrealMCPAssetImportCommands()
{
}

// ---------------------------------------------------------------------------
// Command Router
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPAssetImportCommands::*)(const TSharedPtr<FJsonObject>&);
	static const TMap<FString, Handler> Dispatch = {
		{TEXT("import_fbx_mesh"), &FEpicUnrealMCPAssetImportCommands::HandleImportFbxMesh},
		{TEXT("import_texture"), &FEpicUnrealMCPAssetImportCommands::HandleImportTexture},
		{TEXT("import_audio"), &FEpicUnrealMCPAssetImportCommands::HandleImportAudio},
		{TEXT("export_asset"), &FEpicUnrealMCPAssetImportCommands::HandleExportAsset},
	};

	const Handler* H = Dispatch.Find(CommandType);
	if (H)
	{
		return (this->*(*H))(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown asset import/export command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Import Handlers
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleImportFbxMesh(
	const TSharedPtr<FJsonObject>& Params)
{
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

	// Validate source file exists
	FString Error;
	if (!ValidateSourceFile(SourcePath, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Validate destination path
	if (!ValidateDestinationPath(DestinationPath, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Get optional asset name (default to filename without extension)
	FString AssetName;
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
	{
		AssetName = FPaths::GetBaseFilename(SourcePath);
	}

	// Resolve FBX factory
	UFactory* Factory = ResolveFactoryForExtension(TEXT("fbx"));
	if (!Factory)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to resolve FBX factory"));
	}

	// Build FBX import options
	UFbxImportUI* ImportOptions = BuildFbxImportOptions(Params);

	// Create and process import task
	UAssetImportTask* Task = CreateImportTask(SourcePath, DestinationPath, AssetName, Factory, ImportOptions);
	if (!Task)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create import task"));
	}

	TArray<UObject*> ImportedObjects = ProcessImportTask(Task, Error);
	if (ImportedObjects.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Mark packages dirty (no forced GC)
	MarkPackagesDirty(ImportedObjects);

	// Build response
	TArray<TSharedPtr<FJsonValue>> ImportedAssets;
	for (UObject* Obj : ImportedObjects)
	{
		if (Obj && IsValid(Obj))
		{
			ImportedAssets.Add(MakeShared<FJsonValueString>(Obj->GetPathName()));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("imported_assets"), ImportedAssets);
	Result->SetNumberField(TEXT("count"), ImportedAssets.Num());
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath + TEXT("/") + AssetName);
	
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleImportTexture(
	const TSharedPtr<FJsonObject>& Params)
{
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

	FString Error;
	if (!ValidateSourceFile(SourcePath, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (!ValidateDestinationPath(DestinationPath, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FString AssetName;
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
	{
		AssetName = FPaths::GetBaseFilename(SourcePath);
	}

	// Get file extension for factory resolution
	FString Extension = FPaths::GetExtension(SourcePath).ToLower();
	UFactory* Factory = ResolveFactoryForExtension(Extension);
	if (!Factory)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported texture format: %s"), *Extension));
	}

	// Build texture import options
	TextureCompressionSettings CompressionSettings = TC_Default;
	bool bSRGB = true;
	if (!BuildTextureImportOptions(Params, CompressionSettings, bSRGB))
	{
		// Continue with defaults if options parsing fails
	}

	// Configure TextureFactory settings
	UTextureFactory* TextureFactory = Cast<UTextureFactory>(Factory);
	if (TextureFactory)
	{
		TextureFactory->CompressionSettings = CompressionSettings;
		// Note: SRGB is set per-texture after import, not on factory
	}

	UAssetImportTask* Task = CreateImportTask(SourcePath, DestinationPath, AssetName, Factory, nullptr);
	if (!Task)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create import task"));
	}

	TArray<UObject*> ImportedObjects = ProcessImportTask(Task, Error);
	if (ImportedObjects.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Apply SRGB setting to imported textures
	for (UObject* Obj : ImportedObjects)
	{
		UTexture* Texture = Cast<UTexture>(Obj);
		if (Texture && IsValid(Texture))
		{
			Texture->SRGB = bSRGB;
			FlushRenderingCommands();
			Texture->PostEditChange();
		}
	}

	MarkPackagesDirty(ImportedObjects);

	TArray<TSharedPtr<FJsonValue>> ImportedAssets;
	for (UObject* Obj : ImportedObjects)
	{
		if (Obj && IsValid(Obj))
		{
			ImportedAssets.Add(MakeShared<FJsonValueString>(Obj->GetPathName()));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("imported_assets"), ImportedAssets);
	Result->SetNumberField(TEXT("count"), ImportedAssets.Num());
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath + TEXT("/") + AssetName);
	
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleImportAudio(
	const TSharedPtr<FJsonObject>& Params)
{
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

	FString Error;
	if (!ValidateSourceFile(SourcePath, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (!ValidateDestinationPath(DestinationPath, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FString AssetName;
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
	{
		AssetName = FPaths::GetBaseFilename(SourcePath);
	}

	FString Extension = FPaths::GetExtension(SourcePath).ToLower();
	UFactory* Factory = ResolveFactoryForExtension(Extension);
	if (!Factory)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unsupported audio format: %s"), *Extension));
	}

	// Configure SoundFactory options
	USoundFactory* SoundFactory = Cast<USoundFactory>(Factory);
	if (SoundFactory)
	{
		BuildAudioImportOptions(Params, SoundFactory);
	}

	UAssetImportTask* Task = CreateImportTask(SourcePath, DestinationPath, AssetName, Factory, nullptr);
	if (!Task)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create import task"));
	}

	TArray<UObject*> ImportedObjects = ProcessImportTask(Task, Error);
	if (ImportedObjects.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	MarkPackagesDirty(ImportedObjects);

	TArray<TSharedPtr<FJsonValue>> ImportedAssets;
	for (UObject* Obj : ImportedObjects)
	{
		if (Obj && IsValid(Obj))
		{
			ImportedAssets.Add(MakeShared<FJsonValueString>(Obj->GetPathName()));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("imported_assets"), ImportedAssets);
	Result->SetNumberField(TEXT("count"), ImportedAssets.Num());
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath + TEXT("/") + AssetName);
	
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleExportAsset(
	const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
	}

	FString OutputPath;
	if (!Params->TryGetStringField(TEXT("output_path"), OutputPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("output_path is required"));
	}

	// Resolve asset
	FAssetData AssetData;
	FString Error;
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
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

	// Determine export format from extension or parameter
	FString ExportFormat;
	if (!Params->TryGetStringField(TEXT("export_format"), ExportFormat) || ExportFormat.IsEmpty())
	{
		ExportFormat = FPaths::GetExtension(OutputPath).ToLower();
	}

	// Find exporter for format
	UExporter* Exporter = nullptr;
	for (TObjectIterator<UExporter> It; It; ++It)
	{
		if (IsValid(*It) && It->SupportedClass && Asset->IsA(It->SupportedClass))
		{
			for (const FString& Ext : It->FormatExtension)
			{
				if (Ext.Equals(ExportFormat, ESearchCase::IgnoreCase))
				{
					Exporter = *It;
					break;
				}
			}
			if (Exporter)
			{
				break;
			}
		}
	}

	if (!Exporter)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("No exporter found for format: %s"), *ExportFormat));
	}

	// Perform export
	bool bExportSuccess = UExporter::ExportToFile(Asset, Exporter, *OutputPath, false, false, false) > 0;
	
	if (!bExportSuccess)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Export failed for asset: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("output_path"), OutputPath);
	Result->SetStringField(TEXT("format"), ExportFormat);
	Result->SetStringField(TEXT("message"), TEXT("Export successful"));
	
	return Result;
}

// ---------------------------------------------------------------------------
// Common Helpers Implementation
// ---------------------------------------------------------------------------

UAssetImportTask* FEpicUnrealMCPAssetImportCommands::CreateImportTask(
	const FString& SourcePath,
	const FString& DestinationPath,
	const FString& AssetName,
	UFactory* Factory,
	UObject* Options)
{
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	if (!Task)
	{
		return nullptr;
	}

	Task->Filename = SourcePath;
	Task->DestinationPath = DestinationPath;
	Task->DestinationName = AssetName;
	Task->bAutomated = true;        // Suppress UI popups
	Task->bAsync = false;           // Synchronous for MCP
	Task->bSave = false;            // Don't auto-save, let editor handle it
	Task->bReplaceExisting = true;  // Replace if exists
	Task->bReplaceExistingSettings = false;
	Task->Factory = Factory;
	Task->Options = Options;

	return Task;
}

TArray<UObject*> FEpicUnrealMCPAssetImportCommands::ProcessImportTask(UAssetImportTask* Task, FString& OutError)
{
	if (!Task)
	{
		OutError = TEXT("Invalid import task");
		return TArray<UObject*>();
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	FlushRenderingCommands();
	AssetTools.ImportAssetTasks({Task});
	FlushRenderingCommands();

	// Check results
	if (Task->GetObjects().Num() == 0)
	{
		OutError = FString::Printf(TEXT("Nothing was imported from: %s"), *Task->Filename);
		return TArray<UObject*>();
	}

	return Task->GetObjects();
}

UFactory* FEpicUnrealMCPAssetImportCommands::ResolveFactoryForExtension(const FString& Extension)
	{
	if (Extension == TEXT("fbx"))
	{
		return NewObject<UFbxFactory>();
	}
	else if (Extension == TEXT("png") || Extension == TEXT("jpg") || Extension == TEXT("jpeg") || 
			 Extension == TEXT("exr") || Extension == TEXT("hdr") || Extension == TEXT("tga") ||
			 Extension == TEXT("bmp") || Extension == TEXT("psd"))
	{
		return NewObject<UTextureFactory>();
	}
	else if (Extension == TEXT("wav") || Extension == TEXT("ogg"))
	{
		return NewObject<USoundFactory>();
	}
	
	// Unsupported extension
	return nullptr;
}

UFbxImportUI* FEpicUnrealMCPAssetImportCommands::BuildFbxImportOptions(const TSharedPtr<FJsonObject>& Params)
{
	UFbxImportUI* Options = NewObject<UFbxImportUI>();
	
	// Get import type (static/skeletal/auto)
	FString ImportTypeStr;
	if (Params->TryGetStringField(TEXT("import_type"), ImportTypeStr))
	{
		if (ImportTypeStr == TEXT("static"))
		{
			Options->MeshTypeToImport = FBXIT_StaticMesh;
		}
		else if (ImportTypeStr == TEXT("skeletal"))
		{
			Options->MeshTypeToImport = FBXIT_SkeletalMesh;
		}
		// "auto" leaves as default FBXIT_MAX (auto-detect)
	}

	// Route import options to the correct sub-object based on mesh type
	const bool bIsSkeletal = (Options->MeshTypeToImport == FBXIT_SkeletalMesh);

	if (bIsSkeletal)
	{
		if (!Options->SkeletalMeshImportData)
		{
			return nullptr;
		}

		// Scale settings
		double Scale = 1.0;
		if (Params->TryGetNumberField(TEXT("scale"), Scale))
		{
			Options->SkeletalMeshImportData->ImportUniformScale = static_cast<float>(Scale);
		}

		// Scene unit conversion
		bool bConvertSceneUnit = false;
		if (Params->TryGetBoolField(TEXT("convert_scene_unit"), bConvertSceneUnit))
		{
			Options->SkeletalMeshImportData->bConvertSceneUnit = bConvertSceneUnit;
		}

		// Collision import
		bool bImportCollision = false;
		if (Params->TryGetBoolField(TEXT("import_collision"), bImportCollision))
		{
			Options->SkeletalMeshImportData->bTransformVertexToAbsolute = true;
		}
	}
	else
	{
		if (!Options->StaticMeshImportData)
		{
			return nullptr;
		}

		// Scale settings
		double Scale = 1.0;
		if (Params->TryGetNumberField(TEXT("scale"), Scale))
		{
			Options->StaticMeshImportData->ImportUniformScale = static_cast<float>(Scale);
		}

		// Scene unit conversion
		bool bConvertSceneUnit = false;
		if (Params->TryGetBoolField(TEXT("convert_scene_unit"), bConvertSceneUnit))
		{
			Options->StaticMeshImportData->bConvertSceneUnit = bConvertSceneUnit;
		}

		// Collision import
		bool bImportCollision = false;
		if (Params->TryGetBoolField(TEXT("import_collision"), bImportCollision))
		{
			Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
		}

		// Lightmap UV generation
		bool bGenerateLightmapUV = true;
		if (Params->TryGetBoolField(TEXT("generate_lightmap_uv"), bGenerateLightmapUV))
		{
			Options->StaticMeshImportData->bGenerateLightmapUVs = bGenerateLightmapUV;
		}

		// Nanite settings (UE5.7+)
		bool bNaniteEnabled = false;
		if (Params->TryGetBoolField(TEXT("nanite_enabled"), bNaniteEnabled))
		{
			Options->StaticMeshImportData->bBuildNanite = bNaniteEnabled;
		}

		// LOD group
		FString LODGroup;
		if (Params->TryGetStringField(TEXT("lod_group"), LODGroup))
		{
			Options->StaticMeshImportData->StaticMeshLODGroup = FName(*LODGroup);
		}
	}

	return Options;
}

bool FEpicUnrealMCPAssetImportCommands::BuildTextureImportOptions(
	const TSharedPtr<FJsonObject>& Params,
	TextureCompressionSettings& OutCompressionSettings,
	bool& OutSRGB)
{
	// Texture type presets
	FString TextureType;
	if (Params->TryGetStringField(TEXT("texture_type"), TextureType))
	{
		if (TextureType == TEXT("normal"))
		{
			OutCompressionSettings = TC_Normalmap;
			OutSRGB = false;
		}
		else if (TextureType == TEXT("orm"))
		{
			OutCompressionSettings = TC_Masks;
			OutSRGB = false;
		}
		else if (TextureType == TEXT("hdr"))
		{
			OutCompressionSettings = TC_HDR;
			OutSRGB = false;
		}
		else if (TextureType == TEXT("default"))
		{
			OutCompressionSettings = TC_Default;
			OutSRGB = true;
		}
	}

	// Explicit compression setting
	FString CompressionStr;
	if (Params->TryGetStringField(TEXT("compression"), CompressionStr))
	{
		if (CompressionStr == TEXT("BC7")) OutCompressionSettings = TC_BC7;
		else if (CompressionStr == TEXT("normal") || CompressionStr == TEXT("BC5")) OutCompressionSettings = TC_Normalmap;
		else if (CompressionStr == TEXT("default")) OutCompressionSettings = TC_Default;
		// Add more as needed
	}

	// SRGB override
	Params->TryGetBoolField(TEXT("srgb"), OutSRGB);

	return true;
}

bool FEpicUnrealMCPAssetImportCommands::BuildAudioImportOptions(
	const TSharedPtr<FJsonObject>& Params,
	USoundFactory* OutFactory)
	{
	if (!OutFactory)
	{
		return false;
	}

	bool bAutoCreateCue = false;
	if (Params->TryGetBoolField(TEXT("auto_create_cue"), bAutoCreateCue))
	{
		OutFactory->bAutoCreateCue = bAutoCreateCue;
	}

	bool bIncludeAttenuation = false;
	if (Params->TryGetBoolField(TEXT("include_attenuation"), bIncludeAttenuation))
	{
		OutFactory->bIncludeAttenuationNode = bIncludeAttenuation;
	}

	bool bIncludeLooping = false;
	if (Params->TryGetBoolField(TEXT("include_looping"), bIncludeLooping))
	{
		OutFactory->bIncludeLoopingNode = bIncludeLooping;
	}

	float CueVolume = 1.0f;
	if (Params->TryGetNumberField(TEXT("cue_volume"), CueVolume))
	{
		OutFactory->CueVolume = CueVolume;
	}

	return true;
}

bool FEpicUnrealMCPAssetImportCommands::ValidateSourceFile(const FString& SourcePath, FString& OutError)
{
	if (!FPaths::FileExists(SourcePath))
	{
		OutError = FString::Printf(TEXT("Source file does not exist: %s"), *SourcePath);
		return false;
	}
	
	// Check file is readable
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*SourcePath))
	{
		OutError = FString::Printf(TEXT("Source file not accessible: %s"), *SourcePath);
		return false;
	}
	
	return true;
}

bool FEpicUnrealMCPAssetImportCommands::ValidateDestinationPath(const FString& DestinationPath, FString& OutError)
{
	if (!DestinationPath.StartsWith(TEXT("/Game/")) && !DestinationPath.StartsWith(TEXT("/Engine/")))
	{
		OutError = TEXT("Destination path must start with /Game/ or /Engine/");
		return false;
	}
	
	return true;
}

bool FEpicUnrealMCPAssetImportCommands::PackagePathToDiskPath(const FString& PackagePath, FString& OutDiskPath)
{
	return FPackageName::TryConvertLongPackageNameToFilename(PackagePath, OutDiskPath);
}

void FEpicUnrealMCPAssetImportCommands::MarkPackagesDirty(const TArray<UObject*>& ImportedObjects)
{
	TSet<UPackage*> PackagesToDirty;
	
	for (UObject* Obj : ImportedObjects)
	{
		if (Obj && Obj->GetPackage())
		{
			PackagesToDirty.Add(Obj->GetPackage());
		}
	}
	
	for (UPackage* Package : PackagesToDirty)
	{
		if (Package)
		{
			Package->SetDirtyFlag(true);
		}
	}
	
	// Packages are marked dirty; Content Browser will refresh on next tick automatically
}
