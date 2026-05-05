#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Project / Editor Control MCP commands
 * Handles project settings, plugins, PIE, viewport, save, undo/redo, etc.
 */
class UNREALMCP_API FEpicUnrealMCPProjectEditorCommands
{
public:
    FEpicUnrealMCPProjectEditorCommands();

    // Handle project/editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Project Settings
    TSharedPtr<FJsonObject> HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetDefaultMap(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetGameDefaultMap(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetEditorStartupMap(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetProjectDescription(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMapsAndModes(const TSharedPtr<FJsonObject>& Params);

    // Plugins
    TSharedPtr<FJsonObject> HandleListPlugins(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPluginEnabled(const TSharedPtr<FJsonObject>& Params);

    // Engine Settings
    TSharedPtr<FJsonObject> HandleSetEngineScalability(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetRenderingSetting(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPhysicsSetting(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetInputSetting(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetCollisionSetting(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetAISetting(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetNavigationSetting(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPackagingSetting(const TSharedPtr<FJsonObject>& Params);

    // World Settings
    TSharedPtr<FJsonObject> HandleGetWorldSettings(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWorldSetting(const TSharedPtr<FJsonObject>& Params);

    // Level / Map Management (Phase 1)
    TSharedPtr<FJsonObject> HandleCreateLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleLoadLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDuplicateLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRenameLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListLevels(const TSharedPtr<FJsonObject>& Params);

    // Persistent Level / Sublevel Management (Phase 2)
    TSharedPtr<FJsonObject> HandleGetPersistentLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddSublevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveSublevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetSublevelVisible(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetSublevelLoaded(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateStreamingVolume(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetLevelStreamingSettings(const TSharedPtr<FJsonObject>& Params);

    // World Partition (Phase 3)
    TSharedPtr<FJsonObject> HandleEnableWorldPartition(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWorldPartitionGrid(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWorldPartitionCells(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleLoadWorldPartitionCell(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleUnloadWorldPartitionCell(const TSharedPtr<FJsonObject>& Params);

    // Data Layer / HLOD / OFPA / Bounds / Origin Rebasing (Phase 4)
    TSharedPtr<FJsonObject> HandleCreateDataLayer(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddActorsToDataLayer(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveActorsFromDataLayer(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetDataLayerEnabled(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateHLODLayer(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBuildHLOD(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRebuildHLOD(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetOneFilePerActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetLevelBounds(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWorldOriginRebasing(const TSharedPtr<FJsonObject>& Params);

    // Editor Control
    TSharedPtr<FJsonObject> HandleUndo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRedo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetDirtyAssets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAll(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEditorLog(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateUtilityWidget(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateUtilityBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExecutePythonScript(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExecuteCommandlet(const TSharedPtr<FJsonObject>& Params);

    // Play / Simulate
    TSharedPtr<FJsonObject> HandleStartPIE(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStopPIE(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetPlayState(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStartStandaloneGame(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStartSimulate(const TSharedPtr<FJsonObject>& Params);

    // Viewport / Camera
    TSharedPtr<FJsonObject> HandleGetCameraPosition(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetCameraPosition(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleViewportAction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExportLevel(const TSharedPtr<FJsonObject>& Params);
};
