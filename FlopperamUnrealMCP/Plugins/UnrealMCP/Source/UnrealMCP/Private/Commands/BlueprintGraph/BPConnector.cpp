// BPConnector.cpp
// Created by: Zoscran
// Date: 2025-10-09
// Description: Blueprint node connection

#include "Commands/BlueprintGraph/BPConnector.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetEditorUtilities.h"

TSharedPtr<FJsonObject> FBPConnector::ConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // Extraire paramètres
    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
    FString SourcePinName = Params->GetStringField(TEXT("source_pin_name"));
    FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
    FString TargetPinName = Params->GetStringField(TEXT("target_pin_name"));

    // Charger Blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);

    if (!Blueprint)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Blueprint not found");
        return Result;
    }

    // Obtenir graphe
    UEdGraph* Graph = Blueprint->UbergraphPages[0];
    if (!Graph)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Graph not found");
        return Result;
    }

    // Trouver nœuds
    UK2Node* SourceNode = FindNodeById(Graph, SourceNodeId);
    UK2Node* TargetNode = FindNodeById(Graph, TargetNodeId);

    if (!SourceNode || !TargetNode)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node not found");
        return Result;
    }

    // Trouver pins
    UEdGraphPin* SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_Input);

    if (!SourcePin || !TargetPin)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Pin not found");
        return Result;
    }

    // Valider compatibilité
    if (!ArePinsCompatible(SourcePin, TargetPin))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Pins not compatible");
        return Result;
    }

    // Créer connexion
    SourcePin->MakeLinkTo(TargetPin);

    // Recompiler
    Blueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    // Retour
    Result->SetBoolField("success", true);

    TSharedPtr<FJsonObject> ConnectionInfo = MakeShared<FJsonObject>();
    ConnectionInfo->SetStringField("source_node", SourceNodeId);
    ConnectionInfo->SetStringField("source_pin", SourcePinName);
    ConnectionInfo->SetStringField("target_node", TargetNodeId);
    ConnectionInfo->SetStringField("target_pin", TargetPinName);
    ConnectionInfo->SetStringField("connection_type", SourcePin->PinType.PinCategory.ToString());

    Result->SetObjectField("connection", ConnectionInfo);

    return Result;
}

UK2Node* FBPConnector::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node* K2Node = Cast<UK2Node>(Node);
        if (K2Node && K2Node->GetName() == NodeId)
        {
            return K2Node;
        }
    }
    return nullptr;
}

UEdGraphPin* FBPConnector::FindPinByName(UK2Node* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && Pin->Direction == Direction)
        {
            return Pin;
        }
    }
    return nullptr;
}

bool FBPConnector::ArePinsCompatible(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
    if (SourcePin->Direction != EGPD_Output || TargetPin->Direction != EGPD_Input)
    {
        return false;
    }

    return SourcePin->PinType.PinCategory == TargetPin->PinType.PinCategory;
}