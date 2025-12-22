#include "Commands/BlueprintGraph/Nodes/SwitchEnumEditor.h"
#include "K2Node_SwitchEnum.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool FSwitchEnumEditor::SetEnumType(UK2Node* Node, UEdGraph* Graph, const FString& EnumPath)
{
	if (!Node || !Graph)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid node or graph in SetEnumType"));
		return false;
	}

	// Cast to SwitchEnum node
	UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node);
	if (!SwitchNode)
	{
		UE_LOG(LogTemp, Error, TEXT("Node is not a UK2Node_SwitchEnum"));
		return false;
	}

	// Find the enum by path
	UEnum* TargetEnum = FindEnumByPath(EnumPath);
	if (!TargetEnum)
	{
		UE_LOG(LogTemp, Error, TEXT("Enum not found at path: %s"), *EnumPath);
		return false;
	}

	// Mark node as modified for undo/redo
	Node->Modify();

	// IMPORTANT: UE5.5 Property Setting Pattern
	// ==========================================
	// For simple properties that are public members of a K2Node:
	//   - DO access them directly: SwitchNode->PropertyName = Value
	//   - DO NOT use FindFProperty for public properties
	//   - Always call ReconstructNode() after changing properties that affect pins
	//
	// For SwitchEnum nodes:
	//   - Set the Enum property directly
	//   - Call ReconstructNode() to regenerate pins based on enum values
	//
	// This pattern is simple, efficient, and version-compatible.

	// Set the enum directly on the switch node
	// Access the Enum property and reconstruct the node to regenerate pins
	SwitchNode->Enum = TargetEnum;
	SwitchNode->ReconstructNode();

	// Mark the Blueprint as modified
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// Notify graph of changes
	Graph->NotifyGraphChanged();

	UE_LOG(LogTemp, Display, TEXT("Successfully set enum type on SwitchEnum node: %s"), *TargetEnum->GetName());
	return true;
}

UEnum* FSwitchEnumEditor::FindEnumByPath(const FString& EnumPath)
{
	if (EnumPath.IsEmpty())
	{
		return nullptr;
	}

	// Method 1: Try direct FindObject (works for both /Script/ and /Game/ paths)
	UEnum* FoundEnum = FindObject<UEnum>(nullptr, *EnumPath);
	if (FoundEnum)
	{
		return FoundEnum;
	}

	// Method 2: If path doesn't have a leading slash, add it
	if (!EnumPath.StartsWith(TEXT("/")))
	{
		FString FullPath = FString::Printf(TEXT("/Script/%s"), *EnumPath);

		FoundEnum = FindObject<UEnum>(nullptr, *FullPath);
		if (FoundEnum)
		{
			return FoundEnum;
		}

		// Try /Game path
		FullPath = FString::Printf(TEXT("/Game/%s"), *EnumPath);

		FoundEnum = FindObject<UEnum>(nullptr, *FullPath);
		if (FoundEnum)
		{
			return FoundEnum;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Could not find enum at path: %s"), *EnumPath);
	return nullptr;
}
