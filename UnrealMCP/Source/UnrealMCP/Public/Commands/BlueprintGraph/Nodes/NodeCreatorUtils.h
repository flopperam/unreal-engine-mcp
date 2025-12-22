// Utility header for node creation helpers

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Shared utilities for K2Node creation and initialization
 *
 * This class provides shared functions to avoid code duplication
 * in all node creators (ControlFlowNodes, DataNodes, etc.)
 */
class FNodeCreatorUtils
{
public:
	/**
	 * Fully initializes a K2Node after creation
	 *
	 * Performs the following steps in order:
	 * 1. AllocateDefaultPins() - Creates default pins
	 * 2. ReconstructNode() - Reconstructs the node (notifies Unreal of changes)
	 * 3. NotifyGraphChanged() - Notifies the graph that something changed
	 *
	 * @param Node - The node to initialize (must be non-null)
	 * @param Graph - The graph containing the node (must be non-null)
	 *
	 * @return true if initialization succeeded, false otherwise
	 *
	 * @note This function MUST be called after:
	 *       - NewObject<NodeType>(Graph)
	 *       - Graph->AddNode(Node, ...)
	 *       - Any type-specific configuration (SetExternalMember, StructType, etc.)
	 *
	 * @example
	 * UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	 * Graph->AddNode(Node, true, false);
	 * Node->FunctionReference.SetExternalMember(FunctionName, TargetClass);  // Configuration BEFORE
	 * FNodeCreatorUtils::InitializeK2Node(Node, Graph);  // Initialization AFTER
	 */
	static bool InitializeK2Node(UK2Node* Node, UEdGraph* Graph);

	/**
	 * Extracts X/Y coordinates from a JSON object
	 *
	 * @param Params - JSON object containing "pos_x" and "pos_y"
	 * @param OutX - Reference to store X position (default: 0.0)
	 * @param OutY - Reference to store Y position (default: 0.0)
	 *
	 * @note Validates field existence before attempting to read them
	 */
	static void ExtractNodePosition(const TSharedPtr<class FJsonObject>& Params, double& OutX, double& OutY);
};
