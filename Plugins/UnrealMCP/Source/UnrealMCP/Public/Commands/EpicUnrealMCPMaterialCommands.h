#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Material Graph-related MCP commands
 */
class FEpicUnrealMCPMaterialCommands
{
public:
    FEpicUnrealMCPMaterialCommands();
    ~FEpicUnrealMCPMaterialCommands();

    // Handle material graph commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Command Handlers
    TSharedPtr<FJsonObject> HandleAnalyzeMaterialGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddMaterialNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
};
