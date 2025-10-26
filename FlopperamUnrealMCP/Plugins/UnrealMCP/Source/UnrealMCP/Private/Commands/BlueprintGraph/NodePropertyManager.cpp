// Nom du fichier: NodePropertyManager.cpp
// Date de création: 2025-10-26
// Auteur: Zoscran
// Description: Implementation of Blueprint node property modification

#include "Commands/BlueprintGraph/NodePropertyManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorAssetLibrary.h"

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

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
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
