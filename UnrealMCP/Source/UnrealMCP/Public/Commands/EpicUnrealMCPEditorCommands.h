#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Editor-related MCP commands
 * Handles viewport control, actor manipulation, level management,
 * editor lifecycle, PIE/simulation, selection, and generic reflection.
 */
class UNREALMCP_API FEpicUnrealMCPEditorCommands
{
public:
    	FEpicUnrealMCPEditorCommands();

    // Handle editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);

    // Blueprint actor spawning
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // Editor lifecycle commands
    TSharedPtr<FJsonObject> HandleRequestEditorExit(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRestartEditor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEditorState(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEngineVersion(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAll(const TSharedPtr<FJsonObject>& Params);

    // PIE / Simulation commands
    TSharedPtr<FJsonObject> HandlePlayInEditorStart(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePlayInEditorStop(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSimulateStart(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSimulateStop(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePauseGame(const TSharedPtr<FJsonObject>& Params);

    // Level lifecycle commands
    TSharedPtr<FJsonObject> HandleListLevels(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDuplicateLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetCurrentWorld(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddSublevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveSublevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleToggleSublevelVisibility(const TSharedPtr<FJsonObject>& Params);

    // Selection and viewport commands
    TSharedPtr<FJsonObject> HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSelectActors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFocusViewportOnSelection(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCaptureViewportScreenshot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetViewportStats(const TSharedPtr<FJsonObject>& Params);

    // Generic object reflection commands
    TSharedPtr<FJsonObject> HandleGetObjectProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetObjectProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCallUObjectFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBuildNavMesh(const TSharedPtr<FJsonObject>& Params);
};
