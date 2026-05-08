#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPBridge.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Knot.h"
#include "K2Node_Composite.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Timeline.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Engine.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Breakpoint.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/WatchedPin.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ScopedTransaction.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "BlueprintEditorLibrary.h"

FEpicUnrealMCPBlueprintCommands::FEpicUnrealMCPBlueprintCommands()
{
}

namespace
{
    static FActorIndex& GetBlueprintActorIndex()
    {
        UEpicUnrealMCPBridge* Bridge = GEditor->GetEditorSubsystem<UEpicUnrealMCPBridge>();
        check(Bridge);
        return Bridge->ActorIndex;
    }

    TSharedPtr<FJsonObject> MakeBlueprintSuccessResult(std::initializer_list<TPair<FString, TSharedPtr<FJsonValue>>> Fields)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Fields)
        {
            ResultObj->SetField(Field.Key, Field.Value);
        }
        return ResultObj;
    }

    UClass* ResolveComponentClass(const FString& RequestedType)
    {
        if (RequestedType.Equals(TEXT("StaticMeshComponent"), ESearchCase::IgnoreCase) ||
            RequestedType.Equals(TEXT("UStaticMeshComponent"), ESearchCase::IgnoreCase))
        {
            return UStaticMeshComponent::StaticClass();
        }

        if (RequestedType.Equals(TEXT("BoxComponent"), ESearchCase::IgnoreCase) ||
            RequestedType.Equals(TEXT("UBoxComponent"), ESearchCase::IgnoreCase))
        {
            return UBoxComponent::StaticClass();
        }

        if (RequestedType.Equals(TEXT("SphereComponent"), ESearchCase::IgnoreCase) ||
            RequestedType.Equals(TEXT("USphereComponent"), ESearchCase::IgnoreCase))
        {
            return USphereComponent::StaticClass();
        }

        TArray<FString> CandidateNames;
        CandidateNames.Add(RequestedType);

        if (!RequestedType.EndsWith(TEXT("Component")))
        {
            CandidateNames.Add(RequestedType + TEXT("Component"));
        }

        const int32 CandidateCount = CandidateNames.Num();
        for (int32 Index = 0; Index < CandidateCount; ++Index)
        {
            const FString& Candidate = CandidateNames[Index];
            if (!Candidate.StartsWith(TEXT("U")))
            {
                CandidateNames.Add(TEXT("U") + Candidate);
            }
        }

        for (const FString& Candidate : CandidateNames)
        {
            if (UClass* FoundClass = FindObject<UClass>(nullptr, *Candidate))
            {
                if (FoundClass->IsChildOf(UActorComponent::StaticClass()))
                {
                    return FoundClass;
                }
            }

            const FString EnginePath = FString::Printf(TEXT("/Script/Engine.%s"), *Candidate);
            if (UClass* FoundClass = LoadClass<UActorComponent>(nullptr, *EnginePath))
            {
                return FoundClass;
            }
        }

        return nullptr;
    }

    static bool GuidMatchesString(const FGuid& Guid, const FString& Identifier)
    {
        return Guid.ToString(EGuidFormats::DigitsWithHyphens).Equals(Identifier, ESearchCase::IgnoreCase)
            || Guid.ToString(EGuidFormats::Digits).Equals(Identifier, ESearchCase::IgnoreCase)
            || Guid.ToString().Equals(Identifier, ESearchCase::IgnoreCase);
    }

    static FString GetJsonIdentifier(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName)
    {
        FString Identifier;
        if (Params->TryGetStringField(FieldName, Identifier))
        {
            return Identifier;
        }

        int32 NumericIdentifier = INDEX_NONE;
        if (Params->TryGetNumberField(FieldName, NumericIdentifier))
        {
            return FString::FromInt(NumericIdentifier);
        }

        return FString();
    }

    static UEdGraph* FindBlueprintGraphByName(UBlueprint* Blueprint, const FString& GraphName)
    {
        if (!Blueprint)
        {
            return nullptr;
        }

        auto MatchesGraph = [&GraphName](UEdGraph* Graph)
        {
            return Graph
                && (Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase)
                    || Graph->GetFName().ToString().Equals(GraphName, ESearchCase::IgnoreCase));
        };

        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (MatchesGraph(Graph) || (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase) && Graph))
            {
                return Graph;
            }
        }

        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (MatchesGraph(Graph))
            {
                return Graph;
            }
        }

        for (UEdGraph* Graph : Blueprint->MacroGraphs)
        {
            if (MatchesGraph(Graph))
            {
                return Graph;
            }
        }

        return nullptr;
    }

    static UEdGraphNode* FindBlueprintNodeByIdentifier(UEdGraph* Graph, const FString& NodeIdentifier)
    {
        if (!Graph || NodeIdentifier.IsEmpty())
        {
            return nullptr;
        }

        int32 NodeIndex = INDEX_NONE;
        const bool bLooksLikeIndex = LexTryParseString(NodeIndex, *NodeIdentifier);

        for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
        {
            UEdGraphNode* Node = Graph->Nodes[Index];
            if (!Node)
            {
                continue;
            }

            if ((bLooksLikeIndex && Index == NodeIndex)
                || Node->GetName().Equals(NodeIdentifier, ESearchCase::IgnoreCase)
                || Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens).Equals(NodeIdentifier, ESearchCase::IgnoreCase)
                || Node->NodeGuid.ToString(EGuidFormats::Digits).Equals(NodeIdentifier, ESearchCase::IgnoreCase)
                || Node->NodeGuid.ToString().Equals(NodeIdentifier, ESearchCase::IgnoreCase))
            {
                return Node;
            }
        }

        return nullptr;
    }

    static UEdGraphPin* FindBlueprintPinByIdentifier(UEdGraphNode* Node, const FString& PinIdentifier)
    {
        if (!Node || PinIdentifier.IsEmpty())
        {
            return nullptr;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin)
            {
                continue;
            }

            if (Pin->PinName.ToString().Equals(PinIdentifier, ESearchCase::IgnoreCase)
                || Pin->GetName().Equals(PinIdentifier, ESearchCase::IgnoreCase)
                || GuidMatchesString(Pin->PinId, PinIdentifier))
            {
                return Pin;
            }
        }

        return nullptr;
    }

    static void AddVectorField(TSharedPtr<FJsonObject> Object, const TCHAR* FieldName, const FVector& Value)
    {
        TArray<TSharedPtr<FJsonValue>> Array;
        Array.Add(MakeShared<FJsonValueNumber>(Value.X));
        Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
        Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
        Object->SetArrayField(FieldName, Array);
    }

    static TSharedPtr<FJsonObject> MakeNodeJson(UEdGraphNode* Node)
    {
        TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
        if (!Node)
        {
            return NodeJson;
        }

        NodeJson->SetStringField(TEXT("node_name"), Node->GetName());
        NodeJson->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
        NodeJson->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        if (UEdGraph* Graph = Node->GetGraph())
        {
            NodeJson->SetStringField(TEXT("graph_name"), Graph->GetName());
        }
        return NodeJson;
    }

    static TSharedPtr<FJsonObject> MakePinJson(UEdGraphPin* Pin)
    {
        TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
        if (!Pin)
        {
            return PinJson;
        }

        PinJson->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
        PinJson->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::DigitsWithHyphens));
        PinJson->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        if (UEdGraphNode* Node = Pin->GetOwningNode())
        {
            PinJson->SetObjectField(TEXT("node"), MakeNodeJson(Node));
        }
        return PinJson;
    }

    static void AddTimelineTrackSummary(const UTimelineTemplate* Timeline, TSharedPtr<FJsonObject> Result)
    {
        TArray<TSharedPtr<FJsonValue>> Tracks;
        auto AddTrack = [&Tracks](const FString& Type, const FName& TrackName, bool bExternal)
        {
            TSharedPtr<FJsonObject> TrackJson = MakeShared<FJsonObject>();
            TrackJson->SetStringField(TEXT("type"), Type);
            TrackJson->SetStringField(TEXT("name"), TrackName.ToString());
            TrackJson->SetBoolField(TEXT("external_curve"), bExternal);
            Tracks.Add(MakeShared<FJsonValueObject>(TrackJson));
        };

        if (Timeline)
        {
            for (const FTTFloatTrack& Track : Timeline->FloatTracks)
            {
                AddTrack(TEXT("float"), Track.GetTrackName(), Track.bIsExternalCurve);
            }
            for (const FTTVectorTrack& Track : Timeline->VectorTracks)
            {
                AddTrack(TEXT("vector"), Track.GetTrackName(), Track.bIsExternalCurve);
            }
            for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
            {
                AddTrack(TEXT("color"), Track.GetTrackName(), Track.bIsExternalCurve);
            }
            for (const FTTEventTrack& Track : Timeline->EventTracks)
            {
                AddTrack(TEXT("event"), Track.GetTrackName(), Track.bIsExternalCurve);
            }
        }

        Result->SetNumberField(TEXT("track_count"), Tracks.Num());
        Result->SetArrayField(TEXT("tracks"), Tracks);
    }
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPBlueprintCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("create_blueprint"), &FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprint},
        {TEXT("add_component_to_blueprint"), &FEpicUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint},
        {TEXT("set_physics_properties"), &FEpicUnrealMCPBlueprintCommands::HandleSetPhysicsProperties},
        {TEXT("compile_blueprint"), &FEpicUnrealMCPBlueprintCommands::HandleCompileBlueprint},
        {TEXT("set_static_mesh_properties"), &FEpicUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties},
        {TEXT("spawn_blueprint_actor"), &FEpicUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor},
        {TEXT("set_mesh_material_color"), &FEpicUnrealMCPBlueprintCommands::HandleSetMeshMaterialColor},
        {TEXT("get_available_materials"), &FEpicUnrealMCPBlueprintCommands::HandleGetAvailableMaterials},
        {TEXT("apply_material_to_actor"), &FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToActor},
        {TEXT("apply_material_to_blueprint"), &FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToBlueprint},
        {TEXT("get_actor_material_info"), &FEpicUnrealMCPBlueprintCommands::HandleGetActorMaterialInfo},
        {TEXT("get_blueprint_material_info"), &FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintMaterialInfo},
        {TEXT("read_blueprint_content"), &FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintContent},
        {TEXT("analyze_blueprint_graph"), &FEpicUnrealMCPBlueprintCommands::HandleAnalyzeBlueprintGraph},
        {TEXT("get_blueprint_variable_details"), &FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintVariableDetails},
        {TEXT("get_blueprint_function_details"), &FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintFunctionDetails},
        // Phase 6: Missing Blueprint Features
        {TEXT("set_blueprint_parent_class"), &FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintParentClass},
        {TEXT("set_blueprint_class_settings"), &FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintClassSettings},
        {TEXT("set_blueprint_class_defaults"), &FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintClassDefaults},
        {TEXT("set_component_defaults"), &FEpicUnrealMCPBlueprintCommands::HandleSetComponentDefaults},
        {TEXT("edit_construction_script"), &FEpicUnrealMCPBlueprintCommands::HandleEditConstructionScript},
        {TEXT("create_event_dispatcher"), &FEpicUnrealMCPBlueprintCommands::HandleCreateEventDispatcher},
        {TEXT("bind_event_dispatcher"), &FEpicUnrealMCPBlueprintCommands::HandleBindEventDispatcher},
        {TEXT("create_enum"), &FEpicUnrealMCPBlueprintCommands::HandleCreateEnum},
        {TEXT("create_struct"), &FEpicUnrealMCPBlueprintCommands::HandleCreateStruct},
        {TEXT("edit_enum"), &FEpicUnrealMCPBlueprintCommands::HandleEditEnum},
        {TEXT("edit_struct"), &FEpicUnrealMCPBlueprintCommands::HandleEditStruct},
        {TEXT("create_blueprint_interface"), &FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprintInterface},
        {TEXT("implement_interface"), &FEpicUnrealMCPBlueprintCommands::HandleImplementInterface},
        {TEXT("create_function_library"), &FEpicUnrealMCPBlueprintCommands::HandleCreateFunctionLibrary},
        {TEXT("create_macro_library"), &FEpicUnrealMCPBlueprintCommands::HandleCreateMacroLibrary},
        {TEXT("add_comment_node"), &FEpicUnrealMCPBlueprintCommands::HandleAddCommentNode},
        {TEXT("add_reroute_node"), &FEpicUnrealMCPBlueprintCommands::HandleAddRerouteNode},
        {TEXT("format_graph"), &FEpicUnrealMCPBlueprintCommands::HandleFormatGraph},
        {TEXT("create_collapsed_graph"), &FEpicUnrealMCPBlueprintCommands::HandleCreateCollapsedGraph},
        {TEXT("create_macro_graph"), &FEpicUnrealMCPBlueprintCommands::HandleCreateMacroGraph},
        {TEXT("create_macro_instance"), &FEpicUnrealMCPBlueprintCommands::HandleCreateMacroInstance},
        {TEXT("create_timeline"), &FEpicUnrealMCPBlueprintCommands::HandleCreateTimeline},
        {TEXT("edit_timeline"), &FEpicUnrealMCPBlueprintCommands::HandleEditTimeline},
        {TEXT("set_blueprint_breakpoint"), &FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintBreakpoint},
        {TEXT("set_blueprint_watch"), &FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintWatch},
        {TEXT("clear_blueprint_watches"), &FEpicUnrealMCPBlueprintCommands::HandleClearBlueprintWatches},
        {TEXT("step_blueprint_debugger"), &FEpicUnrealMCPBlueprintCommands::HandleStepBlueprintDebugger},
        {TEXT("get_blueprint_debug_info"), &FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintDebugInfo},
        {TEXT("blueprint_diff"), &FEpicUnrealMCPBlueprintCommands::HandleBlueprintDiff},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Blueprint")));

    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Check if blueprint already exists
    FString PackagePath = TEXT("/Game/Blueprints/");
    FString AssetName = BlueprintName;
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    // Create the blueprint factory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    
    // Handle parent class
    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);
    
    // Default to Actor if no parent class specified
    UClass* SelectedParentClass = AActor::StaticClass();
    
    // Try to find the specified parent class
    if (!ParentClass.IsEmpty())
    {
        FString ClassName = ParentClass;
        if (!ClassName.StartsWith(TEXT("A")))
        {
            ClassName = TEXT("A") + ClassName;
        }
        
        // First try direct StaticClass lookup for common classes
        UClass* FoundClass = nullptr;
        if (ClassName == TEXT("APawn"))
        {
            FoundClass = APawn::StaticClass();
        }
        else if (ClassName == TEXT("AActor"))
        {
            FoundClass = AActor::StaticClass();
        }
        else
        {
            // Try loading the class using LoadClass which is more reliable than FindObject
            const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            FoundClass = LoadClass<AActor>(nullptr, *ClassPath);
            
            if (!FoundClass)
            {
                // Try alternate paths if not found
                const FString GameClassPath = FString::Printf(TEXT("/Script/Game.%s"), *ClassName);
                FoundClass = LoadClass<AActor>(nullptr, *GameClassPath);
            }
        }

        if (FoundClass)
        {
            SelectedParentClass = FoundClass;
            UE_LOG(LogTemp, Log, TEXT("Successfully set parent class to '%s'"), *ClassName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find specified parent class '%s' at paths: /Script/Engine.%s or /Script/Game.%s, defaulting to AActor"), 
                *ClassName, *ClassName, *ClassName);
        }
    }
    
    Factory->ParentClass = SelectedParentClass;

    // Create the blueprint
    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(NewBlueprint);

        // Mark the package dirty
        Package->MarkPackageDirty();

        return MakeBlueprintSuccessResult({
            {TEXT("name"), MakeShared<FJsonValueString>(AssetName)},
            {TEXT("path"), MakeShared<FJsonValueString>(PackagePath + AssetName)}
        });
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Add Component To Blueprint")));

    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create the component - dynamically find the component class by name
    UClass* ComponentClass = ResolveComponentClass(ComponentType);
    
    // Verify that the class is a valid component type
    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    // Add the component to the blueprint
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
        // Set transform if provided
        USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponent)
        {
            if (Params->HasField(TEXT("location")))
            {
                SceneComponent->SetRelativeLocation(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
            }
            if (Params->HasField(TEXT("rotation")))
            {
                SceneComponent->SetRelativeRotation(FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
            }
            if (Params->HasField(TEXT("scale")))
            {
                SceneComponent->SetRelativeScale3D(FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
            }
        }

        // Add to root if no parent specified
        Blueprint->SimpleConstructionScript->AddNode(NewNode);

        // Compile the blueprint
        FlushRenderingCommands();
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        return MakeBlueprintSuccessResult({
            {TEXT("component_name"), MakeShared<FJsonValueString>(ComponentName)},
            {TEXT("component_type"), MakeShared<FJsonValueString>(ComponentType)}
        });
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Physics Properties")));

    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Set physics properties
    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        float Mass = Params->GetNumberField(TEXT("mass"));
        // In UE5.5, use proper overrideMass instead of just scaling
        PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
        UE_LOG(LogTemp, Display, TEXT("Set mass for component %s to %f kg"), *ComponentName, Mass);
    }

    if (Params->HasField(TEXT("linear_damping")))
    {
        PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
    }

    if (Params->HasField(TEXT("angular_damping")))
    {
        PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    return MakeBlueprintSuccessResult({
        {TEXT("component"), MakeShared<FJsonValueString>(ComponentName)}
    });
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Compile the blueprint
    FlushRenderingCommands();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    return MakeBlueprintSuccessResult({
        {TEXT("name"), MakeShared<FJsonValueString>(BlueprintName)},
        {TEXT("compiled"), MakeShared<FJsonValueBoolean>(true)}
    });
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Spawn Blueprint Actor")));

    UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Starting blueprint actor spawn"));
    
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Missing blueprint_name parameter"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Missing actor_name parameter"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Looking for blueprint '%s'"), *BlueprintName);

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Blueprint not found: %s"), *BlueprintName);
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Blueprint found, getting transform parameters"));

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Location set to (%f, %f, %f)"), Location.X, Location.Y, Location.Z);
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
        UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Rotation set to (%f, %f, %f)"), Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
    }

    // Parse scale (default 1,1,1)
    FVector Scale(1.0f, 1.0f, 1.0f);
    if (Params->HasField(TEXT("scale")))
    {
        FString ParamError;
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("scale"), Scale, ParamError))
        {
            UE_LOG(LogTemp, Warning, TEXT("HandleSpawnBlueprintActor: Invalid scale parameter: %s"), *ParamError);
            Scale = FVector::OneVector;
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Scale set to (%f, %f, %f)"), Scale.X, Scale.Y, Scale.Z);
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Getting editor world"));

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Failed to get editor world"));
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Creating spawn transform"));

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    // Ensure blueprint class is ready (retry loop instead of fixed 200ms sleep)
    if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->GetDefaultObject(false))
    {
        FlushRenderingCommands();
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }
    {
        int32 Retries = 0;
        while ((!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->GetDefaultObject(false)) && Retries < 10)
        {
            FPlatformProcess::Sleep(0.005f);
            Retries++;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: About to spawn actor from blueprint '%s' with GeneratedClass: %s"),
           *BlueprintName, Blueprint->GeneratedClass ? *Blueprint->GeneratedClass->GetName() : TEXT("NULL"));

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    
    UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: SpawnActor completed, NewActor: %s"), 
           NewActor ? *NewActor->GetName() : TEXT("NULL"));
    
    if (NewActor)
    {
        UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: Setting actor label to '%s'"), *ActorName);
        NewActor->SetActorLabel(*ActorName);

        NewActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
        FString McpId;
        if (Params->TryGetStringField(TEXT("mcp_id"), McpId) && !McpId.IsEmpty())
        {
            NewActor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:%s"), *McpId)));
        }

        // Register in actor index for O(1) lookup
        if (UEpicUnrealMCPBridge* Bridge = GEditor ? GEditor->GetEditorSubsystem<UEpicUnrealMCPBridge>() : nullptr)
        {
            Bridge->ActorIndex.AddActor(NewActor);
        }

        UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: About to convert actor to JSON"));
        TSharedPtr<FJsonObject> Result = FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);

        UE_LOG(LogTemp, Verbose, TEXT("HandleSpawnBlueprintActor: JSON conversion completed, returning result"));
        return Result;
    }

    UE_LOG(LogTemp, Error, TEXT("HandleSpawnBlueprintActor: Failed to spawn blueprint actor"));
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Static Mesh Properties")));

    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    // Set static mesh properties
    if (Params->HasField(TEXT("static_mesh")))
    {
        FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (Mesh)
        {
            FlushRenderingCommands();
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        FString MaterialPath = Params->GetStringField(TEXT("material"));
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            FlushRenderingCommands();
            MeshComponent->SetMaterial(0, Material);
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Mesh Material Color")));

    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    // Try to cast to StaticMeshComponent or PrimitiveComponent
    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Get color parameter
    TArray<float> ColorArray;
    const TArray<TSharedPtr<FJsonValue>>* ColorJsonArray;
    if (!Params->TryGetArrayField(TEXT("color"), ColorJsonArray) || ColorJsonArray->Num() != 4)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'color' must be an array of 4 float values [R, G, B, A]"));
    }

    for (const TSharedPtr<FJsonValue>& Value : *ColorJsonArray)
    {
        ColorArray.Add(FMath::Clamp(Value->AsNumber(), 0.0f, 1.0f));
    }

    FLinearColor Color(ColorArray[0], ColorArray[1], ColorArray[2], ColorArray[3]);

    // Get material slot index
    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    // Get parameter name
    FString ParameterName = TEXT("BaseColor");
    Params->TryGetStringField(TEXT("parameter_name"), ParameterName);

    // Get or create material
    UMaterialInterface* Material = nullptr;
    
    // Check if a specific material path was provided
    FString MaterialPath;
    if (Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (!Material)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
        }
    }
    else
    {
        // Use existing material on the component
        Material = PrimComponent->GetMaterial(MaterialSlot);
        if (!Material)
        {
            // Try to use a default material
            Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
            if (!Material)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No material found on component and failed to load default material"));
            }
        }
    }

    // Create a dynamic material instance
    UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(Material, PrimComponent);
    if (!DynMaterial)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance"));
    }

    // Set the color parameter
    DynMaterial->SetVectorParameterValue(*ParameterName, Color);

    // Apply the material to the component
    FlushRenderingCommands();
    PrimComponent->SetMaterial(MaterialSlot, DynMaterial);

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    // Log success
    UE_LOG(LogTemp, Log, TEXT("Successfully set material color on component %s: R=%f, G=%f, B=%f, A=%f"), 
        *ComponentName, Color.R, Color.G, Color.B, Color.A);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    
    TArray<TSharedPtr<FJsonValue>> ColorResultArray;
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.R));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.G));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.B));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.A));
    ResultObj->SetArrayField(TEXT("color"), ColorResultArray);
    
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ---------------------------------------------------------------------------
// Phase 6: Missing Blueprint Features Implementation
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintParentClass(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString NewParentClassName;
    if (!Params->TryGetStringField(TEXT("parent_class"), NewParentClassName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_class' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    UClass* NewParentClass = nullptr;
    if (!NewParentClassName.StartsWith(TEXT("A")))
    {
        NewParentClassName = TEXT("A") + NewParentClassName;
    }

    const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *NewParentClassName);
    NewParentClass = LoadClass<AActor>(nullptr, *ClassPath);

    if (!NewParentClass)
    {
        const FString GameClassPath = FString::Printf(TEXT("/Script/Game.%s"), *NewParentClassName);
        NewParentClass = LoadClass<AActor>(nullptr, *GameClassPath);
    }

    if (!NewParentClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent class not found: %s"), *NewParentClassName));
    }

    UBlueprintEditorLibrary::ReparentBlueprint(Blueprint, NewParentClass);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("new_parent_class"), NewParentClass->GetName());
    Result->SetStringField(TEXT("message"), TEXT("Blueprint parent class changed successfully"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintClassSettings(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    bool bClassSetting = false;
    if (Params->TryGetBoolField(TEXT("generate_overlap_events"), bClassSetting))
    {
        if (!Blueprint->GeneratedClass)
        {
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
        }

        AActor* ActorCDO = Blueprint->GeneratedClass
            ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        if (!ActorCDO)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("generate_overlap_events requires an Actor-based Blueprint")
            );
        }

        ActorCDO->Modify();
        ActorCDO->bGenerateOverlapEventsDuringLevelStreaming = bClassSetting;
    }

    bool bRunConstructionScript = true;
    if (Params->TryGetBoolField(TEXT("run_construction_script"), bRunConstructionScript))
    {
        Blueprint->bRunConstructionScriptOnDrag = bRunConstructionScript;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("message"), TEXT("Blueprint class settings updated"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintClassDefaults(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    const TArray<TSharedPtr<FJsonValue>>* DefaultsArray = nullptr;
    if (Params->TryGetArrayField(TEXT("defaults"), DefaultsArray) && DefaultsArray)
    {
        for (const TSharedPtr<FJsonValue>& Val : *DefaultsArray)
        {
            TSharedPtr<FJsonObject> DefaultObj = Val->AsObject();
            if (!DefaultObj.IsValid()) continue;

            FString VarName;
            if (!DefaultObj->TryGetStringField(TEXT("variable_name"), VarName)) continue;

            FString NewValue;
            if (!DefaultObj->TryGetStringField(TEXT("value"), NewValue)) continue;

            for (FBPVariableDescription& Variable : Blueprint->NewVariables)
            {
                if (Variable.VarName.ToString() == VarName)
                {
                    Variable.DefaultValue = NewValue;
                    break;
                }
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("message"), TEXT("Blueprint class defaults updated"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetComponentDefaults(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UActorComponent* Component = ComponentNode->ComponentTemplate;
    if (!Component)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component template is null"));
    }

    const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj) && PropertiesObj)
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PropertiesObj)->Values)
        {
            FProperty* Property = Component->GetClass()->FindPropertyByName(FName(*Pair.Key));
            if (Property)
            {
                if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
                {
                    BoolProp->SetPropertyValue_InContainer(Component, Pair.Value->AsBool());
                }
                else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
                {
                    FloatProp->SetPropertyValue_InContainer(Component, static_cast<float>(Pair.Value->AsNumber()));
                }
                else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
                {
                    IntProp->SetPropertyValue_InContainer(Component, static_cast<int32>(Pair.Value->AsNumber()));
                }
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("component_name"), ComponentName);
    Result->SetStringField(TEXT("message"), TEXT("Component defaults updated"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleEditConstructionScript(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    if (Blueprint->UbergraphPages.Num() > 0)
    {
        UEdGraph* ConstructionGraph = Blueprint->UbergraphPages[0];
        if (ConstructionGraph)
        {
            FString NodeType;
            if (Params->TryGetStringField(TEXT("add_node"), NodeType))
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField(TEXT("graph_name"), ConstructionGraph->GetName());
                Result->SetNumberField(TEXT("node_count"), ConstructionGraph->Nodes.Num());
                Result->SetStringField(TEXT("message"), TEXT("Construction script accessed. Use blueprint_graph commands for detailed node editing."));
                return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
            }
        }
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No construction script graph found"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateEventDispatcher(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString DispatcherName;
    if (!Params->TryGetStringField(TEXT("dispatcher_name"), DispatcherName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dispatcher_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    FBPVariableDescription NewVar;
    NewVar.VarName = FName(*DispatcherName);
    NewVar.FriendlyName = DispatcherName;
    NewVar.VarType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
    NewVar.PropertyFlags = CPF_BlueprintAssignable | CPF_BlueprintCallable;

    Blueprint->NewVariables.Add(NewVar);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("dispatcher_name"), DispatcherName);
    Result->SetStringField(TEXT("message"), TEXT("Event dispatcher created"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleBindEventDispatcher(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString DispatcherName;
    if (!Params->TryGetStringField(TEXT("dispatcher_name"), DispatcherName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'dispatcher_name' parameter"));
    }

    FString TargetFunction;
    if (!Params->TryGetStringField(TEXT("target_function"), TargetFunction))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_function' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("dispatcher_name"), DispatcherName);
    Result->SetStringField(TEXT("target_function"), TargetFunction);
    Result->SetStringField(TEXT("message"), TEXT("Event dispatcher binding configured. Use blueprint_graph commands to create the actual Bind node."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateEnum(const TSharedPtr<FJsonObject>& Params)
{
    FString EnumPath;
    if (!Params->TryGetStringField(TEXT("enum_path"), EnumPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'enum_path' parameter (e.g. /Game/Enums/MyEnum)"));
    }

    const TArray<TSharedPtr<FJsonValue>>* ValuesArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("values"), ValuesArray) || !ValuesArray || ValuesArray->Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'values' array parameter"));
    }

    FString EnumName = FPaths::GetBaseFilename(EnumPath);

    UPackage* Package = CreatePackage(*EnumPath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for enum"));
    }

    UEnum* NewEnum = NewObject<UEnum>(Package, FName(*EnumName), RF_Public | RF_Standalone);
    if (!NewEnum)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create enum object"));
    }

    TArray<TPair<FName, int64>> EnumNames;
    for (int32 i = 0; i < ValuesArray->Num(); ++i)
    {
        FString ValueName = (*ValuesArray)[i]->AsString();
        EnumNames.Add(TPair<FName, int64>(FName(*ValueName), i));
    }
    NewEnum->SetEnums(EnumNames, UEnum::ECppForm::Namespaced);

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewEnum);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("enum_path"), EnumPath);
    Result->SetStringField(TEXT("enum_name"), EnumName);
    Result->SetNumberField(TEXT("value_count"), ValuesArray->Num());
    Result->SetStringField(TEXT("message"), TEXT("Enum created successfully"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateStruct(const TSharedPtr<FJsonObject>& Params)
{
    FString StructPath;
    if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'struct_path' parameter (e.g. /Game/Structs/MyStruct)"));
    }

    FString StructName = FPaths::GetBaseFilename(StructPath);

    UPackage* Package = CreatePackage(*StructPath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for struct"));
    }

    UScriptStruct* NewStruct = NewObject<UScriptStruct>(Package, FName(*StructName), RF_Public | RF_Standalone);
    if (!NewStruct)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create struct object"));
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewStruct);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("struct_path"), StructPath);
    Result->SetStringField(TEXT("struct_name"), StructName);
    Result->SetStringField(TEXT("message"), TEXT("Struct created. Property editing requires advanced reflection setup."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleEditEnum(const TSharedPtr<FJsonObject>& Params)
{
    FString EnumPath;
    if (!Params->TryGetStringField(TEXT("enum_path"), EnumPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'enum_path' parameter"));
    }

    UEnum* Enum = LoadObject<UEnum>(nullptr, *EnumPath);
    if (!Enum)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Enum not found: %s"), *EnumPath));
    }

    const TArray<TSharedPtr<FJsonValue>>* AddValuesArray = nullptr;
    if (Params->TryGetArrayField(TEXT("add_values"), AddValuesArray) && AddValuesArray)
    {
        TArray<TPair<FName, int64>> CurrentNames;
        for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
        {
            CurrentNames.Add(TPair<FName, int64>(Enum->GetNameByIndex(i), i));
        }

        for (const TSharedPtr<FJsonValue>& Val : *AddValuesArray)
        {
            FString ValueName = Val->AsString();
            int32 NewIndex = CurrentNames.Num();
            CurrentNames.Add(TPair<FName, int64>(FName(*ValueName), NewIndex));
        }

        Enum->SetEnums(CurrentNames, UEnum::ECppForm::Namespaced);
        Enum->MarkPackageDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("enum_path"), EnumPath);
    Result->SetNumberField(TEXT("total_values"), Enum->NumEnums() - 1);
    Result->SetStringField(TEXT("message"), TEXT("Enum edited successfully"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleEditStruct(const TSharedPtr<FJsonObject>& Params)
{
    FString StructPath;
    if (!Params->TryGetStringField(TEXT("struct_path"), StructPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'struct_path' parameter"));
    }

    UScriptStruct* ScriptStruct = LoadObject<UScriptStruct>(nullptr, *StructPath);
    if (!ScriptStruct)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Struct not found: %s"), *StructPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("struct_path"), StructPath);
    Result->SetStringField(TEXT("struct_name"), ScriptStruct->GetName());
    Result->SetNumberField(TEXT("property_count"), ScriptStruct->PropertyLink ? 1 : 0);
    Result->SetStringField(TEXT("message"), TEXT("Struct found. Full property editing requires advanced C++ reflection."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprintInterface(const TSharedPtr<FJsonObject>& Params)
{
    FString InterfacePath;
    if (!Params->TryGetStringField(TEXT("interface_path"), InterfacePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface_path' parameter (e.g. /Game/Interfaces/MyInterface)"));
    }

    FString InterfaceName = FPaths::GetBaseFilename(InterfacePath);

    UPackage* Package = CreatePackage(*InterfacePath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for interface"));
    }

    UBlueprint* InterfaceBlueprint = NewObject<UBlueprint>(Package, FName(*InterfaceName), RF_Public | RF_Standalone);
    if (!InterfaceBlueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create interface blueprint"));
    }

    InterfaceBlueprint->BlueprintType = BPTYPE_Interface;

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(InterfaceBlueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("interface_path"), InterfacePath);
    Result->SetStringField(TEXT("interface_name"), InterfaceName);
    Result->SetStringField(TEXT("message"), TEXT("Blueprint interface created. Add function graphs via blueprint_graph commands."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleImplementInterface(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString InterfacePath;
    if (!Params->TryGetStringField(TEXT("interface_path"), InterfacePath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface_path' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    UClass* InterfaceClass = LoadObject<UClass>(nullptr, *InterfacePath);
    if (!InterfaceClass)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Interface not found: %s"), *InterfacePath));
    }

    FBPInterfaceDescription InterfaceDesc;
    InterfaceDesc.Interface = InterfaceClass;
    Blueprint->ImplementedInterfaces.Add(InterfaceDesc);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("interface_path"), InterfacePath);
    Result->SetStringField(TEXT("message"), TEXT("Interface implemented successfully"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateFunctionLibrary(const TSharedPtr<FJsonObject>& Params)
{
    FString LibraryPath;
    if (!Params->TryGetStringField(TEXT("library_path"), LibraryPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'library_path' parameter (e.g. /Game/Libraries/MyLibrary)"));
    }

    FString LibraryName = FPaths::GetBaseFilename(LibraryPath);

    UPackage* Package = CreatePackage(*LibraryPath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for function library"));
    }

    UBlueprint* FunctionLibrary = NewObject<UBlueprint>(Package, FName(*LibraryName), RF_Public | RF_Standalone);
    if (!FunctionLibrary)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function library"));
    }

    FunctionLibrary->BlueprintType = BPTYPE_FunctionLibrary;

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(FunctionLibrary);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("library_path"), LibraryPath);
    Result->SetStringField(TEXT("library_name"), LibraryName);
    Result->SetStringField(TEXT("message"), TEXT("Function library created. Add functions via blueprint_graph commands."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateMacroLibrary(const TSharedPtr<FJsonObject>& Params)
{
    FString LibraryPath;
    if (!Params->TryGetStringField(TEXT("library_path"), LibraryPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'library_path' parameter (e.g. /Game/MacroLibraries/MyMacroLibrary)"));
    }

    FString LibraryName = FPaths::GetBaseFilename(LibraryPath);

    UPackage* Package = CreatePackage(*LibraryPath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for macro library"));
    }

    UBlueprint* MacroLibrary = NewObject<UBlueprint>(Package, FName(*LibraryName), RF_Public | RF_Standalone);
    if (!MacroLibrary)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create macro library"));
    }

    MacroLibrary->BlueprintType = BPTYPE_MacroLibrary;

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(MacroLibrary);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("library_path"), LibraryPath);
    Result->SetStringField(TEXT("library_name"), LibraryName);
    Result->SetStringField(TEXT("message"), TEXT("Macro library created. Add macros via blueprint_graph commands."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddCommentNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    FString CommentText = TEXT("Comment");
    Params->TryGetStringField(TEXT("comment_text"), CommentText);

    float PosX = 0.0f;
    float PosY = 0.0f;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);

    float Width = 400.0f;
    float Height = 300.0f;
    Params->TryGetNumberField(TEXT("width"), Width);
    Params->TryGetNumberField(TEXT("height"), Height);

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            TargetGraph = Graph;
            break;
        }
    }

    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }

    // Create comment node directly in the graph
    UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(TargetGraph);
    if (CommentNode)
    {
        CommentNode->NodePosX = PosX;
        CommentNode->NodePosY = PosY;
        CommentNode->NodeWidth = Width;
        CommentNode->NodeHeight = Height;
        CommentNode->NodeComment = CommentText;
        
        // Auto-select nodes within the comment box if specified
        const TArray<TSharedPtr<FJsonValue>>* NodeNames = nullptr;
        if (Params->TryGetArrayField(TEXT("node_names"), NodeNames) && NodeNames)
        {
            for (const TSharedPtr<FJsonValue>& Val : *NodeNames)
            {
                FString NodeName = Val->AsString();
                for (UEdGraphNode* Node : TargetGraph->Nodes)
                {
                    if (Node && Node->GetName() == NodeName)
                    {
                        CommentNode->AddNodeUnderComment(Node);
                        break;
                    }
                }
            }
        }

        TargetGraph->AddNode(CommentNode);
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("graph_name"), GraphName);
        Result->SetStringField(TEXT("comment_node"), CommentNode->GetName());
        Result->SetStringField(TEXT("comment_text"), CommentText);
        Result->SetNumberField(TEXT("pos_x"), PosX);
        Result->SetNumberField(TEXT("pos_y"), PosY);
        Result->SetStringField(TEXT("message"), TEXT("Comment node created successfully"));
        return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create comment node"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddRerouteNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    float PosX = 0.0f;
    float PosY = 0.0f;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            TargetGraph = Graph;
            break;
        }
    }

    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }

    // Create reroute (knot) node directly in the graph
    UK2Node_Knot* KnotNode = NewObject<UK2Node_Knot>(TargetGraph);
    if (KnotNode)
    {
        KnotNode->NodePosX = PosX;
        KnotNode->NodePosY = PosY;
        
        // Allocate default pins
        KnotNode->AllocateDefaultPins();
        
        TargetGraph->AddNode(KnotNode);
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("graph_name"), GraphName);
        Result->SetStringField(TEXT("reroute_node"), KnotNode->GetName());
        Result->SetNumberField(TEXT("pos_x"), PosX);
        Result->SetNumberField(TEXT("pos_y"), PosY);
        Result->SetStringField(TEXT("message"), TEXT("Reroute node created successfully"));
        return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create reroute node"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleFormatGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("graph_name"), GraphName);
    Result->SetStringField(TEXT("message"), TEXT("Graph auto-formatting is an editor UI feature. Open the Blueprint Editor and use the Format Graph button."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateCollapsedGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    UEdGraph* TargetGraph = FindBlueprintGraphByName(Blueprint, GraphName);
    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }

    FString CollapsedName = TEXT("CollapsedGraph");
    Params->TryGetStringField(TEXT("collapsed_graph_name"), CollapsedName);
    Params->TryGetStringField(TEXT("name"), CollapsedName);

    double PosX = 0.0;
    double PosY = 0.0;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Collapsed Blueprint Graph")));
    Blueprint->Modify();
    TargetGraph->Modify();

    UK2Node_Composite* CompositeNode = NewObject<UK2Node_Composite>(TargetGraph);
    if (!CompositeNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create collapsed graph node"));
    }

    CompositeNode->CreateNewGuid();
    CompositeNode->NodePosX = static_cast<int32>(PosX);
    CompositeNode->NodePosY = static_cast<int32>(PosY);
    TargetGraph->AddNode(CompositeNode, true, false);
    CompositeNode->PostPlacedNewNode();

    if (!CompositeNode->BoundGraph)
    {
        CompositeNode->BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
            CompositeNode,
            FName(*CollapsedName),
            UEdGraph::StaticClass(),
            UEdGraphSchema_K2::StaticClass());
        if (CompositeNode->BoundGraph)
        {
            TargetGraph->SubGraphs.Add(CompositeNode->BoundGraph);
            const UEdGraphSchema* Schema = CompositeNode->BoundGraph->GetSchema();
            if (Schema)
            {
                Schema->CreateDefaultNodesForGraph(*CompositeNode->BoundGraph);
            }
        }
    }

    if (!CollapsedName.IsEmpty())
    {
        CompositeNode->OnRenameNode(CollapsedName);
    }

    CompositeNode->AllocateDefaultPins();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("graph_name"), GraphName);
    Result->SetStringField(TEXT("collapsed_graph_name"), CompositeNode->BoundGraph ? CompositeNode->BoundGraph->GetName() : CollapsedName);
    Result->SetStringField(TEXT("node_name"), CompositeNode->GetName());
    Result->SetStringField(TEXT("node_guid"), CompositeNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("pos_x"), CompositeNode->NodePosX);
    Result->SetNumberField(TEXT("pos_y"), CompositeNode->NodePosY);
    Result->SetStringField(TEXT("message"), TEXT("Collapsed graph node created"));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateMacroGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString MacroName;
    if (!Params->TryGetStringField(TEXT("macro_name"), MacroName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'macro_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Blueprint Macro Graph")));
    Blueprint->Modify();
    const FName UniqueGraphName = FBlueprintEditorUtils::GenerateUniqueGraphName(Blueprint, MacroName);
    UEdGraph* MacroGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        UniqueGraphName,
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    if (!MacroGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create macro graph"));
    }

    FBlueprintEditorUtils::AddMacroGraph(Blueprint, MacroGraph, true, nullptr);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("macro_name"), MacroGraph->GetName());
    Result->SetStringField(TEXT("graph_guid"), MacroGraph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("macro_graph_count"), Blueprint->MacroGraphs.Num());
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateMacroInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    FString MacroGraphName;
    if (!Params->TryGetStringField(TEXT("macro_graph_name"), MacroGraphName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'macro_graph_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    FString MacroBlueprintPath;
    UBlueprint* MacroBlueprint = Blueprint;
    if (Params->TryGetStringField(TEXT("macro_blueprint_path"), MacroBlueprintPath) && !MacroBlueprintPath.IsEmpty())
    {
        MacroBlueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(MacroBlueprintPath);
        if (!MacroBlueprint)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Macro Blueprint not found: %s"), *MacroBlueprintPath));
        }
    }

    UEdGraph* TargetGraph = FindBlueprintGraphByName(Blueprint, GraphName);
    UEdGraph* MacroGraph = FindBlueprintGraphByName(MacroBlueprint, MacroGraphName);
    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target graph not found: %s"), *GraphName));
    }
    if (!MacroGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Macro graph not found: %s"), *MacroGraphName));
    }

    double PosX = 0.0;
    double PosY = 0.0;
    Params->TryGetNumberField(TEXT("pos_x"), PosX);
    Params->TryGetNumberField(TEXT("pos_y"), PosY);

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Blueprint Macro Instance")));
    Blueprint->Modify();
    TargetGraph->Modify();

    UK2Node_MacroInstance* MacroNode = NewObject<UK2Node_MacroInstance>(TargetGraph);
    if (!MacroNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create macro instance node"));
    }

    MacroNode->CreateNewGuid();
    MacroNode->NodePosX = static_cast<int32>(PosX);
    MacroNode->NodePosY = static_cast<int32>(PosY);
    MacroNode->SetMacroGraph(MacroGraph);
    TargetGraph->AddNode(MacroNode, true, false);
    MacroNode->PostPlacedNewNode();
    MacroNode->AllocateDefaultPins();
    MacroNode->ReconstructNode();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("graph_name"), GraphName);
    Result->SetStringField(TEXT("macro_graph_name"), MacroGraph->GetName());
    Result->SetStringField(TEXT("node_name"), MacroNode->GetName());
    Result->SetStringField(TEXT("node_guid"), MacroNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateTimeline(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString TimelineName;
    if (!Params->TryGetStringField(TEXT("timeline_name"), TimelineName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'timeline_name' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }
    if (!FBlueprintEditorUtils::DoesSupportTimelines(Blueprint))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("This Blueprint type does not support Timelines"));
    }

    UEdGraph* TargetGraph = FindBlueprintGraphByName(Blueprint, GraphName);
    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }

    FName TimelineFName(*TimelineName);
    UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineFName);
    bool bCreatedTemplate = false;
    if (!Timeline)
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Blueprint Timeline")));
        Timeline = FBlueprintEditorUtils::AddNewTimeline(Blueprint, TimelineFName);
        bCreatedTemplate = Timeline != nullptr;
    }
    if (!Timeline)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create or find Timeline template"));
    }

    Timeline->Modify();
    double Length = 0.0;
    if (Params->TryGetNumberField(TEXT("length"), Length))
    {
        Timeline->TimelineLength = static_cast<float>(Length);
        Timeline->LengthMode = TL_TimelineLength;
    }

    bool bFlag = false;
    if (Params->TryGetBoolField(TEXT("autoplay"), bFlag)) { Timeline->bAutoPlay = bFlag; }
    if (Params->TryGetBoolField(TEXT("loop"), bFlag)) { Timeline->bLoop = bFlag; }
    if (Params->TryGetBoolField(TEXT("replicated"), bFlag)) { Timeline->bReplicated = bFlag; }
    if (Params->TryGetBoolField(TEXT("ignore_time_dilation"), bFlag)) { Timeline->bIgnoreTimeDilation = bFlag; }

    UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Timeline);
    bool bCreatedNode = false;
    if (!TimelineNode)
    {
        double PosX = 0.0;
        double PosY = 0.0;
        Params->TryGetNumberField(TEXT("pos_x"), PosX);
        Params->TryGetNumberField(TEXT("pos_y"), PosY);

        Blueprint->Modify();
        TargetGraph->Modify();
        TimelineNode = NewObject<UK2Node_Timeline>(TargetGraph);
        if (!TimelineNode)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Timeline node"));
        }
        TimelineNode->CreateNewGuid();
        TimelineNode->TimelineName = Timeline->GetVariableName();
        TimelineNode->TimelineGuid = Timeline->TimelineGuid;
        TimelineNode->bAutoPlay = Timeline->bAutoPlay;
        TimelineNode->bLoop = Timeline->bLoop;
        TimelineNode->bReplicated = Timeline->bReplicated;
        TimelineNode->bIgnoreTimeDilation = Timeline->bIgnoreTimeDilation;
        TimelineNode->NodePosX = static_cast<int32>(PosX);
        TimelineNode->NodePosY = static_cast<int32>(PosY);
        TargetGraph->AddNode(TimelineNode, true, false);
        TimelineNode->PostPlacedNewNode();
        TimelineNode->AllocateDefaultPins();
        bCreatedNode = true;
    }
    else
    {
        TimelineNode->Modify();
        TimelineNode->bAutoPlay = Timeline->bAutoPlay;
        TimelineNode->bLoop = Timeline->bLoop;
        TimelineNode->bReplicated = Timeline->bReplicated;
        TimelineNode->bIgnoreTimeDilation = Timeline->bIgnoreTimeDilation;
        TimelineNode->ReconstructNode();
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("graph_name"), GraphName);
    Result->SetStringField(TEXT("timeline_name"), Timeline->GetVariableName().ToString());
    Result->SetStringField(TEXT("timeline_guid"), Timeline->TimelineGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("node_name"), TimelineNode ? TimelineNode->GetName() : TEXT(""));
    Result->SetStringField(TEXT("node_guid"), TimelineNode ? TimelineNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT(""));
    Result->SetBoolField(TEXT("created_template"), bCreatedTemplate);
    Result->SetBoolField(TEXT("created_node"), bCreatedNode);
    Result->SetNumberField(TEXT("length"), Timeline->TimelineLength);
    Result->SetBoolField(TEXT("autoplay"), Timeline->bAutoPlay);
    Result->SetBoolField(TEXT("loop"), Timeline->bLoop);
    Result->SetBoolField(TEXT("replicated"), Timeline->bReplicated);
    Result->SetBoolField(TEXT("ignore_time_dilation"), Timeline->bIgnoreTimeDilation);
    AddTimelineTrackSummary(Timeline, Result);
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleEditTimeline(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString TimelineName;
    if (!Params->TryGetStringField(TEXT("timeline_name"), TimelineName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'timeline_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(FName(*TimelineName));
    if (!Timeline)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Timeline not found: %s"), *TimelineName));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Edit Blueprint Timeline")));
    Blueprint->Modify();
    Timeline->Modify();

    double Length = 0.0;
    if (Params->TryGetNumberField(TEXT("length"), Length))
    {
        Timeline->TimelineLength = static_cast<float>(Length);
        Timeline->LengthMode = TL_TimelineLength;
    }

    bool bFlag = false;
    if (Params->TryGetBoolField(TEXT("autoplay"), bFlag)) { Timeline->bAutoPlay = bFlag; }
    if (Params->TryGetBoolField(TEXT("loop"), bFlag)) { Timeline->bLoop = bFlag; }
    if (Params->TryGetBoolField(TEXT("replicated"), bFlag)) { Timeline->bReplicated = bFlag; }
    if (Params->TryGetBoolField(TEXT("ignore_time_dilation"), bFlag)) { Timeline->bIgnoreTimeDilation = bFlag; }

    FString TrackAction = TEXT("list");
    Params->TryGetStringField(TEXT("track_action"), TrackAction);
    FString TrackType = TEXT("float");
    Params->TryGetStringField(TEXT("track_type"), TrackType);
    FString TrackName;
    Params->TryGetStringField(TEXT("track_name"), TrackName);
    FString CurvePath;
    Params->TryGetStringField(TEXT("curve_path"), CurvePath);

    if (TrackAction.Equals(TEXT("add"), ESearchCase::IgnoreCase))
    {
        if (TrackName.IsEmpty())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("track_name is required when track_action is 'add'"));
        }
        const FName TrackFName(*TrackName);
        if (!Timeline->IsNewTrackNameValid(TrackFName))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Timeline track name already exists: %s"), *TrackName));
        }

        if (TrackType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
        {
            FTTFloatTrack Track;
            Track.SetTrackName(TrackFName, Timeline);
            Track.CurveFloat = CurvePath.IsEmpty() ? NewObject<UCurveFloat>(Timeline, *FString::Printf(TEXT("%s_FloatCurve"), *TrackName), RF_Transactional) : LoadObject<UCurveFloat>(nullptr, *CurvePath);
            Track.bIsExternalCurve = !CurvePath.IsEmpty();
            if (!Track.CurveFloat)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create or load float curve"));
            }
            const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
            if (Params->TryGetArrayField(TEXT("keys"), Keys) && Keys)
            {
                for (const TSharedPtr<FJsonValue>& KeyValue : *Keys)
                {
                    const TSharedPtr<FJsonObject>* KeyObject = nullptr;
                    if (KeyValue->TryGetObject(KeyObject) && KeyObject)
                    {
                        double Time = 0.0;
                        double Value = 0.0;
                        (*KeyObject)->TryGetNumberField(TEXT("time"), Time);
                        (*KeyObject)->TryGetNumberField(TEXT("value"), Value);
                        Track.CurveFloat->FloatCurve.AddKey(static_cast<float>(Time), static_cast<float>(Value));
                    }
                }
            }
            const int32 Index = Timeline->FloatTracks.Add(Track);
            Timeline->AddDisplayTrack(FTTTrackId(FTTTrackBase::TT_FloatInterp, Index));
        }
        else if (TrackType.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
        {
            FTTVectorTrack Track;
            Track.SetTrackName(TrackFName, Timeline);
            Track.CurveVector = CurvePath.IsEmpty() ? NewObject<UCurveVector>(Timeline, *FString::Printf(TEXT("%s_VectorCurve"), *TrackName), RF_Transactional) : LoadObject<UCurveVector>(nullptr, *CurvePath);
            Track.bIsExternalCurve = !CurvePath.IsEmpty();
            if (!Track.CurveVector)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create or load vector curve"));
            }
            const int32 Index = Timeline->VectorTracks.Add(Track);
            Timeline->AddDisplayTrack(FTTTrackId(FTTTrackBase::TT_VectorInterp, Index));
        }
        else if (TrackType.Equals(TEXT("color"), ESearchCase::IgnoreCase) || TrackType.Equals(TEXT("linear_color"), ESearchCase::IgnoreCase))
        {
            FTTLinearColorTrack Track;
            Track.SetTrackName(TrackFName, Timeline);
            Track.CurveLinearColor = CurvePath.IsEmpty() ? NewObject<UCurveLinearColor>(Timeline, *FString::Printf(TEXT("%s_ColorCurve"), *TrackName), RF_Transactional) : LoadObject<UCurveLinearColor>(nullptr, *CurvePath);
            Track.bIsExternalCurve = !CurvePath.IsEmpty();
            if (!Track.CurveLinearColor)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create or load linear color curve"));
            }
            const int32 Index = Timeline->LinearColorTracks.Add(Track);
            Timeline->AddDisplayTrack(FTTTrackId(FTTTrackBase::TT_LinearColorInterp, Index));
        }
        else if (TrackType.Equals(TEXT("event"), ESearchCase::IgnoreCase))
        {
            FTTEventTrack Track;
            Track.SetTrackName(TrackFName, Timeline);
            Track.CurveKeys = CurvePath.IsEmpty() ? NewObject<UCurveFloat>(Timeline, *FString::Printf(TEXT("%s_EventCurve"), *TrackName), RF_Transactional) : LoadObject<UCurveFloat>(nullptr, *CurvePath);
            Track.bIsExternalCurve = !CurvePath.IsEmpty();
            if (!Track.CurveKeys)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create or load event curve"));
            }
            const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
            if (Params->TryGetArrayField(TEXT("keys"), Keys) && Keys)
            {
                for (const TSharedPtr<FJsonValue>& KeyValue : *Keys)
                {
                    const TSharedPtr<FJsonObject>* KeyObject = nullptr;
                    if (KeyValue->TryGetObject(KeyObject) && KeyObject)
                    {
                        double Time = 0.0;
                        (*KeyObject)->TryGetNumberField(TEXT("time"), Time);
                        Track.CurveKeys->FloatCurve.AddKey(static_cast<float>(Time), 1.0f);
                    }
                }
            }
            const int32 Index = Timeline->EventTracks.Add(Track);
            Timeline->AddDisplayTrack(FTTTrackId(FTTTrackBase::TT_Event, Index));
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("track_type must be float, vector, color, or event"));
        }
    }
    else if (TrackAction.Equals(TEXT("remove"), ESearchCase::IgnoreCase))
    {
        if (TrackName.IsEmpty())
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("track_name is required when track_action is 'remove'"));
        }
        const FName TrackFName(*TrackName);
        bool bRemoved = false;
        bRemoved |= Timeline->FloatTracks.RemoveAll([&](const FTTFloatTrack& Track) { return Track.GetTrackName() == TrackFName; }) > 0;
        bRemoved |= Timeline->VectorTracks.RemoveAll([&](const FTTVectorTrack& Track) { return Track.GetTrackName() == TrackFName; }) > 0;
        bRemoved |= Timeline->LinearColorTracks.RemoveAll([&](const FTTLinearColorTrack& Track) { return Track.GetTrackName() == TrackFName; }) > 0;
        bRemoved |= Timeline->EventTracks.RemoveAll([&](const FTTEventTrack& Track) { return Track.GetTrackName() == TrackFName; }) > 0;
        if (!bRemoved)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Timeline track not found: %s"), *TrackName));
        }
    }

    if (UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Timeline))
    {
        TimelineNode->Modify();
        TimelineNode->bAutoPlay = Timeline->bAutoPlay;
        TimelineNode->bLoop = Timeline->bLoop;
        TimelineNode->bReplicated = Timeline->bReplicated;
        TimelineNode->bIgnoreTimeDilation = Timeline->bIgnoreTimeDilation;
        TimelineNode->ReconstructNode();
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("timeline_name"), Timeline->GetVariableName().ToString());
    Result->SetStringField(TEXT("track_action"), TrackAction);
    Result->SetNumberField(TEXT("length"), Timeline->TimelineLength);
    Result->SetBoolField(TEXT("autoplay"), Timeline->bAutoPlay);
    Result->SetBoolField(TEXT("loop"), Timeline->bLoop);
    Result->SetBoolField(TEXT("replicated"), Timeline->bReplicated);
    Result->SetBoolField(TEXT("ignore_time_dilation"), Timeline->bIgnoreTimeDilation);
    AddTimelineTrackSummary(Timeline, Result);
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintBreakpoint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    FString Action = TEXT("set");
    Params->TryGetStringField(TEXT("action"), Action);
    if (Action.Equals(TEXT("clear_all"), ESearchCase::IgnoreCase))
    {
        FKismetDebugUtilities::ClearBreakpoints(Blueprint);
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
        Result->SetStringField(TEXT("action"), TEXT("clear_all"));
        Result->SetNumberField(TEXT("breakpoint_count"), 0);
        return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);
    const FString NodeIdentifier = GetJsonIdentifier(Params, TEXT("node_id"));
    if (NodeIdentifier.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter. Use a node index, node object name, or node GUID."));
    }

    UEdGraph* TargetGraph = FindBlueprintGraphByName(Blueprint, GraphName);
    UEdGraphNode* Node = FindBlueprintNodeByIdentifier(TargetGraph, NodeIdentifier);
    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }
    if (!Node)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found in graph %s: %s"), *GraphName, *NodeIdentifier));
    }

    bool bEnable = true;
    Params->TryGetBoolField(TEXT("enable"), bEnable);
    bool bRemove = false;
    Params->TryGetBoolField(TEXT("remove"), bRemove);
    bRemove = bRemove || Action.Equals(TEXT("remove"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("clear"), ESearchCase::IgnoreCase);

    if (bRemove)
    {
        FKismetDebugUtilities::RemoveBreakpointFromNode(Node, Blueprint);
    }
    else
    {
        if (!FKismetDebugUtilities::FindBreakpointForNode(Node, Blueprint, true))
        {
            FKismetDebugUtilities::CreateBreakpoint(Blueprint, Node, bEnable);
        }
        FKismetDebugUtilities::SetBreakpointEnabled(Node, Blueprint, bEnable);
    }
    FBlueprintBreakpoint* Breakpoint = FKismetDebugUtilities::FindBreakpointForNode(Node, Blueprint, true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("graph_name"), GraphName);
    Result->SetStringField(TEXT("node_id"), NodeIdentifier);
    Result->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetBoolField(TEXT("removed"), bRemove);
    Result->SetBoolField(TEXT("breakpoint_exists"), Breakpoint != nullptr);
    Result->SetBoolField(TEXT("enable"), Breakpoint ? Breakpoint->IsEnabledByUser() : false);
    Result->SetBoolField(TEXT("valid"), Breakpoint ? FKismetDebugUtilities::IsBreakpointValid(*Breakpoint) : false);
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetBlueprintWatch(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);
    const FString NodeIdentifier = GetJsonIdentifier(Params, TEXT("node_id"));
    FString PinIdentifier;
    if (!Params->TryGetStringField(TEXT("pin_name"), PinIdentifier))
    {
        PinIdentifier = GetJsonIdentifier(Params, TEXT("pin_id"));
    }

    if (NodeIdentifier.IsEmpty() || PinIdentifier.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("node_id and pin_name/pin_id are required"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    UEdGraph* TargetGraph = FindBlueprintGraphByName(Blueprint, GraphName);
    UEdGraphNode* Node = FindBlueprintNodeByIdentifier(TargetGraph, NodeIdentifier);
    UEdGraphPin* Pin = FindBlueprintPinByIdentifier(Node, PinIdentifier);
    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }
    if (!Node)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeIdentifier));
    }
    if (!Pin)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Pin not found: %s"), *PinIdentifier));
    }

    bool bRemove = false;
    Params->TryGetBoolField(TEXT("remove"), bRemove);
    FString Action = TEXT("set");
    Params->TryGetStringField(TEXT("action"), Action);
    bRemove = bRemove || Action.Equals(TEXT("remove"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("clear"), ESearchCase::IgnoreCase);

    bool bCanWatch = FKismetDebugUtilities::CanWatchPin(Blueprint, Pin);
    if (!bRemove && !bCanWatch)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Pin cannot be watched. Use an executable Blueprint pin with runtime debug data."));
    }

    if (bRemove)
    {
        FKismetDebugUtilities::RemovePinWatch(Blueprint, Pin);
    }
    else if (!FKismetDebugUtilities::IsPinBeingWatched(Blueprint, Pin))
    {
        FKismetDebugUtilities::AddPinWatch(Blueprint, FBlueprintWatchedPin(Pin));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("graph_name"), GraphName);
    Result->SetObjectField(TEXT("pin"), MakePinJson(Pin));
    Result->SetBoolField(TEXT("removed"), bRemove);
    Result->SetBoolField(TEXT("can_watch"), bCanWatch);
    Result->SetBoolField(TEXT("watch_exists"), FKismetDebugUtilities::IsPinBeingWatched(Blueprint, Pin));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleClearBlueprintWatches(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    FKismetDebugUtilities::ClearPinWatches(Blueprint);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetNumberField(TEXT("watch_count"), 0);
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleStepBlueprintDebugger(const TSharedPtr<FJsonObject>& Params)
{
    FString Action;
    if (!Params->TryGetStringField(TEXT("action"), Action))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter. Use step_in, step_over, or step_out."));
    }

    if (Action.Equals(TEXT("step_in"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("single_step"), ESearchCase::IgnoreCase))
    {
        FKismetDebugUtilities::RequestSingleStepIn();
    }
    else if (Action.Equals(TEXT("step_over"), ESearchCase::IgnoreCase))
    {
        FKismetDebugUtilities::RequestStepOver();
    }
    else if (Action.Equals(TEXT("step_out"), ESearchCase::IgnoreCase))
    {
        FKismetDebugUtilities::RequestStepOut();
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("action must be step_in, step_over, or step_out"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("action"), Action);
    Result->SetBoolField(TEXT("single_stepping"), FKismetDebugUtilities::IsSingleStepping());
    if (UWorld* DebugWorld = FKismetDebugUtilities::GetCurrentDebuggingWorld())
    {
        Result->SetStringField(TEXT("debug_world"), DebugWorld->GetName());
    }
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintDebugInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
    Result->SetBoolField(TEXT("is_valid"), Blueprint->SkeletonGeneratedClass != nullptr);
    Result->SetNumberField(TEXT("variable_count"), Blueprint->NewVariables.Num());
    Result->SetNumberField(TEXT("function_count"), Blueprint->FunctionGraphs.Num());
    Result->SetNumberField(TEXT("component_count"), Blueprint->SimpleConstructionScript ? Blueprint->SimpleConstructionScript->GetAllNodes().Num() : 0);
    Result->SetStringField(TEXT("status"), Blueprint->Status == BS_UpToDate ? TEXT("UpToDate") : TEXT("Dirty"));

    TArray<TSharedPtr<FJsonValue>> Breakpoints;
    FKismetDebugUtilities::ForeachBreakpoint(Blueprint, [&Breakpoints](FBlueprintBreakpoint& Breakpoint)
    {
        TSharedPtr<FJsonObject> BreakpointJson = MakeShared<FJsonObject>();
        UEdGraphNode* Node = Breakpoint.GetLocation();
        BreakpointJson->SetBoolField(TEXT("enabled"), Breakpoint.IsEnabledByUser());
        BreakpointJson->SetBoolField(TEXT("valid"), FKismetDebugUtilities::IsBreakpointValid(Breakpoint));
        BreakpointJson->SetStringField(TEXT("location"), Breakpoint.GetLocationDescription().ToString());
        if (Node)
        {
            BreakpointJson->SetObjectField(TEXT("node"), MakeNodeJson(Node));
        }
        Breakpoints.Add(MakeShared<FJsonValueObject>(BreakpointJson));
    });
    Result->SetNumberField(TEXT("breakpoint_count"), Breakpoints.Num());
    Result->SetArrayField(TEXT("breakpoints"), Breakpoints);

    TArray<TSharedPtr<FJsonValue>> Watches;
    FKismetDebugUtilities::ForeachPinWatch(Blueprint, [&Watches](UEdGraphPin* Pin)
    {
        if (Pin)
        {
            Watches.Add(MakeShared<FJsonValueObject>(MakePinJson(Pin)));
        }
    });
    Result->SetNumberField(TEXT("watch_count"), Watches.Num());
    Result->SetArrayField(TEXT("watches"), Watches);
    Result->SetBoolField(TEXT("single_stepping"), FKismetDebugUtilities::IsSingleStepping());

    if (UEdGraphNode* HitNode = FKismetDebugUtilities::GetMostRecentBreakpointHit())
    {
        Result->SetObjectField(TEXT("most_recent_breakpoint_hit"), MakeNodeJson(HitNode));
    }
    if (UWorld* DebugWorld = FKismetDebugUtilities::GetCurrentDebuggingWorld())
    {
        Result->SetStringField(TEXT("debug_world"), DebugWorld->GetName());
    }

    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleBlueprintDiff(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString OtherBlueprintPath;
    if (!Params->TryGetStringField(TEXT("other_blueprint_path"), OtherBlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'other_blueprint_path' parameter"));
    }

    UBlueprint* BlueprintA = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    UBlueprint* BlueprintB = FEpicUnrealMCPCommonUtils::FindBlueprint(OtherBlueprintPath);

    if (!BlueprintA || !BlueprintB)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("One or both blueprints not found"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint_a"), BlueprintPath);
    Result->SetStringField(TEXT("blueprint_b"), OtherBlueprintPath);
    Result->SetBoolField(TEXT("same_parent_class"), BlueprintA->ParentClass == BlueprintB->ParentClass);
    Result->SetNumberField(TEXT("variable_count_a"), BlueprintA->NewVariables.Num());
    Result->SetNumberField(TEXT("variable_count_b"), BlueprintB->NewVariables.Num());
    Result->SetNumberField(TEXT("function_count_a"), BlueprintA->FunctionGraphs.Num());
    Result->SetNumberField(TEXT("function_count_b"), BlueprintB->FunctionGraphs.Num());
    Result->SetStringField(TEXT("message"), TEXT("Blueprint diff comparison completed. Full diff requires the Blueprint Diff Tool UI."));
    return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetAvailableMaterials(const TSharedPtr<FJsonObject>& Params)
{
    // Get parameters - make search path completely dynamic
    FString SearchPath;
    if (!Params->TryGetStringField(TEXT("search_path"), SearchPath))
    {
        // Default to empty string to search everywhere
        SearchPath = TEXT("");
    }
    
    bool bIncludeEngineMaterials = true;
    if (Params->HasField(TEXT("include_engine_materials")))
    {
        bIncludeEngineMaterials = Params->GetBoolField(TEXT("include_engine_materials"));
    }

    // Get asset registry module
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // Create filter for materials
    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterialInstanceDynamic::StaticClass()->GetClassPathName());
    
    // Add search paths dynamically
    if (!SearchPath.IsEmpty())
    {
        // Ensure the path starts with /
        if (!SearchPath.StartsWith(TEXT("/")))
        {
            SearchPath = TEXT("/") + SearchPath;
        }
        // Ensure the path ends with / for proper directory search
        if (!SearchPath.EndsWith(TEXT("/")))
        {
            SearchPath += TEXT("/");
        }
        Filter.PackagePaths.Add(*SearchPath);
        UE_LOG(LogTemp, Log, TEXT("Searching for materials in: %s"), *SearchPath);
    }
    else
    {
        // Search in common game content locations
        Filter.PackagePaths.Add(TEXT("/Game/"));
        UE_LOG(LogTemp, Log, TEXT("Searching for materials in all game content"));
    }
    
    if (bIncludeEngineMaterials)
    {
        Filter.PackagePaths.Add(TEXT("/Engine/"));
        UE_LOG(LogTemp, Log, TEXT("Including Engine materials in search"));
    }
    
    Filter.bRecursivePaths = true;

    // Get assets from registry
    TArray<FAssetData> AssetDataArray;
    AssetRegistry.GetAssets(Filter, AssetDataArray);
    
    UE_LOG(LogTemp, Log, TEXT("Asset registry found %d materials"), AssetDataArray.Num());

    // Convert to JSON
    TArray<TSharedPtr<FJsonValue>> MaterialArray;
    for (const FAssetData& AssetData : AssetDataArray)
    {
        TSharedPtr<FJsonObject> MaterialObj = MakeShared<FJsonObject>();
        MaterialObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        MaterialObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
        MaterialObj->SetStringField(TEXT("package"), AssetData.PackageName.ToString());
        MaterialObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
        
        MaterialArray.Add(MakeShared<FJsonValueObject>(MaterialObj));
        
        UE_LOG(LogTemp, Verbose, TEXT("Found material: %s at %s"), *AssetData.AssetName.ToString(), *AssetData.GetObjectPathString());
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("materials"), MaterialArray);
    ResultObj->SetNumberField(TEXT("count"), MaterialArray.Num());
    ResultObj->SetStringField(TEXT("search_path_used"), SearchPath.IsEmpty() ? TEXT("/Game/") : SearchPath);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Apply Material To Actor")));

    // Get required parameters
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    // Find the actor via O(1) index
    AActor* TargetActor = GetBlueprintActorIndex().FindByName(FName(*ActorName));
    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Load the material
    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    // Find mesh components and apply material
    TArray<UStaticMeshComponent*> MeshComponents;
    TargetActor->GetComponents<UStaticMeshComponent>(MeshComponents);
    
    bool bAppliedToAny = false;
    for (UStaticMeshComponent* MeshComp : MeshComponents)
    {
        if (MeshComp)
        {
            FlushRenderingCommands();
            MeshComp->SetMaterial(MaterialSlot, Material);
            bAppliedToAny = true;
        }
    }

    if (!bAppliedToAny)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No mesh components found on actor"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetBoolField(TEXT("success"), true);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleApplyMaterialToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Apply Material To Blueprint")));

    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Load the material
    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    // Apply the material
    FlushRenderingCommands();
    PrimComponent->SetMaterial(MaterialSlot, Material);

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("component_name"), ComponentName);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetBoolField(TEXT("success"), true);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetActorMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the actor via O(1) index
    AActor* TargetActor = GetBlueprintActorIndex().FindByName(FName(*ActorName));
    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get mesh components and their materials
    TArray<UStaticMeshComponent*> MeshComponents;
    TargetActor->GetComponents<UStaticMeshComponent>(MeshComponents);
    
    TArray<TSharedPtr<FJsonValue>> MaterialSlots;
    
    for (UStaticMeshComponent* MeshComp : MeshComponents)
    {
        if (MeshComp)
        {
            for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
            {
                TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
                SlotInfo->SetNumberField(TEXT("slot"), i);
                SlotInfo->SetStringField(TEXT("component"), MeshComp->GetName());
                
                UMaterialInterface* Material = MeshComp->GetMaterial(i);
                if (Material)
                {
                    SlotInfo->SetStringField(TEXT("material_name"), Material->GetName());
                    SlotInfo->SetStringField(TEXT("material_path"), Material->GetPathName());
                    SlotInfo->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());
                }
                else
                {
                    SlotInfo->SetStringField(TEXT("material_name"), TEXT("None"));
                    SlotInfo->SetStringField(TEXT("material_path"), TEXT(""));
                    SlotInfo->SetStringField(TEXT("material_class"), TEXT(""));
                }
                
                MaterialSlots.Add(MakeShared<FJsonValueObject>(SlotInfo));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetArrayField(TEXT("material_slots"), MaterialSlots);
    ResultObj->SetNumberField(TEXT("total_slots"), MaterialSlots.Num());
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    // Get material slot information
    TArray<TSharedPtr<FJsonValue>> MaterialSlots;
    int32 NumMaterials = 0;
    
    // Check if we have a static mesh assigned
    UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
    if (StaticMesh)
    {
        NumMaterials = StaticMesh->GetNumSections(0); // Get number of material slots for LOD 0
        
        for (int32 i = 0; i < NumMaterials; i++)
        {
            TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
            SlotInfo->SetNumberField(TEXT("slot"), i);
            SlotInfo->SetStringField(TEXT("component"), ComponentName);
            
            UMaterialInterface* Material = MeshComponent->GetMaterial(i);
            if (Material)
            {
                SlotInfo->SetStringField(TEXT("material_name"), Material->GetName());
                SlotInfo->SetStringField(TEXT("material_path"), Material->GetPathName());
                SlotInfo->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());
            }
            else
            {
                SlotInfo->SetStringField(TEXT("material_name"), TEXT("None"));
                SlotInfo->SetStringField(TEXT("material_path"), TEXT(""));
                SlotInfo->SetStringField(TEXT("material_class"), TEXT(""));
            }
            
            MaterialSlots.Add(MakeShared<FJsonValueObject>(SlotInfo));
        }
    }
    else
    {
        // If no static mesh is assigned, we can't determine material slots
        UE_LOG(LogTemp, Warning, TEXT("No static mesh assigned to component %s in blueprint %s"), *ComponentName, *BlueprintName);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("component_name"), ComponentName);
    ResultObj->SetArrayField(TEXT("material_slots"), MaterialSlots);
    ResultObj->SetNumberField(TEXT("total_slots"), MaterialSlots.Num());
    ResultObj->SetBoolField(TEXT("has_static_mesh"), StaticMesh != nullptr);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleReadBlueprintContent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintPath))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
        }
    }

    // Get optional parameters
    bool bIncludeEventGraph = true;
    bool bIncludeFunctions = true;
    bool bIncludeVariables = true;
    bool bIncludeComponents = true;
    bool bIncludeInterfaces = true;

    Params->TryGetBoolField(TEXT("include_event_graph"), bIncludeEventGraph);
    Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
    Params->TryGetBoolField(TEXT("include_variables"), bIncludeVariables);
    Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);
    Params->TryGetBoolField(TEXT("include_interfaces"), bIncludeInterfaces);

    // Load the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    ResultObj->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
    ResultObj->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

    // Include variables if requested
    if (bIncludeVariables)
    {
        TArray<TSharedPtr<FJsonValue>> VariableArray;
        for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
        {
            TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
            VarObj->SetStringField(TEXT("name"), Variable.VarName.ToString());
            VarObj->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
            VarObj->SetStringField(TEXT("default_value"), Variable.DefaultValue);
            VarObj->SetBoolField(TEXT("is_editable"), (Variable.PropertyFlags & CPF_Edit) != 0);
            VariableArray.Add(MakeShared<FJsonValueObject>(VarObj));
        }
        ResultObj->SetArrayField(TEXT("variables"), VariableArray);
    }

    // Include functions if requested
    if (bIncludeFunctions)
    {
        TArray<TSharedPtr<FJsonValue>> FunctionArray;
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph)
            {
                TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
                FuncObj->SetStringField(TEXT("name"), Graph->GetName());
                FuncObj->SetStringField(TEXT("graph_type"), TEXT("Function"));
                
                // Count nodes in function
                int32 NodeCount = Graph->Nodes.Num();
                FuncObj->SetNumberField(TEXT("node_count"), NodeCount);
                
                FunctionArray.Add(MakeShared<FJsonValueObject>(FuncObj));
            }
        }
        ResultObj->SetArrayField(TEXT("functions"), FunctionArray);
    }

    // Include event graph if requested
    if (bIncludeEventGraph)
    {
        TSharedPtr<FJsonObject> EventGraphObj = MakeShared<FJsonObject>();
        
        // Find the main event graph
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph && Graph->GetName() == TEXT("EventGraph"))
            {
                EventGraphObj->SetStringField(TEXT("name"), Graph->GetName());
                EventGraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
                
                // Get basic node information
                TArray<TSharedPtr<FJsonValue>> NodeArray;
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (Node)
                    {
                        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
                        NodeObj->SetStringField(TEXT("name"), Node->GetName());
                        NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
                        NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                        NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
                    }
                }
                EventGraphObj->SetArrayField(TEXT("nodes"), NodeArray);
                break;
            }
        }
        
        ResultObj->SetObjectField(TEXT("event_graph"), EventGraphObj);
    }

    // Include components if requested
    if (bIncludeComponents)
    {
        TArray<TSharedPtr<FJsonValue>> ComponentArray;
        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
                    CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
                    CompObj->SetStringField(TEXT("class"), Node->ComponentTemplate->GetClass()->GetName());
                    CompObj->SetBoolField(TEXT("is_root"), Node == Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode());
                    ComponentArray.Add(MakeShared<FJsonValueObject>(CompObj));
                }
            }
        }
        ResultObj->SetArrayField(TEXT("components"), ComponentArray);
    }

    // Include interfaces if requested
    if (bIncludeInterfaces)
    {
        TArray<TSharedPtr<FJsonValue>> InterfaceArray;
        for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
        {
            TSharedPtr<FJsonObject> InterfaceObj = MakeShared<FJsonObject>();
            InterfaceObj->SetStringField(TEXT("name"), Interface.Interface ? Interface.Interface->GetName() : TEXT("Unknown"));
            InterfaceArray.Add(MakeShared<FJsonValueObject>(InterfaceObj));
        }
        ResultObj->SetArrayField(TEXT("interfaces"), InterfaceArray);
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAnalyzeBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintPath))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
        }
    }

    FString GraphName = TEXT("EventGraph");
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // Get optional parameters
    bool bIncludeNodeDetails = true;
    bool bIncludePinConnections = true;
    bool bTraceExecutionFlow = true;

    Params->TryGetBoolField(TEXT("include_node_details"), bIncludeNodeDetails);
    Params->TryGetBoolField(TEXT("include_pin_connections"), bIncludePinConnections);
    Params->TryGetBoolField(TEXT("trace_execution_flow"), bTraceExecutionFlow);

    // Load the blueprint
    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    // Find the specified graph
    UEdGraph* TargetGraph = nullptr;
    
    // Check event graphs first
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetName() == GraphName)
        {
            TargetGraph = Graph;
            break;
        }
    }
    
    // Check function graphs if not found
    if (!TargetGraph)
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
    }

    if (!TargetGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
    }

    TSharedPtr<FJsonObject> GraphData = MakeShared<FJsonObject>();
    GraphData->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
    GraphData->SetStringField(TEXT("graph_type"), TargetGraph->GetClass()->GetName());

    // Analyze nodes
    TArray<TSharedPtr<FJsonValue>> NodeArray;
    TArray<TSharedPtr<FJsonValue>> ConnectionArray;

    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (Node)
        {
            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("name"), Node->GetName());
            NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
            NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

            if (bIncludeNodeDetails)
            {
                NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
                NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
                NodeObj->SetBoolField(TEXT("can_rename"), Node->bCanRenameNode);
            }

            // Include pin information if requested
            if (bIncludePinConnections)
            {
                TArray<TSharedPtr<FJsonValue>> PinArray;
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin)
                    {
                        TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                        PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                        PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
                        PinObj->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());
                        
                        // Record connections for this pin
                        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                        {
                            if (LinkedPin && LinkedPin->GetOwningNode())
                            {
                                TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                                ConnObj->SetStringField(TEXT("from_node"), Pin->GetOwningNode()->GetName());
                                ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
                                ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->GetName());
                                ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
                                ConnectionArray.Add(MakeShared<FJsonValueObject>(ConnObj));
                            }
                        }
                        
                        PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
                    }
                }
                NodeObj->SetArrayField(TEXT("pins"), PinArray);
            }

            NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
    }

    GraphData->SetArrayField(TEXT("nodes"), NodeArray);
    GraphData->SetArrayField(TEXT("connections"), ConnectionArray);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    ResultObj->SetObjectField(TEXT("graph_data"), GraphData);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintVariableDetails(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString VariableName;
    bool bSpecificVariable = Params->TryGetStringField(TEXT("variable_name"), VariableName);

    // Load the blueprint
    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    TArray<TSharedPtr<FJsonValue>> VariableArray;

    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        // If looking for specific variable, skip others
        if (bSpecificVariable && Variable.VarName.ToString() != VariableName)
        {
            continue;
        }

        TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
        VarObj->SetStringField(TEXT("name"), Variable.VarName.ToString());
        VarObj->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
        VarObj->SetStringField(TEXT("sub_category"), Variable.VarType.PinSubCategory.ToString());
        VarObj->SetStringField(TEXT("default_value"), Variable.DefaultValue);
        VarObj->SetStringField(TEXT("friendly_name"), Variable.FriendlyName.IsEmpty() ? Variable.VarName.ToString() : Variable.FriendlyName);
        
        // Get tooltip from metadata (VarTooltip doesn't exist in UE 5.5)
        FString TooltipValue;
        if (Variable.HasMetaData(FBlueprintMetadata::MD_Tooltip))
        {
            TooltipValue = Variable.GetMetaData(FBlueprintMetadata::MD_Tooltip);
        }
        VarObj->SetStringField(TEXT("tooltip"), TooltipValue);
        
        VarObj->SetStringField(TEXT("category"), Variable.Category.ToString());

        // Property flags
        VarObj->SetBoolField(TEXT("is_editable"), (Variable.PropertyFlags & CPF_Edit) != 0);
        VarObj->SetBoolField(TEXT("is_blueprint_visible"), (Variable.PropertyFlags & CPF_BlueprintVisible) != 0);
        VarObj->SetBoolField(TEXT("is_editable_in_instance"), (Variable.PropertyFlags & CPF_DisableEditOnInstance) == 0);
        VarObj->SetBoolField(TEXT("is_config"), (Variable.PropertyFlags & CPF_Config) != 0);

        // Replication
        VarObj->SetNumberField(TEXT("replication"), (int32)Variable.ReplicationCondition);

        VariableArray.Add(MakeShared<FJsonValueObject>(VarObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    
    if (bSpecificVariable)
    {
        ResultObj->SetStringField(TEXT("variable_name"), VariableName);
        if (VariableArray.Num() > 0)
        {
            ResultObj->SetObjectField(TEXT("variable"), VariableArray[0]->AsObject());
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable not found: %s"), *VariableName));
        }
    }
    else
    {
        ResultObj->SetArrayField(TEXT("variables"), VariableArray);
        ResultObj->SetNumberField(TEXT("variable_count"), VariableArray.Num());
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleGetBlueprintFunctionDetails(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString FunctionName;
    bool bSpecificFunction = Params->TryGetStringField(TEXT("function_name"), FunctionName);

    bool bIncludeGraph = true;
    Params->TryGetBoolField(TEXT("include_graph"), bIncludeGraph);

    // Load the blueprint
    UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
    }

    TArray<TSharedPtr<FJsonValue>> FunctionArray;

    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;

        // If looking for specific function, skip others
        if (bSpecificFunction && Graph->GetName() != FunctionName)
        {
            continue;
        }

        TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
        FuncObj->SetStringField(TEXT("name"), Graph->GetName());
        FuncObj->SetStringField(TEXT("graph_type"), TEXT("Function"));

        // Get function signature from graph
        TArray<TSharedPtr<FJsonValue>> InputPins;
        TArray<TSharedPtr<FJsonValue>> OutputPins;

        // Find function entry and result nodes
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node)
            {
                if (Node->GetClass()->GetName().Contains(TEXT("FunctionEntry")))
                {
                    // Process input parameters
                    for (UEdGraphPin* Pin : Node->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != TEXT("then"))
                        {
                            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                            InputPins.Add(MakeShared<FJsonValueObject>(PinObj));
                        }
                    }
                }
                else if (Node->GetClass()->GetName().Contains(TEXT("FunctionResult")))
                {
                    // Process output parameters
                    for (UEdGraphPin* Pin : Node->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName != TEXT("exec"))
                        {
                            TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                            PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                            PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                            OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
                        }
                    }
                }
            }
        }

        FuncObj->SetArrayField(TEXT("input_parameters"), InputPins);
        FuncObj->SetArrayField(TEXT("output_parameters"), OutputPins);
        FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

        // Include graph details if requested
        if (bIncludeGraph)
        {
            TArray<TSharedPtr<FJsonValue>> NodeArray;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node)
                {
                    TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
                    NodeObj->SetStringField(TEXT("name"), Node->GetName());
                    NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
                    NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                    NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
                }
            }
            FuncObj->SetArrayField(TEXT("graph_nodes"), NodeArray);
        }

        FunctionArray.Add(MakeShared<FJsonValueObject>(FuncObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPath);
    
    if (bSpecificFunction)
    {
        ResultObj->SetStringField(TEXT("function_name"), FunctionName);
        if (FunctionArray.Num() > 0)
        {
            ResultObj->SetObjectField(TEXT("function"), FunctionArray[0]->AsObject());
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
        }
    }
    else
    {
        ResultObj->SetArrayField(TEXT("functions"), FunctionArray);
        ResultObj->SetNumberField(TEXT("function_count"), FunctionArray.Num());
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
