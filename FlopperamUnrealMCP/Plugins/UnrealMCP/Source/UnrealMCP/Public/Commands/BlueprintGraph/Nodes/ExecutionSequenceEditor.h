// Header for editing ExecutionSequence nodes (pin management)

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UK2Node;
class UEdGraph;

/**
 * FExecutionSequenceEditor handles pin management for UK2Node_ExecutionSequence.
 * This node allows dynamic addition/removal of "Then N" execution output pins.
 */
class FExecutionSequenceEditor
{
public:
	/**
	 * Add an execution output pin to a Sequence node.
	 *
	 * @param Node The UK2Node_ExecutionSequence node to modify
	 * @param Graph The graph containing the node
	 * @return true if successful, false otherwise
	 */
	static bool AddExecutionPin(UK2Node* Node, UEdGraph* Graph);

	/**
	 * Remove an execution output pin from a Sequence node.
	 *
	 * @param Node The UK2Node_ExecutionSequence node to modify
	 * @param Graph The graph containing the node
	 * @param PinName The name of the pin to remove (e.g., "Then 2")
	 * @return true if successful, false otherwise
	 */
	static bool RemoveExecutionPin(UK2Node* Node, UEdGraph* Graph, const FString& PinName);

	/**
	 * Set the number of execution output pins directly.
	 *
	 * @param Node The UK2Node_ExecutionSequence node to modify
	 * @param Graph The graph containing the node
	 * @param NumPins Target number of output pins
	 * @return true if successful, false otherwise
	 */
	static bool SetNumExecutionPins(UK2Node* Node, UEdGraph* Graph, int32 NumPins);
};
