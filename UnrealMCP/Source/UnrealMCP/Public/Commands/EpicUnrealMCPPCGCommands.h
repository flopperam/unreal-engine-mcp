#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FEpicUnrealMCPPCGCommands
{
public:
    // Main command handler function
    TSharedPtr<FJsonObject> HandleCommand(const FString &CommandType, const TSharedPtr<FJsonObject> &Params);

private:
    // Phase 1: Reading & Analysis
    TSharedPtr<FJsonObject> AnalyzePCGGraph(const TSharedPtr<FJsonObject> &Params);

    // Phase 2: Updating
    TSharedPtr<FJsonObject> UpdatePCGGraphParameter(const TSharedPtr<FJsonObject> &Params);

    // Phase 3: Creation
    TSharedPtr<FJsonObject> CreatePCGGraph(const TSharedPtr<FJsonObject> &Params);
};