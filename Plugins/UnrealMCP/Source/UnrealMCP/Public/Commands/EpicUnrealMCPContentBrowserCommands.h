#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Content Browser / Asset Management MCP commands
 * Handles folder operations, asset CRUD, search, and basic registry queries.
 */
class UNREALMCP_API FEpicUnrealMCPContentBrowserCommands
{
public:
	FEpicUnrealMCPContentBrowserCommands();

	// Handle content browser commands
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Folder operations
	TSharedPtr<FJsonObject> HandleCreateFolder(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteFolder(const TSharedPtr<FJsonObject>& Params);

	// Asset listing / search / resolve
	TSharedPtr<FJsonObject> HandleListAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSearchAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleResolveAssetPath(const TSharedPtr<FJsonObject>& Params);

	// Asset CRUD + load/unload/save
	TSharedPtr<FJsonObject> HandleMoveAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCopyAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleLoadAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleUnloadAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetMetadata(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetAssetMetadata(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleTagAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindRedirectors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFixupRedirectors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindUnusedAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetReferences(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetReferenceGraph(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAuditAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssetRegistrySearch(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBulkRename(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBulkMove(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBulkDelete(const TSharedPtr<FJsonObject>& Params);

	// Primary Asset Label
	TSharedPtr<FJsonObject> HandleCreatePrimaryAssetLabel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeletePrimaryAssetLabel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListPrimaryAssetLabels(const TSharedPtr<FJsonObject>& Params);

	// Asset Manager Settings
	TSharedPtr<FJsonObject> HandleGetAssetManagerSettings(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetAssetManagerSettings(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddPrimaryAssetBundle(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	static bool ResolveFolderDiskPath(const FString& InFolderPath, FString& OutDiskPath, FString& OutError);
	static bool GetAssetData(const FString& AssetPath, FAssetData& OutAssetData, FString& OutError);
	static FString PackagePathFromAssetPath(const FString& AssetPath);
	static FString AssetNameFromAssetPath(const FString& AssetPath);
};
