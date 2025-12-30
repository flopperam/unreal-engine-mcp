// Manages Blueprint event node creation
#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UK2Node_Event;
class UEdGraph;

/**
 * Manages Blueprint event node creation
 * F18: add_event_node - Create specialized event nodes
 *
 * This class handles creation of event nodes in Blueprint graphs,
 * supporting all UE5 native events (ReceiveBeginPlay, ReceiveTick, etc.)
 * and custom events.
 */
class UNREALMCP_API FEventManager
{
public:
	/**
	 * Add an event node to a Blueprint graph
	 * @param Params JSON parameters containing:
	 *   - blueprint_name (string): Name of the Blueprint
	 *   - event_name (string): Name of the event (e.g., "ReceiveBeginPlay", "ReceiveTick")
	 *   - pos_x (number, optional): X position in graph (default: 0.0)
	 *   - pos_y (number, optional): Y position in graph (default: 0.0)
	 * @return JSON response with node_id or error
	 */
	static TSharedPtr<FJsonObject> AddEventNode(const TSharedPtr<FJsonObject>& Params);

private:
	/**
	 * Create an event node in the specified graph
	 * Inspired by chongdashu's implementation
	 * @param Graph The Blueprint event graph
	 * @param EventName Name of the event function
	 * @param Position Position in the graph
	 * @return Created event node or nullptr on failure
	 */
	static UK2Node_Event* CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position);

	/**
	 * Find existing event node with the same name (to avoid duplicates)
	 * @param Graph The Blueprint event graph
	 * @param EventName Name of the event to find
	 * @return Existing event node or nullptr if not found
	 */
	static UK2Node_Event* FindExistingEventNode(UEdGraph* Graph, const FString& EventName);

	/**
	 * Load a Blueprint by name
	 * @param BlueprintName Name or path of the Blueprint
	 * @return Loaded Blueprint or nullptr
	 */
	static UBlueprint* LoadBlueprint(const FString& BlueprintName);

	// Helper functions
	static TSharedPtr<FJsonObject> CreateSuccessResponse(const UK2Node_Event* EventNode);
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
};
