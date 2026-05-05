#include "Commands/EpicUnrealMCPContentBrowserCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SavePackage.h"
#include "UObject/Linker.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectRedirector.h"
#include "FileHelpers.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "ObjectTools.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/PrimaryAssetLabel.h"

FEpicUnrealMCPContentBrowserCommands::FEpicUnrealMCPContentBrowserCommands() {}

// ---------------------------------------------------------------------------
// Command Router
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPContentBrowserCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("create_folder"), &FEpicUnrealMCPContentBrowserCommands::HandleCreateFolder},
        {TEXT("delete_folder"), &FEpicUnrealMCPContentBrowserCommands::HandleDeleteFolder},
        {TEXT("list_assets"), &FEpicUnrealMCPContentBrowserCommands::HandleListAssets},
        {TEXT("search_assets"), &FEpicUnrealMCPContentBrowserCommands::HandleSearchAssets},
        {TEXT("resolve_asset_path"), &FEpicUnrealMCPContentBrowserCommands::HandleResolveAssetPath},
        {TEXT("move_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleMoveAsset},
        {TEXT("copy_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleCopyAsset},
        {TEXT("duplicate_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleDuplicateAsset},
        {TEXT("rename_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleRenameAsset},
        {TEXT("delete_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleDeleteAsset},
        {TEXT("load_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleLoadAsset},
        {TEXT("unload_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleUnloadAsset},
        {TEXT("save_assets"), &FEpicUnrealMCPContentBrowserCommands::HandleSaveAssets},
        {TEXT("get_asset_metadata"), &FEpicUnrealMCPContentBrowserCommands::HandleGetAssetMetadata},
        {TEXT("set_asset_metadata"), &FEpicUnrealMCPContentBrowserCommands::HandleSetAssetMetadata},
        {TEXT("tag_asset"), &FEpicUnrealMCPContentBrowserCommands::HandleTagAsset},
        {TEXT("find_redirectors"), &FEpicUnrealMCPContentBrowserCommands::HandleFindRedirectors},
        {TEXT("fixup_redirectors"), &FEpicUnrealMCPContentBrowserCommands::HandleFixupRedirectors},
        {TEXT("find_unused_assets"), &FEpicUnrealMCPContentBrowserCommands::HandleFindUnusedAssets},
        {TEXT("get_asset_references"), &FEpicUnrealMCPContentBrowserCommands::HandleGetAssetReferences},
        {TEXT("get_asset_dependencies"), &FEpicUnrealMCPContentBrowserCommands::HandleGetAssetDependencies},
        {TEXT("get_asset_reference_graph"), &FEpicUnrealMCPContentBrowserCommands::HandleGetAssetReferenceGraph},
        {TEXT("audit_assets"), &FEpicUnrealMCPContentBrowserCommands::HandleAuditAssets},
        {TEXT("asset_registry_search"), &FEpicUnrealMCPContentBrowserCommands::HandleAssetRegistrySearch},
        {TEXT("bulk_rename"), &FEpicUnrealMCPContentBrowserCommands::HandleBulkRename},
        {TEXT("bulk_move"), &FEpicUnrealMCPContentBrowserCommands::HandleBulkMove},
        {TEXT("bulk_delete"), &FEpicUnrealMCPContentBrowserCommands::HandleBulkDelete},
        {TEXT("create_primary_asset_label"), &FEpicUnrealMCPContentBrowserCommands::HandleCreatePrimaryAssetLabel},
        {TEXT("delete_primary_asset_label"), &FEpicUnrealMCPContentBrowserCommands::HandleDeletePrimaryAssetLabel},
        {TEXT("list_primary_asset_labels"), &FEpicUnrealMCPContentBrowserCommands::HandleListPrimaryAssetLabels},
        {TEXT("get_asset_manager_settings"), &FEpicUnrealMCPContentBrowserCommands::HandleGetAssetManagerSettings},
        {TEXT("set_asset_manager_settings"), &FEpicUnrealMCPContentBrowserCommands::HandleSetAssetManagerSettings},
        {TEXT("add_primary_asset_bundle"), &FEpicUnrealMCPContentBrowserCommands::HandleAddPrimaryAssetBundle},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown content browser command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static IAssetRegistry& GetAssetRegistry()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    return AssetRegistryModule.Get();
}

static IAssetTools& GetAssetTools()
{
    return FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
}

static FString NormalizeContentPackagePath(const FString& InPath)
{
    FString PackagePath = InPath;
    FPaths::NormalizeDirectoryName(PackagePath);

    if (PackagePath.Equals(TEXT("/Content"), ESearchCase::IgnoreCase))
    {
        return TEXT("/Game");
    }
    if (PackagePath.StartsWith(TEXT("/Content/")))
    {
        return TEXT("/Game") + PackagePath.RightChop(8);
    }
    return PackagePath;
}

static FString MakeObjectPath(const FString& PackageName, const FString& AssetName)
{
    return PackageName + TEXT(".") + AssetName;
}

static FString MakeObjectPath(const FString& PackageName)
{
    return MakeObjectPath(PackageName, FPaths::GetBaseFilename(PackageName));
}

static FString FolderPathFromPackageName(const FString& PackageName)
{
    FString FolderPath = FPaths::GetPath(PackageName);
    return FolderPath.IsEmpty() ? TEXT("/Game") : FolderPath;
}

static FString PackageNameFromAnyAssetPath(const FString& AssetPath)
{
    const FSoftObjectPath SoftPath(AssetPath);
    const FString LongPackageName = SoftPath.GetLongPackageName();
    if (!LongPackageName.IsEmpty())
    {
        return LongPackageName;
    }

    int32 DotIndex = INDEX_NONE;
    if (AssetPath.FindChar(TEXT('.'), DotIndex))
    {
        return AssetPath.Left(DotIndex);
    }
    return NormalizeContentPackagePath(AssetPath);
}

static FString AssetNameFromAnyAssetPath(const FString& AssetPath)
{
    const FSoftObjectPath SoftPath(AssetPath);
    const FString AssetName = SoftPath.GetAssetName();
    if (!AssetName.IsEmpty())
    {
        return AssetName;
    }

    int32 DotIndex = INDEX_NONE;
    if (AssetPath.FindChar(TEXT('.'), DotIndex))
    {
        return AssetPath.RightChop(DotIndex + 1);
    }
    return FPaths::GetBaseFilename(AssetPath);
}

static void SplitDestinationPath(const FString& InDestPath, const FString& FallbackAssetName, FString& OutFolderPath, FString& OutAssetName)
{
    const FString DestPath = NormalizeContentPackagePath(InDestPath);
    if (GetAssetRegistry().PathExists(DestPath))
    {
        OutFolderPath = DestPath;
        OutAssetName = FallbackAssetName;
        return;
    }

    const FString PackageName = PackageNameFromAnyAssetPath(DestPath);
    OutFolderPath = FolderPathFromPackageName(PackageName);
    OutAssetName = AssetNameFromAnyAssetPath(DestPath);
    if (OutAssetName.IsEmpty())
    {
        OutAssetName = FallbackAssetName;
    }
}

static TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Strings)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FString& Value : Strings)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

static TArray<TSharedPtr<FJsonValue>> NameArrayToJson(const TArray<FName>& Names)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FName& Value : Names)
    {
        Result.Add(MakeShared<FJsonValueString>(Value.ToString()));
    }
    return Result;
}

static bool TryGetStringArrayField(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, TArray<FString>& OutValues)
{
    OutValues.Reset();

    const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
    if (!Params->TryGetArrayField(FieldName, JsonArray))
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
    {
        const FString StringValue = Value.IsValid() ? Value->AsString() : FString();
        if (!StringValue.IsEmpty())
        {
            OutValues.Add(StringValue);
        }
    }
    return true;
}

static TSharedPtr<FJsonObject> AssetDataToJson(const FAssetData& AssetData, bool bIncludeTags = false)
{
    TSharedPtr<FJsonObject> AssetJson = MakeShared<FJsonObject>();
    AssetJson->SetStringField(TEXT("asset_path"), AssetData.GetSoftObjectPath().ToString());
    AssetJson->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
    AssetJson->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
    AssetJson->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
    AssetJson->SetStringField(TEXT("asset_class"), AssetData.AssetClassPath.ToString());
    AssetJson->SetBoolField(TEXT("is_redirector"), FAssetData::IsRedirectorClassName(AssetData.AssetClassPath));

    if (bIncludeTags)
    {
        TSharedPtr<FJsonObject> TagsJson = MakeShared<FJsonObject>();
        for (const auto Pair : AssetData.TagsAndValues)
        {
            TagsJson->SetStringField(Pair.Key.ToString(), Pair.Value.AsString());
        }
        AssetJson->SetObjectField(TEXT("tags"), TagsJson);
    }

    return AssetJson;
}

bool FEpicUnrealMCPContentBrowserCommands::ResolveFolderDiskPath(const FString& InFolderPath, FString& OutDiskPath, FString& OutError)
{
    FString NormalizedPath = InFolderPath;
    FPaths::NormalizeDirectoryName(NormalizedPath);

    FString PackagePath;
    if (NormalizedPath.StartsWith(TEXT("/Game/")) || NormalizedPath.StartsWith(TEXT("/Game")))
    {
        PackagePath = NormalizedPath;
    }
    else if (NormalizedPath.StartsWith(TEXT("/Content/")))
    {
        PackagePath = TEXT("/Game") + NormalizedPath.RightChop(8);
    }
    else
    {
        OutError = TEXT("Folder path must start with /Game/ or /Content/");
        return false;
    }

    if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath + TEXT("/"), OutDiskPath))
    {
        OutError = FString::Printf(TEXT("Failed to convert package path '%s' to disk path"), *PackagePath);
        return false;
    }
    return true;
}

bool FEpicUnrealMCPContentBrowserCommands::GetAssetData(const FString& AssetPath, FAssetData& OutAssetData, FString& OutError)
{
    const FString NormalizedPath = NormalizeContentPackagePath(AssetPath);
    const FString ObjectPath = NormalizedPath.Contains(TEXT("."))
        ? NormalizedPath
        : MakeObjectPath(NormalizedPath);

    if (NormalizedPath.Contains(TEXT(".")))
    {
        // Already full soft object path (e.g. /Game/Foo.Bar)
        FSoftObjectPath SoftPath(NormalizedPath);
        OutAssetData = GetAssetRegistry().GetAssetByObjectPath(SoftPath);
    }
    else
    {
        // Treat as package path: try appending default class name if missing
        TArray<FAssetData> AssetsByPackage;
        GetAssetRegistry().GetAssetsByPackageName(FName(*NormalizedPath), AssetsByPackage);
        if (AssetsByPackage.Num() > 0)
        {
            OutAssetData = AssetsByPackage[0];
        }
        if (!OutAssetData.IsValid())
        {
            // Some assets are stored with package path + asset name separated by dot
            FSoftObjectPath SoftPath(ObjectPath);
            OutAssetData = GetAssetRegistry().GetAssetByObjectPath(SoftPath);
        }
    }

    if (!OutAssetData.IsValid())
    {
        OutError = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
        return false;
    }
    return true;
}

FString FEpicUnrealMCPContentBrowserCommands::PackagePathFromAssetPath(const FString& AssetPath)
{
    return PackageNameFromAnyAssetPath(AssetPath);
}

FString FEpicUnrealMCPContentBrowserCommands::AssetNameFromAssetPath(const FString& AssetPath)
{
    return AssetNameFromAnyAssetPath(AssetPath);
}

// ---------------------------------------------------------------------------
// Folder Operations
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleCreateFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("folder_path is required"));
    }

    FString DiskPath, Error;
    if (!ResolveFolderDiskPath(FolderPath, DiskPath, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (PlatformFile.DirectoryExists(*DiskPath))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("folder_path"), FolderPath);
        Result->SetStringField(TEXT("message"), TEXT("Folder already exists"));
        return Result;
    }

    if (!PlatformFile.CreateDirectoryTree(*DiskPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create folder: %s"), *DiskPath));
    }

    // Register path with AssetRegistry so Content Browser sees it immediately
    FString PackagePath = FolderPath;
    FPaths::NormalizeDirectoryName(PackagePath);
    if (PackagePath.StartsWith(TEXT("/Content/")))
    {
        PackagePath = TEXT("/Game") + PackagePath.RightChop(8);
    }
    GetAssetRegistry().AddPath(PackagePath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("folder_path"), FolderPath);
    Result->SetStringField(TEXT("disk_path"), DiskPath);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleDeleteFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("folder_path is required"));
    }

    FString DiskPath, Error;
    if (!ResolveFolderDiskPath(FolderPath, DiskPath, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    // Check if folder is empty via AssetRegistry
    FString PackagePath = FolderPath;
    FPaths::NormalizeDirectoryName(PackagePath);
    if (PackagePath.StartsWith(TEXT("/Content/")))
    {
        PackagePath = TEXT("/Game") + PackagePath.RightChop(8);
    }

    TArray<FAssetData> AssetsInFolder;
    GetAssetRegistry().GetAssetsByPath(FName(*PackagePath), AssetsInFolder, true);
    if (AssetsInFolder.Num() > 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Folder '%s' is not empty. Contains %d asset(s)."), *FolderPath, AssetsInFolder.Num()));
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*DiskPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Folder does not exist: %s"), *FolderPath));
    }

    if (!PlatformFile.DeleteDirectory(*DiskPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to delete folder: %s"), *DiskPath));
    }

    GetAssetRegistry().RemovePath(PackagePath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("folder_path"), FolderPath);
    return Result;
}

// ---------------------------------------------------------------------------
// Asset Listing / Search / Resolve
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleListAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("folder_path is required"));
    }

    FString PackagePath = FolderPath;
    FPaths::NormalizeDirectoryName(PackagePath);
    if (PackagePath.StartsWith(TEXT("/Content/")))
    {
        PackagePath = TEXT("/Game") + PackagePath.RightChop(8);
    }

    bool bRecursive = false;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FString ClassFilter;
    Params->TryGetStringField(TEXT("class_name"), ClassFilter);

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*PackagePath));
    Filter.bRecursivePaths = bRecursive;

    if (!ClassFilter.IsEmpty())
    {
        // Convert common shorthand to full path where possible
        FString NormalizedClass = ClassFilter;
        if (!NormalizedClass.Contains(TEXT("/")))
        {
            NormalizedClass = TEXT("/Script/Engine.") + NormalizedClass;
        }
        FTopLevelAssetPath ClassPath(*NormalizedClass);
        if (ClassPath.IsValid())
        {
            Filter.ClassPaths.Add(ClassPath);
        }
    }

    TArray<FAssetData> AssetDataArray;
    GetAssetRegistry().GetAssets(Filter, AssetDataArray);

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    for (const FAssetData& AssetData : AssetDataArray)
    {
        AssetArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData)));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), AssetArray);
    Result->SetNumberField(TEXT("count"), AssetArray.Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleSearchAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Query;
    Params->TryGetStringField(TEXT("query"), Query);

    FString ClassFilter;
    Params->TryGetStringField(TEXT("class_name"), ClassFilter);

    FString FolderPath;
    Params->TryGetStringField(TEXT("folder_path"), FolderPath);

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FARFilter Filter;
    if (!FolderPath.IsEmpty())
    {
        FString PackagePath = FolderPath;
        FPaths::NormalizeDirectoryName(PackagePath);
        if (PackagePath.StartsWith(TEXT("/Content/")))
        {
            PackagePath = TEXT("/Game") + PackagePath.RightChop(8);
        }
        Filter.PackagePaths.Add(FName(*PackagePath));
        Filter.bRecursivePaths = bRecursive;
    }

    if (!ClassFilter.IsEmpty())
    {
        FString NormalizedClass = ClassFilter;
        if (!NormalizedClass.Contains(TEXT("/")))
        {
            NormalizedClass = TEXT("/Script/Engine.") + NormalizedClass;
        }
        FTopLevelAssetPath ClassPath(*NormalizedClass);
        if (ClassPath.IsValid())
        {
            Filter.ClassPaths.Add(ClassPath);
        }
    }

    TArray<FAssetData> AssetDataArray;
    GetAssetRegistry().GetAssets(Filter, AssetDataArray);

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    for (const FAssetData& AssetData : AssetDataArray)
    {
        FString AssetPathStr = AssetData.GetSoftObjectPath().ToString();
        FString AssetNameStr = AssetData.AssetName.ToString();
        if (!Query.IsEmpty())
        {
            if (!AssetPathStr.Contains(Query, ESearchCase::IgnoreCase) &&
                !AssetNameStr.Contains(Query, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        AssetArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData)));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), AssetArray);
    Result->SetNumberField(TEXT("count"), AssetArray.Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleResolveAssetPath(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    const bool bExists = GetAssetData(AssetPath, AssetData, Error);
    const FString PackageName = PackagePathFromAssetPath(AssetPath);
    const FString AssetName = AssetNameFromAssetPath(AssetPath);
    const FString ResolvedPath = bExists
        ? AssetData.GetSoftObjectPath().ToString()
        : MakeObjectPath(PackageName, AssetName);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("input"), AssetPath);
    Result->SetStringField(TEXT("resolved_path"), ResolvedPath);
    Result->SetBoolField(TEXT("exists"), bExists);
    if (bExists)
    {
        Result->SetObjectField(TEXT("asset"), AssetDataToJson(AssetData));
    }
    return Result;
}

// ---------------------------------------------------------------------------
// Asset CRUD
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleMoveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath, DestPath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) ||
        !Params->TryGetStringField(TEXT("dest_path"), DestPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("source_path and dest_path are required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(SourcePath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to load asset for move"));
    }

    FString NewFolderPath;
    FString NewAssetName;
    SplitDestinationPath(DestPath, AssetData.AssetName.ToString(), NewFolderPath, NewAssetName);

    TArray<FAssetRenameData> RenameList;
    RenameList.Add(FAssetRenameData(TWeakObjectPtr<UObject>(AssetObject), NewFolderPath, NewAssetName));

    if (!GetAssetTools().RenameAssets(RenameList))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to move asset"));
    }

    const FString NewPackageName = NewFolderPath / NewAssetName;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("source_path"), SourcePath);
    Result->SetStringField(TEXT("dest_path"), NewPackageName);
    Result->SetStringField(TEXT("asset_path"), MakeObjectPath(NewPackageName, NewAssetName));
    Result->SetStringField(TEXT("message"), TEXT("Asset move queued. Redirector may be created."));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleCopyAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath, DestPath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) ||
        !Params->TryGetStringField(TEXT("dest_path"), DestPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("source_path and dest_path are required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(SourcePath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to load asset for copy"));
    }

    FString NewFolderPath;
    FString NewAssetName;
    SplitDestinationPath(DestPath, AssetData.AssetName.ToString(), NewFolderPath, NewAssetName);

    UObject* NewAsset = GetAssetTools().DuplicateAsset(NewAssetName, NewFolderPath, AssetObject);
    if (!NewAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to copy asset"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("source_path"), SourcePath);
    Result->SetStringField(TEXT("dest_path"), NewAsset->GetPathName());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FString NewName;
    if (!Params->TryGetStringField(TEXT("new_name"), NewName))
    {
        // Default to AssetName_Copy
        NewName = AssetNameFromAssetPath(AssetPath) + TEXT("_Copy");
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to load asset for duplication"));
    }

    const FString SourceFolderPath = AssetData.PackagePath.ToString();
    UObject* NewAsset = GetAssetTools().DuplicateAsset(NewName, SourceFolderPath, AssetObject);

    if (!NewAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to duplicate asset"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("source_path"), AssetPath);
    Result->SetStringField(TEXT("new_path"), NewAsset->GetPathName());
    Result->SetStringField(TEXT("new_name"), NewName);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FString NewName;
    if (!Params->TryGetStringField(TEXT("new_name"), NewName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("new_name is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to load asset for rename"));
    }

    const FString CurrentFolderPath = AssetData.PackagePath.ToString();

    TArray<FAssetRenameData> RenameList;
    RenameList.Add(FAssetRenameData(TWeakObjectPtr<UObject>(AssetObject), CurrentFolderPath, NewName));

    if (!GetAssetTools().RenameAssets(RenameList))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to rename asset"));
    }

    const FString NewPackageName = CurrentFolderPath / NewName;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("old_path"), AssetPath);
    Result->SetStringField(TEXT("new_path"), NewPackageName);
    Result->SetStringField(TEXT("asset_path"), MakeObjectPath(NewPackageName, NewName));
    Result->SetStringField(TEXT("new_name"), NewName);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    const FString PackagePathToDelete = AssetData.PackageName.ToString();
    
    // Ensure all pending render commands (like thumbnail generation or texture updates) are finished before deleting
    FAssetCompilingManager::Get().FinishAllCompilation();
    FlushRenderingCommands();
    
    if (!UEditorAssetLibrary::DeleteAsset(PackagePathToDelete))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to delete asset"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("message"), TEXT("Asset deleted"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleLoadAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FSoftObjectPath SoftPath(AssetPath);
    UObject* LoadedAsset = SoftPath.TryLoad();
    if (!LoadedAsset)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("class"), LoadedAsset->GetClass()->GetName());
    Result->SetStringField(TEXT("message"), TEXT("Asset loaded into memory"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleUnloadAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FString PackageName = PackagePathFromAssetPath(AssetPath);
    UPackage* Package = FindPackage(nullptr, *PackageName);
    if (!Package)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("asset_path"), AssetPath);
        Result->SetStringField(TEXT("message"), TEXT("Package was not loaded; nothing to unload"));
        return Result;
    }

    // Safely mark package for GC instead of forcing immediate unload
    FlushRenderingCommands();
    Package->ClearFlags(RF_Standalone);
    Package->RemoveFromRoot();
    // Do NOT force CollectGarbage here — immediate GC can destroy objects
    // still referenced by the editor UI or other subsystems and crash UE.

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("package_name"), PackageName);
    Result->SetStringField(TEXT("message"), TEXT("Package marked for unload; will be released at next GC"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleSaveAssets(const TSharedPtr<FJsonObject>& Params)
{
    const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("asset_paths"), PathsArray))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_paths array is required"));
    }

    TArray<FString> SavedAssets;
    TArray<FString> FailedAssets;

    for (const TSharedPtr<FJsonValue>& Value : *PathsArray)
    {
        FString AssetPath = Value->AsString();
        if (AssetPath.IsEmpty())
        {
            continue;
        }

        FString PackageName = PackagePathFromAssetPath(AssetPath);
        UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
        if (!Package)
        {
            FailedAssets.Add(AssetPath + TEXT(" (load failed)"));
            continue;
        }

        FString FileName;
        if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, FileName, FPackageName::GetAssetPackageExtension()))
        {
            FailedAssets.Add(AssetPath + TEXT(" (path conversion failed)"));
            continue;
        }

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.Error = nullptr;
        bool bSaved = UPackage::SavePackage(Package, nullptr, *FileName, SaveArgs);

        if (bSaved)
        {
            SavedAssets.Add(AssetPath);
        }
        else
        {
            FailedAssets.Add(AssetPath + TEXT(" (save failed)"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), FailedAssets.Num() == 0);
    {
        TArray<TSharedPtr<FJsonValue>> SavedJson;
        for (const FString& S : SavedAssets) SavedJson.Add(MakeShared<FJsonValueString>(S));
        Result->SetArrayField(TEXT("saved"), SavedJson);
    }
    {
        TArray<TSharedPtr<FJsonValue>> FailedJson;
        for (const FString& S : FailedAssets) FailedJson.Add(MakeShared<FJsonValueString>(S));
        Result->SetArrayField(TEXT("failed"), FailedJson);
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleGetAssetMetadata(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField(TEXT("asset"), AssetDataToJson(AssetData, true));

    UObject* AssetObject = AssetData.GetAsset();
    if (AssetObject)
    {
        TSharedPtr<FJsonObject> MetadataJson = MakeShared<FJsonObject>();
        if (const TMap<FName, FString>* MetadataMap = FMetaData::GetMapForObject(AssetObject))
        {
            for (const TPair<FName, FString>& Pair : *MetadataMap)
            {
                MetadataJson->SetStringField(Pair.Key.ToString(), Pair.Value);
            }
        }
        Result->SetObjectField(TEXT("metadata"), MetadataJson);
    }

    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleSetAssetMetadata(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    FString Key;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) ||
        !Params->TryGetStringField(TEXT("key"), Key))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path and key are required"));
    }

    FString Value;
    const bool bHasValue = Params->TryGetStringField(TEXT("value"), Value);
    bool bRemove = false;
    Params->TryGetBoolField(TEXT("remove"), bRemove);

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject || !AssetObject->GetOutermost())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to load asset for metadata edit"));
    }

    FMetaData& MetaData = AssetObject->GetOutermost()->GetMetaData();
    if (bRemove)
    {
        MetaData.RemoveValue(AssetObject, *Key);
    }
    else
    {
        if (!bHasValue)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("value is required unless remove=true"));
        }
        MetaData.SetValue(AssetObject, *Key, *Value);
    }

    AssetObject->GetOutermost()->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("key"), Key);
    Result->SetBoolField(TEXT("removed"), bRemove);
    if (!bRemove)
    {
        Result->SetStringField(TEXT("value"), Value);
    }
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleTagAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    FString Key;
    FString Value;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) ||
        !Params->TryGetStringField(TEXT("tag_key"), Key))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path and tag_key are required"));
    }
    Params->TryGetStringField(TEXT("tag_value"), Value);

    TSharedPtr<FJsonObject> ForwardedParams = MakeShared<FJsonObject>();
    ForwardedParams->SetStringField(TEXT("asset_path"), AssetPath);
    ForwardedParams->SetStringField(TEXT("key"), Key);
    ForwardedParams->SetStringField(TEXT("value"), Value);
    return HandleSetAssetMetadata(ForwardedParams);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleFindRedirectors(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    Params->TryGetStringField(TEXT("folder_path"), FolderPath);

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FARFilter Filter;
    if (!FolderPath.IsEmpty())
    {
        Filter.PackagePaths.Add(FName(*NormalizeContentPackagePath(FolderPath)));
        Filter.bRecursivePaths = bRecursive;
    }

    TArray<FAssetData> AssetDataArray;
    GetAssetRegistry().GetAssets(Filter, AssetDataArray);

    TArray<TSharedPtr<FJsonValue>> Redirectors;
    for (const FAssetData& AssetData : AssetDataArray)
    {
        if (FAssetData::IsRedirectorClassName(AssetData.AssetClassPath))
        {
            Redirectors.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("redirectors"), Redirectors);
    Result->SetNumberField(TEXT("count"), Redirectors.Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleFixupRedirectors(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> AssetPaths;
    TryGetStringArrayField(Params, TEXT("asset_paths"), AssetPaths);

    TArray<FAssetData> RedirectorData;
    if (AssetPaths.Num() > 0)
    {
        for (const FString& AssetPath : AssetPaths)
        {
            FAssetData AssetData;
            FString Error;
            if (GetAssetData(AssetPath, AssetData, Error) && FAssetData::IsRedirectorClassName(AssetData.AssetClassPath))
            {
                RedirectorData.Add(AssetData);
            }
        }
    }
    else
    {
        TSharedPtr<FJsonObject> FindResult = HandleFindRedirectors(Params);
        const TArray<TSharedPtr<FJsonValue>>* RedirectorArray = nullptr;
        if (FindResult->TryGetArrayField(TEXT("redirectors"), RedirectorArray))
        {
            for (const TSharedPtr<FJsonValue>& RedirectorValue : *RedirectorArray)
            {
                const TSharedPtr<FJsonObject>* RedirectorObject = nullptr;
                if (RedirectorValue->TryGetObject(RedirectorObject))
                {
                    FString RedirectorPath;
                    if ((*RedirectorObject)->TryGetStringField(TEXT("asset_path"), RedirectorPath))
                    {
                        FAssetData AssetData;
                        FString Error;
                        if (GetAssetData(RedirectorPath, AssetData, Error))
                        {
                            RedirectorData.Add(AssetData);
                        }
                    }
                }
            }
        }
    }

    TArray<UObjectRedirector*> Redirectors;
    TArray<FString> FixedPaths;
    for (const FAssetData& AssetData : RedirectorData)
    {
        UObjectRedirector* Redirector = Cast<UObjectRedirector>(AssetData.GetAsset());
        if (Redirector)
        {
            Redirectors.Add(Redirector);
            FixedPaths.Add(AssetData.GetSoftObjectPath().ToString());
        }
    }

    if (Redirectors.Num() > 0)
    {
        GetAssetTools().FixupReferencers(Redirectors, false, ERedirectFixupMode::DeleteFixedUpRedirectors);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("count"), Redirectors.Num());
    Result->SetArrayField(TEXT("redirectors"), StringArrayToJson(FixedPaths));
    Result->SetBoolField(TEXT("fixup_in_progress"), GetAssetTools().IsFixupReferencersInProgress());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleFindUnusedAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath))
    {
        FolderPath = TEXT("/Game");
    }

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*NormalizeContentPackagePath(FolderPath)));
    Filter.bRecursivePaths = bRecursive;

    TArray<FAssetData> AssetDataArray;
    GetAssetRegistry().GetAssets(Filter, AssetDataArray);

    TArray<TSharedPtr<FJsonValue>> UnusedAssets;
    for (const FAssetData& AssetData : AssetDataArray)
    {
        if (FAssetData::IsRedirectorClassName(AssetData.AssetClassPath))
        {
            continue;
        }

        TArray<FName> Referencers;
        GetAssetRegistry().GetReferencers(AssetData.PackageName, Referencers);
        if (Referencers.Num() == 0)
        {
            UnusedAssets.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), UnusedAssets);
    Result->SetNumberField(TEXT("count"), UnusedAssets.Num());
    Result->SetStringField(TEXT("note"), TEXT("Uses on-disk Asset Registry referencers; unsaved in-memory references may not be reflected."));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleGetAssetReferences(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TArray<FName> Referencers;
    GetAssetRegistry().GetReferencers(AssetData.PackageName, Referencers);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetData.GetSoftObjectPath().ToString());
    Result->SetArrayField(TEXT("referencers"), NameArrayToJson(Referencers));
    Result->SetNumberField(TEXT("count"), Referencers.Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    TArray<FName> Dependencies;
    GetAssetRegistry().GetDependencies(AssetData.PackageName, Dependencies);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetData.GetSoftObjectPath().ToString());
    Result->SetArrayField(TEXT("dependencies"), NameArrayToJson(Dependencies));
    Result->SetNumberField(TEXT("count"), Dependencies.Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleGetAssetReferenceGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    int32 MaxDepth = 1;
    Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);
    MaxDepth = FMath::Clamp(MaxDepth, 1, 5);

    TSet<FName> Visited;
    TArray<TSharedPtr<FJsonValue>> Nodes;
    TArray<TSharedPtr<FJsonValue>> Edges;

    TFunction<void(FName, int32)> Visit = [&](FName PackageName, int32 Depth)
    {
        if (Visited.Contains(PackageName) || Depth > MaxDepth)
        {
            return;
        }
        Visited.Add(PackageName);

        TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
        Node->SetStringField(TEXT("package_name"), PackageName.ToString());
        Node->SetNumberField(TEXT("depth"), Depth);
        Nodes.Add(MakeShared<FJsonValueObject>(Node));

        TArray<FName> Dependencies;
        GetAssetRegistry().GetDependencies(PackageName, Dependencies);
        for (const FName& Dependency : Dependencies)
        {
            TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
            Edge->SetStringField(TEXT("from"), PackageName.ToString());
            Edge->SetStringField(TEXT("to"), Dependency.ToString());
            Edge->SetStringField(TEXT("type"), TEXT("dependency"));
            Edges.Add(MakeShared<FJsonValueObject>(Edge));
            Visit(Dependency, Depth + 1);
        }

        TArray<FName> Referencers;
        GetAssetRegistry().GetReferencers(PackageName, Referencers);
        for (const FName& Referencer : Referencers)
        {
            TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
            Edge->SetStringField(TEXT("from"), Referencer.ToString());
            Edge->SetStringField(TEXT("to"), PackageName.ToString());
            Edge->SetStringField(TEXT("type"), TEXT("referencer"));
            Edges.Add(MakeShared<FJsonValueObject>(Edge));
            Visit(Referencer, Depth + 1);
        }
    };

    Visit(AssetData.PackageName, 0);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("root"), AssetData.PackageName.ToString());
    Result->SetArrayField(TEXT("nodes"), Nodes);
    Result->SetArrayField(TEXT("edges"), Edges);
    Result->SetNumberField(TEXT("node_count"), Nodes.Num());
    Result->SetNumberField(TEXT("edge_count"), Edges.Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleAuditAssets(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> AssetPaths;
    TryGetStringArrayField(Params, TEXT("asset_paths"), AssetPaths);

    TArray<FAssetData> AssetsToAudit;
    if (AssetPaths.Num() > 0)
    {
        for (const FString& AssetPath : AssetPaths)
        {
            FAssetData AssetData;
            FString Error;
            if (GetAssetData(AssetPath, AssetData, Error))
            {
                AssetsToAudit.Add(AssetData);
            }
        }
    }
    else
    {
        FString FolderPath;
        Params->TryGetStringField(TEXT("folder_path"), FolderPath);
        if (FolderPath.IsEmpty())
        {
            FolderPath = TEXT("/Game");
        }
        bool bRecursive = true;
        Params->TryGetBoolField(TEXT("recursive"), bRecursive);

        FARFilter Filter;
        Filter.PackagePaths.Add(FName(*NormalizeContentPackagePath(FolderPath)));
        Filter.bRecursivePaths = bRecursive;
        GetAssetRegistry().GetAssets(Filter, AssetsToAudit);
    }

    TArray<TSharedPtr<FJsonValue>> AuditRows;
    int64 TotalDiskSize = 0;
    for (const FAssetData& AssetData : AssetsToAudit)
    {
        TSharedPtr<FJsonObject> Row = AssetDataToJson(AssetData, true);

        TArray<FName> Dependencies;
        TArray<FName> Referencers;
        GetAssetRegistry().GetDependencies(AssetData.PackageName, Dependencies);
        GetAssetRegistry().GetReferencers(AssetData.PackageName, Referencers);

        Row->SetNumberField(TEXT("dependency_count"), Dependencies.Num());
        Row->SetNumberField(TEXT("referencer_count"), Referencers.Num());
        Row->SetNumberField(TEXT("tag_count"), AssetData.TagsAndValues.Num());

        TOptional<FAssetPackageData> PackageData = GetAssetRegistry().GetAssetPackageDataCopy(AssetData.PackageName);
        if (PackageData.IsSet())
        {
            Row->SetNumberField(TEXT("disk_size"), static_cast<double>(PackageData->DiskSize));
            Row->SetBoolField(TEXT("read_only"), PackageData->IsReadOnly());
            Row->SetBoolField(TEXT("has_virtualized_payloads"), PackageData->HasVirtualizedPayloads());
            TotalDiskSize += PackageData->DiskSize;
        }

        AuditRows.Add(MakeShared<FJsonValueObject>(Row));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), AuditRows);
    Result->SetNumberField(TEXT("count"), AuditRows.Num());
    Result->SetNumberField(TEXT("total_disk_size"), static_cast<double>(TotalDiskSize));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleAssetRegistrySearch(const TSharedPtr<FJsonObject>& Params)
{
    FString Query;
    FString TagKey;
    FString TagValue;
    Params->TryGetStringField(TEXT("query"), Query);
    Params->TryGetStringField(TEXT("tag_key"), TagKey);
    Params->TryGetStringField(TEXT("tag_value"), TagValue);

    TSharedPtr<FJsonObject> SearchParams = MakeShared<FJsonObject>();
    FString FolderPath;
    FString ClassName;
    bool bRecursive = true;
    if (Params->TryGetStringField(TEXT("folder_path"), FolderPath)) SearchParams->SetStringField(TEXT("folder_path"), FolderPath);
    if (Params->TryGetStringField(TEXT("class_name"), ClassName)) SearchParams->SetStringField(TEXT("class_name"), ClassName);
    if (Params->TryGetBoolField(TEXT("recursive"), bRecursive)) SearchParams->SetBoolField(TEXT("recursive"), bRecursive);
    SearchParams->SetStringField(TEXT("query"), Query);

    TSharedPtr<FJsonObject> SearchResult = HandleSearchAssets(SearchParams);
    const TArray<TSharedPtr<FJsonValue>>* AssetsArray = nullptr;
    if (!SearchResult->TryGetArrayField(TEXT("assets"), AssetsArray))
    {
        return SearchResult;
    }

    TArray<TSharedPtr<FJsonValue>> FilteredAssets;
    for (const TSharedPtr<FJsonValue>& AssetValue : *AssetsArray)
    {
        const TSharedPtr<FJsonObject>* AssetObject = nullptr;
        if (!AssetValue->TryGetObject(AssetObject))
        {
            continue;
        }

        FString FoundAssetPath;
        if (!(*AssetObject)->TryGetStringField(TEXT("asset_path"), FoundAssetPath))
        {
            continue;
        }

        FAssetData AssetData;
        FString Error;
        if (!GetAssetData(FoundAssetPath, AssetData, Error))
        {
            continue;
        }

        if (!TagKey.IsEmpty())
        {
            const FName TagName(*TagKey);
            if (TagValue.IsEmpty())
            {
                if (!AssetData.TagsAndValues.Contains(TagName))
                {
                    continue;
                }
            }
            else if (!AssetData.TagsAndValues.ContainsKeyValue(TagName, TagValue))
            {
                continue;
            }
        }

        FilteredAssets.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AssetData, true)));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), FilteredAssets);
    Result->SetNumberField(TEXT("count"), FilteredAssets.Num());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleBulkRename(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> AssetPaths;
    if (!TryGetStringArrayField(Params, TEXT("asset_paths"), AssetPaths) || AssetPaths.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_paths array is required"));
    }

    TArray<FString> NewNames;
    TryGetStringArrayField(Params, TEXT("new_names"), NewNames);

    FString SearchText;
    FString ReplaceText;
    FString Prefix;
    FString Suffix;
    Params->TryGetStringField(TEXT("search_text"), SearchText);
    Params->TryGetStringField(TEXT("replace_text"), ReplaceText);
    Params->TryGetStringField(TEXT("prefix"), Prefix);
    Params->TryGetStringField(TEXT("suffix"), Suffix);

    TArray<FAssetRenameData> RenameList;
    TArray<FString> RenamedTargets;
    TArray<FString> Failed;

    for (int32 Index = 0; Index < AssetPaths.Num(); ++Index)
    {
        FAssetData AssetData;
        FString Error;
        if (!GetAssetData(AssetPaths[Index], AssetData, Error))
        {
            Failed.Add(AssetPaths[Index] + TEXT(" (") + Error + TEXT(")"));
            continue;
        }

        UObject* AssetObject = AssetData.GetAsset();
        if (!AssetObject)
        {
            Failed.Add(AssetPaths[Index] + TEXT(" (load failed)"));
            continue;
        }

        FString NewName = NewNames.IsValidIndex(Index) ? NewNames[Index] : AssetData.AssetName.ToString();
        if (!SearchText.IsEmpty())
        {
            NewName = NewName.Replace(*SearchText, *ReplaceText);
        }
        NewName = Prefix + NewName + Suffix;

        RenameList.Add(FAssetRenameData(TWeakObjectPtr<UObject>(AssetObject), AssetData.PackagePath.ToString(), NewName));
        RenamedTargets.Add(AssetData.PackagePath.ToString() / NewName);
    }

    if (RenameList.Num() > 0 && !GetAssetTools().RenameAssets(RenameList))
    {
        Failed.Add(TEXT("RenameAssets failed for one or more assets"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("requested"), AssetPaths.Num());
    Result->SetNumberField(TEXT("renamed"), RenameList.Num());
    Result->SetArrayField(TEXT("targets"), StringArrayToJson(RenamedTargets));
    Result->SetArrayField(TEXT("failed"), StringArrayToJson(Failed));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleBulkMove(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> AssetPaths;
    FString DestPath;
    if (!TryGetStringArrayField(Params, TEXT("asset_paths"), AssetPaths) ||
        !Params->TryGetStringField(TEXT("dest_path"), DestPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_paths and dest_path are required"));
    }

    FString DiskPath;
    FString Error;
    const FString DestinationFolder = NormalizeContentPackagePath(DestPath);
    if (ResolveFolderDiskPath(DestinationFolder, DiskPath, Error))
    {
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*DiskPath);
        GetAssetRegistry().AddPath(DestinationFolder);
    }

    TArray<FAssetRenameData> RenameList;
    TArray<FString> MovedTargets;
    TArray<FString> Failed;

    for (const FString& AssetPath : AssetPaths)
    {
        FAssetData AssetData;
        if (!GetAssetData(AssetPath, AssetData, Error))
        {
            Failed.Add(AssetPath + TEXT(" (") + Error + TEXT(")"));
            continue;
        }

        UObject* AssetObject = AssetData.GetAsset();
        if (!AssetObject)
        {
            Failed.Add(AssetPath + TEXT(" (load failed)"));
            continue;
        }

        RenameList.Add(FAssetRenameData(TWeakObjectPtr<UObject>(AssetObject), DestinationFolder, AssetData.AssetName.ToString()));
        MovedTargets.Add(DestinationFolder / AssetData.AssetName.ToString());
    }

    if (RenameList.Num() > 0 && !GetAssetTools().RenameAssets(RenameList))
    {
        Failed.Add(TEXT("RenameAssets failed for one or more assets"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("dest_path"), DestinationFolder);
    Result->SetNumberField(TEXT("requested"), AssetPaths.Num());
    Result->SetNumberField(TEXT("moved"), RenameList.Num());
    Result->SetArrayField(TEXT("targets"), StringArrayToJson(MovedTargets));
    Result->SetArrayField(TEXT("failed"), StringArrayToJson(Failed));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleBulkDelete(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> AssetPaths;
    if (!TryGetStringArrayField(Params, TEXT("asset_paths"), AssetPaths) || AssetPaths.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_paths array is required"));
    }

    TArray<FString> DeletedPaths;
    TArray<FString> FailedPaths;
    for (const FString& Path : AssetPaths)
    {
        if (UEditorAssetLibrary::DeleteAsset(Path))
        {
            DeletedPaths.Add(Path);
        }
        else
        {
            FailedPaths.Add(Path + TEXT(" (delete failed)"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("requested"), AssetPaths.Num());
    Result->SetNumberField(TEXT("deleted"), DeletedPaths.Num());
    Result->SetArrayField(TEXT("deleted_paths"), StringArrayToJson(DeletedPaths));
    Result->SetArrayField(TEXT("failed"), StringArrayToJson(FailedPaths));
    return Result;
}

// ---------------------------------------------------------------------------
// Primary Asset Label
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleCreatePrimaryAssetLabel(const TSharedPtr<FJsonObject>& Params)
{
    FString LabelPath;
    if (!Params->TryGetStringField(TEXT("label_path"), LabelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("label_path is required"));
    }

    const UClass* LabelClass = UPrimaryAssetLabel::StaticClass();
    if (!LabelClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PrimaryAssetLabel class not available"));
    }

    FString DiskPath, Error;
    if (!ResolveFolderDiskPath(LabelPath, DiskPath, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    FString FolderPath, LabelName;
    SplitDestinationPath(LabelPath, TEXT("NewAssetLabel"), FolderPath, LabelName);

    FString PackageName = FolderPath / LabelName;
    FString FileName;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, FileName, FPackageName::GetAssetPackageExtension()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to resolve disk path"));
    }

    UPrimaryAssetLabel* NewLabel = nullptr;
    EObjectFlags Flags = RF_Public | RF_Standalone | RF_MarkAsNative;

    UPackage* Package = CreatePackage(*PackageName);
    if (Package)
    {
        NewLabel = NewObject<UPrimaryAssetLabel>(Package, LabelClass, FName(*LabelName), Flags);

        if (NewLabel)
        {
            TArray<FString> AssetPaths;
            TryGetStringArrayField(Params, TEXT("asset_paths"), AssetPaths);
            if (AssetPaths.Num() > 0)
            {
                for (const FString& AssetPath : AssetPaths)
                {
                    FString AssetPackageName = PackageNameFromAnyAssetPath(AssetPath);
                    NewLabel->ExplicitAssets.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(AssetPackageName)));
                }
            }

            Package->MarkPackageDirty();
            FString LabelFileName;
            if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, LabelFileName, FPackageName::GetAssetPackageExtension()))
            {
                FSavePackageArgs SaveArgs;
                SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
                if (UPackage::SavePackage(Package, NewLabel, *LabelFileName, SaveArgs))
                {
                    GetAssetRegistry().ScanPathsSynchronous({FolderPath});
                }
            }
        }
    }

    if (!NewLabel)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Primary Asset Label"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("label_path"), MakeObjectPath(PackageName, LabelName));
    Result->SetNumberField(TEXT("asset_count"), NewLabel->ExplicitAssets.Num());
    Result->SetStringField(TEXT("message"), TEXT("Primary Asset Label created"));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleDeletePrimaryAssetLabel(const TSharedPtr<FJsonObject>& Params)
{
    FString LabelPath;
    if (!Params->TryGetStringField(TEXT("label_path"), LabelPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("label_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(LabelPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    UObject* LabelObject = AssetData.GetAsset();
    UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(LabelObject);
    if (!Label)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Asset is not a Primary Asset Label"));
    }

    const bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetData.GetSoftObjectPath().ToString());

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("label_path"), LabelPath);
    Result->SetBoolField(TEXT("deleted"), bDeleted);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleListPrimaryAssetLabels(const TSharedPtr<FJsonObject>& Params)
{
    const UClass* LabelClass = UPrimaryAssetLabel::StaticClass();
    if (!LabelClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PrimaryAssetLabel class not available"));
    }

    FARFilter Filter;
    Filter.ClassPaths.Add(LabelClass->GetClassPathName());

    FString FolderPath;
    if (Params->TryGetStringField(TEXT("folder_path"), FolderPath))
    {
        Filter.PackagePaths.Add(FName(*NormalizeContentPackagePath(FolderPath)));
    }

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);
    Filter.bRecursivePaths = bRecursive;

    TArray<FAssetData> AssetDataArray;
    GetAssetRegistry().GetAssets(Filter, AssetDataArray);

    TArray<TSharedPtr<FJsonValue>> Labels;
    for (const FAssetData& AssetData : AssetDataArray)
    {
        TSharedPtr<FJsonObject> LabelJson = AssetDataToJson(AssetData, true);

        UObject* LabelObject = AssetData.GetAsset();
        if (LabelObject)
        {
            UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(LabelObject);
            if (Label)
            {
                TArray<TSharedPtr<FJsonValue>> AssetArr;
                for (const TSoftObjectPtr<UObject>& AssetPtr : Label->ExplicitAssets)
                {
                    AssetArr.Add(MakeShared<FJsonValueString>(AssetPtr.ToSoftObjectPath().ToString()));
                }
                LabelJson->SetArrayField(TEXT("managed_assets"), AssetArr);
                LabelJson->SetNumberField(TEXT("managed_asset_count"), Label->ExplicitAssets.Num());
            }
        }

        Labels.Add(MakeShared<FJsonValueObject>(LabelJson));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("labels"), Labels);
    Result->SetNumberField(TEXT("count"), Labels.Num());
    return Result;
}

// ---------------------------------------------------------------------------
// Asset Manager Settings
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleGetAssetManagerSettings(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    const UAssetManager* AssetManagerPtr = UAssetManager::GetIfInitialized();
    if (!AssetManagerPtr)
    {
        Result->SetStringField(TEXT("error"), TEXT("Asset Manager not initialized"));
        Result->SetArrayField(TEXT("primary_asset_types"), TArray<TSharedPtr<FJsonValue>>());
        Result->SetNumberField(TEXT("type_count"), 0);
        return Result;
    }

    const UAssetManager& AssetManager = *AssetManagerPtr;

    TArray<FPrimaryAssetTypeInfo> TypeInfos;
    AssetManager.GetPrimaryAssetTypeInfoList(TypeInfos);

    TArray<TSharedPtr<FJsonValue>> TypeInfoArray;
    for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfos)
    {
        TSharedPtr<FJsonObject> TypeJson = MakeShared<FJsonObject>();
        TypeJson->SetStringField(TEXT("type"), TypeInfo.PrimaryAssetType.ToString());
        if (TypeInfo.AssetBaseClassLoaded)
        {
            TypeJson->SetStringField(TEXT("base_class"), TypeInfo.AssetBaseClassLoaded->GetName());
        }
        TypeJson->SetBoolField(TEXT("has_blueprint_classes"), TypeInfo.bHasBlueprintClasses);
        TypeJson->SetBoolField(TEXT("is_editor_only"), TypeInfo.bIsEditorOnly);

        TArray<TSharedPtr<FJsonValue>> ScanPaths;
        for (const FString& Path : TypeInfo.AssetScanPaths)
        {
            ScanPaths.Add(MakeShared<FJsonValueString>(Path));
        }
        TypeJson->SetArrayField(TEXT("scan_paths"), ScanPaths);

        TypeInfoArray.Add(MakeShared<FJsonValueObject>(TypeJson));
    }
    Result->SetArrayField(TEXT("primary_asset_types"), TypeInfoArray);
    Result->SetNumberField(TEXT("type_count"), TypeInfoArray.Num());

    Result->SetStringField(TEXT("note"), TEXT("UE 5.7+ default rules are managed via UAssetManagerSettings, not runtime API."));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleSetAssetManagerSettings(const TSharedPtr<FJsonObject>& Params)
{
    // Build the DefaultGame.ini path
    FString ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"));
    FPaths::MakeStandardFilename(ConfigPath);
    ConfigPath = FConfigCacheIni::NormalizeConfigIniPath(ConfigPath);

    const TCHAR* Section = TEXT("/Script/Engine.AssetManagerSettings");
    bool bWroteAny = false;

    int32 DefaultPriority = -1;
    if (Params->TryGetNumberField(TEXT("default_priority"), DefaultPriority))
    {
        GConfig->SetInt(Section, TEXT("DefaultPriority"), DefaultPriority, *ConfigPath);
        bWroteAny = true;
    }

    int32 DefaultChunkId = -1;
    if (Params->TryGetNumberField(TEXT("default_chunk_id"), DefaultChunkId))
    {
        GConfig->SetInt(Section, TEXT("DefaultChunkId"), DefaultChunkId, *ConfigPath);
        bWroteAny = true;
    }

    bool bShouldBeInMainManifest = false;
    if (Params->TryGetBoolField(TEXT("default_should_be_in_main_manifest"), bShouldBeInMainManifest))
    {
        GConfig->SetBool(Section, TEXT("bShouldBeInMainManifest"), bShouldBeInMainManifest, *ConfigPath);
        bWroteAny = true;
    }

    bool bShouldBeLoadedOnDemand = false;
    if (Params->TryGetBoolField(TEXT("default_should_be_loaded_on_demand"), bShouldBeLoadedOnDemand))
    {
        GConfig->SetBool(Section, TEXT("bShouldBeLoadedOnDemand"), bShouldBeLoadedOnDemand, *ConfigPath);
        bWroteAny = true;
    }

    GConfig->Flush(false, *ConfigPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("wrote_any"), bWroteAny);
    Result->SetStringField(TEXT("config_path"), ConfigPath);
    Result->SetStringField(TEXT("message"), TEXT("Asset Manager settings written to DefaultGame.ini. Restart editor for changes to take effect."));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPContentBrowserCommands::HandleAddPrimaryAssetBundle(const TSharedPtr<FJsonObject>& Params)
{
    FString BundleName;
    if (!Params->TryGetStringField(TEXT("bundle_name"), BundleName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("bundle_name is required"));
    }

    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_path is required"));
    }

    FAssetData AssetData;
    FString Error;
    if (!GetAssetData(AssetPath, AssetData, Error))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
    }

    // Write bundle mapping to DefaultGame.ini
    FString ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"));
    FPaths::MakeStandardFilename(ConfigPath);
    ConfigPath = FConfigCacheIni::NormalizeConfigIniPath(ConfigPath);

    const TCHAR* Section = TEXT("/Script/Engine.AssetManagerSettings");
    FString Key = FString::Printf(TEXT("BundleMapping_%s"), *BundleName);
    FString ExistingValue;
    GConfig->GetString(Section, *Key, ExistingValue, *ConfigPath);

    // Append asset path if not already present
    TArray<FString> MappedPaths;
    ExistingValue.ParseIntoArray(MappedPaths, TEXT(","), true);
    if (!MappedPaths.Contains(AssetPath))
    {
        if (!ExistingValue.IsEmpty())
        {
            ExistingValue += TEXT(",");
        }
        ExistingValue += AssetPath;
        GConfig->SetString(Section, *Key, *ExistingValue, *ConfigPath);
        GConfig->Flush(false, *ConfigPath);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("bundle_name"), BundleName);
    Result->SetStringField(TEXT("mapped_assets"), ExistingValue);
    Result->SetStringField(TEXT("config_path"), ConfigPath);
    Result->SetStringField(TEXT("message"), TEXT("Bundle mapping written to DefaultGame.ini. Restart editor for changes to take effect."));
    return Result;
}
