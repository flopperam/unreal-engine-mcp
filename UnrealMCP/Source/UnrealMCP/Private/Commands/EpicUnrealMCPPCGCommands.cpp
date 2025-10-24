#include "Commands/EpicUnrealMCPPCGCommands.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"
#include "EditorAssetLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"

// Main router for PCG commands
TSharedPtr<FJsonObject> FEpicUnrealMCPPCGCommands::HandleCommand(const FString &CommandType, const TSharedPtr<FJsonObject> &Params)
{
    if (CommandType == TEXT("analyze_pcg_graph"))
    {
        return AnalyzePCGGraph(Params);
    }
    else if (CommandType == TEXT("update_pcg_graph_parameter"))
    {
        return UpdatePCGGraphParameter(Params);
    }
    else if (CommandType == TEXT("create_pcg_graph"))
    {
        return CreatePCGGraph(Params);
    }

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), TEXT("Unknown PCG command"));
    return Response;
}

// Phase 1: Read and analyze a PCG graph
TSharedPtr<FJsonObject> FEpicUnrealMCPPCGCommands::AnalyzePCGGraph(const TSharedPtr<FJsonObject> &Params)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    FString GraphPath = Params->GetStringField(TEXT("graph_path"));

    UPCGGraph *Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
    if (!Graph)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load PCG Graph at path: %s"), *GraphPath));
        return Response;
    }

    TArray<TSharedPtr<FJsonValue>> NodesJson;
    for (UPCGNode *Node : Graph->GetNodes())
    {
        TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject);
        NodeJson->SetStringField(TEXT("node_title"), Node->GetNodeTitle().ToString());
        NodeJson->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());

        // Extract exposed parameters from the node's settings
        if (UPCGSettings *Settings = Node->GetSettings())
        {
            TArray<TSharedPtr<FJsonValue>> ParamsJson;
            for (TFieldIterator<FProperty> PropIt(Settings->GetClass()); PropIt; ++PropIt)
            {
                FProperty *Property = *PropIt;
                if (Property->HasAnyPropertyFlags(CPF_Edit)) // Check if property is editable
                {
                    TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
                    ParamJson->SetStringField(TEXT("name"), Property->GetName());
                    ParamJson->SetStringField(TEXT("type"), Property->GetCPPType());
                    // We can expand this to get the actual value later
                    ParamsJson.Add(MakeShareable(new FJsonValueObject(ParamJson)));
                }
            }
            NodeJson->SetArrayField(TEXT("parameters"), ParamsJson);
        }
        NodesJson.Add(MakeShareable(new FJsonValueObject(NodeJson)));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("nodes"), NodesJson);
    return Response;
}

// Phase 2: Update a parameter on a PCG graph node
TSharedPtr<FJsonObject> FEpicUnrealMCPPCGCommands::UpdatePCGGraphParameter(const TSharedPtr<FJsonObject> &Params)
{
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    FString GraphPath = Params->GetStringField(TEXT("graph_path"));
    FString NodeTitle = Params->GetStringField(TEXT("node_title"));
    FString ParameterName = Params->GetStringField(TEXT("parameter_name"));

    UPCGGraph *Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
    if (!Graph)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("PCG Graph not found."));
        return Response;
    }

    UPCGNode *TargetNode = nullptr;
    for (UPCGNode *Node : Graph->GetNodes())
    {
        if (Node->GetNodeTitle().ToString() == NodeTitle)
        {
            TargetNode = Node;
            break;
        }
    }

    if (!TargetNode || !TargetNode->GetSettings())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Node not found or has no settings."));
        return Response;
    }

    // Find property and set its value
    FProperty *Property = TargetNode->GetSettings()->GetClass()->FindPropertyByName(*ParameterName);
    if (!Property)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Parameter not found on node."));
        return Response;
    }

    // This is a simplified example for a float. A full implementation needs type switching.
    if (FNumericProperty *NumericProperty = CastField<FNumericProperty>(Property))
    {
        double NewValue = Params->GetNumberField(TEXT("new_value"));
        NumericProperty->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(TargetNode->GetSettings()), NewValue);

        Graph->Modify();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s to %f"), *ParameterName, NewValue));
    }
    else
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Parameter type not supported for modification yet."));
    }

    return Response;
}

// Phase 3: Create a new PCG graph asset from a JSON description
TSharedPtr<FJsonObject> FEpicUnrealMCPPCGCommands::CreatePCGGraph(const TSharedPtr<FJsonObject> &Params)
{
    // Note: Creating nodes and connections programmatically is highly complex and involves
    // graph schemas and transaction buffers. This is a placeholder for a future, more
    // advanced implementation. For now, it just creates an empty graph asset.

    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    FString NewGraphName = Params->GetStringField(TEXT("graph_name"));
    FString PackagePath = TEXT("/Game/PCG/");

    FString FullPath = PackagePath + NewGraphName;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Asset with this name already exists."));
        return Response;
    }

    UPackage *Package = CreatePackage(*FullPath);
    UPCGGraph *NewGraph = NewObject<UPCGGraph>(Package, *NewGraphName, RF_Public | RF_Standalone);

    if (NewGraph)
    {
        FAssetRegistryModule::AssetCreated(NewGraph);
        NewGraph->MarkPackageDirty();
        UEditorAssetLibrary::SaveAsset(FullPath, false);

        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("asset_path"), NewGraph->GetPathName());
    }
    else
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Failed to create new PCG Graph asset."));
    }

    return Response;
}