#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for VRoid / VRM avatar MCP commands.
 *
 * Prerequisites:
 * - VRM4U plugin must be installed and enabled for import operations.
 * - All handlers run on GameThread via AsyncTask from EpicUnrealMCPBridge.
 *
 * UE 5.7 Safety:
 * - Editor operations run on GameThread.
 * - Plugin absence produces actionable errors (no crashes).
 * - Asset import uses standard UAssetImportTask pipeline.
 */
class UNREALMCP_API FEpicUnrealMCPVroidCommands
{
public:
	FEpicUnrealMCPVroidCommands();

	// Handle VRM/avatar commands
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// ---------------------------------------------------------------------------
	// Plugin / Capability Detection
	// ---------------------------------------------------------------------------

	/** Check whether VRM4U (or another VRM importer plugin) is available. */
	TSharedPtr<FJsonObject> HandleVroidCheckPlugin(const TSharedPtr<FJsonObject>& Params);

	// ---------------------------------------------------------------------------
	// Import Handlers
	// ---------------------------------------------------------------------------

	/**
	 * Import a local .vrm file into the project.
	 * Uses the asset import task pipeline; VRM4U registers its factory
	 * automatically when the plugin is enabled.
	 */
	TSharedPtr<FJsonObject> HandleVroidImportVrm(const TSharedPtr<FJsonObject>& Params);

	// ---------------------------------------------------------------------------
	// Spawn / Placement Handlers
	// ---------------------------------------------------------------------------

	/**
	 * Spawn an imported VRM avatar into the editor world.
	 * Expects the SkeletalMesh asset path produced by vroid_import_vrm.
	 */
	TSharedPtr<FJsonObject> HandleVroidSpawnAvatar(const TSharedPtr<FJsonObject>& Params);

	// ---------------------------------------------------------------------------
	// Validation Handlers
	// ---------------------------------------------------------------------------

	/**
	 * Validate that an imported avatar asset has the expected sub-assets:
	 * SkeletalMesh, Materials, Textures, Skeleton, PhysicsAsset.
	 */
	TSharedPtr<FJsonObject> HandleVroidValidateAvatarAsset(const TSharedPtr<FJsonObject>& Params);

	// ---------------------------------------------------------------------------
	// Common Helpers
	// ---------------------------------------------------------------------------

	/** Check if any VRM importer plugin is loaded. */
	static bool IsVrmPluginAvailable();

	/** Resolve the name of the active VRM plugin (for diagnostics). */
	static FString GetVrmPluginName();

	/** Create a standard import task for a .vrm file. */
	UAssetImportTask* CreateVrmImportTask(
		const FString& SourcePath,
		const FString& DestinationPath,
		const FString& AssetName);

	/** Process the import task and return imported objects. */
	TArray<UObject*> ProcessVrmImportTask(UAssetImportTask* Task, FString& OutError);

	/** Find the primary SkeletalMesh from imported objects. */
	static USkeletalMesh* FindSkeletalMeshInImports(const TArray<UObject*>& ImportedObjects);

	/** Find an imported asset by class. */
	template<typename T>
	static T* FindImportedAsset(const TArray<UObject*>& ImportedObjects);
};
