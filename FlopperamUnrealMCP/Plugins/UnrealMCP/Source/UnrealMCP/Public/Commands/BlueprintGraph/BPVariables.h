#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Forward declarations
struct FEdGraphPinType;
struct FBPVariableDescription;

/**
 * Utility class for creating and managing Blueprint variables
 */
class UNREALMCP_API FBPVariables
{
public:
    /**
     * Creates a new variable in a Blueprint
     * @param Params JSON containing blueprint_name, variable_name, variable_type, default_value, is_public, tooltip, category
     * @return JSON with success and variable details
     */
    static TSharedPtr<FJsonObject> CreateVariable(const TSharedPtr<FJsonObject>& Params);

    /**
     * Modifies properties of an existing variable without deleting it
     * @param Params JSON containing blueprint_name, variable_name, and optional properties:
     *        is_blueprint_writable, is_public, is_editable_in_instance,
     *        tooltip, category, default_value, is_config, expose_on_spawn,
     *        var_name, var_type, friendly_name, replication
     * @return JSON with success and modified properties
     */
    static TSharedPtr<FJsonObject> SetVariableProperties(const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Converts a type string to FEdGraphPinType
     * Supported types: bool, int, float, string, vector, rotator
     */
    static FEdGraphPinType GetPinTypeFromString(const FString& TypeString);

    /**
     * Sets the default value of a variable
     */
    static void SetDefaultValue(FBPVariableDescription& Variable, const TSharedPtr<FJsonValue>& Value);
};