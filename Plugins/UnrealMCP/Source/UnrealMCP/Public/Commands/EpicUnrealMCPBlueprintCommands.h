#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Blueprint-related MCP commands
 */
class FEpicUnrealMCPBlueprintCommands
{
public:
    	FEpicUnrealMCPBlueprintCommands();

    // Handle blueprint commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific blueprint command handlers (only used functions)
    TSharedPtr<FJsonObject> HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& Params);
    
    // Material management functions
    TSharedPtr<FJsonObject> HandleGetAvailableMaterials(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleApplyMaterialToBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorMaterialInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintMaterialInfo(const TSharedPtr<FJsonObject>& Params);

    // Blueprint analysis functions
    TSharedPtr<FJsonObject> HandleReadBlueprintContent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAnalyzeBlueprintGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintVariableDetails(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintFunctionDetails(const TSharedPtr<FJsonObject>& Params);

    // --- Phase 6: Missing Blueprint Features ---
    
    // Parent Class / Inheritance
    TSharedPtr<FJsonObject> HandleSetBlueprintParentClass(const TSharedPtr<FJsonObject>& Params);
    
    // Blueprint Class Settings & Defaults
    TSharedPtr<FJsonObject> HandleSetBlueprintClassSettings(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintClassDefaults(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetComponentDefaults(const TSharedPtr<FJsonObject>& Params);
    
    // Construction Script
    TSharedPtr<FJsonObject> HandleEditConstructionScript(const TSharedPtr<FJsonObject>& Params);
    
    // Event Dispatchers
    TSharedPtr<FJsonObject> HandleCreateEventDispatcher(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBindEventDispatcher(const TSharedPtr<FJsonObject>& Params);
    
    // Enum / Struct
    TSharedPtr<FJsonObject> HandleCreateEnum(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateStruct(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleEditEnum(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleEditStruct(const TSharedPtr<FJsonObject>& Params);
    
    // Interface / Function Library / Macro Library
    TSharedPtr<FJsonObject> HandleCreateBlueprintInterface(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleImplementInterface(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateFunctionLibrary(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMacroLibrary(const TSharedPtr<FJsonObject>& Params);
    
    // Graph Organization
    TSharedPtr<FJsonObject> HandleAddCommentNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddRerouteNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFormatGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateCollapsedGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMacroGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMacroInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateTimeline(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleEditTimeline(const TSharedPtr<FJsonObject>& Params);
    
    // Debug / Profiler / Breakpoint / Watch
    TSharedPtr<FJsonObject> HandleSetBlueprintBreakpoint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintWatch(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleClearBlueprintWatches(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStepBlueprintDebugger(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintDebugInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBlueprintDiff(const TSharedPtr<FJsonObject>& Params);

};
