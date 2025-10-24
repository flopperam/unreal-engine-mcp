#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FEpicUnrealMCPUtilityCommands
{
public:
    // Main command handler function
    TSharedPtr<FJsonObject> HandleCommand(const FString &CommandType, const TSharedPtr<FJsonObject> &Params);

private:
    // Python script execution
    TSharedPtr<FJsonObject> ExecutePythonScript(const TSharedPtr<FJsonObject> &Params);
};