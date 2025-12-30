// Manages Blueprint node creation and manipulation

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Handles adding and managing nodes in Blueprint graphs
 * Static class for Blueprint node operations (no namespace to avoid conflicts)
 */
class UNREALMCP_API FBlueprintNodeManager
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
		 * Create a Call Function node
		 * @param Graph - Target graph
		 * @param Params - Node parameters (must include target_function, optional: target_blueprint, pos_x, pos_y)
		 * @return Created node or nullptr
		 */
		static class UK2Node* CreateCallFunctionNode(class UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);

		/**
		 * Create a Comparison node (==, >, <, >=, <=)
		 * @param Graph - Target graph
		 * @param Params - Node parameters (optional: comparison_type, pos_x, pos_y)
		 * @return Created node or nullptr
		 */
		static class UK2Node* CreateComparisonNode(class UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);

		/**
		 * Create a Branch node (conditional flow)
		 * @param Graph - Target graph
		 * @param Params - Node parameters (optional: pos_x, pos_y)
		 * @return Created node or nullptr
		 */
		static class UK2Node* CreateBranchNode(class UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params);


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
