/**
 * Filename: NodeManager.cpp
 * Creation Date: 2025-01-06
 * Author: Zoscran
 * Description: Implementation of Blueprint node management
 */

#include "Commands/BlueprintGraph/NodeManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "EditorAssetLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace BlueprintGraph
{
	TSharedPtr<FJsonObject> FNodeManager::AddNode(const TSharedPtr<FJsonObject>& Params)
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

		FString NodeType;
		if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
		{
			return CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
		}

		// Get optional node parameters
		const TSharedPtr<FJsonObject>* NodeParamsPtr;
		TSharedPtr<FJsonObject> NodeParams;
		if (Params->TryGetObjectField(TEXT("node_params"), NodeParamsPtr))
		{
			NodeParams = *NodeParamsPtr;
		}
		else
		{
			NodeParams = MakeShareable(new FJsonObject);
		}

		// Load the Blueprint
		UBlueprint* BP = LoadBlueprint(BlueprintName);
		if (!BP)
		{
			return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
		}

		// Get the event graph
		if (BP->UbergraphPages.Num() == 0)
		{
			return CreateErrorResponse(TEXT("Blueprint has no event graph"));
		}

		UEdGraph* Graph = BP->UbergraphPages[0];
		if (!Graph)
		{
			return CreateErrorResponse(TEXT("Failed to get Blueprint event graph"));
		}

		// Create node based on type
		UK2Node* NewNode = nullptr;

		if (NodeType.Equals(TEXT("Print"), ESearchCase::IgnoreCase))
		{
			NewNode = CreatePrintNode(Graph, NodeParams);
		}
		else if (NodeType.Equals(TEXT("Event"), ESearchCase::IgnoreCase))
		{
			NewNode = CreateEventNode(Graph, NodeParams);
		}
		else if (NodeType.Equals(TEXT("VariableGet"), ESearchCase::IgnoreCase))
		{
			NewNode = CreateVariableGetNode(Graph, NodeParams);
		}
		else if (NodeType.Equals(TEXT("VariableSet"), ESearchCase::IgnoreCase))
		{
			NewNode = CreateVariableSetNode(Graph, NodeParams);
		}
		else
		{
			return CreateErrorResponse(FString::Printf(TEXT("Unknown node type: %s"), *NodeType));
		}

		if (!NewNode)
		{
			return CreateErrorResponse(FString::Printf(TEXT("Failed to create %s node"), *NodeType));
		}

		// Notify changes
		Graph->NotifyGraphChanged();
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

		return CreateSuccessResponse(NewNode, NodeType);
	}

	UK2Node* FNodeManager::CreatePrintNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);
		if (!PrintNode)
		{
			return nullptr;
		}

		UFunction* PrintFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)
		);

		if (!PrintFunc)
		{
			return nullptr;
		}

		PrintNode->SetFromFunction(PrintFunc);

		// Set position
		double PosX = 0.0;
		double PosY = 0.0;
		Params->TryGetNumberField(TEXT("pos_x"), PosX);
		Params->TryGetNumberField(TEXT("pos_y"), PosY);

		PrintNode->NodePosX = static_cast<int32>(PosX);
		PrintNode->NodePosY = static_cast<int32>(PosY);

		PrintNode->AllocateDefaultPins();

		// Set message if provided
		FString Message;
		if (Params->TryGetStringField(TEXT("message"), Message))
		{
			UEdGraphPin* InStringPin = PrintNode->FindPin(TEXT("InString"));
			if (InStringPin)
			{
				InStringPin->DefaultValue = Message;
			}
		}

		Graph->AddNode(PrintNode, true, false);
		return PrintNode;
	}

	UK2Node_Event* FNodeManager::CreateEventNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
	{
		if (!Graph)
		{
			return nullptr;
		}

		FString EventType;
		if (!Params->TryGetStringField(TEXT("event_type"), EventType))
		{
			EventType = TEXT("BeginPlay");
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		if (!EventNode)
		{
			return nullptr;
		}

		if (EventType.Equals(TEXT("BeginPlay"), ESearchCase::IgnoreCase))
		{
			EventNode->EventReference.SetExternalMember(
				GET_FUNCTION_NAME_CHECKED(AActor, ReceiveBeginPlay),
				AActor::StaticClass()
			);
		}
		else if (EventType.Equals(TEXT("Tick"), ESearchCase::IgnoreCase))
		{
			EventNode->EventReference.SetExternalMember(
				GET_FUNCTION_NAME_CHECKED(AActor, ReceiveTick),
				AActor::StaticClass()
			);
		}
		else
		{
			EventNode->CustomFunctionName = FName(*EventType);
		}

		double PosX = 0.0;
		double PosY = 0.0;
		Params->TryGetNumberField(TEXT("pos_x"), PosX);
		Params->TryGetNumberField(TEXT("pos_y"), PosY);

		EventNode->NodePosX = static_cast<int32>(PosX);
		EventNode->NodePosY = static_cast<int32>(PosY);

		EventNode->AllocateDefaultPins();
		Graph->AddNode(EventNode, true, false);

		return EventNode;
	}

	UK2Node* FNodeManager::CreateVariableGetNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
	{
		if (!Graph)
		{
			return nullptr;
		}

		FString VariableName;
		if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
		{
			return nullptr;
		}

		UK2Node_VariableGet* VarGetNode = NewObject<UK2Node_VariableGet>(Graph);
		if (!VarGetNode)
		{
			return nullptr;
		}

		VarGetNode->VariableReference.SetSelfMember(FName(*VariableName));

		double PosX = 0.0;
		double PosY = 0.0;
		Params->TryGetNumberField(TEXT("pos_x"), PosX);
		Params->TryGetNumberField(TEXT("pos_y"), PosY);

		VarGetNode->NodePosX = static_cast<int32>(PosX);
		VarGetNode->NodePosY = static_cast<int32>(PosY);

		VarGetNode->AllocateDefaultPins();
		Graph->AddNode(VarGetNode, true, false);

		return VarGetNode;
	}

	UK2Node* FNodeManager::CreateVariableSetNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
	{
		if (!Graph)
		{
			return nullptr;
		}

		FString VariableName;
		if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
		{
			return nullptr;
		}

		UK2Node_VariableSet* VarSetNode = NewObject<UK2Node_VariableSet>(Graph);
		if (!VarSetNode)
		{
			return nullptr;
		}

		VarSetNode->VariableReference.SetSelfMember(FName(*VariableName));

		double PosX = 0.0;
		double PosY = 0.0;
		Params->TryGetNumberField(TEXT("pos_x"), PosX);
		Params->TryGetNumberField(TEXT("pos_y"), PosY);

		VarSetNode->NodePosX = static_cast<int32>(PosX);
		VarSetNode->NodePosY = static_cast<int32>(PosY);

		VarSetNode->AllocateDefaultPins();
		Graph->AddNode(VarSetNode, true, false);

		return VarSetNode;
	}

	UBlueprint* FNodeManager::LoadBlueprint(const FString& BlueprintName)
	{
		// Try direct path first
		FString BlueprintPath = BlueprintName;

		// If no path prefix, assume /Game/Blueprints/
		if (!BlueprintPath.StartsWith(TEXT("/")))
		{
			BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintPath;
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

	TSharedPtr<FJsonObject> FNodeManager::CreateSuccessResponse(const UK2Node* Node, const FString& NodeType)
	{
		TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
		Response->SetStringField(TEXT("node_type"), NodeType);
		Response->SetNumberField(TEXT("pos_x"), Node->NodePosX);
		Response->SetNumberField(TEXT("pos_y"), Node->NodePosY);
		return Response;
	}

	TSharedPtr<FJsonObject> FNodeManager::CreateErrorResponse(const FString& ErrorMessage)
	{
		TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), ErrorMessage);
		return Response;
	}
}
