#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

FEpicUnrealMCPBlueprintCommands::FEpicUnrealMCPBlueprintCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        return HandleAddComponentToBlueprint(Params);
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        return HandleSetPhysicsProperties(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_properties"))
    {
        return HandleSetStaticMeshProperties(Params);
    }
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    else if (CommandType == TEXT("set_mesh_material_color"))
    {
        return HandleSetMeshMaterialColor(Params);
    }
    else if (CommandType == TEXT("create_blueprint_variable"))
    {
        return HandleCreateBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("add_blueprint_event_node"))
    {
        return HandleAddBlueprintEventNode(Params);
    }
    else if (CommandType == TEXT("add_blueprint_function_node"))
    {
        return HandleAddBlueprintFunctionNode(Params);
    }
    else if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        return HandleConnectBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_branch_node"))
    {
        return HandleAddBlueprintBranchNode(Params);
    }
    else if (CommandType == TEXT("create_blueprint_custom_event"))
    {
        return HandleCreateBlueprintCustomEvent(Params);
    }
    
    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString PackagePath = TEXT("/Game/Blueprints/");
    FString AssetName = BlueprintName;
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);

    UClass* SelectedParentClass = AActor::StaticClass();

    if (!ParentClass.IsEmpty())
    {
        FString ClassName = ParentClass;
        if (!ClassName.StartsWith(TEXT("A")))
        {
            ClassName = TEXT("A") + ClassName;
        }

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
            // Try loading the class - LoadClass is more reliable than FindObject for runtime lookups
            const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            FoundClass = LoadClass<AActor>(nullptr, *ClassPath);

            if (!FoundClass)
            {
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

    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        FAssetRegistryModule::AssetCreated(NewBlueprint);
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
        return ResultObj;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UClass* ComponentClass = nullptr;

    ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentType);

    if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
    {
        FString ComponentTypeWithSuffix = ComponentType + TEXT("Component");
        ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentTypeWithSuffix);
    }

    if (!ComponentClass && !ComponentType.StartsWith(TEXT("U")))
    {
        FString ComponentTypeWithPrefix = TEXT("U") + ComponentType;
        ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentTypeWithPrefix);

        if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
        {
            FString ComponentTypeWithBoth = TEXT("U") + ComponentType + TEXT("Component");
            ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentTypeWithBoth);
        }
    }

    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
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

        Blueprint->SimpleConstructionScript->AddNode(NewNode);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("component_name"), ComponentName);
        ResultObj->SetStringField(TEXT("component_type"), ComponentType);
        return ResultObj;
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
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

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        float Mass = Params->GetNumberField(TEXT("mass"));
        // UE5.5 requires SetMassOverrideInKg for proper mass control
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

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    ResultObj->SetBoolField(TEXT("compiled"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FEpicUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));

    // Small delay to let the engine process newly compiled classes
    FPlatformProcess::Sleep(0.2f);

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);

    if (NewActor)
    {
        NewActor->SetActorLabel(*ActorName);
        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
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

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    if (Params->HasField(TEXT("static_mesh")))
    {
        FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (Mesh)
        {
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        FString MaterialPath = Params->GetStringField(TEXT("material"));
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            MeshComponent->SetMaterial(0, Material);
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
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

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

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

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    FString ParameterName = TEXT("BaseColor");
    Params->TryGetStringField(TEXT("parameter_name"), ParameterName);

    UMaterialInterface* Material = nullptr;

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
        Material = PrimComponent->GetMaterial(MaterialSlot);
        if (!Material)
        {
            Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
            if (!Material)
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No material found on component and failed to load default material"));
            }
        }
    }

    UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(Material, PrimComponent);
    if (!DynMaterial)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance"));
    }

    DynMaterial->SetVectorParameterValue(*ParameterName, Color);
    PrimComponent->SetMaterial(MaterialSlot, DynMaterial);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    UE_LOG(LogTemp, Log, TEXT("Set material color on component %s: R=%f, G=%f, B=%f, A=%f"),
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

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FEdGraphPinType PinType;

    if (VariableType == TEXT("bool"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (VariableType == TEXT("int"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType == TEXT("float"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType == TEXT("string"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType == TEXT("vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (VariableType == TEXT("rotator"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else if (VariableType == TEXT("transform"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *VariableType);
        if (FoundClass)
        {
            PinType.PinSubCategoryObject = FoundClass;
        }
    }

    FBPVariableDescription NewVariable;
    NewVariable.VarName = *VariableName;
    NewVariable.VarGuid = FGuid::NewGuid();
    NewVariable.FriendlyName = VariableName;
    NewVariable.Category = NSLOCTEXT("BlueprintEditor", "UserVariables", "Variables");
    NewVariable.PropertyFlags = CPF_Edit | CPF_BlueprintVisible;
    NewVariable.VarType = PinType;

    if (Params->HasField(TEXT("default_value")))
    {
        const TSharedPtr<FJsonValue> DefaultValue = Params->TryGetField(TEXT("default_value"));

        if (VariableType == TEXT("bool"))
        {
            NewVariable.DefaultValue = DefaultValue->AsBool() ? TEXT("true") : TEXT("false");
        }
        else if (VariableType == TEXT("int"))
        {
            NewVariable.DefaultValue = FString::FromInt((int32)DefaultValue->AsNumber());
        }
        else if (VariableType == TEXT("float"))
        {
            NewVariable.DefaultValue = FString::SanitizeFloat(DefaultValue->AsNumber());
        }
        else if (VariableType == TEXT("string"))
        {
            NewVariable.DefaultValue = DefaultValue->AsString();
        }
        else if (VariableType == TEXT("vector"))
        {
            const TArray<TSharedPtr<FJsonValue>>* VectorArray;
            if (DefaultValue->TryGetArray(VectorArray) && VectorArray->Num() == 3)
            {
                float X = (*VectorArray)[0]->AsNumber();
                float Y = (*VectorArray)[1]->AsNumber();
                float Z = (*VectorArray)[2]->AsNumber();
                NewVariable.DefaultValue = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
            }
        }
    }

    Blueprint->NewVariables.Add(NewVariable);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetStringField(TEXT("variable_type"), VariableType);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddBlueprintEventNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventType;
    if (!Params->TryGetStringField(TEXT("event_type"), EventType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_type' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(0, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FEpicUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    UK2Node_Event* EventNode = nullptr;

    if (EventType == TEXT("BeginPlay"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveBeginPlay"), NodePosition);
    }
    else if (EventType == TEXT("Tick"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveTick"), NodePosition);
    }
    else if (EventType == TEXT("BeginOverlap"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveActorBeginOverlap"), NodePosition);
    }
    else if (EventType == TEXT("EndOverlap"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveActorEndOverlap"), NodePosition);
    }
    else if (EventType == TEXT("Hit"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveHit"), NodePosition);
    }
    else if (EventType == TEXT("AnyDamage"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveAnyDamage"), NodePosition);
    }
    else if (EventType == TEXT("Destroyed"))
    {
        EventNode = FEpicUnrealMCPCommonUtils::CreateEventNode(EventGraph, TEXT("ReceiveDestroyed"), NodePosition);
    }
    else if (EventType == TEXT("Custom"))
    {
        FString CustomEventName = TEXT("CustomEvent");
        Params->TryGetStringField(TEXT("custom_event_name"), CustomEventName);

        UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventGraph);
        CustomEventNode->CustomFunctionName = *CustomEventName;
        CustomEventNode->NodePosX = NodePosition.X;
        CustomEventNode->NodePosY = NodePosition.Y;
        EventGraph->AddNode(CustomEventNode, true);
        CustomEventNode->PostPlacedNewNode();
        CustomEventNode->AllocateDefaultPins();
        CustomEventNode->ReconstructNode();

        EventNode = CustomEventNode;
    }
    else
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown event type: %s"), *EventType));
    }

    if (!EventNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("event_type"), EventType);
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
    ResultObj->SetNumberField(TEXT("position_x"), NodePosition.X);
    ResultObj->SetNumberField(TEXT("position_y"), NodePosition.Y);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddBlueprintFunctionNode(const TSharedPtr<FJsonObject>& Params)
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
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(300, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FEpicUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    FString FunctionClass;
    Params->TryGetStringField(TEXT("function_class"), FunctionClass);

    UFunction* Function = nullptr;

    if (!FunctionClass.IsEmpty())
    {
        UClass* TargetClass = FindObject<UClass>(ANY_PACKAGE, *FunctionClass);
        if (TargetClass)
        {
            Function = TargetClass->FindFunctionByName(*FunctionName);
        }
    }
    else
    {
        if (Blueprint->ParentClass)
        {
            Function = Blueprint->ParentClass->FindFunctionByName(*FunctionName);
        }
    }

    if (!Function)
    {
        UClass* GameplayStatics = UGameplayStatics::StaticClass();
        Function = GameplayStatics->FindFunctionByName(*FunctionName);
    }

    if (!Function)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
    }

    UK2Node_CallFunction* FunctionNode = FEpicUnrealMCPCommonUtils::CreateFunctionCallNode(EventGraph, Function, NodePosition);
    if (!FunctionNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create function call node"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);
    ResultObj->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
    ResultObj->SetNumberField(TEXT("position_x"), NodePosition.X);
    ResultObj->SetNumberField(TEXT("position_y"), NodePosition.Y);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId, SourcePin, TargetNodeId, TargetPin;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId) ||
        !Params->TryGetStringField(TEXT("source_pin"), SourcePin) ||
        !Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId) ||
        !Params->TryGetStringField(TEXT("target_pin"), TargetPin))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing connection parameters"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find event graph"));
    }

    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;

    FGuid SourceGuid, TargetGuid;
    if (FGuid::Parse(SourceNodeId, SourceGuid) && FGuid::Parse(TargetNodeId, TargetGuid))
    {
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (Node->NodeGuid == SourceGuid)
            {
                SourceNode = Node;
            }
            if (Node->NodeGuid == TargetGuid)
            {
                TargetNode = Node;
            }
        }
    }

    if (!SourceNode || !TargetNode)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not find source or target nodes"));
    }

    bool bConnected = FEpicUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, SourceNode, SourcePin, TargetNode, TargetPin);
    if (!bConnected)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect nodes"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("source_node"), SourceNodeId);
    ResultObj->SetStringField(TEXT("target_node"), TargetNodeId);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleAddBlueprintBranchNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    FVector2D NodePosition(600, 0);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FEpicUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(EventGraph);
    BranchNode->NodePosX = NodePosition.X;
    BranchNode->NodePosY = NodePosition.Y;
    EventGraph->AddNode(BranchNode, true);
    BranchNode->PostPlacedNewNode();
    BranchNode->AllocateDefaultPins();
    BranchNode->ReconstructNode();

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_type"), TEXT("Branch"));
    ResultObj->SetStringField(TEXT("node_id"), BranchNode->NodeGuid.ToString());
    ResultObj->SetNumberField(TEXT("position_x"), NodePosition.X);
    ResultObj->SetNumberField(TEXT("position_y"), NodePosition.Y);
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPBlueprintCommands::HandleCreateBlueprintCustomEvent(const TSharedPtr<FJsonObject>& Params)
{
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

    UBlueprint* Blueprint = FEpicUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or create event graph"));
    }

    UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventGraph);
    CustomEventNode->CustomFunctionName = *EventName;
    CustomEventNode->NodePosX = 0;
    CustomEventNode->NodePosY = 0;
    EventGraph->AddNode(CustomEventNode, true);
    CustomEventNode->PostPlacedNewNode();
    CustomEventNode->AllocateDefaultPins();

    if (Params->HasField(TEXT("input_params")))
    {
        const TArray<TSharedPtr<FJsonValue>>* ParamsArray;
        if (Params->TryGetArrayField(TEXT("input_params"), ParamsArray))
        {
            for (const TSharedPtr<FJsonValue>& ParamValue : *ParamsArray)
            {
                const TSharedPtr<FJsonObject>* ParamObj;
                if (ParamValue->TryGetObject(ParamObj))
                {
                    FString ParamName, ParamType;
                    if ((*ParamObj)->TryGetStringField(TEXT("name"), ParamName) &&
                        (*ParamObj)->TryGetStringField(TEXT("type"), ParamType))
                    {
                        FBPVariableDescription NewParam;
                        NewParam.VarName = *ParamName;
                        NewParam.VarGuid = FGuid::NewGuid();
                        NewParam.FriendlyName = ParamName;

                        if (ParamType == TEXT("bool"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
                        }
                        else if (ParamType == TEXT("int"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Int;
                        }
                        else if (ParamType == TEXT("float"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Real;
                            NewParam.VarType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
                        }
                        else if (ParamType == TEXT("string"))
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_String;
                        }
                        else
                        {
                            NewParam.VarType.PinCategory = UEdGraphSchema_K2::PC_Object;
                            UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *ParamType);
                            if (FoundClass)
                            {
                                NewParam.VarType.PinSubCategoryObject = FoundClass;
                            }
                        }

                        TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
                        NewPin->PinName = NewParam.VarName;
                        NewPin->PinType = NewParam.VarType;
                        CustomEventNode->UserDefinedPins.Add(NewPin);
                    }
                }
            }
        }
    }

    CustomEventNode->ReconstructNode();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("event_name"), EventName);
    ResultObj->SetStringField(TEXT("node_id"), CustomEventNode->NodeGuid.ToString());
    ResultObj->SetBoolField(TEXT("success"), true);

    return ResultObj;
}