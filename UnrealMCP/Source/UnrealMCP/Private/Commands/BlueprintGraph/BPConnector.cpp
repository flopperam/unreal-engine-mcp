#include "Commands/BlueprintGraph/BPConnector.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"

namespace
{
UBlueprint* LoadConnectorBlueprint(const FString& BlueprintName)
{
    FString BlueprintPath = BlueprintName;
    if (!BlueprintPath.StartsWith(TEXT("/")))
    {
        BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintPath;
    }

    if (!BlueprintPath.Contains(TEXT(".")))
    {
        BlueprintPath += TEXT(".") + FPaths::GetBaseFilename(BlueprintPath);
    }

    if (UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath))
    {
        return Blueprint;
    }

    if (UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        return Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    }

    return nullptr;
}

UEdGraph* FindConnectorGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
    if (!Blueprint)
    {
        return nullptr;
    }

    if (!FunctionName.IsEmpty())
    {
        for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
        {
            if (FuncGraph && (FuncGraph->GetFName().ToString() == FunctionName ||
                              (FuncGraph->GetOuter() && FuncGraph->GetOuter()->GetFName().ToString() == FunctionName)))
            {
                return FuncGraph;
            }
        }

        for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
        {
            if (FuncGraph && FuncGraph->GetFName().ToString().Contains(FunctionName))
            {
                return FuncGraph;
            }
        }

        return nullptr;
    }

    return Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}
}

TSharedPtr<FJsonObject> FBPConnector::ConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // Extraire paramètres
    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
    FString SourcePinName = Params->GetStringField(TEXT("source_pin_name"));
    FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
    FString TargetPinName = Params->GetStringField(TEXT("target_pin_name"));

    FString FunctionName;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);

    // Charger Blueprint - handle both full paths and simple names
    UBlueprint* Blueprint = LoadConnectorBlueprint(BlueprintName);

    if (!Blueprint)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Blueprint not found");
        return Result;
    }

    UEdGraph* Graph = FindConnectorGraph(Blueprint, FunctionName);

    if (!Graph)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Graph not found");
        return Result;
    }

    // Find nodes
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

    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    if (!Schema || !Schema->TryCreateConnection(SourcePin, TargetPin))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Pins not compatible");
        return Result;
    }

    bool bSkipCompile = false;
    Params->TryGetBoolField(TEXT("skip_compile"), bSkipCompile);

    Blueprint->MarkPackageDirty();
    Graph->NotifyGraphChanged();
    if (!bSkipCompile)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }

    // Return
    Result->SetBoolField("success", true);
    Result->SetBoolField("compiled", !bSkipCompile);

    TSharedPtr<FJsonObject> ConnectionInfo = MakeShared<FJsonObject>();
    ConnectionInfo->SetStringField("source_node", SourceNodeId);
    ConnectionInfo->SetStringField("source_pin", SourcePinName);
    ConnectionInfo->SetStringField("target_node", TargetNodeId);
    ConnectionInfo->SetStringField("target_pin", TargetPinName);
    ConnectionInfo->SetStringField("connection_type", SourcePin->PinType.PinCategory.ToString());

    Result->SetObjectField("connection", ConnectionInfo);

    return Result;
}

TSharedPtr<FJsonObject> FBPConnector::BreakPinLinks(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    FString NodeId = Params->GetStringField(TEXT("node_id"));
    FString PinName = Params->GetStringField(TEXT("pin_name"));

    FString FunctionName;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);

    FString PinDirectionString;
    Params->TryGetStringField(TEXT("pin_direction"), PinDirectionString);
    EEdGraphPinDirection Direction = PinDirectionString.Equals(TEXT("input"), ESearchCase::IgnoreCase) ? EGPD_Input : EGPD_Output;

    UBlueprint* Blueprint = LoadConnectorBlueprint(BlueprintName);
    if (!Blueprint)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Blueprint not found");
        return Result;
    }

    UEdGraph* Graph = FindConnectorGraph(Blueprint, FunctionName);
    if (!Graph)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Graph not found");
        return Result;
    }

    UK2Node* Node = FindNodeById(Graph, NodeId);
    if (!Node)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node not found");
        return Result;
    }

    UEdGraphPin* Pin = FindPinByName(Node, PinName, Direction);
    if (!Pin)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Pin not found");
        return Result;
    }

    const int32 RemovedLinks = Pin->LinkedTo.Num();
    if (const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>())
    {
        Schema->BreakPinLinks(*Pin, true);
    }
    else
    {
        Pin->BreakAllPinLinks(true);
    }

    Blueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    Result->SetBoolField("success", true);
    Result->SetStringField("node", NodeId);
    Result->SetStringField("pin", PinName);
    Result->SetNumberField("removed_links", RemovedLinks);
    return Result;
}

UK2Node* FBPConnector::FindNodeById(UEdGraph* Graph, const FString& NodeId)
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

        // Try matching by NodeGuid first
        if (Node->NodeGuid.ToString().Equals(NodeId, ESearchCase::IgnoreCase))
        {
            UK2Node* K2Node = Cast<UK2Node>(Node);
            return K2Node;  // Return even if nullptr (caller will handle)
        }

        // Try matching by GetName()
        if (Node->GetName().Equals(NodeId, ESearchCase::IgnoreCase))
        {
            UK2Node* K2Node = Cast<UK2Node>(Node);
            return K2Node;  // Return even if nullptr (caller will handle)
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

    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    return Schema && Schema->CanCreateConnection(SourcePin, TargetPin).Response != CONNECT_RESPONSE_DISALLOW;
}
