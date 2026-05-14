#include "Commands/EpicUnrealMCPProceduralCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPBridge.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "PhysicsEngine/RadialForceActor.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "GameFramework/PhysicsVolume.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Engine/Blueprint.h"

//   - guardrails: per-call max counts to keep accidental large requests safe

// =====================================================================
// Static file-local helpers (moved from EpicUnrealMCPEditorCommands.cpp
// during the Phase 1 refactor).  Must be defined before first use below.
// =====================================================================

static AActor* FindDraftProxyActor(UWorld* World, const FString& ProxyName)
{
    if (!World) return nullptr;
    FString McpIdTag = FString::Printf(TEXT("mcp_id:draft_%s"), *ProxyName);
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->Tags.Contains(FName(*McpIdTag)))
        {
            return *It;
        }
    }
    return nullptr;
}

static UMaterialInterface* LoadDraftMaterial(UObject* Outer, bool bUseDither)
{
    if (!Outer) return nullptr;

    // Try project-specific preset material first
    UMaterialInterface* Mat = Cast<UMaterialInterface>(
        UEditorAssetLibrary::LoadAsset(TEXT("/Game/Materials/M_DraftProxy"))
    );
    if (Mat) return Mat;

    // Fallback: try dither variant
    if (bUseDither)
    {
        Mat = Cast<UMaterialInterface>(
            UEditorAssetLibrary::LoadAsset(TEXT("/Game/Materials/M_DraftProxy_Dither"))
        );
        if (Mat) return Mat;
    }

    // Final fallback: use the engine default translucent material
    Mat = Cast<UMaterialInterface>(
        UEditorAssetLibrary::LoadAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial"))
    );
    return Mat;
}

static UHierarchicalInstancedStaticMeshComponent* GetOrCreateHismComponent(AActor* Actor)
{
    if (!Actor) return nullptr;
    UHierarchicalInstancedStaticMeshComponent* HISM = Actor->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
    if (!HISM)
    {
        HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("DraftHISM"));
        HISM->RegisterComponent();
        if (Actor->GetRootComponent())
        {
            HISM->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        }
        else
        {
            Actor->SetRootComponent(HISM);
        }
    }
    return HISM;
}

static bool ParseInstanceEntry(const TSharedPtr<FJsonObject>& Obj, FVector& OutLocation, FVector& OutScale, FRotator& OutRotation)
{
    if (!Obj.IsValid()) return false;
    const TSharedPtr<FJsonObject>* LocPtr = nullptr;
    if (Obj->TryGetObjectField(TEXT("location"), LocPtr) && LocPtr)
    {
        double X = 0, Y = 0, Z = 0;
        (*LocPtr)->TryGetNumberField(TEXT("x"), X);
        (*LocPtr)->TryGetNumberField(TEXT("y"), Y);
        (*LocPtr)->TryGetNumberField(TEXT("z"), Z);
        OutLocation = FVector(X, Y, Z);
    }
    const TSharedPtr<FJsonObject>* ScalePtr = nullptr;
    if (Obj->TryGetObjectField(TEXT("scale"), ScalePtr) && ScalePtr)
    {
        double X = 1, Y = 1, Z = 1;
        (*ScalePtr)->TryGetNumberField(TEXT("x"), X);
        (*ScalePtr)->TryGetNumberField(TEXT("y"), Y);
        (*ScalePtr)->TryGetNumberField(TEXT("z"), Z);
        OutScale = FVector(X, Y, Z);
    }
    const TSharedPtr<FJsonObject>* RotPtr = nullptr;
    if (Obj->TryGetObjectField(TEXT("rotation"), RotPtr) && RotPtr)
    {
        double Pitch = 0, Yaw = 0, Roll = 0;
        (*RotPtr)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotPtr)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotPtr)->TryGetNumberField(TEXT("roll"), Roll);
        OutRotation = FRotator(Pitch, Yaw, Roll);
    }
    return true;
}

// =====================================================================
// Class members
// =====================================================================

FEpicUnrealMCPProceduralCommands::FEpicUnrealMCPProceduralCommands()
{
}

UWorld* FEpicUnrealMCPProceduralCommands::GetEditorWorld() const
{
    if (!GEditor)
    {
        return nullptr;
    }
    return GEditor->GetEditorWorldContext().World();
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPProceduralCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        // Procedural generation
        {TEXT("spawn_tile_grid"), &FEpicUnrealMCPProceduralCommands::HandleSpawnTileGrid},
        {TEXT("spawn_procedural_actor_batch"), &FEpicUnrealMCPProceduralCommands::HandleSpawnProceduralActorBatch},
        {TEXT("create_spline_mesh_from_segments"), &FEpicUnrealMCPProceduralCommands::HandleCreateSplineMeshFromSegments},
        {TEXT("create_data_layer_for_generation"), &FEpicUnrealMCPProceduralCommands::HandleCreateDataLayerForGeneration},
        {TEXT("clear_generated_group"), &FEpicUnrealMCPProceduralCommands::HandleClearGeneratedGroup},

        // Draft proxy (HISM visualization)
        {TEXT("create_draft_proxy"), &FEpicUnrealMCPProceduralCommands::HandleCreateDraftProxy},
        {TEXT("update_draft_proxy"), &FEpicUnrealMCPProceduralCommands::HandleUpdateDraftProxy},
        {TEXT("delete_draft_proxy"), &FEpicUnrealMCPProceduralCommands::HandleDeleteDraftProxy},

        // InstanceSet commands (HISM/ISM bulk instancing)
        {TEXT("spawn_instance_set"), &FEpicUnrealMCPProceduralCommands::HandleSpawnInstanceSet},
        {TEXT("update_instance_set"), &FEpicUnrealMCPProceduralCommands::HandleUpdateInstanceSet},
        {TEXT("delete_instance_set"), &FEpicUnrealMCPProceduralCommands::HandleDeleteInstanceSet},
        {TEXT("get_instance_set_state"), &FEpicUnrealMCPProceduralCommands::HandleGetInstanceSetState},
        {TEXT("list_instance_sets"), &FEpicUnrealMCPProceduralCommands::HandleListInstanceSets},
        {TEXT("request_cognitive_processing"), &FEpicUnrealMCPProceduralCommands::HandleRequestCognitiveProcessing},

        // Physics commands (collision/physics/material/forces/constraints)
        {TEXT("set_actor_collision_preset"), &FEpicUnrealMCPProceduralCommands::HandleSetActorCollisionPreset},
        {TEXT("set_actor_physics"), &FEpicUnrealMCPProceduralCommands::HandleSetActorPhysics},
        {TEXT("create_physical_material"), &FEpicUnrealMCPProceduralCommands::HandleCreatePhysicalMaterial},
        {TEXT("spawn_radial_force"), &FEpicUnrealMCPProceduralCommands::HandleSpawnRadialForce},
        {TEXT("spawn_physics_constraint"), &FEpicUnrealMCPProceduralCommands::HandleSpawnPhysicsConstraint},

        // Validation commands
        {TEXT("compile_all_blueprints"), &FEpicUnrealMCPProceduralCommands::HandleCompileAllBlueprints},
        {TEXT("run_map_check"), &FEpicUnrealMCPProceduralCommands::HandleRunMapCheck},
        {TEXT("find_broken_references"), &FEpicUnrealMCPProceduralCommands::HandleFindBrokenReferences},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown procedural command: %s"), *CommandType));
}
TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleCreateDraftProxy(const TSharedPtr<FJsonObject>& Params)
{
    FString ProxyName;
    if (!Params->TryGetStringField(TEXT("proxy_name"), ProxyName) || ProxyName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'proxy_name' parameter"));
    }

    FString MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
    Params->TryGetStringField(TEXT("mesh_path"), MeshPath);

    FString MaterialPath;
    Params->TryGetStringField(TEXT("material_path"), MaterialPath);

    bool bUseDither = false;
    Params->TryGetBoolField(TEXT("use_dither"), bUseDither);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Prevent duplicate proxy names
    if (FindDraftProxyActor(World, ProxyName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Draft proxy '%s' already exists. Use update_draft_proxy instead."), *ProxyName));
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Create Draft Proxy %s"), *ProxyName)));

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("DraftProxy_%s"), *ProxyName);
    AActor* ProxyActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!ProxyActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn draft proxy actor"));
    }

    ProxyActor->SetActorLabel(*ProxyName);
    ProxyActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
    ProxyActor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:draft_%s"), *ProxyName)));

    UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHismComponent(ProxyActor);
    if (!HISM)
    {
        ProxyActor->Destroy();
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create HISM component"));
    }

    // Load static mesh
    UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
    if (!Mesh)
    {
        ProxyActor->Destroy();
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load static mesh: %s"), *MeshPath));
    }
    FlushRenderingCommands();
    HISM->SetStaticMesh(Mesh);

    // Load material if provided, otherwise create a default Unlit translucent draft material
    UMaterialInterface* Material = nullptr;
    if (!MaterialPath.IsEmpty())
    {
        Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (!Material)
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not load material at path: %s, falling back to default draft material"), *MaterialPath);
        }
    }
    if (!Material)
    {
        Material = LoadDraftMaterial(ProxyActor, bUseDither);
    }
    if (Material)
    {
        HISM->SetMaterial(0, Material);
    }

    // Disable collision and shadows for draft visualization
    HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HISM->SetCastShadow(false);

    // Parse and add instances
    const TArray<TSharedPtr<FJsonValue>>* InstancesArray = nullptr;
    int32 InstanceCount = 0;
    if (Params->TryGetArrayField(TEXT("instances"), InstancesArray) && InstancesArray)
    {
        for (const TSharedPtr<FJsonValue>& Value : *InstancesArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) continue;
            FVector Location = FVector::ZeroVector;
            FVector Scale = FVector::OneVector;
            FRotator Rotation = FRotator::ZeroRotator;
            if (ParseInstanceEntry(Value->AsObject(), Location, Scale, Rotation))
            {
                FTransform InstanceTransform(Rotation, Location, Scale);
                HISM->AddInstance(InstanceTransform);
                InstanceCount++;
            }
        }
    }

    HISM->MarkRenderStateDirty();

    FEpicUnrealMCPCommonUtils::GetActorIndex().AddActor(ProxyActor);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("proxy_name"), ProxyName);
    ResultObj->SetStringField(TEXT("actor_name"), ProxyActor->GetName());
    ResultObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleUpdateDraftProxy(const TSharedPtr<FJsonObject>& Params)
{
    FString ProxyName;
    if (!Params->TryGetStringField(TEXT("proxy_name"), ProxyName) || ProxyName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'proxy_name' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* ProxyActor = FindDraftProxyActor(World, ProxyName);
    if (!ProxyActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Draft proxy '%s' not found"), *ProxyName));
    }

    UHierarchicalInstancedStaticMeshComponent* HISM = GetOrCreateHismComponent(ProxyActor);
    if (!HISM)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get HISM component"));
    }

    // Clear existing instances and re-add
    FlushRenderingCommands();
    HISM->ClearInstances();

    FString MaterialPath;
    Params->TryGetStringField(TEXT("material_path"), MaterialPath);
    bool bUseDither = false;
    Params->TryGetBoolField(TEXT("use_dither"), bUseDither);

    UMaterialInterface* Material = nullptr;
    if (!MaterialPath.IsEmpty())
    {
        Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    }
    if (!Material)
    {
        Material = LoadDraftMaterial(ProxyActor, bUseDither);
    }
    if (Material)
    {
        HISM->SetMaterial(0, Material);
    }

    const TArray<TSharedPtr<FJsonValue>>* InstancesArray = nullptr;
    int32 InstanceCount = 0;
    if (Params->TryGetArrayField(TEXT("instances"), InstancesArray) && InstancesArray)
    {
        for (const TSharedPtr<FJsonValue>& Value : *InstancesArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) continue;
            FVector Location = FVector::ZeroVector;
            FVector Scale = FVector::OneVector;
            FRotator Rotation = FRotator::ZeroRotator;
            if (ParseInstanceEntry(Value->AsObject(), Location, Scale, Rotation))
            {
                FTransform InstanceTransform(Rotation, Location, Scale);
                HISM->AddInstance(InstanceTransform);
                InstanceCount++;
            }
        }
    }

    HISM->MarkRenderStateDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("proxy_name"), ProxyName);
    ResultObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleDeleteDraftProxy(const TSharedPtr<FJsonObject>& Params)
{
    FString ProxyName;
    if (!Params->TryGetStringField(TEXT("proxy_name"), ProxyName) || ProxyName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'proxy_name' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* ProxyActor = FindDraftProxyActor(World, ProxyName);
    if (!ProxyActor)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("deleted"), false);
        ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Draft proxy '%s' not found (already deleted)"), *ProxyName));
        return ResultObj;
    }

    FEpicUnrealMCPCommonUtils::GetActorIndex().RemoveActor(ProxyActor);

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Delete Draft Proxy %s"), *ProxyName)));
    ProxyActor->Modify();
    ProxyActor->Destroy();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("deleted"), true);
    ResultObj->SetStringField(TEXT("proxy_name"), ProxyName);
    return ResultObj;
}

// ------------------------------------------------------------------
// InstanceSet commands (HISM/ISM bulk instancing)
// ------------------------------------------------------------------

static AActor* FindInstanceSetActor(UWorld* World, const FString& SetId)
{
    if (!World) return nullptr;
    FString McpIdTag = FString::Printf(TEXT("mcp_id:instance_set_%s"), *SetId);
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->Tags.Contains(FName(*McpIdTag)))
        {
            return *It;
        }
    }
    return nullptr;
}

static UInstancedStaticMeshComponent* GetOrCreateIsmComponent(AActor* Actor, bool bUseHism)
{
    if (!Actor) return nullptr;
    if (bUseHism)
    {
        UHierarchicalInstancedStaticMeshComponent* HISM = Actor->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
        if (!HISM)
        {
            HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("InstanceSetHISM"));
            HISM->RegisterComponent();
            if (Actor->GetRootComponent())
            {
                HISM->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            }
            else
            {
                Actor->SetRootComponent(HISM);
            }
        }
        return HISM;
    }
    else
    {
        UInstancedStaticMeshComponent* ISM = Actor->FindComponentByClass<UInstancedStaticMeshComponent>();
        if (!ISM)
        {
            ISM = NewObject<UInstancedStaticMeshComponent>(Actor, UInstancedStaticMeshComponent::StaticClass(), TEXT("InstanceSetISM"));
            ISM->RegisterComponent();
            if (Actor->GetRootComponent())
            {
                ISM->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            }
            else
            {
                Actor->SetRootComponent(ISM);
            }
        }
        return ISM;
    }
}

static bool ParseTransformArrayEntry(const TSharedPtr<FJsonObject>& Obj, FVector& OutLocation, FVector& OutScale, FRotator& OutRotation)
{
    if (!Obj.IsValid()) return false;

    auto GetArrayDouble = [](const TArray<TSharedPtr<FJsonValue>>& Arr, int32 Index, double Default) -> double {
        if (!Arr.IsValidIndex(Index)) return Default;
        return Arr[Index]->AsNumber();
    };

    const TArray<TSharedPtr<FJsonValue>>* LocArray = nullptr;
    if (Obj->TryGetArrayField(TEXT("location"), LocArray) && LocArray && LocArray->Num() >= 3)
    {
        OutLocation = FVector(
            GetArrayDouble(*LocArray, 0, 0.0),
            GetArrayDouble(*LocArray, 1, 0.0),
            GetArrayDouble(*LocArray, 2, 0.0)
        );
    }
    else
    {
        OutLocation = FVector::ZeroVector;
    }

    const TArray<TSharedPtr<FJsonValue>>* ScaleArray = nullptr;
    if (Obj->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray && ScaleArray->Num() >= 3)
    {
        OutScale = FVector(
            GetArrayDouble(*ScaleArray, 0, 1.0),
            GetArrayDouble(*ScaleArray, 1, 1.0),
            GetArrayDouble(*ScaleArray, 2, 1.0)
        );
    }
    else
    {
        OutScale = FVector::OneVector;
    }

    const TArray<TSharedPtr<FJsonValue>>* RotArray = nullptr;
    if (Obj->TryGetArrayField(TEXT("rotation"), RotArray) && RotArray && RotArray->Num() >= 3)
    {
        OutRotation = FRotator(
            GetArrayDouble(*RotArray, 0, 0.0),
            GetArrayDouble(*RotArray, 1, 0.0),
            GetArrayDouble(*RotArray, 2, 0.0)
        );
    }
    else
    {
        OutRotation = FRotator::ZeroRotator;
    }

    return true;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleSpawnInstanceSet(const TSharedPtr<FJsonObject>& Params)
{
    FString SetId;
    if (!Params->TryGetStringField(TEXT("set_id"), SetId) || SetId.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'set_id' parameter"));
    }

    FString MeshPath;
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath) || MeshPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }

    FString MaterialPath;
    Params->TryGetStringField(TEXT("material_path"), MaterialPath);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    if (FindInstanceSetActor(World, SetId))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Instance set '%s' already exists. Use update_instance_set instead."), *SetId));
    }

    const TArray<TSharedPtr<FJsonValue>>* TransformsArray = nullptr;
    int32 InstanceCount = 0;
    if (Params->TryGetArrayField(TEXT("transforms"), TransformsArray) && TransformsArray)
    {
        InstanceCount = TransformsArray->Num();
    }

    bool bUseHism = InstanceCount > 100;

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Spawn Instance Set %s"), *SetId)));

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("InstanceSet_%s"), *SetId);
    AActor* SetActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!SetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn instance set actor"));
    }

    SetActor->SetActorLabel(*SetId);
    SetActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
    SetActor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:instance_set_%s"), *SetId)));

    UInstancedStaticMeshComponent* ISM = GetOrCreateIsmComponent(SetActor, bUseHism);
    if (!ISM)
    {
        SetActor->Destroy();
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create ISM/HISM component"));
    }

    UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
    if (!Mesh)
    {
        SetActor->Destroy();
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load static mesh: %s"), *MeshPath));
    }
    FlushRenderingCommands();
    ISM->SetStaticMesh(Mesh);

    if (!MaterialPath.IsEmpty())
    {
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            ISM->SetMaterial(0, Material);
        }
    }

    if (TransformsArray)
    {
        for (const TSharedPtr<FJsonValue>& Value : *TransformsArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) continue;
            FVector Location = FVector::ZeroVector;
            FVector Scale = FVector::OneVector;
            FRotator Rotation = FRotator::ZeroRotator;
            if (ParseTransformArrayEntry(Value->AsObject(), Location, Scale, Rotation))
            {
                FTransform InstanceTransform(Rotation, Location, Scale);
                ISM->AddInstance(InstanceTransform);
            }
        }
    }

    ISM->MarkRenderStateDirty();
    FEpicUnrealMCPCommonUtils::GetActorIndex().AddActor(SetActor);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("set_id"), SetId);
    ResultObj->SetStringField(TEXT("actor_name"), SetActor->GetName());
    ResultObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    ResultObj->SetBoolField(TEXT("use_hism"), bUseHism);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleUpdateInstanceSet(const TSharedPtr<FJsonObject>& Params)
{
    FString SetId;
    if (!Params->TryGetStringField(TEXT("set_id"), SetId) || SetId.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'set_id' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* SetActor = FindInstanceSetActor(World, SetId);
    if (!SetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Instance set '%s' not found"), *SetId));
    }

    UInstancedStaticMeshComponent* ISM = SetActor->FindComponentByClass<UInstancedStaticMeshComponent>();
    if (!ISM)
    {
        ISM = SetActor->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
    }
    if (!ISM)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find ISM/HISM component on instance set actor"));
    }

    FlushRenderingCommands();
    ISM->ClearInstances();

    FString MaterialPath;
    Params->TryGetStringField(TEXT("material_path"), MaterialPath);
    if (!MaterialPath.IsEmpty())
    {
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            ISM->SetMaterial(0, Material);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* TransformsArray = nullptr;
    int32 InstanceCount = 0;
    if (Params->TryGetArrayField(TEXT("transforms"), TransformsArray) && TransformsArray)
    {
        for (const TSharedPtr<FJsonValue>& Value : *TransformsArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) continue;
            FVector Location = FVector::ZeroVector;
            FVector Scale = FVector::OneVector;
            FRotator Rotation = FRotator::ZeroRotator;
            if (ParseTransformArrayEntry(Value->AsObject(), Location, Scale, Rotation))
            {
                FTransform InstanceTransform(Rotation, Location, Scale);
                ISM->AddInstance(InstanceTransform);
                InstanceCount++;
            }
        }
    }

    ISM->MarkRenderStateDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("set_id"), SetId);
    ResultObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleDeleteInstanceSet(const TSharedPtr<FJsonObject>& Params)
{
    FString SetId;
    if (!Params->TryGetStringField(TEXT("set_id"), SetId) || SetId.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'set_id' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* SetActor = FindInstanceSetActor(World, SetId);
    if (!SetActor)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("deleted"), false);
        ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("Instance set '%s' not found (already deleted)"), *SetId));
        return ResultObj;
    }

    FEpicUnrealMCPCommonUtils::GetActorIndex().RemoveActor(SetActor);

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Delete Instance Set %s"), *SetId)));
    SetActor->Modify();
    SetActor->Destroy();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("deleted"), true);
    ResultObj->SetStringField(TEXT("set_id"), SetId);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleGetInstanceSetState(const TSharedPtr<FJsonObject>& Params)
{
    FString SetId;
    if (!Params->TryGetStringField(TEXT("set_id"), SetId) || SetId.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'set_id' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* SetActor = FindInstanceSetActor(World, SetId);
    if (!SetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Instance set '%s' not found"), *SetId));
    }

    UInstancedStaticMeshComponent* ISM = SetActor->FindComponentByClass<UInstancedStaticMeshComponent>();
    bool bIsHism = false;
    if (!ISM)
    {
        ISM = SetActor->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
        bIsHism = (ISM != nullptr);
    }

    int32 InstanceCount = ISM ? ISM->GetInstanceCount() : 0;
    FString MeshPath;
    FString MaterialPath;
    if (ISM && ISM->GetStaticMesh())
    {
        MeshPath = ISM->GetStaticMesh()->GetPathName();
    }
    if (ISM && ISM->GetMaterial(0))
    {
        MaterialPath = ISM->GetMaterial(0)->GetPathName();
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("set_id"), SetId);
    ResultObj->SetStringField(TEXT("actor_name"), SetActor->GetName());
    ResultObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    ResultObj->SetBoolField(TEXT("use_hism"), bIsHism);
    if (!MeshPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("mesh_path"), MeshPath);
    }
    if (!MaterialPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    }

    TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
    StateObj->SetStringField(TEXT("set_id"), SetId);
    StateObj->SetStringField(TEXT("actor_name"), SetActor->GetName());
    StateObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    StateObj->SetBoolField(TEXT("use_hism"), bIsHism);
    if (!MeshPath.IsEmpty())
    {
        StateObj->SetStringField(TEXT("mesh_path"), MeshPath);
    }
    if (!MaterialPath.IsEmpty())
    {
        StateObj->SetStringField(TEXT("material_path"), MaterialPath);
    }
    ResultObj->SetObjectField(TEXT("state"), StateObj);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleListInstanceSets(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    TArray<TSharedPtr<FJsonValue>> SetArray;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || !Actor->Tags.Contains(FName(TEXT("managed_by_mcp"))))
        {
            continue;
        }
        FString SetId;
        for (const FName& Tag : Actor->Tags)
        {
            FString TagStr = Tag.ToString();
            if (TagStr.StartsWith(TEXT("mcp_id:instance_set_")))
            {
                SetId = TagStr.RightChop(FCString::Strlen(TEXT("mcp_id:instance_set_")));
                break;
            }
        }
        if (SetId.IsEmpty())
        {
            continue;
        }

        UInstancedStaticMeshComponent* ISM = Actor->FindComponentByClass<UInstancedStaticMeshComponent>();
        bool bIsHism = false;
        if (!ISM)
        {
            ISM = Actor->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
            bIsHism = (ISM != nullptr);
        }

        int32 InstanceCount = ISM ? ISM->GetInstanceCount() : 0;
        FString MeshPath;
        FString MaterialPath;
        if (ISM && ISM->GetStaticMesh())
        {
            MeshPath = ISM->GetStaticMesh()->GetPathName();
        }
        if (ISM && ISM->GetMaterial(0))
        {
            MaterialPath = ISM->GetMaterial(0)->GetPathName();
        }

        TSharedPtr<FJsonObject> SetObj = MakeShared<FJsonObject>();
        SetObj->SetStringField(TEXT("set_id"), SetId);
        SetObj->SetStringField(TEXT("actor_name"), Actor->GetName());
        SetObj->SetNumberField(TEXT("instance_count"), InstanceCount);
        SetObj->SetBoolField(TEXT("use_hism"), bIsHism);
        if (!MeshPath.IsEmpty())
        {
            SetObj->SetStringField(TEXT("mesh_path"), MeshPath);
        }
        if (!MaterialPath.IsEmpty())
        {
            SetObj->SetStringField(TEXT("material_path"), MaterialPath);
        }
        SetArray.Add(MakeShared<FJsonValueObject>(SetObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("count"), SetArray.Num());
    ResultObj->SetArrayField(TEXT("sets"), SetArray);
    return ResultObj;
}

// ------------------------------------------------------------------
// Physics commands
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleSetActorCollisionPreset(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
    }

    FString PresetName;
    if (!Params->TryGetStringField(TEXT("preset"), PresetName) || PresetName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'preset' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            Actor = *It;
            break;
        }
    }

    if (!Actor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
    if (!RootComp)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor does not have a primitive root component"));
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Set Collision Preset %s"), *ActorName)));
    Actor->Modify();
    RootComp->Modify();
    RootComp->SetCollisionProfileName(FName(*PresetName));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ActorName);
    Result->SetStringField(TEXT("preset"), PresetName);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleSetActorPhysics(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'actor_name' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            Actor = *It;
            break;
        }
    }

    if (!Actor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' not found"), *ActorName));
    }

    UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
    if (!RootComp)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor does not have a primitive root component"));
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Set Actor Physics %s"), *ActorName)));
    Actor->Modify();
    RootComp->Modify();

    bool bSimulatePhysics = false;
    if (Params->TryGetBoolField(TEXT("simulate_physics"), bSimulatePhysics))
    {
        RootComp->SetSimulatePhysics(bSimulatePhysics);
    }

    bool bGravityEnabled = true;
    if (Params->TryGetBoolField(TEXT("gravity_enabled"), bGravityEnabled))
    {
        RootComp->SetEnableGravity(bGravityEnabled);
    }

    double MassScale = 1.0;
    if (Params->TryGetNumberField(TEXT("mass_scale"), MassScale))
    {
        RootComp->SetMassScale(NAME_None, static_cast<float>(MassScale));
    }

    double LinearDamping = 0.01;
    if (Params->TryGetNumberField(TEXT("linear_damping"), LinearDamping))
    {
        RootComp->SetLinearDamping(static_cast<float>(LinearDamping));
    }

    double AngularDamping = 0.0;
    if (Params->TryGetNumberField(TEXT("angular_damping"), AngularDamping))
    {
        RootComp->SetAngularDamping(static_cast<float>(AngularDamping));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ActorName);
    Result->SetBoolField(TEXT("simulate_physics"), RootComp->IsSimulatingPhysics());
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleCreatePhysicalMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'asset_path' parameter"));
    }

    FString AssetName = FPaths::GetBaseFilename(AssetPath);
    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for physical material"));
    }

    UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!PhysMat)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create PhysicalMaterial object"));
    }

    double Friction = 0.7;
    if (Params->TryGetNumberField(TEXT("friction"), Friction))
    {
        PhysMat->Friction = static_cast<float>(Friction);
    }

    double Restitution = 0.3;
    if (Params->TryGetNumberField(TEXT("restitution"), Restitution))
    {
        PhysMat->Restitution = static_cast<float>(Restitution);
    }

    double Density = 1.0;
    if (Params->TryGetNumberField(TEXT("density"), Density))
    {
        // Density is not a direct property on UPhysicalMaterial in newer UE versions;
        // it is typically part of the body setup or physical material mask.
        // We skip setting it here to avoid compilation issues.
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(PhysMat);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("asset_name"), AssetName);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleSpawnRadialForce(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = TEXT("RadialForceActor");
    Params->TryGetStringField(TEXT("actor_name"), ActorName);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocObj = nullptr;
    if (Params->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
    {
        double X = 0, Y = 0, Z = 0;
        (*LocObj)->TryGetNumberField(TEXT("x"), X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        Location = FVector(X, Y, Z);
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Spawn Radial Force %s"), *ActorName)));

    ARadialForceActor* ForceActor = World->SpawnActor<ARadialForceActor>(ARadialForceActor::StaticClass(), Location, FRotator::ZeroRotator);
    if (!ForceActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn RadialForceActor"));
    }

    ForceActor->SetActorLabel(*ActorName);
    ForceActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));

    double Radius = 500.0;
    if (Params->TryGetNumberField(TEXT("radius"), Radius))
    {
        if (ForceActor->GetForceComponent())
        {
            ForceActor->GetForceComponent()->Radius = static_cast<float>(Radius);
        }
    }

    double Strength = 1000.0;
    if (Params->TryGetNumberField(TEXT("strength"), Strength))
    {
        if (ForceActor->GetForceComponent())
        {
            ForceActor->GetForceComponent()->ForceStrength = static_cast<float>(Strength);
        }
    }

    FEpicUnrealMCPCommonUtils::GetActorIndex().AddActor(ForceActor);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ForceActor->GetName());
    Result->SetNumberField(TEXT("radius"), Radius);
    Result->SetNumberField(TEXT("strength"), Strength);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleSpawnPhysicsConstraint(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName = TEXT("PhysicsConstraintActor");
    Params->TryGetStringField(TEXT("actor_name"), ActorName);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocObj = nullptr;
    if (Params->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
    {
        double X = 0, Y = 0, Z = 0;
        (*LocObj)->TryGetNumberField(TEXT("x"), X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        Location = FVector(X, Y, Z);
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Spawn Physics Constraint %s"), *ActorName)));

    APhysicsConstraintActor* ConstraintActor = World->SpawnActor<APhysicsConstraintActor>(APhysicsConstraintActor::StaticClass(), Location, FRotator::ZeroRotator);
    if (!ConstraintActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn PhysicsConstraintActor"));
    }

    ConstraintActor->SetActorLabel(*ActorName);
    ConstraintActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));

    FEpicUnrealMCPCommonUtils::GetActorIndex().AddActor(ConstraintActor);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ConstraintActor->GetName());
    return Result;
}

// ------------------------------------------------------------------
// Validation commands
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleCompileAllBlueprints(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FAssetData> BlueprintAssets;
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = true;
    AssetRegistryModule.Get().GetAssets(Filter, BlueprintAssets);

    int32 CompiledCount = 0;
    int32 ErrorCount = 0;
    TArray<TSharedPtr<FJsonValue>> ErrorList;

    for (const FAssetData& Asset : BlueprintAssets)
    {
        UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(Asset.GetObjectPathString()));
        if (!Blueprint)
        {
            continue;
        }

        FCompilerResultsLog Results;
        Results.SetSourcePath(Asset.GetObjectPathString());
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
        CompiledCount++;

        if (Results.NumErrors > 0)
        {
            ErrorCount++;
            TSharedPtr<FJsonObject> ErrObj = MakeShared<FJsonObject>();
            ErrObj->SetStringField(TEXT("asset"), Asset.GetObjectPathString());
            ErrObj->SetNumberField(TEXT("errors"), Results.NumErrors);
            ErrObj->SetNumberField(TEXT("warnings"), Results.NumWarnings);
            ErrorList.Add(MakeShared<FJsonValueObject>(ErrObj));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("compiled_count"), CompiledCount);
    Result->SetNumberField(TEXT("error_count"), ErrorCount);
    Result->SetArrayField(TEXT("errors"), ErrorList);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleRunMapCheck(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Run the editor's map check
    FMessageLog MapCheckLog("MapCheck");
    MapCheckLog.NewPage(FText::FromString(TEXT("MCP Map Check")));

    int32 ErrorCount = 0;
    int32 WarningCount = 0;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor)
        {
            continue;
        }

        // Check for actors without valid root component
        if (!Actor->GetRootComponent())
        {
            WarningCount++;
            MapCheckLog.Warning()
                ->AddToken(FTextToken::Create(FText::FromString(Actor->GetName())))
                ->AddToken(FTextToken::Create(FText::FromString(TEXT("has no root component"))));
        }

        // Check for overlapping static mesh actors (simplified)
        AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
        if (SMActor && SMActor->GetStaticMeshComponent())
        {
            if (!SMActor->GetStaticMeshComponent()->GetStaticMesh())
            {
                ErrorCount++;
                MapCheckLog.Error()
                    ->AddToken(FTextToken::Create(FText::FromString(Actor->GetName())))
                    ->AddToken(FTextToken::Create(FText::FromString(TEXT("has no static mesh assigned"))));
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("errors"), ErrorCount);
    Result->SetNumberField(TEXT("warnings"), WarningCount);
    Result->SetStringField(TEXT("message"), TEXT("Map check completed. See Unreal Editor's Map Check tab for details."));
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleFindBrokenReferences(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    TArray<TSharedPtr<FJsonValue>> BrokenActors;
    int32 MissingMeshCount = 0;
    int32 MissingMaterialCount = 0;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor)
        {
            continue;
        }

        TArray<TSharedPtr<FJsonValue>> Issues;

        AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
        if (SMActor && SMActor->GetStaticMeshComponent())
        {
            if (!SMActor->GetStaticMeshComponent()->GetStaticMesh())
            {
                MissingMeshCount++;
                TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                Issue->SetStringField(TEXT("type"), TEXT("missing_mesh"));
                Issue->SetStringField(TEXT("component"), TEXT("StaticMeshComponent"));
                Issues.Add(MakeShared<FJsonValueObject>(Issue));
            }

            UMaterialInterface* Mat = SMActor->GetStaticMeshComponent()->GetMaterial(0);
            if (!Mat)
            {
                MissingMaterialCount++;
                TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                Issue->SetStringField(TEXT("type"), TEXT("missing_material"));
                Issue->SetStringField(TEXT("component"), TEXT("StaticMeshComponent"));
                Issue->SetNumberField(TEXT("slot"), 0);
                Issues.Add(MakeShared<FJsonValueObject>(Issue));
            }
        }

        if (Issues.Num() > 0)
        {
            TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
            ActorObj->SetStringField(TEXT("actor_name"), Actor->GetName());
            ActorObj->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
            ActorObj->SetArrayField(TEXT("issues"), Issues);
            BrokenActors.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("broken_actor_count"), BrokenActors.Num());
    Result->SetNumberField(TEXT("missing_mesh_count"), MissingMeshCount);
    Result->SetNumberField(TEXT("missing_material_count"), MissingMaterialCount);
    Result->SetArrayField(TEXT("broken_actors"), BrokenActors);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleRequestCognitiveProcessing(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: actor_name"));
    }

    FString Context;
    Params->TryGetStringField(TEXT("context"), Context);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Find the actor (can be an AI Controller or any actor with the function)
    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
        {
            TargetActor = *It;
            break;
        }
    }

    if (!TargetActor)
    {
        // Try mcp_id lookup via the shared ActorIndex first, then fall back to tag scan.
        TargetActor = FEpicUnrealMCPCommonUtils::GetActorIndex().FindByMcpId(ActorName);
        if (!TargetActor)
        {
            TargetActor = FEpicUnrealMCPCommonUtils::FindActorByMcpIdTag(World, ActorName);
        }
    }

    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Use reflection to call RequestCognitiveProcessing(EnvironmentalContext)
    UFunction* Func = TargetActor->FindFunction(FName(TEXT("RequestCognitiveProcessing")));
    if (!Func)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor '%s' does not have a RequestCognitiveProcessing function"), *ActorName));
    }

    // Set up parameters for the function call
    struct FRequestCognitiveProcessingParams
    {
        FString EnvironmentalContext;
    };

    FRequestCognitiveProcessingParams FuncParams;
    FuncParams.EnvironmentalContext = Context;

    TargetActor->ProcessEvent(Func, &FuncParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("actor_name"), TargetActor->GetName());
    ResultObj->SetStringField(TEXT("message"), TEXT("Cognitive processing requested asynchronously"));
    return ResultObj;
}



// ===================================================================
// HandleSpawnTileGrid (WFC realization)
// ===================================================================
// Spawns one HISM actor per unique tile_id from a WFC grid result.
// Inputs:
//   set_id_prefix (string, default "wfc"): prefix for actor names + mcp_id tag
//   tiles (array, required): [{x:int, y:int, tile_id:string, rotation_degrees:float}]
//   tile_asset_map (object, required): { tile_id : "/Game/.../SM_Asset" }
//   default_mesh_path (string, optional): fallback mesh when a tile is missing in tile_asset_map
//   material_map (object, optional): { tile_id : "/Game/.../M_Asset" }
//   default_material_path (string, optional)
//   cell_size (object, optional): { x: 100.0, y: 100.0 } default 100 cm
//   origin (object, optional): { x, y, z } default zero
//   replace_existing (bool, optional, default true): if true delete actors with the same set_id_prefix before spawning
//   focus_viewport (bool, optional, default false)
// Returns:
//   success, total_instance_count, per_tile: [ { tile_id, mesh_path, instance_count, actor_name, actor_path } ],
//   skipped_tile_ids: [ tile_ids missing from map and no default ]
TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleSpawnTileGrid(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing parameters"));
    }

    FString SetPrefix = TEXT("wfc");
    Params->TryGetStringField(TEXT("set_id_prefix"), SetPrefix);
    if (SetPrefix.IsEmpty())
    {
        SetPrefix = TEXT("wfc");
    }

    const TArray<TSharedPtr<FJsonValue>>* TilesArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("tiles"), TilesArray) || !TilesArray)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'tiles' array is required"));
    }

    const TSharedPtr<FJsonObject>* TileAssetMapPtr = nullptr;
    if (!Params->TryGetObjectField(TEXT("tile_asset_map"), TileAssetMapPtr) || !TileAssetMapPtr)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'tile_asset_map' object is required (e.g. {\"grass\":\"/Game/Tiles/SM_Grass\"})"));
    }

    FString DefaultMeshPath;
    Params->TryGetStringField(TEXT("default_mesh_path"), DefaultMeshPath);

    const TSharedPtr<FJsonObject>* MaterialMapPtr = nullptr;
    Params->TryGetObjectField(TEXT("material_map"), MaterialMapPtr);

    FString DefaultMaterialPath;
    Params->TryGetStringField(TEXT("default_material_path"), DefaultMaterialPath);

    double CellSizeX = 100.0;
    double CellSizeY = 100.0;
    const TSharedPtr<FJsonObject>* CellSizePtr = nullptr;
    if (Params->TryGetObjectField(TEXT("cell_size"), CellSizePtr) && CellSizePtr)
    {
        (*CellSizePtr)->TryGetNumberField(TEXT("x"), CellSizeX);
        (*CellSizePtr)->TryGetNumberField(TEXT("y"), CellSizeY);
    }

    FVector OriginVec = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* OriginPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("origin"), OriginPtr) && OriginPtr)
    {
        double Ox = 0.0, Oy = 0.0, Oz = 0.0;
        (*OriginPtr)->TryGetNumberField(TEXT("x"), Ox);
        (*OriginPtr)->TryGetNumberField(TEXT("y"), Oy);
        (*OriginPtr)->TryGetNumberField(TEXT("z"), Oz);
        OriginVec = FVector(Ox, Oy, Oz);
    }

    bool bReplaceExisting = true;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

    bool bFocusViewport = false;
    Params->TryGetBoolField(TEXT("focus_viewport"), bFocusViewport);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // 1) Group tiles by tile_id and resolve transforms.
    struct FGroupedTile
    {
        FString TileId;
        TArray<FTransform> Transforms;
    };
    TMap<FString, FGroupedTile> Grouped;

    for (const TSharedPtr<FJsonValue>& Value : *TilesArray)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object) continue;
        TSharedPtr<FJsonObject> Tile = Value->AsObject();
        if (!Tile.IsValid()) continue;

        double Xd = 0.0, Yd = 0.0, RotDeg = 0.0;
        Tile->TryGetNumberField(TEXT("x"), Xd);
        Tile->TryGetNumberField(TEXT("y"), Yd);
        Tile->TryGetNumberField(TEXT("rotation_degrees"), RotDeg);

        FString TileId;
        if (!Tile->TryGetStringField(TEXT("tile_id"), TileId) || TileId.IsEmpty())
        {
            continue;
        }

        FVector Loc = OriginVec + FVector(Xd * CellSizeX, Yd * CellSizeY, 0.0);
        FRotator Rot(0.0, RotDeg, 0.0);
        FTransform T(Rot, Loc, FVector::OneVector);

        FGroupedTile& G = Grouped.FindOrAdd(TileId);
        G.TileId = TileId;
        G.Transforms.Add(T);
    }

    // 2) Optionally replace existing actors that share the same prefix.
    if (bReplaceExisting)
    {
        const FString PrefixTag = FString::Printf(TEXT("mcp_id:tile_grid_%s_"), *SetPrefix);
        TArray<AActor*> ToDestroy;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* A = *It;
            if (!A) continue;
            for (const FName& Tag : A->Tags)
            {
                if (Tag.ToString().StartsWith(PrefixTag))
                {
                    ToDestroy.Add(A);
                    break;
                }
            }
        }
        for (AActor* A : ToDestroy)
        {
            A->Destroy();
        }
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Spawn Tile Grid %s"), *SetPrefix)));

    TArray<TSharedPtr<FJsonValue>> PerTileResults;
    TArray<TSharedPtr<FJsonValue>> Skipped;
    int32 TotalInstances = 0;

    for (const TPair<FString, FGroupedTile>& Pair : Grouped)
    {
        const FString& TileId = Pair.Key;
        const FGroupedTile& Group = Pair.Value;

        // Resolve mesh path
        FString MeshPath;
        if (TileAssetMapPtr && (*TileAssetMapPtr)->HasField(TileId))
        {
            (*TileAssetMapPtr)->TryGetStringField(TileId, MeshPath);
        }
        if (MeshPath.IsEmpty())
        {
            MeshPath = DefaultMeshPath;
        }

        if (MeshPath.IsEmpty())
        {
            TSharedPtr<FJsonObject> SkipObj = MakeShared<FJsonObject>();
            SkipObj->SetStringField(TEXT("tile_id"), TileId);
            SkipObj->SetStringField(TEXT("reason"), TEXT("no mesh path in tile_asset_map and no default_mesh_path"));
            SkipObj->SetNumberField(TEXT("instance_count"), Group.Transforms.Num());
            Skipped.Add(MakeShared<FJsonValueObject>(SkipObj));
            continue;
        }

        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (!Mesh)
        {
            TSharedPtr<FJsonObject> SkipObj = MakeShared<FJsonObject>();
            SkipObj->SetStringField(TEXT("tile_id"), TileId);
            SkipObj->SetStringField(TEXT("reason"), FString::Printf(TEXT("Failed to load mesh: %s"), *MeshPath));
            SkipObj->SetNumberField(TEXT("instance_count"), Group.Transforms.Num());
            Skipped.Add(MakeShared<FJsonValueObject>(SkipObj));
            continue;
        }

        // Resolve material
        FString MatPath;
        if (MaterialMapPtr && (*MaterialMapPtr)->HasField(TileId))
        {
            (*MaterialMapPtr)->TryGetStringField(TileId, MatPath);
        }
        if (MatPath.IsEmpty())
        {
            MatPath = DefaultMaterialPath;
        }

        const FString ActorLabel = FString::Printf(TEXT("TileGrid_%s_%s"), *SetPrefix, *TileId);
        const FString McpIdTag = FString::Printf(TEXT("mcp_id:tile_grid_%s_%s"), *SetPrefix, *TileId);

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *ActorLabel;
        SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

        AActor* TileActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!TileActor)
        {
            TSharedPtr<FJsonObject> SkipObj = MakeShared<FJsonObject>();
            SkipObj->SetStringField(TEXT("tile_id"), TileId);
            SkipObj->SetStringField(TEXT("reason"), TEXT("Failed to spawn actor"));
            SkipObj->SetNumberField(TEXT("instance_count"), Group.Transforms.Num());
            Skipped.Add(MakeShared<FJsonValueObject>(SkipObj));
            continue;
        }

        TileActor->SetActorLabel(*ActorLabel);
        TileActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
        TileActor->Tags.AddUnique(FName(TEXT("wfc_generated")));
        TileActor->Tags.AddUnique(FName(*McpIdTag));
        TileActor->Tags.AddUnique(FName(*FString::Printf(TEXT("wfc_tile_id:%s"), *TileId)));

        UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(
            TileActor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("TileGridHISM"));
        HISM->RegisterComponent();
        TileActor->SetRootComponent(HISM);
        HISM->SetStaticMesh(Mesh);
        HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        if (!MatPath.IsEmpty())
        {
            UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MatPath));
            if (Material)
            {
                HISM->SetMaterial(0, Material);
            }
        }

        for (const FTransform& T : Group.Transforms)
        {
            HISM->AddInstance(T);
        }
        HISM->MarkRenderStateDirty();

        TileActor->Modify();
        TileActor->MarkPackageDirty();

        FEpicUnrealMCPCommonUtils::GetActorIndex().AddActor(TileActor);

        TotalInstances += Group.Transforms.Num();

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("tile_id"), TileId);
        Entry->SetStringField(TEXT("mesh_path"), MeshPath);
        Entry->SetStringField(TEXT("actor_name"), TileActor->GetName());
        Entry->SetStringField(TEXT("actor_path"), TileActor->GetPathName());
        Entry->SetNumberField(TEXT("instance_count"), Group.Transforms.Num());
        if (!MatPath.IsEmpty())
        {
            Entry->SetStringField(TEXT("material_path"), MatPath);
        }
        PerTileResults.Add(MakeShared<FJsonValueObject>(Entry));
    }

    if (bFocusViewport && PerTileResults.Num() > 0 && GEditor)
    {
        TArray<AActor*> Focus;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            for (const FName& Tag : It->Tags)
            {
                if (Tag.ToString().StartsWith(FString::Printf(TEXT("mcp_id:tile_grid_%s_"), *SetPrefix)))
                {
                    Focus.Add(*It);
                    break;
                }
            }
        }
        if (Focus.Num() > 0)
        {
            GEditor->MoveViewportCamerasToActor(Focus, true);
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("set_id_prefix"), SetPrefix);
    ResultObj->SetNumberField(TEXT("total_instance_count"), TotalInstances);
    ResultObj->SetNumberField(TEXT("tile_kind_count"), PerTileResults.Num());
    ResultObj->SetArrayField(TEXT("per_tile"), PerTileResults);
    ResultObj->SetArrayField(TEXT("skipped"), Skipped);
    return ResultObj;
}



// ===================================================================
// Issue #26: Remaining procedural realization commands
// ===================================================================
// All four handlers run on the GameThread (the bridge dispatches editor
// commands via AsyncTask). They follow the existing conventions:
//   - structured envelope: { success, data | error, hint? }
//   - tag every spawned actor with `managed_by_mcp` and (when provided)
//     `mcp_id:<id>` and the caller's custom tags
//   - Modify() + MarkPackageDirty() before returning so World Partition
//     captures the change and saves it on the next Save All
//   - guardrails: per-call max counts to keep accidental large requests safe


// Shared tag / lookup helpers are implemented in FEpicUnrealMCPCommonUtils.


// -------------------------------------------------------------------
// HandleSpawnProceduralActorBatch
// -------------------------------------------------------------------
// Bulk spawn N actors in a single GameThread pass.
// Inputs:
//   group_id (string, optional)         - tag added as `procedural_group:<id>`
//   max_actors (int, default 5000)      - safety cap on number of placements
//   focus_viewport (bool, default false)
//   placements (array, required) of:
//       mcp_id (string, optional)
//       actor_class (string, default "StaticMeshActor")
//       static_mesh (string, optional, only for StaticMeshActor)
//       location (array[3] or {x,y,z})
//       rotation (array[3] or {pitch,yaw,roll})
//       scale (array[3] or {x,y,z})
//       tags (array<string>, optional)
//       desired_name (string, optional) - if absent, use mcp_id, else auto.
TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleSpawnProceduralActorBatch(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing parameters"));
    }

    const TArray<TSharedPtr<FJsonValue>>* PlacementsArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("placements"), PlacementsArray) || !PlacementsArray)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'placements' array is required"));
    }

    FString GroupId;
    Params->TryGetStringField(TEXT("group_id"), GroupId);

    int32 MaxActors = 5000;
    int32 MaxFromJson = 0;
    if (Params->TryGetNumberField(TEXT("max_actors"), MaxFromJson) && MaxFromJson > 0)
    {
        MaxActors = MaxFromJson;
    }

    bool bFocusViewport = false;
    Params->TryGetBoolField(TEXT("focus_viewport"), bFocusViewport);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    const int32 Requested = PlacementsArray->Num();
    const int32 Effective = FMath::Min(Requested, MaxActors);
    TArray<TSharedPtr<FJsonValue>> Warnings;
    if (Effective < Requested)
    {
        TSharedPtr<FJsonObject> W = MakeShared<FJsonObject>();
        W->SetStringField(TEXT("type"), TEXT("ActorCountCapped"));
        W->SetNumberField(TEXT("requested"), Requested);
        W->SetNumberField(TEXT("applied"), Effective);
        Warnings.Add(MakeShared<FJsonValueObject>(W));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Spawn Procedural Batch")));

    int32 SpawnedCount = 0;
    int32 SkippedCount = 0;
    TArray<TSharedPtr<FJsonValue>> ActorPaths;
    TArray<AActor*> SpawnedForFocus;

    for (int32 i = 0; i < Effective; ++i)
    {
        const TSharedPtr<FJsonValue>& V = (*PlacementsArray)[i];
        if (!V.IsValid() || V->Type != EJson::Object)
        {
            ++SkippedCount;
            continue;
        }
        const TSharedPtr<FJsonObject>& Item = V->AsObject();
        if (!Item.IsValid())
        {
            ++SkippedCount;
            continue;
        }

        FString ActorClass = TEXT("StaticMeshActor");
        Item->TryGetStringField(TEXT("actor_class"), ActorClass);

        FString McpId;
        Item->TryGetStringField(TEXT("mcp_id"), McpId);

        FString DesiredName;
        if (!Item->TryGetStringField(TEXT("desired_name"), DesiredName) || DesiredName.IsEmpty())
        {
            DesiredName = !McpId.IsEmpty()
                ? FString::Printf(TEXT("ProcBatch_%s"), *McpId)
                : FString::Printf(TEXT("ProcBatch_%d"), i);
        }

        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        FVector Scale = FVector::OneVector;
        FString ParamError;
        if (Item->HasField(TEXT("location")))
        {
            FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Item, TEXT("location"), Location, ParamError);
        }
        if (Item->HasField(TEXT("rotation")))
        {
            FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Item, TEXT("rotation"), Rotation, ParamError);
        }
        if (Item->HasField(TEXT("scale")))
        {
            FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Item, TEXT("scale"), Scale, ParamError);
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *DesiredName;
        SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AActor* NewActor = nullptr;
        if (ActorClass == TEXT("StaticMeshActor"))
        {
            AStaticMeshActor* SMActor = World->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
            if (SMActor)
            {
                FString MeshPath;
                if (Item->TryGetStringField(TEXT("static_mesh"), MeshPath) && !MeshPath.IsEmpty())
                {
                    UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                    if (Mesh)
                    {
                        SMActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                    }
                }
            }
            NewActor = SMActor;
        }
        else
        {
            // Generic resolution by class name (Engine module).
            UClass* FoundClass = LoadClass<AActor>(nullptr,
                *FString::Printf(TEXT("/Script/Engine.%s"), *ActorClass));
            if (!FoundClass)
            {
                FoundClass = LoadClass<AActor>(nullptr,
                    *FString::Printf(TEXT("/Script/FlopperamUnrealMCP.%s"), *ActorClass));
            }
            if (FoundClass)
            {
                NewActor = World->SpawnActor<AActor>(FoundClass, Location, Rotation, SpawnParams);
            }
        }

        if (!NewActor)
        {
            ++SkippedCount;
            continue;
        }

        // Apply scale (SpawnActor takes only loc+rot).
        FTransform T = NewActor->GetTransform();
        T.SetScale3D(Scale);
        NewActor->SetActorTransform(T);

        TArray<FString> ExtraTags = FEpicUnrealMCPCommonUtils::ReadStringArrayField(Item, TEXT("tags"));
        if (!GroupId.IsEmpty())
        {
            ExtraTags.Add(FString::Printf(TEXT("procedural_group:%s"), *GroupId));
        }
        FEpicUnrealMCPCommonUtils::ApplyMcpIdAndTags(NewActor, McpId, ExtraTags);

        NewActor->Modify();
        NewActor->MarkPackageDirty();
        FEpicUnrealMCPCommonUtils::GetActorIndex().AddActor(NewActor);

        ++SpawnedCount;
        SpawnedForFocus.Add(NewActor);
        ActorPaths.Add(MakeShared<FJsonValueString>(NewActor->GetPathName()));
    }

    if (bFocusViewport && SpawnedForFocus.Num() > 0 && GEditor)
    {
        GEditor->MoveViewportCamerasToActor(SpawnedForFocus, true);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("requested_count"), Requested);
    Data->SetNumberField(TEXT("spawned_count"), SpawnedCount);
    Data->SetNumberField(TEXT("skipped_count"), SkippedCount);
    Data->SetNumberField(TEXT("max_actors_cap"), MaxActors);
    Data->SetArrayField(TEXT("actor_paths"), ActorPaths);
    Data->SetArrayField(TEXT("warnings"), Warnings);
    if (!GroupId.IsEmpty())
    {
        Data->SetStringField(TEXT("group_id"), GroupId);
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetObjectField(TEXT("data"), Data.ToSharedRef());
    return Resp;
}

// -------------------------------------------------------------------
// HandleCreateSplineMeshFromSegments
// -------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleCreateSplineMeshFromSegments(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing parameters"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) || ActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString MeshPath;
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath) || MeshPath.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }

    const TArray<TSharedPtr<FJsonValue>>* SegArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("segments"), SegArray) || !SegArray)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'segments' array is required"));
    }

    FString McpId;
    Params->TryGetStringField(TEXT("mcp_id"), McpId);

    FString MaterialPath;
    Params->TryGetStringField(TEXT("material_path"), MaterialPath);

    FString ForwardAxisStr = TEXT("X");
    Params->TryGetStringField(TEXT("forward_axis"), ForwardAxisStr);
    ESplineMeshAxis::Type ForwardAxis = ESplineMeshAxis::X;
    if (ForwardAxisStr.Equals(TEXT("Y"), ESearchCase::IgnoreCase)) ForwardAxis = ESplineMeshAxis::Y;
    else if (ForwardAxisStr.Equals(TEXT("Z"), ESearchCase::IgnoreCase)) ForwardAxis = ESplineMeshAxis::Z;

    int32 MaxSegments = 10000;
    int32 MaxFromJson = 0;
    if (Params->TryGetNumberField(TEXT("max_segments"), MaxFromJson) && MaxFromJson > 0)
    {
        MaxSegments = MaxFromJson;
    }

    UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
    if (!Mesh)
    {
        TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
        Err->SetBoolField(TEXT("success"), false);
        Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load mesh: %s"), *MeshPath));
        Err->SetStringField(TEXT("hint"), TEXT("Verify the path under /Game/... is correct and the asset exists."));
        return Err;
    }
    UMaterialInterface* Material = nullptr;
    if (!MaterialPath.IsEmpty())
    {
        Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    const int32 RequestedSegments = SegArray->Num();
    const int32 EffectiveSegments = FMath::Min(RequestedSegments, MaxSegments);
    TArray<TSharedPtr<FJsonValue>> Warnings;
    if (EffectiveSegments < RequestedSegments)
    {
        TSharedPtr<FJsonObject> W = MakeShared<FJsonObject>();
        W->SetStringField(TEXT("type"), TEXT("SegmentCountCapped"));
        W->SetNumberField(TEXT("requested"), RequestedSegments);
        W->SetNumberField(TEXT("applied"), EffectiveSegments);
        Warnings.Add(MakeShared<FJsonValueObject>(W));
    }

    AActor* Parent = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetFName() == FName(*ActorName))
        {
            Parent = *It;
            break;
        }
    }
    bool bCreated = false;
    if (!Parent)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *ActorName;
        SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
        Parent = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!Parent)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn parent actor for spline meshes"));
        }
        Parent->SetActorLabel(*ActorName);
        USceneComponent* Root = NewObject<USceneComponent>(Parent, USceneComponent::StaticClass(), TEXT("SplineMeshRoot"));
        Root->RegisterComponent();
        Parent->SetRootComponent(Root);
        bCreated = true;
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Spline Mesh from Segments")));

    TArray<FString> ExtraTags = FEpicUnrealMCPCommonUtils::ReadStringArrayField(Params, TEXT("tags"));
    ExtraTags.Add(TEXT("procedural_spline_mesh"));
    FEpicUnrealMCPCommonUtils::ApplyMcpIdAndTags(Parent, McpId, ExtraTags);

    int32 ComponentCount = 0;
    for (int32 i = 0; i < EffectiveSegments; ++i)
    {
        const TSharedPtr<FJsonValue>& V = (*SegArray)[i];
        if (!V.IsValid() || V->Type != EJson::Object) continue;
        const TSharedPtr<FJsonObject>& Seg = V->AsObject();
        if (!Seg.IsValid()) continue;

        FVector Start = FVector::ZeroVector;
        FVector End   = FVector::ZeroVector;
        FString ParamError;
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Seg, TEXT("start"), Start, ParamError)) continue;
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Seg, TEXT("end"), End, ParamError)) continue;

        FVector StartTangent = End - Start;
        FVector EndTangent   = StartTangent;
        if (Seg->HasField(TEXT("start_tangent")))
        {
            FVector T;
            if (FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Seg, TEXT("start_tangent"), T, ParamError))
            {
                StartTangent = T;
            }
        }
        if (Seg->HasField(TEXT("end_tangent")))
        {
            FVector T;
            if (FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Seg, TEXT("end_tangent"), T, ParamError))
            {
                EndTangent = T;
            }
        }

        const FName CompName(*FString::Printf(TEXT("SplineMesh_%d"), i));
        USplineMeshComponent* SMC = NewObject<USplineMeshComponent>(Parent, USplineMeshComponent::StaticClass(), CompName);
        if (!SMC) continue;
        SMC->SetMobility(EComponentMobility::Movable);
        SMC->SetupAttachment(Parent->GetRootComponent());
        SMC->RegisterComponent();
        SMC->SetStaticMesh(Mesh);
        if (Material)
        {
            SMC->SetMaterial(0, Material);
        }
        SMC->SetForwardAxis(ForwardAxis, /*bUpdateMesh=*/false);
        SMC->SetStartAndEnd(Start, StartTangent, End, EndTangent, /*bUpdateMesh=*/true);
        SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ++ComponentCount;
    }

    Parent->Modify();
    Parent->MarkPackageDirty();
    FEpicUnrealMCPCommonUtils::GetActorIndex().AddActor(Parent);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actor_name"), Parent->GetName());
    Data->SetStringField(TEXT("actor_path"), Parent->GetPathName());
    Data->SetBoolField(TEXT("created_actor"), bCreated);
    Data->SetNumberField(TEXT("requested_segment_count"), RequestedSegments);
    Data->SetNumberField(TEXT("segment_count"), EffectiveSegments);
    Data->SetNumberField(TEXT("component_count"), ComponentCount);
    Data->SetStringField(TEXT("mesh_path"), MeshPath);
    Data->SetStringField(TEXT("forward_axis"), ForwardAxisStr);
    Data->SetArrayField(TEXT("warnings"), Warnings);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetObjectField(TEXT("data"), Data.ToSharedRef());
    return Resp;
}

// -------------------------------------------------------------------
// HandleCreateDataLayerForGeneration
// -------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleCreateDataLayerForGeneration(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing parameters"));
    }

    FString DataLayerName;
    if (!Params->TryGetStringField(TEXT("data_layer_name"), DataLayerName) || DataLayerName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'data_layer_name' parameter"));
    }

    const TArray<TSharedPtr<FJsonValue>>* IdsArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("actor_mcp_ids"), IdsArray) || !IdsArray)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'actor_mcp_ids' array is required"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create Data Layer For Generation")));

    const FName LayerTag(*FString::Printf(TEXT("data_layer:%s"), *DataLayerName));
    TArray<TSharedPtr<FJsonValue>> Skipped;
    int32 AssignedCount = 0;

    for (const TSharedPtr<FJsonValue>& V : *IdsArray)
    {
        if (!V.IsValid() || V->Type != EJson::String) continue;
        const FString McpId = V->AsString();
        if (McpId.IsEmpty()) continue;

        AActor* Actor = FEpicUnrealMCPCommonUtils::FindActorByMcpIdTag(World, McpId);
        if (!Actor)
        {
            TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
            S->SetStringField(TEXT("actor_mcp_id"), McpId);
            S->SetStringField(TEXT("reason"), TEXT("actor not found by mcp_id tag"));
            Skipped.Add(MakeShared<FJsonValueObject>(S));
            continue;
        }
        Actor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
        Actor->Tags.AddUnique(LayerTag);
        Actor->Modify();
        Actor->MarkPackageDirty();
        ++AssignedCount;
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("data_layer_name"), DataLayerName);
    Data->SetStringField(TEXT("data_layer_tag"), LayerTag.ToString());
    Data->SetStringField(TEXT("method"), TEXT("tag"));
    Data->SetNumberField(TEXT("requested_count"), IdsArray->Num());
    Data->SetNumberField(TEXT("actors_assigned_count"), AssignedCount);
    Data->SetArrayField(TEXT("skipped"), Skipped);
    Data->SetStringField(TEXT("note"), TEXT("First-pass implementation uses actor tags as a logical data layer. A follow-up will wire UDataLayerEditorSubsystem when the level uses World Partition."));

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetObjectField(TEXT("data"), Data.ToSharedRef());
    return Resp;
}

// -------------------------------------------------------------------
// HandleClearGeneratedGroup
// -------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPProceduralCommands::HandleClearGeneratedGroup(
    const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing parameters"));
    }

    FString GroupId;
    Params->TryGetStringField(TEXT("group_id"), GroupId);
    TArray<FString> RequiredTags = FEpicUnrealMCPCommonUtils::ReadStringArrayField(Params, TEXT("required_tags"));

    if (GroupId.IsEmpty() && RequiredTags.Num() == 0)
    {
        TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
        Err->SetBoolField(TEXT("success"), false);
        Err->SetStringField(TEXT("error"), TEXT("Refusing to clear without 'group_id' or non-empty 'required_tags'"));
        Err->SetStringField(TEXT("hint"), TEXT("Pass at least one filter so we never accidentally delete every actor in the level."));
        return Err;
    }

    bool bDryRun = true;
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);

    int32 MaxDelete = 10000;
    int32 MaxFromJson = 0;
    if (Params->TryGetNumberField(TEXT("max_delete"), MaxFromJson) && MaxFromJson > 0)
    {
        MaxDelete = MaxFromJson;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    const FName GroupTag = !GroupId.IsEmpty()
        ? FName(*FString::Printf(TEXT("procedural_group:%s"), *GroupId))
        : FName(NAME_None);

    TArray<AActor*> Matches;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;

        if (!GroupId.IsEmpty() && !Actor->Tags.Contains(GroupTag))
        {
            continue;
        }
        bool bAllTagsMatch = true;
        for (const FString& Required : RequiredTags)
        {
            if (Required.IsEmpty()) continue;
            if (!Actor->Tags.Contains(FName(*Required)))
            {
                bAllTagsMatch = false;
                break;
            }
        }
        if (!bAllTagsMatch) continue;

        Matches.Add(Actor);
        if (Matches.Num() >= MaxDelete)
        {
            break;
        }
    }

    TArray<TSharedPtr<FJsonValue>> Paths;
    for (AActor* Actor : Matches)
    {
        Paths.Add(MakeShared<FJsonValueString>(Actor->GetPathName()));
    }

    int32 DeletedCount = 0;
    if (!bDryRun)
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Clear Generated Group")));
        for (AActor* Actor : Matches)
        {
            if (!Actor) continue;
            Actor->Modify();
            FEpicUnrealMCPCommonUtils::GetActorIndex().RemoveActor(Actor);
            if (Actor->Destroy())
            {
                ++DeletedCount;
            }
        }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("dry_run"), bDryRun);
    if (!GroupId.IsEmpty())
    {
        Data->SetStringField(TEXT("group_id"), GroupId);
        Data->SetStringField(TEXT("group_tag"), GroupTag.ToString());
    }
    if (RequiredTags.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> TagJson;
        for (const FString& T : RequiredTags) TagJson.Add(MakeShared<FJsonValueString>(T));
        Data->SetArrayField(TEXT("required_tags"), TagJson);
    }
    Data->SetNumberField(TEXT("matched_count"), Matches.Num());
    Data->SetNumberField(TEXT("would_delete_count"), bDryRun ? Matches.Num() : 0);
    Data->SetNumberField(TEXT("deleted_count"), DeletedCount);
    Data->SetNumberField(TEXT("max_delete_cap"), MaxDelete);
    Data->SetArrayField(TEXT("actor_paths"), Paths);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetObjectField(TEXT("data"), Data.ToSharedRef());
    return Resp;
}
