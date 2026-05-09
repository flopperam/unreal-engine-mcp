#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Rendering and Lighting MCP commands
 */
class FEpicUnrealMCPRenderingCommands
{
public:
    FEpicUnrealMCPRenderingCommands();
    ~FEpicUnrealMCPRenderingCommands();

    // Handle rendering commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // CVar helpers
    bool IsInPIE() const;
    TSharedPtr<FJsonObject> SetCVarInt(const FString& CVarName, int32 Value);
    TSharedPtr<FJsonObject> SetCVarFloat(const FString& CVarName, float Value);
    TSharedPtr<FJsonObject> GetCVarValue(const FString& CVarName);
    TSharedPtr<FJsonObject> CreateCVarResult(const FString& CVarName, bool bSuccess, const FString& Error = TEXT(""));

    // Command handlers
    TSharedPtr<FJsonObject> HandleSetGlobalIllumination(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetLumenEnabled(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetLumenSceneDetail(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetLumenReflectionQuality(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetHardwareRayTracing(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPathTracing(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetVirtualShadowMaps(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetShadowQuality(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetAntiAliasing(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetTSRSettings(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetUpscaler(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetNaniteVisualization(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetShaderCompileStatus(const TSharedPtr<FJsonObject>& Params);
};
