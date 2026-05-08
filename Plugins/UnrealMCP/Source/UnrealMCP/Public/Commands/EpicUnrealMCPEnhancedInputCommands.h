#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for Unreal Engine Enhanced Input MCP commands.
 *
 * Targets UE 5.7 Enhanced Input assets and runtime subsystems:
 * UInputAction, UInputMappingContext, UEnhancedInputLocalPlayerSubsystem,
 * and UEnhancedInputUserSettings.
 */
class UNREALMCP_API FEpicUnrealMCPEnhancedInputCommands
{
public:
    FEpicUnrealMCPEnhancedInputCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleCreateInputAction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddEnhancedInputMapping(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveEnhancedInputMapping(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConfigureEnhancedInputAction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConfigureEnhancedInputMapping(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListEnhancedInputAssets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetEnhancedInputDebugInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddRuntimeMappingContext(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveRuntimeMappingContext(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetupEnhancedInputBinding(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetupRebindUI(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRebindEnhancedInputKey(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConfigureLocalMultiplayerInput(const TSharedPtr<FJsonObject>& Params);
};
