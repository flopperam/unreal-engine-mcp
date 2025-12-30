// Header for editing MakeArray nodes (element management)

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UK2Node;
class UEdGraph;

/**
 * FMakeArrayEditor handles pin management for UK2Node_MakeArray.
 * This node allows dynamic addition/removal of array element input pins.
 */
class FMakeArrayEditor
{
public:
	/**
	 * Add an array element input pin to a MakeArray node.
	 *
	 * @param Node The UK2Node_MakeArray node to modify
	 * @param Graph The graph containing the node
	 * @return true if successful, false otherwise
	 */
	static bool AddArrayElementPin(UK2Node* Node, UEdGraph* Graph);

	/**
	 * Remove an array element input pin from a MakeArray node.
	 *
	 * @param Node The UK2Node_MakeArray node to modify
	 * @param Graph The graph containing the node
	 * @param PinName The name of the pin to remove (e.g., "[2]")
	 * @return true if successful, false otherwise
	 */
	static bool RemoveArrayElementPin(UK2Node* Node, UEdGraph* Graph, const FString& PinName);

	/**
	 * Set the number of array element input pins directly.
	 *
	 * @param Node The UK2Node_MakeArray node to modify
	 * @param Graph The graph containing the node
	 * @param NumElements Target number of input pins
	 * @return true if successful, false otherwise
	 */
	static bool SetNumArrayElements(UK2Node* Node, UEdGraph* Graph, int32 NumElements);
};
