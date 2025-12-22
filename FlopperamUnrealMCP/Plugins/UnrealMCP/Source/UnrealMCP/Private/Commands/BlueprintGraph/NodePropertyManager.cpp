#include "Commands/BlueprintGraph/NodePropertyManager.h"
#include "Commands/BlueprintGraph/Nodes/SwitchEnumEditor.h"
#include "Commands/BlueprintGraph/Nodes/ExecutionSequenceEditor.h"
#include "Commands/BlueprintGraph/Nodes/MakeArrayEditor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_MakeArray.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Select.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EditorAssetLibrary.h"
#include "Json.h"

TSharedPtr<FJsonObject> FNodePropertyManager::SetNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
	// Validate parameters
	if (!Params.IsValid())
	{
		return CreateErrorResponse(TEXT("Invalid parameters"));
	}

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString NodeID;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
	{
		return CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
	}

	// ===================================================
	// CHECK FOR SEMANTIC ACTION (new mode)
	// ===================================================
	FString Action;
	bool bHasAction = Params->HasField(TEXT("action"));

	if (bHasAction)
	{
		if (Params->TryGetStringField(TEXT("action"), Action))
		{
			if (!Action.IsEmpty())
			{
				// Semantic editing mode - delegate to EditNode
				return EditNode(Params);
			}
		}
	}

	// ===================================================
	// LEGACY MODE: Simple property modification
	// ===================================================
	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		UE_LOG(LogTemp, Error, TEXT("SetNodeProperty: Missing 'property_name' parameter"));
		return CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	if (!Params->HasField(TEXT("property_value")))
	{
		return CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
	}

	TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));

	// Get optional function name
	FString FunctionName;
	Params->TryGetStringField(TEXT("function_name"), FunctionName);

	// Load the Blueprint
	UBlueprint* Blueprint = LoadBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	// Get the appropriate graph
	UEdGraph* Graph = GetGraph(Blueprint, FunctionName);
	if (!Graph)
	{
		if (FunctionName.IsEmpty())
		{
			return CreateErrorResponse(TEXT("Blueprint has no event graph"));
		}
		else
		{
			return CreateErrorResponse(FString::Printf(TEXT("Function graph not found: %s"), *FunctionName));
		}
	}

	// Find the node
	UEdGraphNode* Node = FindNodeByID(Graph, NodeID);
	if (!Node)
	{
		return CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeID));
	}

	// Attempt to set property based on node type
	bool Success = false;

	// Try as Print node (UK2Node_CallFunction)
	UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(Node);
	if (CallFuncNode)
	{
		Success = SetPrintNodeProperty(CallFuncNode, PropertyName, PropertyValue);
	}

	// Try as Variable node
	if (!Success)
	{
		UK2Node* K2Node = Cast<UK2Node>(Node);
		if (K2Node)
		{
			Success = SetVariableNodeProperty(K2Node, PropertyName, PropertyValue);
		}
	}

	// Try generic properties
	if (!Success)
	{
		Success = SetGenericNodeProperty(Node, PropertyName, PropertyValue);
	}

	if (!Success)
	{
		return CreateErrorResponse(FString::Printf(
			TEXT("Failed to set property '%s' on node (property not supported or invalid value)"),
			*PropertyName));
	}

	// Notify changes
	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	UE_LOG(LogTemp, Display,
		TEXT("Successfully set '%s' on node '%s' in %s"),
		*PropertyName, *NodeID, *BlueprintName);

	return CreateSuccessResponse(PropertyName);
}

TSharedPtr<FJsonObject> FNodePropertyManager::EditNode(const TSharedPtr<FJsonObject>& Params)
{
	// Validate parameters
	if (!Params.IsValid())
	{
		return CreateErrorResponse(TEXT("Invalid parameters"));
	}

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString NodeID;
	if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
	{
		return CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
	}

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action))
	{
		return CreateErrorResponse(TEXT("Missing 'action' parameter"));
	}

	// Get optional function name
	FString FunctionName;
	Params->TryGetStringField(TEXT("function_name"), FunctionName);

	// Load the Blueprint
	UBlueprint* Blueprint = LoadBlueprint(BlueprintName);
	if (!Blueprint)
	{
		return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
	}

	// Get the appropriate graph
	UEdGraph* Graph = GetGraph(Blueprint, FunctionName);
	if (!Graph)
	{
		if (FunctionName.IsEmpty())
		{
			return CreateErrorResponse(TEXT("Blueprint has no event graph"));
		}
		else
		{
			return CreateErrorResponse(FString::Printf(TEXT("Function graph not found: %s"), *FunctionName));
		}
	}

	// Find the node
	UEdGraphNode* Node = FindNodeByID(Graph, NodeID);
	if (!Node)
	{
		return CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeID));
	}

	// Cast to K2Node (edit operations require K2Node)
	UK2Node* K2Node = Cast<UK2Node>(Node);
	if (!K2Node)
	{
		return CreateErrorResponse(TEXT("Node is not a K2Node (cannot edit this node type)"));
	}

	// Dispatch the edit action
	return DispatchEditAction(K2Node, Graph, Action, Params);
}

TSharedPtr<FJsonObject> FNodePropertyManager::DispatchEditAction(
	UK2Node* Node,
	UEdGraph* Graph,
	const FString& Action,
	const TSharedPtr<FJsonObject>& Params)
{
	if (!Node || !Graph || !Params.IsValid())
	{
		return CreateErrorResponse(TEXT("Invalid node or graph"));
	}

	// === SWITCHENUM: Set enum type and auto-generate pins ===
	if (Action.Equals(TEXT("set_enum_type"), ESearchCase::IgnoreCase))
	{
		FString EnumPath;
		if (!Params->TryGetStringField(TEXT("enum_type"), EnumPath))
		{
			if (!Params->TryGetStringField(TEXT("enum_path"), EnumPath))
			{
				return CreateErrorResponse(TEXT("Missing 'enum_type' or 'enum_path' parameter"));
			}
		}

		bool bSuccess = FSwitchEnumEditor::SetEnumType(Node, Graph, EnumPath);
		if (bSuccess)
		{
			TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), TEXT("set_enum_type"));
			Response->SetStringField(TEXT("enum_type"), EnumPath);
			return Response;
		}
		else
		{
			return CreateErrorResponse(FString::Printf(TEXT("Failed to set enum type: %s"), *EnumPath));
		}
	}

	// === EXECUTIONSEQUENCE/MAKEARRAY: Add pin ===
	if (Action.Equals(TEXT("add_pin"), ESearchCase::IgnoreCase))
	{
		bool bSuccess = FExecutionSequenceEditor::AddExecutionPin(Node, Graph);

		// If ExecutionSequence failed, try MakeArray
		if (!bSuccess)
		{
			bSuccess = FMakeArrayEditor::AddArrayElementPin(Node, Graph);
		}

		if (bSuccess)
		{
			TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), TEXT("add_pin"));
			return Response;
		}
		else
		{
			return CreateErrorResponse(TEXT("Failed to add pin"));
		}
	}

	// === EXECUTIONSEQUENCE/MAKEARRAY: Remove pin ===
	if (Action.Equals(TEXT("remove_pin"), ESearchCase::IgnoreCase))
	{
		FString PinName;
		if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
		{
			return CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
		}

		bool bSuccess = FExecutionSequenceEditor::RemoveExecutionPin(Node, Graph, PinName);
		if (!bSuccess)
		{
			// Try MakeArray if ExecutionSequence failed
			bSuccess = FMakeArrayEditor::RemoveArrayElementPin(Node, Graph, PinName);
		}

		if (bSuccess)
		{
			TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), TEXT("remove_pin"));
			Response->SetStringField(TEXT("pin_name"), PinName);
			return Response;
		}
		else
		{
			return CreateErrorResponse(FString::Printf(TEXT("Failed to remove pin: %s"), *PinName));
		}
	}

	// === MAKEARRAY: Set number of array elements ===
	if (Action.Equals(TEXT("set_num_elements"), ESearchCase::IgnoreCase))
	{
		double NumElementsDouble = 0.0;
		if (!Params->TryGetNumberField(TEXT("num_elements"), NumElementsDouble))
		{
			return CreateErrorResponse(TEXT("Missing 'num_elements' parameter"));
		}
		int32 NumElements = static_cast<int32>(NumElementsDouble);

		bool bSuccess = FMakeArrayEditor::SetNumArrayElements(Node, Graph, NumElements);
		if (bSuccess)
		{
			TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), TEXT("set_num_elements"));
			Response->SetNumberField(TEXT("num_elements"), NumElements);
			return Response;
		}
		else
		{
			return CreateErrorResponse(FString::Printf(TEXT("Failed to set array elements to %d"), NumElements));
		}
	}

	// Unknown action
	return CreateErrorResponse(FString::Printf(TEXT("Unknown action: %s"), *Action));
}

bool FNodePropertyManager::SetPrintNodeProperty(
	UK2Node_CallFunction* PrintNode,
	const FString& PropertyName,
	const TSharedPtr<FJsonValue>& Value)
{
	if (!PrintNode || !Value.IsValid())
	{
		return false;
	}

	// Handle "message" property
	if (PropertyName.Equals(TEXT("message"), ESearchCase::IgnoreCase))
	{
		FString MessageValue;
		if (Value->TryGetString(MessageValue))
		{
			UEdGraphPin* InStringPin = PrintNode->FindPin(TEXT("InString"));
			if (InStringPin)
			{
				InStringPin->DefaultValue = MessageValue;
				return true;
			}
		}
	}

	// Handle "duration" property
	if (PropertyName.Equals(TEXT("duration"), ESearchCase::IgnoreCase))
	{
		double DurationValue;
		if (Value->TryGetNumber(DurationValue))
		{
			UEdGraphPin* DurationPin = PrintNode->FindPin(TEXT("Duration"));
			if (DurationPin)
			{
				DurationPin->DefaultValue = FString::SanitizeFloat(DurationValue);
				return true;
			}
		}
	}

	return false;
}

bool FNodePropertyManager::SetVariableNodeProperty(
	UK2Node* VarNode,
	const FString& PropertyName,
	const TSharedPtr<FJsonValue>& Value)
{
	if (!VarNode || !Value.IsValid())
	{
		return false;
	}

	// Handle "variable_name" property
	if (PropertyName.Equals(TEXT("variable_name"), ESearchCase::IgnoreCase))
	{
		FString VarName;
		if (Value->TryGetString(VarName))
		{
			UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(VarNode);
			if (VarGet)
			{
				VarGet->VariableReference.SetSelfMember(FName(*VarName));
				VarGet->ReconstructNode();
				return true;
			}

			UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(VarNode);
			if (VarSet)
			{
				VarSet->VariableReference.SetSelfMember(FName(*VarName));
				VarSet->ReconstructNode();
				return true;
			}
		}
	}

	return false;
}

bool FNodePropertyManager::SetGenericNodeProperty(
	UEdGraphNode* Node,
	const FString& PropertyName,
	const TSharedPtr<FJsonValue>& Value)
{
	if (!Node || !Value.IsValid())
	{
		return false;
	}

	// Handle "pos_x" property
	if (PropertyName.Equals(TEXT("pos_x"), ESearchCase::IgnoreCase))
	{
		double PosX;
		if (Value->TryGetNumber(PosX))
		{
			Node->NodePosX = static_cast<int32>(PosX);
			return true;
		}
	}

	// Handle "pos_y" property
	if (PropertyName.Equals(TEXT("pos_y"), ESearchCase::IgnoreCase))
	{
		double PosY;
		if (Value->TryGetNumber(PosY))
		{
			Node->NodePosY = static_cast<int32>(PosY);
			return true;
		}
	}

	// Handle "comment" property
	if (PropertyName.Equals(TEXT("comment"), ESearchCase::IgnoreCase))
	{
		FString Comment;
		if (Value->TryGetString(Comment))
		{
			Node->NodeComment = Comment;
			return true;
		}
	}

	return false;
}

UEdGraph* FNodePropertyManager::GetGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// If no function name, return EventGraph
	if (FunctionName.IsEmpty())
	{
		if (Blueprint->UbergraphPages.Num() > 0)
		{
			return Blueprint->UbergraphPages[0];
		}
		return nullptr;
	}

	// Search in function graphs
	for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
	{
		if (FuncGraph && FuncGraph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			return FuncGraph;
		}
	}

	return nullptr;
}

UEdGraphNode* FNodePropertyManager::FindNodeByID(UEdGraph* Graph, const FString& NodeID)
{
	if (!Graph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		// Try matching by NodeGuid
		if (Node->NodeGuid.ToString().Equals(NodeID, ESearchCase::IgnoreCase))
		{
			return Node;
		}

		// Try matching by GetName()
		if (Node->GetName().Equals(NodeID, ESearchCase::IgnoreCase))
		{
			return Node;
		}
	}

	return nullptr;
}

UBlueprint* FNodePropertyManager::LoadBlueprint(const FString& BlueprintName)
{
	// Try direct path first
	FString BlueprintPath = BlueprintName;

	// If no path prefix, assume /Game/Blueprints/
	if (!BlueprintPath.StartsWith(TEXT("/")))
	{
		BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintName;
	}

	// Add .Blueprint suffix if not present
	if (!BlueprintPath.Contains(TEXT(".")))
	{
		BlueprintPath += TEXT(".") + FPaths::GetBaseFilename(BlueprintPath);
	}

	// Try to load the Blueprint
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);

	// If not found, try with UEditorAssetLibrary
	if (!BP)
	{
		FString AssetPath = BlueprintPath;
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
			BP = Cast<UBlueprint>(Asset);
		}
	}

	return BP;
}

TSharedPtr<FJsonObject> FNodePropertyManager::CreateSuccessResponse(const FString& PropertyName)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("updated_property"), PropertyName);
	return Response;
}

TSharedPtr<FJsonObject> FNodePropertyManager::CreateErrorResponse(const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), ErrorMessage);
	return Response;
}
