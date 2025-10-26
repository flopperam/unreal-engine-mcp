// Nom du fichier: NodePropertyManager.h
// Date de création: 2025-10-26
// Auteur: Zoscran
// Description: Blueprint node property modification manager

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

class UEdGraph;
class UEdGraphNode;
class UBlueprint;
class UK2Node;

/**
 * Manages Blueprint node property modification
 * Supports modifying properties on various node types
 */
class UNREALMCP_API FNodePropertyManager
{
public:
	/**
	 * Set a property on a Blueprint node
	 * @param Params JSON parameters containing:
	 *   - blueprint_name (string): Name of the Blueprint
	 *   - node_id (string): ID of the node
	 *   - property_name (string): Name of property to set
	 *   - property_value (any): Value to set
	 *   - function_name (string, optional): Function graph name (null = EventGraph)
	 * @return JSON response with updated_property or error
	 */
	static TSharedPtr<FJsonObject> SetNodeProperty(const TSharedPtr<FJsonObject>& Params);

private:
	/**
	 * Get the appropriate graph (EventGraph or Function Graph)
	 */
	static UEdGraph* GetGraph(UBlueprint* Blueprint, const FString& FunctionName);

	/**
	 * Find a node by its ID
	 */
	static UEdGraphNode* FindNodeByID(UEdGraph* Graph, const FString& NodeID);

	/**
	 * Set property on a Print (CallFunction) node
	 */
	static bool SetPrintNodeProperty(
		UK2Node_CallFunction* PrintNode,
		const FString& PropertyName,
		const TSharedPtr<FJsonValue>& Value);

	/**
	 * Set property on a Variable Get/Set node
	 */
	static bool SetVariableNodeProperty(
		UK2Node* VarNode,
		const FString& PropertyName,
		const TSharedPtr<FJsonValue>& Value);

	/**
	 * Set generic property (position, comment, etc.)
	 */
	static bool SetGenericNodeProperty(
		UEdGraphNode* Node,
		const FString& PropertyName,
		const TSharedPtr<FJsonValue>& Value);

	/**
	 * Load a Blueprint by name
	 */
	static UBlueprint* LoadBlueprint(const FString& BlueprintName);

	// Helper functions
	static TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& PropertyName);
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
};
