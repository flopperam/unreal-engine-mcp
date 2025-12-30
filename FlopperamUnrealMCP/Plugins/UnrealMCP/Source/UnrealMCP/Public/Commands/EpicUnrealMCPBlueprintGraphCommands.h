// Blueprint Graph Commands Handler

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

    // Set variable properties in Blueprint (F19)
    TSharedPtr<FJsonObject> HandleSetVariableProperties(const TSharedPtr<FJsonObject>& Params);

    // Add event node to Blueprint graph
    TSharedPtr<FJsonObject> HandleAddEventNode(const TSharedPtr<FJsonObject>& Params);

    // Delete node from Blueprint graph (F20)
    TSharedPtr<FJsonObject> HandleDeleteNode(const TSharedPtr<FJsonObject>& Params);

    // Set node property in Blueprint graph (F21)
    TSharedPtr<FJsonObject> HandleSetNodeProperty(const TSharedPtr<FJsonObject>& Params);

    // Create function in Blueprint
    TSharedPtr<FJsonObject> HandleCreateFunction(const TSharedPtr<FJsonObject>& Params);

    // Add function input parameter
    TSharedPtr<FJsonObject> HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params);

    // Add function output parameter
    TSharedPtr<FJsonObject> HandleAddFunctionOutput(const TSharedPtr<FJsonObject>& Params);

    // Delete function from Blueprint
    TSharedPtr<FJsonObject> HandleDeleteFunction(const TSharedPtr<FJsonObject>& Params);

    // Rename function in Blueprint
    TSharedPtr<FJsonObject> HandleRenameFunction(const TSharedPtr<FJsonObject>& Params);
};
