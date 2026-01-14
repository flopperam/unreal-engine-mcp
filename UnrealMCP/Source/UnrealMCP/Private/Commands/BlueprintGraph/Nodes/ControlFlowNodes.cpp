#include "Commands/BlueprintGraph/Nodes/ControlFlowNodes.h"
#include "Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_ExecutionSequence.h"
#include "EdGraphSchema_K2.h"
#include "Json.h"

UK2Node* FControlFlowNodeCreator::CreateBranchNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	// Create a Branch node using K2Node_IfThenElse
	UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
	if (!BranchNode)
	{
		return nullptr;
	}

	// Set position
	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	BranchNode->NodePosX = static_cast<int32>(PosX);
	BranchNode->NodePosY = static_cast<int32>(PosY);

	// Add to graph
	Graph->AddNode(BranchNode, false, false);
	BranchNode->CreateNewGuid();
	BranchNode->PostPlacedNewNode();

	// Initialize the node (AllocateDefaultPins + ReconstructNode + NotifyGraphChanged)
	FNodeCreatorUtils::InitializeK2Node(BranchNode, Graph);

	return BranchNode;
}

UK2Node* FControlFlowNodeCreator::CreateComparisonNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	// Create a Promotable Operator node for comparisons
	// Note: UK2Node_PromotableOperator in UE5.5 doesn't expose SetOperator() method
	// The node is created with default operator (Equal) and Unreal handles initialization
	UK2Node_PromotableOperator* ComparisonNode = NewObject<UK2Node_PromotableOperator>(Graph);
	if (!ComparisonNode)
	{
		return nullptr;
	}

	// Set position
	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	ComparisonNode->NodePosX = static_cast<int32>(PosX);
	ComparisonNode->NodePosY = static_cast<int32>(PosY);

	// Add to graph
	Graph->AddNode(ComparisonNode, false, false);
	ComparisonNode->CreateNewGuid();
	ComparisonNode->PostPlacedNewNode();

	// Initialize the node FIRST
	FNodeCreatorUtils::InitializeK2Node(ComparisonNode, Graph);

	// Set pin type if specified (int, float, string, bool, etc.) - AFTER initialization
	FString PinType;
	if (Params->TryGetStringField(TEXT("pin_type"), PinType))
	{
		// Find and update the A and B pins to the specified type
		UEdGraphPin* PinA = ComparisonNode->FindPin(TEXT("A"));
		UEdGraphPin* PinB = ComparisonNode->FindPin(TEXT("B"));

		if (PinA && PinB)
		{
			// Create a proper FEdGraphPinType structure
			FEdGraphPinType NewPinType;

			if (PinType.Equals(TEXT("int"), ESearchCase::IgnoreCase))
			{
				NewPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
			}
			else if (PinType.Equals(TEXT("float"), ESearchCase::IgnoreCase) || PinType.Equals(TEXT("double"), ESearchCase::IgnoreCase))
			{
				NewPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			}
			else if (PinType.Equals(TEXT("string"), ESearchCase::IgnoreCase))
			{
				NewPinType.PinCategory = UEdGraphSchema_K2::PC_String;
			}
			else if (PinType.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
			{
				NewPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			}
			else if (PinType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
			{
				NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
				NewPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
			}
			else if (PinType.Equals(TEXT("name"), ESearchCase::IgnoreCase))
			{
				NewPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
			}
			else if (PinType.Equals(TEXT("text"), ESearchCase::IgnoreCase))
			{
				NewPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
			}

			// Apply the entire pin type structure
			PinA->PinType = NewPinType;
			PinB->PinType = NewPinType;

			// Notify schema that pins have changed
			ComparisonNode->ReconstructNode();
			Graph->NotifyGraphChanged();
		}
	}

	return ComparisonNode;
}

UK2Node* FControlFlowNodeCreator::CreateSwitchNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	// Create a Switch node (K2Node_Switch)
	UK2Node_Switch* SwitchNode = NewObject<UK2Node_Switch>(Graph);
	if (!SwitchNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SwitchNode->NodePosX = static_cast<int32>(PosX);
	SwitchNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SwitchNode, false, false);
	SwitchNode->CreateNewGuid();
	SwitchNode->PostPlacedNewNode();
	FNodeCreatorUtils::InitializeK2Node(SwitchNode, Graph);

	return SwitchNode;
}

UK2Node* FControlFlowNodeCreator::CreateSwitchEnumNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	// Create a Switch on Enum node (K2Node_SwitchEnum)
	UK2Node_SwitchEnum* SwitchEnumNode = NewObject<UK2Node_SwitchEnum>(Graph);
	if (!SwitchEnumNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SwitchEnumNode->NodePosX = static_cast<int32>(PosX);
	SwitchEnumNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SwitchEnumNode, false, false);
	SwitchEnumNode->CreateNewGuid();
	SwitchEnumNode->PostPlacedNewNode();
	FNodeCreatorUtils::InitializeK2Node(SwitchEnumNode, Graph);

	return SwitchEnumNode;
}

UK2Node* FControlFlowNodeCreator::CreateSwitchIntegerNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	// Create a Switch on Integer node (K2Node_SwitchInteger)
	UK2Node_SwitchInteger* SwitchIntNode = NewObject<UK2Node_SwitchInteger>(Graph);
	if (!SwitchIntNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SwitchIntNode->NodePosX = static_cast<int32>(PosX);
	SwitchIntNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SwitchIntNode, false, false);
	SwitchIntNode->CreateNewGuid();
	SwitchIntNode->PostPlacedNewNode();
	FNodeCreatorUtils::InitializeK2Node(SwitchIntNode, Graph);

	return SwitchIntNode;
}

UK2Node* FControlFlowNodeCreator::CreateExecutionSequenceNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	// Create an Execution Sequence node (K2Node_ExecutionSequence)
	UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(Graph);
	if (!SeqNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SeqNode->NodePosX = static_cast<int32>(PosX);
	SeqNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SeqNode, false, false);
	SeqNode->CreateNewGuid();
	SeqNode->PostPlacedNewNode();
	FNodeCreatorUtils::InitializeK2Node(SeqNode, Graph);

	return SeqNode;
}
