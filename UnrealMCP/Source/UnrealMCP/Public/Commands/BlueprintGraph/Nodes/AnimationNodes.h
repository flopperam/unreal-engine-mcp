// Header for creating animation nodes (Timeline)

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

class UK2Node;

/**
 * Creator for Unreal Blueprint animation nodes
 */
class FAnimationNodeCreator
{
public:
	/**
	 * Creates a Timeline node (K2Node_Timeline)
	 * @param Graph - The graph to add the node to
	 * @param Params - JSON parameters containing pos_x, pos_y, timeline_name
	 * @return The created node or nullptr on error
	 */
	static UK2Node* CreateTimelineNode(UEdGraph* Graph, const TSharedPtr<class FJsonObject>& Params);
};
