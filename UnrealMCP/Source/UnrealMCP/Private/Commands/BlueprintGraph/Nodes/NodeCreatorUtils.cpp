#include "Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "K2Node.h"
#include "EdGraph/EdGraph.h"
#include "Json.h"

bool FNodeCreatorUtils::InitializeK2Node(UK2Node* Node, UEdGraph* Graph)
{
	// Basic checks
	if (!Node || !Graph)
	{
		return false;
	}

	// 1. Allocate default pins
	Node->AllocateDefaultPins();

	// 2. Reconstruct the node (notifies Unreal of changes)
	Node->ReconstructNode();

	// 3. Notify the graph that something changed
	Graph->NotifyGraphChanged();

	return true;
}

void FNodeCreatorUtils::ExtractNodePosition(const TSharedPtr<FJsonObject>& Params, double& OutX, double& OutY)
{
	// Initialize default values
	OutX = 0.0;
	OutY = 0.0;

	// Check that Params is valid
	if (!Params.IsValid())
	{
		return;
	}

	// Try to get pos_x
	if (!Params->TryGetNumberField(TEXT("pos_x"), OutX))
	{
		OutX = 0.0;
	}

	// Try to get pos_y
	if (!Params->TryGetNumberField(TEXT("pos_y"), OutY))
	{
		OutY = 0.0;
	}
}
