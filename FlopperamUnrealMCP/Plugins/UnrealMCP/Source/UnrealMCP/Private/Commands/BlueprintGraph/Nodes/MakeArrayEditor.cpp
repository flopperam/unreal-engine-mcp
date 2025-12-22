#include "Commands/BlueprintGraph/Nodes/MakeArrayEditor.h"
#include "K2Node_MakeArray.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool FMakeArrayEditor::AddArrayElementPin(UK2Node* Node, UEdGraph* Graph)
{
	if (!Node || !Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid node or graph in AddArrayElementPin"));
		return false;
	}

	// Cast to MakeArray node
	UK2Node_MakeArray* MakeArrayNode = Cast<UK2Node_MakeArray>(Node);
	if (!MakeArrayNode)
	{
		UE_LOG(LogTemp, Error, TEXT("Node is not a UK2Node_MakeArray"));
		return false;
	}

	// Mark node as modified for undo/redo
	MakeArrayNode->Modify();

	// IMPORTANT: UE5.5 Pin Management Pattern
	// ==========================================
	// DO NOT use FindFProperty to access internal properties like "NumInputs"
	// Instead, use the PUBLIC API METHODS from IK2Node_AddPinInterface:
	//
	// For MakeArray/MakeContainer nodes:
	//   - AddInputPin() - Add a new input element pin
	//   - RemoveInputPin(UEdGraphPin* Pin) - Remove an existing input pin
	//
	// MakeArray inherits from UK2Node_MakeContainer which implements IK2Node_AddPinInterface
	// This pattern ensures compatibility across UE versions and respects Unreal's API design.

	// Use the public API method: AddInputPin (from IK2Node_AddPinInterface)
	MakeArrayNode->AddInputPin();

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

bool FMakeArrayEditor::RemoveArrayElementPin(UK2Node* Node, UEdGraph* Graph, const FString& PinName)
{
	if (!Node || !Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid node or graph in RemoveArrayElementPin"));
		return false;
	}

	// Cast to MakeArray node
	UK2Node_MakeArray* MakeArrayNode = Cast<UK2Node_MakeArray>(Node);
	if (!MakeArrayNode)
	{
		UE_LOG(LogTemp, Error, TEXT("Node is not a UK2Node_MakeArray"));
		return false;
	}

	// Find the pin by name
	UEdGraphPin* PinToRemove = MakeArrayNode->FindPin(*PinName);
	if (!PinToRemove)
	{
		UE_LOG(LogTemp, Error, TEXT("Pin not found: %s"), *PinName);
		return false;
	}

	// Enforce minimum - must keep at least one input pin
	int32 InputPinCount = 0;
	for (UEdGraphPin* Pin : MakeArrayNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PC_Wildcard)
		{
			InputPinCount++;
		}
	}

	if (InputPinCount <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot remove the last array element pin"));
		return false;
	}

	// Mark node as modified for undo/redo
	MakeArrayNode->Modify();

	// Break all connections on this pin
	PinToRemove->BreakAllPinLinks();

	// Use the public API method: RemoveInputPin
	MakeArrayNode->RemoveInputPin(PinToRemove);

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

bool FMakeArrayEditor::SetNumArrayElements(UK2Node* Node, UEdGraph* Graph, int32 NumElements)
{
	if (!Node || !Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid node or graph in SetNumArrayElements"));
		return false;
	}

	// Cast to MakeArray node
	UK2Node_MakeArray* MakeArrayNode = Cast<UK2Node_MakeArray>(Node);
	if (!MakeArrayNode)
	{
		UE_LOG(LogTemp, Error, TEXT("Node is not a UK2Node_MakeArray"));
		return false;
	}

	// Validate element count
	if (NumElements < 1)
	{
		UE_LOG(LogTemp, Error, TEXT("MakeArray must have at least 1 element pin"));
		return false;
	}

	// Mark node as modified for undo/redo
	MakeArrayNode->Modify();

	// Count current input pins (excluding output pin)
	int32 CurrentElementCount = 0;
	for (UEdGraphPin* Pin : MakeArrayNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PC_Wildcard)
		{
			CurrentElementCount++;
		}
	}

	// Add pins if needed
	while (CurrentElementCount < NumElements)
	{
		MakeArrayNode->AddInputPin();
		CurrentElementCount++;
	}

	// Remove pins if needed
	while (CurrentElementCount > NumElements)
	{
		// Find the last input pin and remove it
		UEdGraphPin* LastInputPin = nullptr;
		for (UEdGraphPin* Pin : MakeArrayNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PC_Wildcard)
			{
				LastInputPin = Pin;
			}
		}

		if (LastInputPin)
		{
			LastInputPin->BreakAllPinLinks();
			MakeArrayNode->RemoveInputPin(LastInputPin);
		}
		CurrentElementCount--;
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
