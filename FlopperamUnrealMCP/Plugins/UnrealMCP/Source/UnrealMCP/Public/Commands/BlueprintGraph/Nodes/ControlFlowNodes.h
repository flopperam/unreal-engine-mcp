// Header for creating control flow nodes (Branch, Switch, Comparison, ExecutionSequence)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Creator for Unreal Blueprint control flow nodes
 */
class FControlFlowNodeCreator
{
public:
	/**
	 * Creates a Branch node (K2Node_IfThenElse)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateBranchNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Comparison node (K2Node_PromotableOperator)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing:
	 *                 - pos_x, pos_y: position
	 *                 - pin_type: pin types (int, float, string, bool, vector, name, text)
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateComparisonNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Switch node (K2Node_Switch)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateSwitchNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Switch on Enum node (K2Node_SwitchEnum)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, enum_type
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateSwitchEnumNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Switch on Integer node (K2Node_SwitchInteger)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateSwitchIntegerNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates an Execution Sequence node (K2Node_ExecutionSequence)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateExecutionSequenceNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
