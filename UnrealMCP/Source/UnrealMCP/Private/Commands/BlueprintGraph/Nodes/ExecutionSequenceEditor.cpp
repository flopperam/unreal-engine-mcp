#include "Commands/BlueprintGraph/Nodes/ExecutionSequenceEditor.h"
#include "K2Node_ExecutionSequence.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool FExecutionSequenceEditor::AddExecutionPin(UK2Node* Node, UEdGraph* Graph)
{
	if (!Node || !Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid node or graph in AddExecutionPin"));
		return false;
	}

	// Cast to ExecutionSequence node
	UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(Node);
	if (!SeqNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddExecutionPin: Node is not a UK2Node_ExecutionSequence"));
		return false;
	}

	// Mark node as modified for undo/redo
	SeqNode->Modify();

	// IMPORTANT: UE5.5 Pin Management Pattern
	// ==========================================
	// DO NOT use FindFProperty to access internal properties like "NumOutputs"
	// Instead, use the PUBLIC API METHODS provided by the K2Node class:
	//
	// For ExecutionSequence nodes:
	//   - GetThenPinGivenIndex(int32 Index) - Get pin at specific index
	//   - InsertPinIntoExecutionNode(UEdGraphPin* PinToInsertBefore, EPinInsertPosition) - Add new pin
	//   - RemovePinFromExecutionNode(UEdGraphPin* TargetPin) - Remove existing pin
	//   - CanRemoveExecutionPin() - Check if pin can be removed
	//
	// This pattern ensures compatibility across UE versions and respects Unreal's API design.

	// Find the last "then" pin to insert after it
	UEdGraphPin* LastThenPin = nullptr;
	int32 PinIndex = 0;
	while (UEdGraphPin* ThenPin = SeqNode->GetThenPinGivenIndex(PinIndex))
	{
		LastThenPin = ThenPin;
		PinIndex++;
	}

	if (LastThenPin)
	{
		// Insert a new pin after the last one using the public API
		SeqNode->InsertPinIntoExecutionNode(LastThenPin, EPinInsertPosition::After);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddExecutionPin: Could not find any existing 'then' pins"));
		return false;
	}

	// Mark the Blueprint as modified
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// Notify graph of changes
	Graph->NotifyGraphChanged();

	return true;
}

bool FExecutionSequenceEditor::RemoveExecutionPin(UK2Node* Node, UEdGraph* Graph, const FString& PinName)
{
	if (!Node || !Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid node or graph in RemoveExecutionPin"));
		return false;
	}

	// Cast to ExecutionSequence node
	UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(Node);
	if (!SeqNode)
	{
		UE_LOG(LogTemp, Error, TEXT("Node is not a UK2Node_ExecutionSequence"));
		return false;
	}

	// Find the pin by name
	UEdGraphPin* PinToRemove = SeqNode->FindPin(*PinName);
	if (!PinToRemove)
	{
		UE_LOG(LogTemp, Error, TEXT("Pin not found: %s"), *PinName);
		return false;
	}

	// Check if we can remove this pin
	if (!SeqNode->CanRemoveExecutionPin())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot remove the last execution pin"));
		return false;
	}

	// Mark node as modified for undo/redo
	SeqNode->Modify();

	// Break all connections on this pin
	PinToRemove->BreakAllPinLinks();

	// Use the public API method to remove the pin
	SeqNode->RemovePinFromExecutionNode(PinToRemove);

	// Mark the Blueprint as modified
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// Notify graph of changes
	Graph->NotifyGraphChanged();

	return true;
}

bool FExecutionSequenceEditor::SetNumExecutionPins(UK2Node* Node, UEdGraph* Graph, int32 NumPins)
{
	if (!Node || !Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid node or graph in SetNumExecutionPins"));
		return false;
	}

	// Cast to ExecutionSequence node
	UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(Node);
	if (!SeqNode)
	{
		UE_LOG(LogTemp, Error, TEXT("Node is not a UK2Node_ExecutionSequence"));
		return false;
	}

	// Validate pin count
	if (NumPins < 1)
	{
		UE_LOG(LogTemp, Error, TEXT("ExecutionSequence must have at least 1 output pin"));
		return false;
	}

	// Mark node as modified for undo/redo
	SeqNode->Modify();

	// Count current execution pins
	int32 CurrentPinCount = 0;
	int32 PinIndex = 0;
	while (SeqNode->GetThenPinGivenIndex(PinIndex))
	{
		CurrentPinCount++;
		PinIndex++;
	}

	// Add pins if needed
	while (CurrentPinCount < NumPins)
	{
		// Find the last pin
		UEdGraphPin* LastThenPin = SeqNode->GetThenPinGivenIndex(CurrentPinCount - 1);
		if (LastThenPin)
		{
			SeqNode->InsertPinIntoExecutionNode(LastThenPin, EPinInsertPosition::After);
		}
		CurrentPinCount++;
	}

	// Remove pins if needed
	while (CurrentPinCount > NumPins)
	{
		// Get the last pin and remove it
		UEdGraphPin* LastThenPin = SeqNode->GetThenPinGivenIndex(CurrentPinCount - 1);
		if (LastThenPin)
		{
			LastThenPin->BreakAllPinLinks();
			SeqNode->RemovePinFromExecutionNode(LastThenPin);
		}
		CurrentPinCount--;
	}

	// Mark the Blueprint as modified
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// Notify graph of changes
	Graph->NotifyGraphChanged();

	return true;
}
