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

    // Material Instance & Parameter Handlers (Phase 1)
    TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateDynamicMaterialInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleBatchUpdateMaterialParameters(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialScalarParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialVectorParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialTextureParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialStaticSwitchParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleEditMaterialParameterCollection(const TSharedPtr<FJsonObject>& Params);

    // Phase 2: Advanced Material Types
    TSharedPtr<FJsonObject> HandleCreateAdvancedMaterial(const TSharedPtr<FJsonObject>& Params);

    // Internal helpers
    TSharedPtr<FJsonObject> ApplyBatchParameters(UMaterialInstance* Instance, const TArray<TSharedPtr<FJsonValue>>& Parameters);
};
