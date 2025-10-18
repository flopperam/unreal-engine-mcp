/**
 * @file EpicUnrealMCPBlueprintGraphCommands.h
 * @brief Blueprint Graph Commands Handler
 * @author Zoscran
 * @date 2025-01-13
 * @version 1.0
 *
 * Handles all Blueprint graph manipulation commands including:
 * - add_blueprint_node
 * - connect_nodes
 * - create_variable
 */

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FEpicUnrealMCPBlueprintGraphCommands
{
public:
    FEpicUnrealMCPBlueprintGraphCommands();
    ~FEpicUnrealMCPBlueprintGraphCommands();

    /**
     * Main command handler for Blueprint Graph operations
     * @param CommandType The type of command to execute
     * @param Params JSON parameters for the command
     * @return JSON response object
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Add node to Blueprint graph
    TSharedPtr<FJsonObject> HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params);

    // Connect nodes in Blueprint graph
    TSharedPtr<FJsonObject> HandleConnectNodes(const TSharedPtr<FJsonObject>& Params);

    // Create variable in Blueprint
    TSharedPtr<FJsonObject> HandleCreateVariable(const TSharedPtr<FJsonObject>& Params);
};