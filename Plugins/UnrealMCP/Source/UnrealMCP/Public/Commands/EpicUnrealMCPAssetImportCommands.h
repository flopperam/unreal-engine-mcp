#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Asset Import / Export MCP commands
 * Handles FBX/Texture/Audio import and asset export with UE5.7 API compliance.
 * 
 * UE5.7 Module Dependencies:
 * - UnrealEd: UFbxFactory, UFbxImportUI, UTextureFactory, UAssetImportTask
 * - AudioEditor: USoundFactory
 * 
 * Thread Safety: All handlers run on GameThread via AsyncTask from EpicUnrealMCPBridge.
 * GC Safety: No forced garbage collection. Packages marked dirty for natural GC cycle.
 */
class UNREALMCP_API FEpicUnrealMCPAssetImportCommands
{
public:
	FEpicUnrealMCPAssetImportCommands();

	// Handle asset import/export commands
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// ---------------------------------------------------------------------------
	// Import Handlers
	// ---------------------------------------------------------------------------
	
	// FBX Mesh Import (Static/Skeletal)
	TSharedPtr<FJsonObject> HandleImportFbxMesh(const TSharedPtr<FJsonObject>& Params);
	
	// Texture Import (PNG/JPG/EXR/HDR with Normal/ORM/HDR settings)
	TSharedPtr<FJsonObject> HandleImportTexture(const TSharedPtr<FJsonObject>& Params);
	
	// Audio Import (WAV/OGG with SoundCue options)
	TSharedPtr<FJsonObject> HandleImportAudio(const TSharedPtr<FJsonObject>& Params);

	// Generic Import (GLTF/OBJ/USD/Alembic/Datasmith with conditional plugin support)
	TSharedPtr<FJsonObject> HandleImportGeneric(const TSharedPtr<FJsonObject>& Params);

	// Reimport existing asset
	TSharedPtr<FJsonObject> HandleReimportAsset(const TSharedPtr<FJsonObject>& Params);

	// Import Preset save/load
	TSharedPtr<FJsonObject> HandleSaveImportPreset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleLoadImportPreset(const TSharedPtr<FJsonObject>& Params);

	// ---------------------------------------------------------------------------
	// Export Handlers
	// ---------------------------------------------------------------------------

	// Asset Export (to FBX/OBJ/PNG/etc.)
	TSharedPtr<FJsonObject> HandleExportAsset(const TSharedPtr<FJsonObject>& Params);

	// Screenshot capture
	TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);

	// Level export (JSON manifest)
	TSharedPtr<FJsonObject> HandleExportLevel(const TSharedPtr<FJsonObject>& Params);

	// ---------------------------------------------------------------------------
	// Common Helpers
	// ---------------------------------------------------------------------------
	
	/**
	 * Create an import task with UE5.7 UAssetImportTask API.
	 * @param SourcePath - Absolute disk path to source file
	 * @param DestinationPath - Package path (e.g., "/Game/Imported")
	 * @param AssetName - Desired asset name (empty = use filename)
	 * @param Factory - Specific factory (null = auto-detect from extension)
	 * @param Options - Import options object (UFbxImportUI, etc.)
	 * @return Configured UAssetImportTask
	 */
	UAssetImportTask* CreateImportTask(
		const FString& SourcePath,
		const FString& DestinationPath,
		const FString& AssetName,
		UFactory* Factory,
		UObject* Options);
	
	/**
	 * Process import task and return imported objects.
	 * @param Task - Configured import task
	 * @param OutError - Error message on failure
	 * @return Array of imported UObject* (empty on failure)
	 */
	TArray<UObject*> ProcessImportTask(UAssetImportTask* Task, FString& OutError);
	
	/**
	 * Resolve appropriate factory for file extension.
	 * @param Extension - File extension without dot (e.g., "fbx", "png")
	 * @return Factory instance (may be null if extension unsupported)
	 */
	UFactory* ResolveFactoryForExtension(const FString& Extension);
	
	/**
	 * Build FBX import options (UFbxImportUI) from JSON parameters.
	 * @param Params - JSON parameters from MCP tool
	 * @return Configured UFbxImportUI instance
	 */
	UFbxImportUI* BuildFbxImportOptions(const TSharedPtr<FJsonObject>& Params);
	
	/**
	 * Build Texture import options from JSON parameters.
	 * @param Params - JSON parameters
	 * @param OutCompressionSettings - Output compression settings enum
	 * @param OutSRGB - Output sRGB flag
	 * @param OutFlipGreenChannel - Output flip green channel flag
	 * @param OutMipGenSettings - Output mip generation settings
	 * @return true if options parsed successfully
	 */
	bool BuildTextureImportOptions(
		const TSharedPtr<FJsonObject>& Params,
		TextureCompressionSettings& OutCompressionSettings,
		bool& OutSRGB,
		bool& OutFlipGreenChannel,
		TextureMipGenSettings& OutMipGenSettings);
	
	/**
	 * Build Audio import options from JSON parameters.
	 * @param Params - JSON parameters
	 * @param OutFactory - Output configured USoundFactory
	 * @return true if options parsed successfully
	 */
	bool BuildAudioImportOptions(const TSharedPtr<FJsonObject>& Params, USoundFactory* OutFactory);
	
	/**
	 * Validate source file exists and is readable.
	 * @param SourcePath - Absolute disk path
	 * @param OutError - Error message on failure
	 * @return true if file is valid for import
	 */
	static bool ValidateSourceFile(const FString& SourcePath, FString& OutError);
	
	/**
	 * Validate destination package path.
	 * @param DestinationPath - Package path (e.g., "/Game/Imported")
	 * @param OutError - Error message on failure
	 * @return true if path is valid
	 */
	static bool ValidateDestinationPath(const FString& DestinationPath, FString& OutError);
	
	/**
	 * Convert package path to absolute disk path for validation.
	 * @param PackagePath - Package path
	 * @param OutDiskPath - Output absolute disk path
	 * @return true if conversion successful
	 */
	static bool PackagePathToDiskPath(const FString& PackagePath, FString& OutDiskPath);
	
	/**
	 * Mark packages dirty after import (no forced GC per UE5.7 safety guidelines).
	 * @param ImportedObjects - Objects that were imported
	 */
	static void MarkPackagesDirty(const TArray<UObject*>& ImportedObjects);

	/**
	 * Auto-generate placeholder materials for a static mesh and assign them to slots.
	 * @param StaticMesh - The mesh to generate materials for
	 * @param DestinationPath - Package path where materials will be created
	 * @return Number of materials created
	 */
	static int32 AutoGenerateMaterialsForMesh(UStaticMesh* StaticMesh, const FString& DestinationPath);

	/**
	 * Fallback texture export using IImageWrapper.
	 * Used when UExporter::RunAssetExportTask cannot find an appropriate exporter.
	 * Reads the texture source data (mip 0) and writes it via IImageWrapper.
	 * @param Texture - The UTexture2D to export
	 * @param OutputPath - Absolute disk path for the output file
	 * @param ExportFormat - File format extension (e.g., "png", "bmp", "exr")
	 * @return true if export was successful
	 */
	static bool ExportTextureViaImageUtils(UTexture2D* Texture, const FString& OutputPath, const FString& ExportFormat);
};
