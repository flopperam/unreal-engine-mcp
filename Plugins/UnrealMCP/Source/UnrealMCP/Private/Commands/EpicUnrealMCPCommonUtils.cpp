#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/TextureCube.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet/GameplayStatics.h"

// FActorIndex implementation

void FActorIndex::AddActor(AActor* Actor)
{
    if (!Actor || !IsValid(Actor)) return;
    NameIndex.Add(FName(*Actor->GetName()), Actor);

    for (const FName& Tag : Actor->Tags)
    {
        FString TagStr = Tag.ToString();
        if (TagStr.StartsWith(TEXT("mcp_id:")))
        {
            FString McpId = TagStr.Mid(7);
            McpIdIndex.Add(McpId, Actor);
            break; // Use first mcp_id tag
        }
    }
}

void FActorIndex::RemoveActor(AActor* Actor)
{
    if (!Actor) return;
    NameIndex.Remove(FName(*Actor->GetName()));

    for (const FName& Tag : Actor->Tags)
    {
        FString TagStr = Tag.ToString();
        if (TagStr.StartsWith(TEXT("mcp_id:")))
        {
            McpIdIndex.Remove(TagStr.Mid(7));
            break;
        }
    }
}

AActor* FActorIndex::FindByName(const FName& Name)
{
    TWeakObjectPtr<AActor>* Found = NameIndex.Find(Name);
    if (Found && Found->IsValid())
    {
        return Found->Get();
    }
    return nullptr;
}

AActor* FActorIndex::FindByMcpId(const FString& McpId)
{
    TWeakObjectPtr<AActor>* Found = McpIdIndex.Find(McpId);
    if (Found && Found->IsValid())
    {
        return Found->Get();
    }
    return nullptr;
}

void FActorIndex::RebuildFromWorld(UWorld* World)
{
    Clear();
    if (!World) return;

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            AddActor(Actor);
        }
    }
}

void FActorIndex::Clear()
{
    NameIndex.Reset();
    McpIdIndex.Reset();
}

// JSON Utilities
TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FEpicUnrealMCPCommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FEpicUnrealMCPCommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FEpicUnrealMCPCommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FEpicUnrealMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

bool FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FVector& OutVector, FString& OutError)
{
    if (!JsonObject->HasField(FieldName))
    {
        OutError = FString::Printf(TEXT("Missing field: %s"), *FieldName);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (!JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        OutError = FString::Printf(TEXT("Field %s must be an array"), *FieldName);
        return false;
    }

    if (JsonArray->Num() < 3)
    {
        OutError = FString::Printf(TEXT("Field %s must have at least 3 elements, got %d"), *FieldName, JsonArray->Num());
        return false;
    }

    for (int32 i = 0; i < 3; ++i)
    {
        if ((*JsonArray)[i]->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("Field %s element [%d] must be a number"), *FieldName, i);
            return false;
        }
    }

    OutVector.X = (float)(*JsonArray)[0]->AsNumber();
    OutVector.Y = (float)(*JsonArray)[1]->AsNumber();
    OutVector.Z = (float)(*JsonArray)[2]->AsNumber();

    return true;
}

FRotator FEpicUnrealMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

bool FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FRotator& OutRotator, FString& OutError)
{
    if (!JsonObject->HasField(FieldName))
    {
        OutError = FString::Printf(TEXT("Missing field: %s"), *FieldName);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (!JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        OutError = FString::Printf(TEXT("Field %s must be an array"), *FieldName);
        return false;
    }

    if (JsonArray->Num() < 3)
    {
        OutError = FString::Printf(TEXT("Field %s must have at least 3 elements, got %d"), *FieldName, JsonArray->Num());
        return false;
    }

    for (int32 i = 0; i < 3; ++i)
    {
        if ((*JsonArray)[i]->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("Field %s element [%d] must be a number"), *FieldName, i);
            return false;
        }
    }

    OutRotator.Pitch = (float)(*JsonArray)[0]->AsNumber();
    OutRotator.Yaw = (float)(*JsonArray)[1]->AsNumber();
    OutRotator.Roll = (float)(*JsonArray)[2]->AsNumber();

    return true;
}

// Blueprint Utilities
UBlueprint* FEpicUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

UBlueprint* FEpicUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    // The correct object path for a Blueprint asset is /Game/Path/AssetName.AssetName
    FString ObjectPath;

    // Check if BlueprintName is already a full path (starts with /)
    if (BlueprintName.StartsWith(TEXT("/")))
    {
        // It's already a full path, use it directly with the class suffix
        FString AssetName = FPaths::GetBaseFilename(BlueprintName);
        ObjectPath = FString::Printf(TEXT("%s.%s"), *BlueprintName, *AssetName);
    }
    else
    {
        // It's just a name, add the default /Game/Blueprints/ prefix
        ObjectPath = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *BlueprintName, *BlueprintName);
    }

    // First, try to load the object directly, as it's the fastest method.
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
    if (Blueprint)
    {
        return Blueprint;
    }

    // If direct loading fails, try to find the asset using the Asset Registry.
    // This is more robust for newly created assets.
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));

    if (AssetData.IsValid())
    {
        Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
        if (Blueprint)
        {
            return Blueprint;
        }
    }

    // Fallback for cases where the asset lives outside /Game/Blueprints.
    // Gameplay Framework helpers create assets in domain-specific folders.
    if (!BlueprintName.StartsWith(TEXT("/")))
    {
        FARFilter Filter;
        Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        Filter.bRecursivePaths = true;

        TArray<FAssetData> BlueprintAssets;
        AssetRegistryModule.Get().GetAssets(Filter, BlueprintAssets);
        for (const FAssetData& Candidate : BlueprintAssets)
        {
            if (Candidate.AssetName.ToString().Equals(BlueprintName, ESearchCase::IgnoreCase))
            {
                Blueprint = Cast<UBlueprint>(Candidate.GetAsset());
                if (Blueprint)
                {
                    return Blueprint;
                }
            }
        }
    }

    // Fallback for cases where the asset is in memory but not yet fully saved,
    // where it might be found via its package path.
    FString PackagePath = TEXT("/Game/Blueprints/") + BlueprintName;
    Blueprint = FindObject<UBlueprint>(nullptr, *PackagePath);

    if (!Blueprint)
    {
         UE_LOG(LogTemp, Error, TEXT("FindBlueprintByName: Failed to find or load blueprint: %s"), *BlueprintName);
    }

    return Blueprint;
}

UEdGraph* FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Blueprint node utilities
UK2Node_Event* FEpicUnrealMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

UK2Node_CallFunction* FEpicUnrealMCPCommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* FEpicUnrealMCPCommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

UK2Node_VariableSet* FEpicUnrealMCPCommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

UK2Node_InputAction* FEpicUnrealMCPCommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

UK2Node_Self* FEpicUnrealMCPCommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

bool FEpicUnrealMCPCommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                           UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph || !SourceNode || !TargetNode)
    {
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        return true;
    }
    
    return false;
}

UEdGraphPin* FEpicUnrealMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Verbose, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Verbose, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Verbose, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Verbose, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Verbose, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Actor utilities
TSharedPtr<FJsonValue> FEpicUnrealMCPCommonUtils::ActorToJson(AActor* Actor)
{
    TSharedPtr<FJsonObject> Object = ActorToJsonObject(Actor);
    if (!Object)
    {
        return MakeShared<FJsonValueNull>();
    }
    return MakeShared<FJsonValueObject>(Object);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);

    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);

    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);

    if (bDetailed)
    {
        FString StaticMeshPath;
        UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Actor->GetComponentByClass(UStaticMeshComponent::StaticClass()));
        if (MeshComp && MeshComp->GetStaticMesh())
        {
            StaticMeshPath = MeshComp->GetStaticMesh()->GetPathName();
        }
        ActorObject->SetStringField(TEXT("static_mesh"), StaticMeshPath);

        // Light component serialization
        ULightComponent* LightComp = Cast<ULightComponent>(Actor->GetComponentByClass(ULightComponent::StaticClass()));
        if (LightComp)
        {
            ActorObject->SetNumberField(TEXT("light_intensity"), LightComp->Intensity);
            FLinearColor Color = LightComp->GetLightColor();
            TArray<TSharedPtr<FJsonValue>> ColorArray;
            ColorArray.Add(MakeShared<FJsonValueNumber>(Color.R));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Color.G));
            ColorArray.Add(MakeShared<FJsonValueNumber>(Color.B));
            ActorObject->SetArrayField(TEXT("light_color"), ColorArray);
            ActorObject->SetBoolField(TEXT("light_temperature_enabled"), LightComp->bUseTemperature);
            ActorObject->SetNumberField(TEXT("light_temperature"), LightComp->Temperature);
            ActorObject->SetBoolField(TEXT("cast_shadows"), LightComp->CastShadows);
            ActorObject->SetNumberField(TEXT("shadow_bias"), LightComp->ShadowBias);
            ActorObject->SetNumberField(TEXT("contact_shadow_length"), LightComp->ContactShadowLength);
            ActorObject->SetNumberField(TEXT("volumetric_scattering_intensity"), LightComp->VolumetricScatteringIntensity);
            ActorObject->SetBoolField(TEXT("affects_world"), LightComp->bAffectsWorld);

            // Point/Spot light specific
            UPointLightComponent* PointComp = Cast<UPointLightComponent>(LightComp);
            if (PointComp)
            {
                ActorObject->SetNumberField(TEXT("attenuation_radius"), PointComp->AttenuationRadius);
                ActorObject->SetNumberField(TEXT("source_radius"), PointComp->SourceRadius);
                ActorObject->SetNumberField(TEXT("soft_source_radius"), PointComp->SoftSourceRadius);
                ActorObject->SetBoolField(TEXT("use_inverse_squared_falloff"), PointComp->bUseInverseSquaredFalloff);
            }

            // Spot light specific
            USpotLightComponent* SpotComp = Cast<USpotLightComponent>(LightComp);
            if (SpotComp)
            {
                ActorObject->SetNumberField(TEXT("inner_cone_angle"), SpotComp->InnerConeAngle);
                ActorObject->SetNumberField(TEXT("outer_cone_angle"), SpotComp->OuterConeAngle);
            }

            // Rect light specific
            URectLightComponent* RectComp = Cast<URectLightComponent>(LightComp);
            if (RectComp)
            {
                ActorObject->SetNumberField(TEXT("source_width"), RectComp->SourceWidth);
                ActorObject->SetNumberField(TEXT("source_height"), RectComp->SourceHeight);
                ActorObject->SetNumberField(TEXT("barn_door_angle"), RectComp->BarnDoorAngle);
                ActorObject->SetNumberField(TEXT("barn_door_length"), RectComp->BarnDoorLength);
            }
        }

        // Sky light specific
        USkyLightComponent* SkyComp = Cast<USkyLightComponent>(Actor->GetComponentByClass(USkyLightComponent::StaticClass()));
        if (SkyComp)
        {
            ActorObject->SetNumberField(TEXT("sky_intensity"), SkyComp->Intensity);
            if (UTextureCube* Cubemap = SkyComp->Cubemap.Get())
            {
                ActorObject->SetStringField(TEXT("cubemap_path"), Cubemap->GetPathName());
            }
        }

        // Sky atmosphere specific
        USkyAtmosphereComponent* AtmosComp = Cast<USkyAtmosphereComponent>(Actor->GetComponentByClass(USkyAtmosphereComponent::StaticClass()));
        if (AtmosComp)
        {
            ActorObject->SetBoolField(TEXT("transform_mode"), AtmosComp->TransformMode == ESkyAtmosphereTransformMode::PlanetTopAtAbsoluteWorldOrigin);
        }

        // Exponential height fog specific
        UExponentialHeightFogComponent* FogComp = Cast<UExponentialHeightFogComponent>(Actor->GetComponentByClass(UExponentialHeightFogComponent::StaticClass()));
        if (FogComp)
        {
            ActorObject->SetNumberField(TEXT("fog_density"), FogComp->FogDensity);
            ActorObject->SetNumberField(TEXT("fog_height_falloff"), FogComp->FogHeightFalloff);
            ActorObject->SetNumberField(TEXT("fog_max_opacity"), FogComp->FogMaxOpacity);
            ActorObject->SetNumberField(TEXT("start_distance"), FogComp->StartDistance);
            ActorObject->SetBoolField(TEXT("volumetric_fog_enabled"), FogComp->bEnableVolumetricFog);
        }
    }

    TArray<TSharedPtr<FJsonValue>> TagsArray;
    for (const FName& Tag : Actor->Tags)
    {
        TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    ActorObject->SetArrayField(TEXT("tags"), TagsArray);

    ActorObject->SetBoolField(TEXT("success"), true);

    return ActorObject;
}

UK2Node_Event* FEpicUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

bool FEpicUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    // Security: only allow setting properties that are in the allowlist.
    // This prevents arbitrary property mutation on any UObject via MCP.
    static const TSet<FString> AllowedProperties = {
        // Mobility
        TEXT("Mobility"),
        // Transform
        TEXT("RelativeLocation"),
        TEXT("RelativeRotation"),
        TEXT("RelativeScale3D"),
        TEXT("AttachParent"),
        TEXT("AttachSocketName"),
        // Physics
        TEXT("BodyInstance"),
        TEXT("SimulatePhysics"),
        TEXT("IsGravityEnabled"),
        TEXT("MassOverride"),
        TEXT("MassInKg"),
        TEXT("LinearDamping"),
        TEXT("AngularDamping"),
        TEXT("EnableGravity"),
        TEXT("PhysMaterialOverride"),
        // Collision
        TEXT("CollisionEnabled"),
        TEXT("CollisionProfileName"),
        TEXT("bGenerateOverlapEvents"),
        // Rendering
        TEXT("Visibility"),
        TEXT("Hidden"),
        TEXT("bHidden"),
        TEXT("bVisible"),
        TEXT("CastShadow"),
        TEXT("bCastDynamicShadow"),
        TEXT("bCastStaticShadow"),
        TEXT("bAffectDynamicIndirectLighting"),
        TEXT("bAffectIndirectLightingWhileHidden"),
        // Material
        TEXT("OverrideMaterials"),
        // Static mesh
        TEXT("StaticMesh"),
        TEXT("StaticMeshComponent"),
        // Lighting
        TEXT("LightIntensity"),
        TEXT("LightColor"),
        TEXT("AttenuationRadius"),
        TEXT("InnerConeAngle"),
        TEXT("OuterConeAngle"),
        TEXT("bUseTemperature"),
        TEXT("Temperature"),
        TEXT("bAffectsWorld"),
        TEXT("bUseInverseSquaredFalloff"),
        TEXT("SourceRadius"),
        TEXT("SoftSourceRadius"),
        TEXT("CastShadows"),
        TEXT("ShadowBias"),
        TEXT("ContactShadowLength"),
        TEXT("VolumetricScatteringIntensity"),
        TEXT("SourceWidth"),
        TEXT("SourceHeight"),
        TEXT("BarnDoorAngle"),
        TEXT("BarnDoorLength"),
        TEXT("FogDensity"),
        TEXT("FogHeightFalloff"),
        TEXT("FogMaxOpacity"),
        TEXT("StartDistance"),
        TEXT("bEnableVolumetricFog"),
        // Gameplay
        TEXT("bReplicates"),
        TEXT("bAlwaysRelevant"),
        TEXT("NetUpdateFrequency"),
        TEXT("NetPriority"),
        TEXT("bNetLoadOnClient"),
        TEXT("bCanBeDamaged"),
        TEXT("Tags"),
        // String/Text
        TEXT("Text"),
        TEXT("String"),
        TEXT("DisplayName"),
        TEXT("Description"),
        // Numeric
        TEXT("Value"),
        TEXT("FloatValue"),
        TEXT("IntValue"),
        TEXT("bBooleanValue"),
        // Enum
        TEXT("EnumValue"),
        // Node-specific (for set_node_property)
        TEXT("NodeComment"),
        TEXT("bCommentBubbleVisible"),
        TEXT("bIsReadOnly"),
        TEXT("ErrorType"),
        TEXT("ErrorMsg"),
        TEXT("bLocalVar"),
        TEXT("bVarChanged"),
        TEXT("bVarTypeChanged"),
        TEXT("OutputType"),
        TEXT("InputType"),
        TEXT("PinType"),
        TEXT("PinSubCategory"),
        TEXT("PinSubCategoryObject"),
        TEXT("DefaultValue"),
        TEXT("AutogeneratedDefaultValue"),
        TEXT("DefaultObject"),
        TEXT("DefaultTextValue"),
        TEXT("LinkedTo"),
        TEXT("bDefaultValueIsIgnored"),
        TEXT("bAdvancedView"),
        TEXT("bHidden"),
        TEXT("bNotConnectable"),
        TEXT("bIsConst"),
        TEXT("bIsReference"),
        TEXT("bIsArray"),
        TEXT("bIsMap"),
        TEXT("bIsSet"),
        TEXT("bIsOutParam"),
        TEXT("bIsReturn"),
        TEXT("bCanTreatLikeRefParam"),
        TEXT("bOverrideRef"),
        TEXT("bSelfContext"),
        TEXT("bIsWorldContext"),
        TEXT("bIsAutoExpand"),
        TEXT("bIsAutoCollapsing"),
        TEXT("bIsAutoSelect"),
        TEXT("bIsVisible"),
        TEXT("bIsEditable"),
        TEXT("bIsTransient"),
        TEXT("bIsComponentBound"),
        TEXT("bIsPinHidden"),
        TEXT("bShouldDefaultValueBeReadOnly"),
        TEXT("CustomImpulseVal"),
        TEXT("ImpulseType"),
    };

    FString PropertyNameStr = PropertyName;
    if (!AllowedProperties.Contains(PropertyNameStr))
    {
        OutErrorMessage = FString::Printf(
            TEXT("Property '%s' is not in the allowlist for SetObjectProperty. "
                 "Only explicitly approved properties can be set via MCP."),
            *PropertyName);
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"), 
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
} 
