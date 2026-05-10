#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Sequencer MCP commands.
 */
class FEpicUnrealMCPSequencerCommands
{
public:
    FEpicUnrealMCPSequencerCommands();
    ~FEpicUnrealMCPSequencerCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleCreateLevelSequence(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddActorBinding(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddTransformTrack(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddCameraCutTrack(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddEventTrack(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddKeyframe(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPlaybackRange(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetFrameRate(const TSharedPtr<FJsonObject>& Params);
};
