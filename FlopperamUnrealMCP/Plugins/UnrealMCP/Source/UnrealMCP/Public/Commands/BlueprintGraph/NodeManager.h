/**
 * Filename: NodeManager.h
 * Creation Date: 2025-01-06
 * Author: Zoscran
 * Description: Manages Blueprint node creation and manipulation
 */

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Namespace for Blueprint Graph operations
 */
namespace BlueprintGraph
{
	/**
	 * Handles adding and managing nodes in Blueprint graphs
	 */
	class UNREALMCP_API FNodeManager
	{
	public:
		/**
		 * Add a new node to a Blueprint graph
		 * 
		 * @param Params - JSON parameters containing:
		 *   - blueprint_name (string): Name of the Blueprint to modify
		 *   - node_type (string): Type of node to create ("Print", "Event", "Function", etc.)
		 *   - node_params (object): Additional node configuration
		 *     - pos_x (float): X position in graph
		 *     - pos_y (float): Y position in graph
		 *     - [type-specific parameters]
		 * 
		 * @return JSON response containing:
		 *   - success (bool): Whether operation succeeded
		 *   - node_id (string): GUID of created node
		 *   - node_type (string): Type of node created
		 *   - error (string): Error message if failed
		 */
		static TSharedPtr<FJsonObject> AddNode(const TSharedPtr<FJsonObject>& Params);

	private:
		/**
		 * Create a Print String node
		 * @param Graph - Target graph
		 * @param Params - Node parameters
		 * @return Created node or nullptr
		 */
		static class UK2Node* CreatePrintNode(class UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);

		/**
		 * Create an Event node (BeginPlay, Tick, etc.)
		 * @param Graph - Target graph
		 * @param Params - Node parameters (must include event_type)
		 * @return Created node or nullptr
		 */
		static class UK2Node_Event* CreateEventNode(class UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);

		/**
		 * Create a Variable Get node
		 * @param Graph - Target graph
		 * @param Params - Node parameters (must include variable_name)
		 * @return Created node or nullptr
		 */
		static class UK2Node* CreateVariableGetNode(class UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);

		/**
		 * Create a Variable Set node
		 * @param Graph - Target graph
		 * @param Params - Node parameters (must include variable_name)
		 * @return Created node or nullptr
		 */
		static class UK2Node* CreateVariableSetNode(class UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);

		/**
		 * Load a Blueprint by name
		 * @param BlueprintName - Name or path of Blueprint
		 * @return Blueprint object or nullptr
		 */
		static class UBlueprint* LoadBlueprint(const FString& BlueprintName);

		/**
		 * Create success response with node info
		 */
		static TSharedPtr<FJsonObject> CreateSuccessResponse(const class UK2Node* Node, const FString& NodeType);

		/**
		 * Create error response
		 */
		static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
	};
}
