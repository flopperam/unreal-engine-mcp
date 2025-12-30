// Manages Blueprint function lifecycle (create, delete, rename)

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UBlueprint;
class UEdGraph;

/**
 * Manages Blueprint function lifecycle
 * Handles creation, deletion, and renaming of Blueprint functions
 */
class UNREALMCP_API FFunctionManager
{
public:
	/**
	 * Create a new Blueprint function
	 * @param Params JSON parameters containing:
	 *   - blueprint_name (string): Name of the Blueprint
	 *   - function_name (string): Name for the new function
	 *   - return_type (string, optional): Return type (default: "void")
	 * @return JSON response with function_name and graph_id or error
	 */
	static TSharedPtr<FJsonObject> CreateFunction(const TSharedPtr<FJsonObject>& Params);

	/**
	 * Delete a Blueprint function
	 * @param Params JSON parameters containing:
	 *   - blueprint_name (string): Name of the Blueprint
	 *   - function_name (string): Name of the function to delete
	 * @return JSON response with deleted_function_name or error
	 */
	static TSharedPtr<FJsonObject> DeleteFunction(const TSharedPtr<FJsonObject>& Params);

	/**
	 * Rename a Blueprint function
	 * @param Params JSON parameters containing:
	 *   - blueprint_name (string): Name of the Blueprint
	 *   - old_function_name (string): Current function name
	 *   - new_function_name (string): New function name
	 * @return JSON response with new_function_name or error
	 */
	static TSharedPtr<FJsonObject> RenameFunction(const TSharedPtr<FJsonObject>& Params);

private:
	/**
	 * Load a Blueprint by name
	 * @param BlueprintName Name or path of the Blueprint
	 * @return Loaded Blueprint or nullptr
	 */
	static UBlueprint* LoadBlueprint(const FString& BlueprintName);

	/**
	 * Validate function name (no spaces, special chars, etc.)
	 * @param FunctionName Name to validate
	 * @return true if valid
	 */
	static bool ValidateFunctionName(const FString& FunctionName);

	/**
	 * Check if function already exists
	 * @param Blueprint The Blueprint to search
	 * @param FunctionName Name to check
	 * @return true if function exists
	 */
	static bool FunctionExists(UBlueprint* Blueprint, const FString& FunctionName);

	/**
	 * Find a function graph by name
	 * @param Blueprint The Blueprint containing functions
	 * @param FunctionName Name of the function
	 * @return Function graph or nullptr
	 */
	static UEdGraph* FindFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName);

	// Helper functions for JSON responses
	static TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& FunctionName, const FString& GraphID = TEXT(""));
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
};
