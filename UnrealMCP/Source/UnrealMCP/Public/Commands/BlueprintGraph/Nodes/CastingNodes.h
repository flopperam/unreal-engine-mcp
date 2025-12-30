// Header for creating casting nodes (DynamicCast, ClassDynamicCast, CastByteToEnum)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Creator for Unreal Blueprint casting nodes
 */
class FCastingNodeCreator
{
public:
	/**
	 * Creates a Dynamic Cast node (K2Node_DynamicCast)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, target_class
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateDynamicCastNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Class Dynamic Cast node (K2Node_ClassDynamicCast)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, target_class
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateClassDynamicCastNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);

	/**
	 * Creates a Cast Byte To Enum node (K2Node_CastByteToEnum)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, enum_type
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateCastByteToEnumNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
