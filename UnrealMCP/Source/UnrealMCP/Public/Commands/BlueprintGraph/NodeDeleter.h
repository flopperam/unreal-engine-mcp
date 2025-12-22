// Manages Blueprint node deletion
#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UEdGraph;
class UEdGraphNode;
class UBlueprint;

/**
 * Manages Blueprint node deletion
 * Deletes nodes from EventGraph or Function Graphs
 */
class UNREALMCP_API FNodeDeleter
{
public:
	/**
	 * Delete a node from a Blueprint graph
	 * @param Params JSON parameters containing:
	 *   - blueprint_name (string): Name of the Blueprint
	 *   - node_id (string): ID of the node to delete
	 *   - function_name (string, optional): Function graph name (null = EventGraph)
	 * @return JSON response with deleted_node_id or error
	 */
	static TSharedPtr<FJsonObject> DeleteNode(const TSharedPtr<FJsonObject>& Params);

private:
	/**
	 * Get the appropriate graph (EventGraph or Function Graph)
	 * @param Blueprint The Blueprint containing the graph
	 * @param FunctionName Name of function (empty = EventGraph)
	 * @return The graph or nullptr
	 */
	static UEdGraph* GetGraph(UBlueprint* Blueprint, const FString& FunctionName);

	/**
	 * Find a node by its ID (tries NodeGuid and GetName())
	 * @param Graph The graph to search
	 * @param NodeID The node identifier
	 * @return Found node or nullptr
	 */
	static UEdGraphNode* FindNodeByID(UEdGraph* Graph, const FString& NodeID);

	/**
	 * Remove a node from the graph
	 * @param Graph The graph containing the node
	 * @param Node The node to remove
	 * @return true if successful
	 */
	static bool RemoveNode(UEdGraph* Graph, UEdGraphNode* Node);

	/**
	 * Load a Blueprint by name
	 * @param BlueprintName Name or path of the Blueprint
	 * @return Loaded Blueprint or nullptr
	 */
	static UBlueprint* LoadBlueprint(const FString& BlueprintName);

	// Helper functions
	static TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& DeletedNodeID);
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
};
