#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Data Table MCP commands.
 */
class FEpicUnrealMCPDataTableCommands
{
public:
    FEpicUnrealMCPDataTableCommands();
    ~FEpicUnrealMCPDataTableCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleImportCSVToDataTable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params);

    // Helper to find a UScriptStruct by path
    UScriptStruct* FindRowStruct(const FString& StructPath, FString& OutError);
    // Helper to set struct property from JSON value
    bool SetStructProperty(void* StructMemory, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError);
};
