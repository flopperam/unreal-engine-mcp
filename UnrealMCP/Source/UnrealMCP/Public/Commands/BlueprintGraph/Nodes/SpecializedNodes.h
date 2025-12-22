// Header for creating specialized nodes (Timeline, GetDataTableRow, AddComponentByClass, Self, Knot)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Creator for Unreal Blueprint specialized nodes
 */
class FSpecializedNodeCreator
{
public:
	/**
	 * Creates a Get Data Table Row node (K2Node_GetDataTableRow)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, data_table
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateGetDataTableRowNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates an Add Component By Class node (K2Node_AddComponentByClass)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, component_class
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateAddComponentByClassNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Self node (K2Node_Self)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateSelfNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Construct Object From Class node (K2Node_ConstructObjectFromClass)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, class_type
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateConstructObjectNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Knot node (K2Node_Knot) - for reorganizing connections
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateKnotNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
