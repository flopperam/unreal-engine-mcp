// Manages Blueprint function input and output parameters

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UBlueprint;
class UEdGraph;

/**
 * Manages Blueprint function input and output parameters
 * Handles adding parameters to function definitions
 */
class UNREALMCP_API FFunctionIO
{
public:
	/**
	 * Add a parameter (input or output) to a Blueprint function
	 * Unified function for both input and output parameters
	 * @param Params JSON parameters containing:
	 *   - blueprint_name (string): Name of the Blueprint
	 *   - function_name (string): Name of the function
	 *   - param_name (string): Name of the parameter
	 *   - param_type (string): Type (bool, int, float, string, vector, etc.)
	 *   - is_array (bool, optional): Whether parameter is an array
	 *   - direction (string): "input" or "output"
	 * @return JSON response with param_name and param_type or error
	 */
	static TSharedPtr<FJsonObject> AddFunctionIO(const TSharedPtr<FJsonObject>& Params);

	/**
	 * Add an input parameter to a Blueprint function
	 * @deprecated Use AddFunctionIO with direction="input" instead
	 */
	static TSharedPtr<FJsonObject> AddFunctionInput(const TSharedPtr<FJsonObject>& Params);

	/**
	 * Add an output parameter to a Blueprint function
	 * @deprecated Use AddFunctionIO with direction="output" instead
	 */
	static TSharedPtr<FJsonObject> AddFunctionOutput(const TSharedPtr<FJsonObject>& Params);

private:
	/**
	 * Add a parameter to a function (internal)
	 * @param Blueprint The Blueprint containing the function
	 * @param FunctionName Name of the function
	 * @param ParamName Name of the parameter
	 * @param ParamType Type of the parameter
	 * @param bIsInput true for input, false for output
	 * @param bIsArray true if parameter is an array
	 * @return true if successful
	 */
	static bool AddFunctionParameter(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		const FString& ParamName,
		const FString& ParamType,
		bool bIsInput,
		bool bIsArray = false
	);

	/**
	 * Load a Blueprint by name
	 * @param BlueprintName Name or path of the Blueprint
	 * @return Loaded Blueprint or nullptr
	 */
	static UBlueprint* LoadBlueprint(const FString& BlueprintName);

	/**
	 * Convert string type name to UE FProperty type
	 * @param TypeName Type name (bool, int, float, string, vector, etc.)
	 * @return FEdGraphPinType or default
	 */
	static FEdGraphPinType GetPropertyTypeFromString(const FString& TypeName);

	/**
	 * Find a function graph by name
	 * @param Blueprint The Blueprint containing functions
	 * @param FunctionName Name of the function
	 * @return Function graph or nullptr
	 */
	static UEdGraph* FindFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName);

	/**
	 * Validate parameter name
	 * @param ParamName Name to validate
	 * @return true if valid
	 */
	static bool ValidateParameterName(const FString& ParamName);

	// Helper functions for JSON responses
	static TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& ParamName, const FString& ParamType, bool bIsInput);
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
};
