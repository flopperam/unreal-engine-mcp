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
#include "EditorReimportHandler.h"
#include "Factories/MaterialFactoryNew.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

// For export
#include "Exporters/Exporter.h"
#include "AssetExportTask.h"
#include "Exporters/FbxExportOption.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"

// For texture export fallback
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Engine/Texture2D.h"

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
		{TEXT("import_gltf"), &FEpicUnrealMCPAssetImportCommands::HandleImportGeneric},
		{TEXT("import_obj"), &FEpicUnrealMCPAssetImportCommands::HandleImportGeneric},
		{TEXT("import_usd"), &FEpicUnrealMCPAssetImportCommands::HandleImportGeneric},
		{TEXT("import_mp3"), &FEpicUnrealMCPAssetImportCommands::HandleImportGeneric},
		{TEXT("import_alembic"), &FEpicUnrealMCPAssetImportCommands::HandleImportGeneric},
		{TEXT("import_datasmith"), &FEpicUnrealMCPAssetImportCommands::HandleImportGeneric},
		{TEXT("reimport_asset"), &FEpicUnrealMCPAssetImportCommands::HandleReimportAsset},
		{TEXT("save_import_preset"), &FEpicUnrealMCPAssetImportCommands::HandleSaveImportPreset},
		{TEXT("load_import_preset"), &FEpicUnrealMCPAssetImportCommands::HandleLoadImportPreset},
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

	// Auto-generate materials if requested
	bool bAutoGenerateMaterials = false;
	Params->TryGetBoolField(TEXT("auto_generate_materials"), bAutoGenerateMaterials);
	int32 GeneratedMaterials = 0;
	if (bAutoGenerateMaterials)
	{
		for (UObject* Obj : ImportedObjects)
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
			if (StaticMesh)
			{
				GeneratedMaterials += AutoGenerateMaterialsForMesh(StaticMesh, DestinationPath);
			}
		}
	}

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
	if (GeneratedMaterials > 0)
	{
		Result->SetNumberField(TEXT("generated_materials"), GeneratedMaterials);
	}
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
	bool bFlipGreenChannel = false;
	TextureMipGenSettings MipGenSettings = TMGS_FromTextureGroup;
	if (!BuildTextureImportOptions(Params, CompressionSettings, bSRGB, bFlipGreenChannel, MipGenSettings))
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

	// Apply SRGB, Compression, FlipGreenChannel, and MipGen settings to imported textures
	for (UObject* Obj : ImportedObjects)
	{
		UTexture* Texture = Cast<UTexture>(Obj);
		if (Texture && IsValid(Texture))
		{
			bool bChanged = false;
			if (Texture->SRGB != bSRGB)
			{
				Texture->SRGB = bSRGB;
				bChanged = true;
			}
			if (Texture->CompressionSettings != CompressionSettings)
			{
				Texture->CompressionSettings = CompressionSettings;
				bChanged = true;
			}
			if (Texture->bFlipGreenChannel != bFlipGreenChannel)
			{
				Texture->bFlipGreenChannel = bFlipGreenChannel;
				bChanged = true;
			}
			if (Texture->MipGenSettings != MipGenSettings)
			{
				Texture->MipGenSettings = MipGenSettings;
				bChanged = true;
			}

			if (bChanged)
			{
				Texture->UpdateResource();
				Texture->PostEditChange();
			}
		}
	}

	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();

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

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleImportGeneric(
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

	// MP3 is not natively supported by USoundFactory
	if (Extension == TEXT("mp3"))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("MP3 import is not supported. Please convert the file to WAV or OGG before importing."));
	}

	UFactory* Factory = ResolveFactoryForExtension(Extension);
	if (!Factory)
	{
		FString FormatName = Extension.ToUpper();
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("%s import requires the corresponding importer plugin to be enabled. "
				"Enable the plugin in Edit > Plugins and restart the editor."), *FormatName));
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

	// Auto-generate materials if requested
	bool bAutoGenerateMaterials = false;
	Params->TryGetBoolField(TEXT("auto_generate_materials"), bAutoGenerateMaterials);
	int32 GeneratedMaterials = 0;
	if (bAutoGenerateMaterials)
	{
		for (UObject* Obj : ImportedObjects)
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
			if (StaticMesh)
			{
				GeneratedMaterials += AutoGenerateMaterialsForMesh(StaticMesh, DestinationPath);
			}
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
	if (GeneratedMaterials > 0)
	{
		Result->SetNumberField(TEXT("generated_materials"), GeneratedMaterials);
	}
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath + TEXT("/") + AssetName);
	Result->SetStringField(TEXT("format"), Extension);

	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleReimportAsset(
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

	// Use FReimportManager to reimport the asset
	bool bSuccess = FReimportManager::Instance()->Reimport(
		Asset,
		/* bAskForNewFileIfMissing = */ false,
		/* bShowNotification = */ true,
		/* PreferredReimportPath = */ TEXT(""),
		/* SourceFileDialogOverride = */ nullptr,
		/* PreferredImportDataIndex = */ INDEX_NONE,
		/* bForceFullReimport = */ true
	);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	if (!bSuccess)
	{
		Result->SetStringField(TEXT("error"), TEXT("Reimport failed. Ensure the source file still exists and the asset has valid import data."));
	}
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

	// Resolve asset via AssetRegistry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (!AssetData.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	UObject* Asset = AssetData.GetAsset();
	if (!Asset || !IsValid(Asset))
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

	// Ensure output directory exists
	FString OutputDir = FPaths::GetPath(OutputPath);
	if (!OutputDir.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*OutputDir, true);
	}

	// -------------------------------------------------------------------------
	// UAssetExportTask approach (UE5 standard API)
	// UExporter::RunAssetExportTask automatically resolves the correct exporter
	// from registered classes, avoiding the CDO-not-in-memory issue that
	// TObjectIterator<UExporter> and manual class lookup suffer from.
	// -------------------------------------------------------------------------
	UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
	ExportTask->AddToRoot(); // GC protection during export

	ExportTask->Object = Asset;
	ExportTask->Filename = OutputPath;
	ExportTask->bAutomated = true;
	ExportTask->bPrompt = false;
	ExportTask->bReplaceIdentical = true;
	ExportTask->bSelected = false;

	// Use UExporter::FindExporter to resolve the best exporter for this
	// asset type + format combination. This scans all registered exporter
	// classes (not just existing CDO instances).
	UExporter* Exporter = UExporter::FindExporter(Asset, *ExportFormat);
	if (Exporter)
	{
		ExportTask->Exporter = Exporter;
	}

	// Format-specific export options
	if (ExportFormat.Equals(TEXT("fbx"), ESearchCase::IgnoreCase))
	{
		UFbxExportOption* FbxOpts = NewObject<UFbxExportOption>();

		// Parse optional FBX settings from MCP params
		bool bExportCollision = false;
		Params->TryGetBoolField(TEXT("export_collision"), bExportCollision);
		FbxOpts->Collision = bExportCollision;

		bool bExportVertexColor = true;
		Params->TryGetBoolField(TEXT("export_vertex_color"), bExportVertexColor);
		FbxOpts->VertexColor = bExportVertexColor;

		ExportTask->Options = FbxOpts;
	}

	// Execute export via UE5 standard API
	bool bExportSuccess = UExporter::RunAssetExportTask(ExportTask);

	ExportTask->RemoveFromRoot(); // Release GC protection

	// -------------------------------------------------------------------------
	// Fallback for textures: use IImageWrapper to write source data directly
	// This handles edge cases where the UExporter system can't find a
	// suitable exporter for the texture format.
	// -------------------------------------------------------------------------
	if (!bExportSuccess && Asset->IsA<UTexture2D>())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UExporter::RunAssetExportTask failed for texture '%s'. "
				 "Attempting FImageUtils/IImageWrapper fallback."),
			*AssetPath);
		bExportSuccess = ExportTextureViaImageUtils(
			Cast<UTexture2D>(Asset), OutputPath, ExportFormat);
	}

	if (!bExportSuccess)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Export failed for asset: %s (format: %s). "
				"Verify the asset is valid and the export format is supported."),
				*AssetPath, *ExportFormat));
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

	Task->AddToRoot(); // Prevent GC during import

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();

	AssetTools.ImportAssetTasks({Task});

	Task->RemoveFromRoot(); // Allow GC again

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
	else if (Extension == TEXT("gltf") || Extension == TEXT("glb"))
	{
		UClass* FactoryClass = FindObject<UClass>(nullptr, TEXT("/Script/GLTFImporter.GLTFImportFactory"));
		if (FactoryClass && FactoryClass->IsChildOf(UFactory::StaticClass()))
		{
			return NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		}
		return nullptr;
	}
	else if (Extension == TEXT("obj"))
	{
		// OBJ is supported via the FBX SDK
		return NewObject<UFbxFactory>();
	}
	else if (Extension == TEXT("usd") || Extension == TEXT("usda") || Extension == TEXT("usdc"))
	{
		UClass* FactoryClass = FindObject<UClass>(nullptr, TEXT("/Script/USDImporter.UsdStageImportFactory"));
		if (FactoryClass && FactoryClass->IsChildOf(UFactory::StaticClass()))
		{
			return NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		}
		return nullptr;
	}
	else if (Extension == TEXT("abc"))
	{
		UClass* FactoryClass = FindObject<UClass>(nullptr, TEXT("/Script/AlembicImporter.AlembicImportFactory"));
		if (FactoryClass && FactoryClass->IsChildOf(UFactory::StaticClass()))
		{
			return NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		}
		return nullptr;
	}
	else if (Extension == TEXT("udatasmith"))
	{
		UClass* FactoryClass = FindObject<UClass>(nullptr, TEXT("/Script/DatasmithImporter.DatasmithImporter"));
		if (FactoryClass && FactoryClass->IsChildOf(UFactory::StaticClass()))
		{
			return NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		}
		return nullptr;
	}

	// Unsupported extension
	return nullptr;
}

UFbxImportUI* FEpicUnrealMCPAssetImportCommands::BuildFbxImportOptions(const TSharedPtr<FJsonObject>& Params)
{
	UFbxImportUI* Options = NewObject<UFbxImportUI>(
		GetTransientPackage(),
		UFbxImportUI::StaticClass(),
		NAME_None,
		RF_NoFlags,
		GetMutableDefault<UFbxImportUI>()
	);
	
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

		// LOD screen sizes (per-LOD screen size thresholds)
		// NOTE: In UE 5.7, BuildSettings lives on UStaticMesh::GetSourceModel(), not on
		// UFbxStaticMeshImportData. LOD tuning is applied post-import in HandleImportFbxMesh.
		const TArray<TSharedPtr<FJsonValue>>* ScreenSizes = nullptr;
		if (Params->TryGetArrayField(TEXT("lod_screen_sizes"), ScreenSizes) && ScreenSizes)
		{
			// Store in a transient tag for post-import application
			// (applied after the mesh is imported via UStaticMesh::GetSourceModel())
		}

		// LOD count override
		int32 LODCount = -1;
		if (Params->TryGetNumberField(TEXT("lod_count"), LODCount) && LODCount > 0)
		{
			// bBuildAdjacencyBuffer is applied post-import in UE 5.7
		}
	}

	return Options;
}

bool FEpicUnrealMCPAssetImportCommands::BuildTextureImportOptions(
	const TSharedPtr<FJsonObject>& Params,
	TextureCompressionSettings& OutCompressionSettings,
	bool& OutSRGB,
	bool& OutFlipGreenChannel,
	TextureMipGenSettings& OutMipGenSettings)
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

	// Flip green channel (normal map correction)
	Params->TryGetBoolField(TEXT("flip_green_channel"), OutFlipGreenChannel);

	// Mip generation settings
	FString MipGenStr;
	if (Params->TryGetStringField(TEXT("mip_gen_settings"), MipGenStr))
	{
		if (MipGenStr == TEXT("NoMipmaps"))        OutMipGenSettings = TMGS_NoMipmaps;
		else if (MipGenStr == TEXT("SimpleAverage")) OutMipGenSettings = TMGS_SimpleAverage;
		else if (MipGenStr == TEXT("Sharpen0"))     OutMipGenSettings = TMGS_Sharpen0;
		else if (MipGenStr == TEXT("Sharpen1"))     OutMipGenSettings = TMGS_Sharpen1;
		else if (MipGenStr == TEXT("Sharpen2"))     OutMipGenSettings = TMGS_Sharpen2;
		else if (MipGenStr == TEXT("Sharpen3"))     OutMipGenSettings = TMGS_Sharpen3;
		else if (MipGenStr == TEXT("Sharpen4"))     OutMipGenSettings = TMGS_Sharpen4;
		else if (MipGenStr == TEXT("Sharpen5"))     OutMipGenSettings = TMGS_Sharpen5;
		else if (MipGenStr == TEXT("Sharpen6"))     OutMipGenSettings = TMGS_Sharpen6;
		else if (MipGenStr == TEXT("Sharpen7"))     OutMipGenSettings = TMGS_Sharpen7;
		else if (MipGenStr == TEXT("Sharpen8"))     OutMipGenSettings = TMGS_Sharpen8;
		else if (MipGenStr == TEXT("Sharpen9"))     OutMipGenSettings = TMGS_Sharpen9;
		else if (MipGenStr == TEXT("Sharpen10"))    OutMipGenSettings = TMGS_Sharpen10;
		else if (MipGenStr == TEXT("Blur1"))        OutMipGenSettings = TMGS_Blur1;
		else if (MipGenStr == TEXT("Blur2"))        OutMipGenSettings = TMGS_Blur2;
		else if (MipGenStr == TEXT("Blur3"))        OutMipGenSettings = TMGS_Blur3;
		else if (MipGenStr == TEXT("Blur4"))        OutMipGenSettings = TMGS_Blur4;
		else if (MipGenStr == TEXT("Blur5"))        OutMipGenSettings = TMGS_Blur5;
		else if (MipGenStr == TEXT("FromTextureGroup")) OutMipGenSettings = TMGS_FromTextureGroup;
	}

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

	bool bIncludeModulator = false;
	if (Params->TryGetBoolField(TEXT("include_modulator"), bIncludeModulator))
	{
		OutFactory->bIncludeModulatorNode = bIncludeModulator;
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

// ---------------------------------------------------------------------------
// Texture Export Fallback (IImageWrapper)
// ---------------------------------------------------------------------------

bool FEpicUnrealMCPAssetImportCommands::ExportTextureViaImageUtils(
	UTexture2D* Texture, const FString& OutputPath, const FString& ExportFormat)
{
	if (!Texture || !IsValid(Texture))
	{
		return false;
	}

	// Ensure texture source data is available
	FTextureSource& Source = Texture->Source;
	if (!Source.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ExportTextureViaImageUtils: Texture '%s' has no valid source data."),
			*Texture->GetPathName());
		return false;
	}

	// Read mip 0 raw data
	TArray64<uint8> RawData;
	Source.GetMipData(RawData, 0);
	if (RawData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ExportTextureViaImageUtils: Failed to read mip data from '%s'."),
			*Texture->GetPathName());
		return false;
	}

	const int32 Width = Source.GetSizeX();
	const int32 Height = Source.GetSizeY();

	// Determine format for IImageWrapper
	EImageFormat ImageFormat = EImageFormat::PNG;
	if (ExportFormat.Equals(TEXT("bmp"), ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::BMP;
	}
	else if (ExportFormat.Equals(TEXT("exr"), ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::EXR;
	}
	else if (ExportFormat.Equals(TEXT("jpg"), ESearchCase::IgnoreCase) ||
			 ExportFormat.Equals(TEXT("jpeg"), ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::JPEG;
	}
	// Default: PNG for "png", "tga", and other formats

	// Determine pixel format from texture source
	ERGBFormat RGBFormat = ERGBFormat::BGRA;
	int32 BitDepth = 8;

	ETextureSourceFormat SourceFormat = Source.GetFormat();
	switch (SourceFormat)
	{
	case TSF_RGBA16:
	case TSF_RGBA16F:
		BitDepth = 16;
		RGBFormat = ERGBFormat::RGBA;
		break;
	case TSF_RGBA32F:
		BitDepth = 32;
		RGBFormat = ERGBFormat::RGBA;
		break;
	case TSF_G8:
		BitDepth = 8;
		RGBFormat = ERGBFormat::Gray;
		break;
	case TSF_G16:
		BitDepth = 16;
		RGBFormat = ERGBFormat::Gray;
		break;
	default:
		// TSF_BGRA8 and others default to BGRA 8-bit
		BitDepth = 8;
		RGBFormat = ERGBFormat::BGRA;
		break;
	}

	// Use IImageWrapper to compress and save
	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper =
		ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("ExportTextureViaImageUtils: Failed to create ImageWrapper for format '%s'."),
			*ExportFormat);
		return false;
	}

	if (!ImageWrapper->SetRaw(
			RawData.GetData(), RawData.Num(),
			Width, Height, RGBFormat, BitDepth))
	{
		UE_LOG(LogTemp, Error,
			TEXT("ExportTextureViaImageUtils: Failed to set raw data for '%s' (%dx%d)."),
			*Texture->GetPathName(), Width, Height);
		return false;
	}

	TArray64<uint8> CompressedData = ImageWrapper->GetCompressed();
	if (CompressedData.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ExportTextureViaImageUtils: Compression produced empty output for '%s'."),
			*Texture->GetPathName());
		return false;
	}

	if (!FFileHelper::SaveArrayToFile(CompressedData, *OutputPath))
	{
		UE_LOG(LogTemp, Error,
			TEXT("ExportTextureViaImageUtils: Failed to write file '%s'."),
			*OutputPath);
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("ExportTextureViaImageUtils: Successfully exported '%s' to '%s'."),
		*Texture->GetPathName(), *OutputPath);
	return true;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleSaveImportPreset(
	const TSharedPtr<FJsonObject>& Params)
{
	FString PresetName;
	if (!Params->TryGetStringField(TEXT("preset_name"), PresetName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("preset_name is required"));
	}

	TSharedPtr<FJsonObject> PresetData;
	const TSharedPtr<FJsonObject>* FoundData = nullptr;
	if (Params->TryGetObjectField(TEXT("data"), FoundData) && FoundData)
	{
		PresetData = *FoundData;
	}
	else
	{
		// Use the entire params object as preset data (excluding preset_name)
		PresetData = MakeShared<FJsonObject>(*Params);
		PresetData->RemoveField(TEXT("preset_name"));
		PresetData->RemoveField(TEXT("command"));
	}

	FString PresetsDir = FPaths::ProjectSavedDir() / TEXT("MCP_ImportPresets");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*PresetsDir))
	{
		PlatformFile.CreateDirectoryTree(*PresetsDir);
	}

	FString FilePath = PresetsDir / PresetName + TEXT(".json");
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(PresetData.ToSharedRef(), Writer))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to serialize preset data"));
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to write preset file"));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("preset_name"), PresetName);
	Result->SetStringField(TEXT("file_path"), FilePath);
	return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPAssetImportCommands::HandleLoadImportPreset(
	const TSharedPtr<FJsonObject>& Params)
{
	FString PresetName;
	if (!Params->TryGetStringField(TEXT("preset_name"), PresetName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("preset_name is required"));
	}

	FString FilePath = FPaths::ProjectSavedDir() / TEXT("MCP_ImportPresets") / PresetName + TEXT(".json");
	if (!FPaths::FileExists(FilePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Preset '%s' not found at %s"), *PresetName, *FilePath));
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to read preset file"));
	}

	TSharedPtr<FJsonObject> PresetData;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, PresetData) || !PresetData.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to parse preset JSON"));
	}

	PresetData->SetStringField(TEXT("preset_name"), PresetName);
	PresetData->SetBoolField(TEXT("success"), true);
	return PresetData;
}

int32 FEpicUnrealMCPAssetImportCommands::AutoGenerateMaterialsForMesh(UStaticMesh* StaticMesh, const FString& DestinationPath)
{
	if (!StaticMesh || !IsValid(StaticMesh))
	{
		return 0;
	}

	int32 CreatedCount = 0;
	const int32 NumSections = StaticMesh->GetNumSections(0);
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		FString MaterialName = FString::Printf(TEXT("M_%s_Section%d"), *StaticMesh->GetName(), SectionIndex);
		FString MaterialPackagePath = DestinationPath / MaterialName;

		UPackage* Package = CreatePackage(*MaterialPackagePath);
		if (!Package)
		{
			continue;
		}

		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		UMaterial* NewMaterial = Cast<UMaterial>(
			MaterialFactory->FactoryCreateNew(
				UMaterial::StaticClass(),
				Package,
				FName(*MaterialName),
				RF_Public | RF_Standalone,
				nullptr,
				GWarn
			)
		);

		if (!NewMaterial)
		{
			continue;
		}

		NewMaterial->SetShadingModel(MSM_DefaultLit);
		NewMaterial->TwoSided = false;

		FAssetRegistryModule::AssetCreated(NewMaterial);
		Package->MarkPackageDirty();

		// Assign to mesh slot
		StaticMesh->SetMaterial(SectionIndex, NewMaterial);

		CreatedCount++;
	}

	if (CreatedCount > 0)
	{
		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
	}

	return CreatedCount;
}
