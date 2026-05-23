#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/BlueprintGraph/NodeManager.h"
#include "Commands/BlueprintGraph/BPConnector.h"
#include "Commands/BlueprintGraph/BPVariables.h"
#include "Commands/BlueprintGraph/EventManager.h"
#include "Commands/BlueprintGraph/NodeDeleter.h"
#include "Commands/BlueprintGraph/NodePropertyManager.h"
#include "Commands/BlueprintGraph/Function/FunctionManager.h"
#include "Commands/BlueprintGraph/Function/FunctionIO.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace
{
TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& Source)
{
    TSharedPtr<FJsonObject> Clone = MakeShared<FJsonObject>();
    if (Source.IsValid())
    {
        Clone->Values = Source->Values;
    }
    return Clone;
}

void CopyJsonFieldIfMissing(const TSharedPtr<FJsonObject>& Source, const TCHAR* FieldName, const TSharedPtr<FJsonObject>& Target)
{
    if (!Source.IsValid() || !Target.IsValid() || Target->HasField(FieldName))
    {
        return;
    }

    const TSharedPtr<FJsonValue>* Value = Source->Values.Find(FieldName);
    if (Value && Value->IsValid())
    {
        Target->SetField(FieldName, *Value);
    }
}

bool JsonBoolWithDefault(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const bool DefaultValue)
{
    bool Value = DefaultValue;
    if (Object.IsValid())
    {
        Object->TryGetBoolField(FieldName, Value);
    }
    return Value;
}

UBlueprint* LoadGraphCommandBlueprint(const FString& BlueprintName)
{
    if (UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName))
    {
        return Blueprint;
    }

    FString BlueprintPath = BlueprintName;
    if (!BlueprintPath.StartsWith(TEXT("/")))
    {
        BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintPath;
    }
    if (!BlueprintPath.Contains(TEXT(".")))
    {
        BlueprintPath += TEXT(".") + FPaths::GetBaseFilename(BlueprintPath);
    }

    return LoadObject<UBlueprint>(nullptr, *BlueprintPath);
}

UEdGraph* FindGraphForBatchCommand(UBlueprint* Blueprint, const FString& FunctionName)
{
    if (!Blueprint)
    {
        return nullptr;
    }

    if (FunctionName.IsEmpty() || FunctionName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
    {
        return Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
    }

    for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
    {
        if (FunctionGraph &&
            (FunctionGraph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase) ||
             FunctionGraph->GetFName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase)))
        {
            return FunctionGraph;
        }
    }

    return nullptr;
}

TSharedPtr<FJsonObject> ClearGraphBody(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);

    UBlueprint* Blueprint = LoadGraphCommandBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* Graph = FindGraphForBatchCommand(Blueprint, FunctionName);
    if (!Graph)
    {
        const FString ErrorMessage = FunctionName.IsEmpty()
            ? FString(TEXT("Blueprint has no event graph"))
            : FString::Printf(TEXT("Function graph not found: %s"), *FunctionName);
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    const bool bKeepFunctionEntry = JsonBoolWithDefault(Params, TEXT("keep_function_entry"), true);
    const bool bKeepFunctionResult = JsonBoolWithDefault(Params, TEXT("keep_function_result"), true);
    const bool bKeepEventNodes = JsonBoolWithDefault(Params, TEXT("keep_event_nodes"), false);
    const bool bBreakKeptLinks = JsonBoolWithDefault(Params, TEXT("break_kept_links"), true);

    Blueprint->Modify();
    Graph->Modify();

    int32 RemovedCount = 0;
    int32 KeptCount = 0;
    TArray<UEdGraphNode*> Nodes = Graph->Nodes;

    for (UEdGraphNode* Node : Nodes)
    {
        if (!Node)
        {
            continue;
        }

        const bool bKeepNode =
            (bKeepFunctionEntry && Cast<UK2Node_FunctionEntry>(Node)) ||
            (bKeepFunctionResult && Cast<UK2Node_FunctionResult>(Node)) ||
            (bKeepEventNodes && Cast<UK2Node_Event>(Node));

        Node->Modify();
        if (bKeepNode)
        {
            if (bBreakKeptLinks)
            {
                Node->BreakAllNodeLinks();
            }
            ++KeptCount;
            continue;
        }

        Node->BreakAllNodeLinks();
        Graph->RemoveNode(Node);
        ++RemovedCount;
    }

    Graph->NotifyGraphChanged();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("removed_nodes"), RemovedCount);
    Result->SetNumberField(TEXT("kept_nodes"), KeptCount);
    Result->SetStringField(TEXT("graph_name"), Graph->GetName());
    return Result;
}

FString ResolveNodeId(const TMap<FString, FString>& Aliases, const FString& Reference)
{
    if (const FString* NodeId = Aliases.Find(Reference))
    {
        return *NodeId;
    }
    return Reference;
}

bool TryGetAnyStringField(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& FieldNames, FString& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    for (const FString& FieldName : FieldNames)
    {
        if (Object->TryGetStringField(FieldName, OutValue))
        {
            return true;
        }
    }

    return false;
}

TSharedPtr<FJsonObject> BuildAddNodeParams(const TSharedPtr<FJsonObject>& BatchParams, const TSharedPtr<FJsonObject>& Operation)
{
    TSharedPtr<FJsonObject> AddParams = MakeShared<FJsonObject>();
    CopyJsonFieldIfMissing(Operation, TEXT("blueprint_name"), AddParams);
    CopyJsonFieldIfMissing(BatchParams, TEXT("blueprint_name"), AddParams);
    CopyJsonFieldIfMissing(Operation, TEXT("node_type"), AddParams);

    const TSharedPtr<FJsonObject>* NodeParamsObject = nullptr;
    TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
    if (Operation->TryGetObjectField(TEXT("node_params"), NodeParamsObject) && NodeParamsObject && NodeParamsObject->IsValid())
    {
        NodeParams = CloneJsonObject(*NodeParamsObject);
    }

    const TSet<FString> ReservedFields =
    {
        TEXT("op"),
        TEXT("command"),
        TEXT("alias"),
        TEXT("blueprint_name"),
        TEXT("node_type"),
        TEXT("node_params")
    };

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Operation->Values)
    {
        if (!ReservedFields.Contains(Pair.Key) && !NodeParams->HasField(Pair.Key))
        {
            NodeParams->SetField(Pair.Key, Pair.Value);
        }
    }

    CopyJsonFieldIfMissing(BatchParams, TEXT("function_name"), NodeParams);
    AddParams->SetObjectField(TEXT("node_params"), NodeParams);
    return AddParams;
}

bool SaveGraphBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }

    Blueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    const FString PackageName = FPackageName::ObjectPathToPackageName(Blueprint->GetPathName());
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, SaveArgs);
}
}

FEpicUnrealMCPBlueprintGraphCommands::FEpicUnrealMCPBlueprintGraphCommands()
{
}

FEpicUnrealMCPBlueprintGraphCommands::~FEpicUnrealMCPBlueprintGraphCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("add_blueprint_node"))
    {
        return HandleAddBlueprintNode(Params);
    }
    else if (CommandType == TEXT("execute_blueprint_graph_batch") ||
             CommandType == TEXT("batch_blueprint_graph_commands"))
    {
        return HandleBatchGraphCommands(Params);
    }
    else if (CommandType == TEXT("add_set_fields_in_struct_node") ||
             CommandType == TEXT("add_set_struct_fields_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("SetFieldsInStruct"));
    }
    else if (CommandType == TEXT("add_macro_instance_node") ||
             CommandType == TEXT("add_standard_macro_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("MacroInstance"));
    }
    else if (CommandType == TEXT("add_for_each_loop_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("ForEachLoop"));
    }
    else if (CommandType == TEXT("add_for_each_loop_with_break_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("ForEachLoopWithBreak"));
    }
    else if (CommandType == TEXT("add_reverse_for_each_loop_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("ReverseForEachLoop"));
    }
    else if (CommandType == TEXT("add_for_loop_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("ForLoop"));
    }
    else if (CommandType == TEXT("add_for_loop_with_break_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("ForLoopWithBreak"));
    }
    else if (CommandType == TEXT("add_while_loop_node"))
    {
        return HandleAddBlueprintNodeAlias(Params, TEXT("WhileLoop"));
    }
    else if (CommandType == TEXT("connect_nodes"))
    {
        return HandleConnectNodes(Params);
    }
    else if (CommandType == TEXT("break_pin_links"))
    {
        return HandleBreakPinLinks(Params);
    }
    else if (CommandType == TEXT("create_variable"))
    {
        return HandleCreateVariable(Params);
    }
    else if (CommandType == TEXT("set_blueprint_variable_properties"))
    {
        return HandleSetVariableProperties(Params);
    }
    else if (CommandType == TEXT("add_event_node"))
    {
        return HandleAddEventNode(Params);
    }
    else if (CommandType == TEXT("delete_node"))
    {
        return HandleDeleteNode(Params);
    }
    else if (CommandType == TEXT("set_node_property"))
    {
        return HandleSetNodeProperty(Params);
    }
    else if (CommandType == TEXT("create_function"))
    {
        return HandleCreateFunction(Params);
    }
    else if (CommandType == TEXT("create_override_function"))
    {
        return HandleCreateOverrideFunction(Params);
    }
    else if (CommandType == TEXT("add_function_input"))
    {
        return HandleAddFunctionInput(Params);
    }
    else if (CommandType == TEXT("add_function_output"))
    {
        return HandleAddFunctionOutput(Params);
    }
    else if (CommandType == TEXT("delete_function"))
    {
        return HandleDeleteFunction(Params);
    }
    else if (CommandType == TEXT("rename_function"))
    {
        return HandleRenameFunction(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint graph command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleAddBlueprintNode: Adding %s node to blueprint '%s'"), *NodeType, *BlueprintName);

    // Use the NodeManager to add the node
    return FBlueprintNodeManager::AddNode(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleAddBlueprintNodeAlias(const TSharedPtr<FJsonObject>& Params, const FString& NodeType)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid parameters"));
    }

    TSharedPtr<FJsonObject> AliasedParams = MakeShared<FJsonObject>();
    AliasedParams->Values = Params->Values;
    AliasedParams->SetStringField(TEXT("node_type"), NodeType);

    return HandleAddBlueprintNode(AliasedParams);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
    }

    FString SourcePinName;
    if (!Params->TryGetStringField(TEXT("source_pin_name"), SourcePinName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_pin_name' parameter"));
    }

    FString TargetNodeId;
    if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
    }

    FString TargetPinName;
    if (!Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleConnectNodes: Connecting %s.%s to %s.%s in blueprint '%s'"),
        *SourceNodeId, *SourcePinName, *TargetNodeId, *TargetPinName, *BlueprintName);

    // Use the BPConnector to connect the nodes
    return FBPConnector::ConnectNodes(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleBreakPinLinks(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeId;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    FString PinName;
    if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pin_name' parameter"));
    }

    UE_LOG(LogTemp, Display,
        TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleBreakPinLinks: Breaking links on %s.%s in blueprint '%s'"),
        *NodeId, *PinName, *BlueprintName);

    return FBPConnector::BreakPinLinks(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleCreateVariable(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_type' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleCreateVariable: Creating %s variable '%s' in blueprint '%s'"),
        *VariableType, *VariableName, *BlueprintName);

    // Use the BPVariables to create the variable
    return FBPVariables::CreateVariable(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleSetVariableProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleSetVariableProperties: Modifying variable '%s' in blueprint '%s'"),
        *VariableName, *BlueprintName);

    // Use the BPVariables to set the variable properties
    return FBPVariables::SetVariableProperties(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleAddEventNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleAddEventNode: Adding event '%s' to blueprint '%s'"),
        *EventName, *BlueprintName);

    // Use the EventManager to add the event node
    return FEventManager::AddEventNode(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleDeleteNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeID;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    UE_LOG(LogTemp, Display,
        TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleDeleteNode: Deleting node '%s' from blueprint '%s'"),
        *NodeID, *BlueprintName);

    return FNodeDeleter::DeleteNode(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleSetNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeID;
    if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    // Check if this is semantic mode (action parameter) or legacy mode (property_name)
    bool bHasAction = Params->HasField(TEXT("action"));

    if (bHasAction)
    {
        // Semantic mode - delegate directly to SetNodeProperty
        FString Action;
        Params->TryGetStringField(TEXT("action"), Action);
        UE_LOG(LogTemp, Display,
            TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleSetNodeProperty: Semantic mode - action '%s' on node '%s' in blueprint '%s'"),
            *Action, *NodeID, *BlueprintName);
    }
    else
    {
        // Legacy mode - require property_name
        FString PropertyName;
        if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
        }

        UE_LOG(LogTemp, Display,
            TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleSetNodeProperty: Legacy mode - Setting '%s' on node '%s' in blueprint '%s'"),
            *PropertyName, *NodeID, *BlueprintName);
    }

    return FNodePropertyManager::SetNodeProperty(Params);
}


TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleCreateFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleCreateFunction: Creating function '%s' in blueprint '%s'"),
        *FunctionName, *BlueprintName);

    return FFunctionManager::CreateFunction(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleCreateOverrideFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintName);
    }
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UFunction* OverrideFunction = nullptr;
    UClass* OverrideClass = FBlueprintEditorUtils::GetOverrideFunctionClass(Blueprint, FName(*FunctionName), &OverrideFunction);
    if (!OverrideClass || !OverrideFunction)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Override function not found: %s"), *FunctionName));
    }

    for (UEdGraph* ExistingGraph : Blueprint->FunctionGraphs)
    {
        if (ExistingGraph && ExistingGraph->GetFName() == FName(*FunctionName))
        {
            TSharedPtr<FJsonObject> ExistingResult = MakeShared<FJsonObject>();
            ExistingResult->SetBoolField(TEXT("success"), true);
            ExistingResult->SetBoolField(TEXT("created"), false);
            ExistingResult->SetStringField(TEXT("function_name"), FunctionName);
            ExistingResult->SetStringField(TEXT("graph_name"), ExistingGraph->GetName());
            ExistingResult->SetStringField(TEXT("signature_class"), OverrideClass->GetName());
            return ExistingResult;
        }
    }

    Blueprint->Modify();
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, false, OverrideClass);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    const bool bSaved = SaveGraphBlueprint(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("created"), true);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("function_name"), FunctionName);
    Result->SetStringField(TEXT("graph_name"), NewGraph ? NewGraph->GetName() : FunctionName);
    Result->SetStringField(TEXT("signature_class"), OverrideClass->GetName());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleAddFunctionInput: Adding input '%s' to function '%s' in blueprint '%s'"),
        *ParamName, *FunctionName, *BlueprintName);

    return FFunctionIO::AddFunctionInput(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleAddFunctionOutput(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleAddFunctionOutput: Adding output '%s' to function '%s' in blueprint '%s'"),
        *ParamName, *FunctionName, *BlueprintName);

    return FFunctionIO::AddFunctionOutput(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleDeleteFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleDeleteFunction: Deleting function '%s' from blueprint '%s'"),
        *FunctionName, *BlueprintName);

    return FFunctionManager::DeleteFunction(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleRenameFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString OldFunctionName;
    if (!Params->TryGetStringField(TEXT("old_function_name"), OldFunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_function_name' parameter"));
    }

    FString NewFunctionName;
    if (!Params->TryGetStringField(TEXT("new_function_name"), NewFunctionName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_function_name' parameter"));
    }

    UE_LOG(LogTemp, Display, TEXT("FEpicUnrealMCPBlueprintGraphCommands::HandleRenameFunction: Renaming function '%s' to '%s' in blueprint '%s'"),
        *OldFunctionName, *NewFunctionName, *BlueprintName);

    return FFunctionManager::RenameFunction(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintGraphCommands::HandleBatchGraphCommands(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid parameters"));
    }

    const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
    if (!Params->TryGetArrayField(TEXT("operations"), Operations))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'operations' array"));
    }

    const bool bStopOnError = JsonBoolWithDefault(Params, TEXT("stop_on_error"), true);
    TMap<FString, FString> NodeAliases;
    TArray<TSharedPtr<FJsonValue>> Results;
    bool bAllSucceeded = true;

    for (int32 Index = 0; Index < Operations->Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Operation = (*Operations)[Index].IsValid() ? (*Operations)[Index]->AsObject() : nullptr;
        TSharedPtr<FJsonObject> OperationResult = nullptr;
        FString OperationName;

        if (!Operation.IsValid())
        {
            OperationResult = FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Operation must be a JSON object"));
        }
        else if (!Operation->TryGetStringField(TEXT("op"), OperationName) &&
                 !Operation->TryGetStringField(TEXT("command"), OperationName))
        {
            OperationResult = FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Operation is missing 'op' or 'command'"));
        }
        else if (OperationName.Equals(TEXT("add_node"), ESearchCase::IgnoreCase) ||
                 OperationName.Equals(TEXT("add_blueprint_node"), ESearchCase::IgnoreCase))
        {
            OperationResult = HandleAddBlueprintNode(BuildAddNodeParams(Params, Operation));

            bool bOperationSuccess = false;
            if (OperationResult.IsValid() && OperationResult->TryGetBoolField(TEXT("success"), bOperationSuccess) && bOperationSuccess)
            {
                FString Alias;
                FString NodeId;
                if (Operation->TryGetStringField(TEXT("alias"), Alias) && OperationResult->TryGetStringField(TEXT("node_id"), NodeId))
                {
                    NodeAliases.Add(Alias, NodeId);
                }
            }
        }
        else if (OperationName.Equals(TEXT("connect"), ESearchCase::IgnoreCase) ||
                 OperationName.Equals(TEXT("connect_nodes"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> ConnectParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), ConnectParams);
            CopyJsonFieldIfMissing(Params, TEXT("function_name"), ConnectParams);

            FString SourceReference;
            FString TargetReference;
            FString SourcePin;
            FString TargetPin;
            if (TryGetAnyStringField(Operation, { TEXT("from"), TEXT("source"), TEXT("source_node"), TEXT("source_node_id") }, SourceReference))
            {
                ConnectParams->SetStringField(TEXT("source_node_id"), ResolveNodeId(NodeAliases, SourceReference));
            }
            if (TryGetAnyStringField(Operation, { TEXT("to"), TEXT("target"), TEXT("target_node"), TEXT("target_node_id") }, TargetReference))
            {
                ConnectParams->SetStringField(TEXT("target_node_id"), ResolveNodeId(NodeAliases, TargetReference));
            }
            if (TryGetAnyStringField(Operation, { TEXT("from_pin"), TEXT("source_pin"), TEXT("source_pin_name") }, SourcePin))
            {
                ConnectParams->SetStringField(TEXT("source_pin_name"), SourcePin);
            }
            if (TryGetAnyStringField(Operation, { TEXT("to_pin"), TEXT("target_pin"), TEXT("target_pin_name") }, TargetPin))
            {
                ConnectParams->SetStringField(TEXT("target_pin_name"), TargetPin);
            }
            if (!ConnectParams->HasField(TEXT("skip_compile")))
            {
                ConnectParams->SetBoolField(TEXT("skip_compile"), true);
            }

            OperationResult = HandleConnectNodes(ConnectParams);
        }
        else if (OperationName.Equals(TEXT("set_property"), ESearchCase::IgnoreCase) ||
                 OperationName.Equals(TEXT("set_node_property"), ESearchCase::IgnoreCase) ||
                 OperationName.Equals(TEXT("set_pin_default"), ESearchCase::IgnoreCase) ||
                 OperationName.Equals(TEXT("set_pin_default_object"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> PropertyParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), PropertyParams);
            CopyJsonFieldIfMissing(Params, TEXT("function_name"), PropertyParams);

            FString NodeReference;
            if (TryGetAnyStringField(Operation, { TEXT("node"), TEXT("node_id") }, NodeReference))
            {
                PropertyParams->SetStringField(TEXT("node_id"), ResolveNodeId(NodeAliases, NodeReference));
            }

            if (OperationName.Equals(TEXT("set_pin_default"), ESearchCase::IgnoreCase) ||
                OperationName.Equals(TEXT("set_pin_default_object"), ESearchCase::IgnoreCase))
            {
                FString PinName;
                if (Operation->TryGetStringField(TEXT("pin"), PinName))
                {
                    PropertyParams->SetStringField(TEXT("property_name"),
                        OperationName.Equals(TEXT("set_pin_default_object"), ESearchCase::IgnoreCase)
                            ? FString::Printf(TEXT("pin_default_object:%s"), *PinName)
                            : FString::Printf(TEXT("pin_default:%s"), *PinName));
                }

                if (!PropertyParams->HasField(TEXT("property_value")))
                {
                    if (TSharedPtr<FJsonValue> Value = Operation->TryGetField(TEXT("value")))
                    {
                        PropertyParams->SetField(TEXT("property_value"), Value);
                    }
                }
            }

            OperationResult = HandleSetNodeProperty(PropertyParams);
        }
        else if (OperationName.Equals(TEXT("break_pin_links"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> BreakParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), BreakParams);
            CopyJsonFieldIfMissing(Params, TEXT("function_name"), BreakParams);

            FString NodeReference;
            if (TryGetAnyStringField(Operation, { TEXT("node"), TEXT("node_id") }, NodeReference))
            {
                BreakParams->SetStringField(TEXT("node_id"), ResolveNodeId(NodeAliases, NodeReference));
            }

            OperationResult = HandleBreakPinLinks(BreakParams);
        }
        else if (OperationName.Equals(TEXT("delete_node"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> DeleteParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), DeleteParams);
            CopyJsonFieldIfMissing(Params, TEXT("function_name"), DeleteParams);

            FString NodeReference;
            if (TryGetAnyStringField(Operation, { TEXT("node"), TEXT("node_id") }, NodeReference))
            {
                DeleteParams->SetStringField(TEXT("node_id"), ResolveNodeId(NodeAliases, NodeReference));
            }

            OperationResult = HandleDeleteNode(DeleteParams);
        }
        else if (OperationName.Equals(TEXT("clear_graph"), ESearchCase::IgnoreCase) ||
                 OperationName.Equals(TEXT("clear_graph_body"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> ClearParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), ClearParams);
            CopyJsonFieldIfMissing(Params, TEXT("function_name"), ClearParams);
            OperationResult = ClearGraphBody(ClearParams);
        }
        else if (OperationName.Equals(TEXT("create_variable"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> CreateParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), CreateParams);
            OperationResult = HandleCreateVariable(CreateParams);
        }
        else if (OperationName.Equals(TEXT("set_blueprint_variable_properties"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> VariableParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), VariableParams);
            OperationResult = HandleSetVariableProperties(VariableParams);
        }
        else if (OperationName.Equals(TEXT("create_function"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> FunctionParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), FunctionParams);
            OperationResult = HandleCreateFunction(FunctionParams);
        }
        else if (OperationName.Equals(TEXT("create_override_function"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> FunctionParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), FunctionParams);
            OperationResult = HandleCreateOverrideFunction(FunctionParams);
        }
        else if (OperationName.Equals(TEXT("add_function_input"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> InputParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), InputParams);
            OperationResult = HandleAddFunctionInput(InputParams);
        }
        else if (OperationName.Equals(TEXT("add_function_output"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonObject> OutputParams = CloneJsonObject(Operation);
            CopyJsonFieldIfMissing(Params, TEXT("blueprint_name"), OutputParams);
            OperationResult = HandleAddFunctionOutput(OutputParams);
        }
        else
        {
            OperationResult = FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported batch operation: %s"), *OperationName));
        }

        bool bOperationSuccess = false;
        if (!OperationResult.IsValid() || !OperationResult->TryGetBoolField(TEXT("success"), bOperationSuccess) || !bOperationSuccess)
        {
            bAllSucceeded = false;
        }

        TSharedPtr<FJsonObject> WrappedResult = MakeShared<FJsonObject>();
        WrappedResult->SetNumberField(TEXT("index"), Index);
        WrappedResult->SetStringField(TEXT("op"), OperationName);
        WrappedResult->SetBoolField(TEXT("success"), bOperationSuccess);
        if (OperationResult.IsValid())
        {
            WrappedResult->SetObjectField(TEXT("result"), OperationResult);
            FString ErrorMessage;
            if (!bOperationSuccess && OperationResult->TryGetStringField(TEXT("error"), ErrorMessage))
            {
                WrappedResult->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        Results.Add(MakeShared<FJsonValueObject>(WrappedResult));

        if (!bOperationSuccess && bStopOnError)
        {
            break;
        }
    }

    bool bCompile = true;
    bool bSave = true;
    Params->TryGetBoolField(TEXT("compile"), bCompile);
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bSaved = false;
    bool bCompiled = false;
    FString BlueprintName;
    if ((bCompile || bSave) && Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        if (UBlueprint* Blueprint = LoadGraphCommandBlueprint(BlueprintName))
        {
            if (bSave)
            {
                bSaved = SaveGraphBlueprint(Blueprint);
                bCompiled = true;
            }
            else if (bCompile)
            {
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                bCompiled = true;
            }
        }
    }

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), bAllSucceeded);
    Response->SetArrayField(TEXT("results"), Results);
    Response->SetBoolField(TEXT("compiled"), bCompiled);
    Response->SetBoolField(TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> AliasResults;
    for (const TPair<FString, FString>& Pair : NodeAliases)
    {
        TSharedPtr<FJsonObject> AliasObject = MakeShared<FJsonObject>();
        AliasObject->SetStringField(TEXT("alias"), Pair.Key);
        AliasObject->SetStringField(TEXT("node_id"), Pair.Value);
        AliasResults.Add(MakeShared<FJsonValueObject>(AliasObject));
    }
    Response->SetArrayField(TEXT("aliases"), AliasResults);
    return Response;
}
