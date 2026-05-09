#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Audio MCP commands.
 */
class FEpicUnrealMCPAudioCommands
{
public:
    FEpicUnrealMCPAudioCommands();
    ~FEpicUnrealMCPAudioCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleCreateSoundCue(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddAudioComponent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetSoundAttenuation(const TSharedPtr<FJsonObject>& Params);
};
