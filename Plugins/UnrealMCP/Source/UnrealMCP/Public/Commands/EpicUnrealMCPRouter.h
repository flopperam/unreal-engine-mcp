#pragma once

#include "CoreMinimal.h"

/**
 * Static command router for EpicUnrealMCPBridge.
 * Maps command name strings to integer route IDs.
 */
class UNREALMCP_API FEpicUnrealMCPRouter
{
public:
    static int32 RouteCommand(const FString& CommandType);
};
