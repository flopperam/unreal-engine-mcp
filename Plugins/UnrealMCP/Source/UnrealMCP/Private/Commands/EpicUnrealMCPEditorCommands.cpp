#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPBridge.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "NavModifierVolume.h"
#include "AINavigation/NavLinkProxy.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/RectLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/SkyLight.h"
#include "Camera/CameraActor.h"
#include "Engine/PostProcessVolume.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Components/SplineComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "PhysicsEngine/RadialForceActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "GameFramework/PhysicsVolume.h"

FEpicUnrealMCPEditorCommands::FEpicUnrealMCPEditorCommands()
{
}

static FActorIndex& GetActorIndex()
{
    UEpicUnrealMCPBridge* Bridge = GEditor->GetEditorSubsystem<UEpicUnrealMCPBridge>();
    check(Bridge);
    return Bridge->ActorIndex;
}

UWorld* FEpicUnrealMCPEditorCommands::GetEditorWorld() const
{
    if (!GEditor)
    {
        return nullptr;
    }
    return GEditor->GetEditorWorldContext().World();
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPEditorCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("get_actors_in_level"), &FEpicUnrealMCPEditorCommands::HandleGetActorsInLevel},
        {TEXT("find_actors_by_name"), &FEpicUnrealMCPEditorCommands::HandleFindActorsByName},
        {TEXT("spawn_actor"), &FEpicUnrealMCPEditorCommands::HandleSpawnActor},
        {TEXT("delete_actor"), &FEpicUnrealMCPEditorCommands::HandleDeleteActor},
        {TEXT("set_actor_transform"), &FEpicUnrealMCPEditorCommands::HandleSetActorTransform},
        {TEXT("find_actor_by_mcp_id"), &FEpicUnrealMCPEditorCommands::HandleFindActorByMcpId},
        {TEXT("set_actor_transform_by_mcp_id"), &FEpicUnrealMCPEditorCommands::HandleSetActorTransformByMcpId},
        {TEXT("delete_actor_by_mcp_id"), &FEpicUnrealMCPEditorCommands::HandleDeleteActorByMcpId},
        {TEXT("apply_scene_delta"), &FEpicUnrealMCPEditorCommands::HandleApplySceneDelta},
        {TEXT("clone_actor"), &FEpicUnrealMCPEditorCommands::HandleCloneActor},
        {TEXT("create_nav_mesh_volume"), &FEpicUnrealMCPEditorCommands::HandleCreateNavMeshVolume},
        {TEXT("create_patrol_route"), &FEpicUnrealMCPEditorCommands::HandleCreatePatrolRoute},
        {TEXT("create_spline_from_points"), &FEpicUnrealMCPEditorCommands::HandleCreateSplineFromPoints},
        {TEXT("set_ai_behavior"), &FEpicUnrealMCPEditorCommands::HandleSetAIBehavior},
        {TEXT("create_behavior_tree"), &FEpicUnrealMCPEditorCommands::HandleCreateBehaviorTree},
        {TEXT("create_blackboard"), &FEpicUnrealMCPEditorCommands::HandleCreateBlackboard},
        {TEXT("create_nav_modifier_volume"), &FEpicUnrealMCPEditorCommands::HandleCreateNavModifierVolume},
        {TEXT("create_nav_link_proxy"), &FEpicUnrealMCPEditorCommands::HandleCreateNavLinkProxy},
        {TEXT("set_actor_collision_preset"), &FEpicUnrealMCPEditorCommands::HandleSetActorCollisionPreset},
        {TEXT("set_actor_physics"), &FEpicUnrealMCPEditorCommands::HandleSetActorPhysics},
        {TEXT("create_physical_material"), &FEpicUnrealMCPEditorCommands::HandleCreatePhysicalMaterial},
        {TEXT("spawn_radial_force"), &FEpicUnrealMCPEditorCommands::HandleSpawnRadialForce},
        {TEXT("spawn_physics_constraint"), &FEpicUnrealMCPEditorCommands::HandleSpawnPhysicsConstraint},
        {TEXT("compile_all_blueprints"), &FEpicUnrealMCPEditorCommands::HandleCompileAllBlueprints},
        {TEXT("run_map_check"), &FEpicUnrealMCPEditorCommands::HandleRunMapCheck},
        {TEXT("find_broken_references"), &FEpicUnrealMCPEditorCommands::HandleFindBrokenReferences},
        {TEXT("create_draft_proxy"), &FEpicUnrealMCPEditorCommands::HandleCreateDraftProxy},
        {TEXT("update_draft_proxy"), &FEpicUnrealMCPEditorCommands::HandleUpdateDraftProxy},
        {TEXT("delete_draft_proxy"), &FEpicUnrealMCPEditorCommands::HandleDeleteDraftProxy},
        {TEXT("spawn_instance_set"), &FEpicUnrealMCPEditorCommands::HandleSpawnInstanceSet},
        {TEXT("update_instance_set"), &FEpicUnrealMCPEditorCommands::HandleUpdateInstanceSet},
        {TEXT("delete_instance_set"), &FEpicUnrealMCPEditorCommands::HandleDeleteInstanceSet},
        {TEXT("get_instance_set_state"), &FEpicUnrealMCPEditorCommands::HandleGetInstanceSetState},
        {TEXT("list_instance_sets"), &FEpicUnrealMCPEditorCommands::HandleListInstanceSets},
        {TEXT("request_cognitive_processing"), &FEpicUnrealMCPEditorCommands::HandleRequestCognitiveProcessing},
        {TEXT("spawn_tile_grid"), &FEpicUnrealMCPEditorCommands::HandleSpawnTileGrid},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->Tags.Contains(FName(TEXT("managed_by_mcp"))))
        {
            // Light-weight summary to keep response small (avoids TCP
            // send-buffer overflow with large actor counts).
            ActorArray.Add(MakeShared<FJsonValueObject>(
                FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor, false)));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && (Actor->GetName().Contains(Pattern, ESearchCase::IgnoreCase) || Actor->GetActorLabel().Contains(Pattern, ESearchCase::IgnoreCase)))
        {
            MatchingActors.Add(FEpicUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);
    FString ParamError;

    if (Params->HasField(TEXT("location")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
        }
    }
    if (Params->HasField(TEXT("rotation")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), Rotation, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError));
        }
    }
    if (Params->HasField(TEXT("scale")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("scale"), Scale, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'scale': %s"), *ParamError));
        }
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists (O(1) via index)
    if (GetActorIndex().FindByName(FName(*ActorName)))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
    }

    // Also check the actual world for actors with this FName.
    // The index only tracks MCP-spawned actors within this session;
    // saved actors from previous runs won't be in the index.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetFName() == FName(*ActorName))
        {
            // Backfill the index so subsequent lookups are O(1)
            GetActorIndex().AddActor(*It);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Actor with name '%s' already exists in the level"), *ActorName));
        }
    }

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Spawn Actor")));

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewMeshActor)
        {
            // Check for an optional static_mesh parameter to assign a mesh
            FString MeshPath;
            if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
                if (Mesh)
                {
                    FlushRenderingCommands();
                    NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Could not find static mesh at path: %s"), *MeshPath);
                }
            }
        }
        NewActor = NewMeshActor;
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SkyLight"))
    {
        NewActor = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("RectLight"))
    {
        NewActor = World->SpawnActor<ARectLight>(ARectLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SkyAtmosphere"))
    {
        NewActor = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("ExponentialHeightFog"))
    {
        NewActor = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PostProcessVolume"))
    {
        APostProcessVolume* NewPPVolume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), Location, Rotation, SpawnParams);
        if (NewPPVolume)
        {
            NewPPVolume->bUnbound = true;
        }
        NewActor = NewPPVolume;
    }
    else
    {
        // Try to resolve generic class by name
        UClass* FoundClass = nullptr;
        
        // Try common prefixes
        TArray<FString> Candidates;
        Candidates.Add(ActorType);
        if (!ActorType.StartsWith(TEXT("A"))) Candidates.Add(TEXT("A") + ActorType);
        
        for (const FString& ClassName : Candidates)
        {
            // Try FlopperamUnrealMCP (game module)
            FoundClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/FlopperamUnrealMCP.%s"), *ClassName));
            if (FoundClass) break;
            
            // Try Engine
            FoundClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
            if (FoundClass) break;
            
            // Try generic FindObject
            FoundClass = FindObject<UClass>(nullptr, *ClassName);
            if (FoundClass && FoundClass->IsChildOf(AActor::StaticClass())) break;
        }

        if (FoundClass)
        {
            NewActor = World->SpawnActor<AActor>(FoundClass, Location, Rotation, SpawnParams);
        }
        else
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
        }
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Apply mcp_id tag if provided
        FString McpId;
        Params->TryGetStringField(TEXT("mcp_id"), McpId);
        NewActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
        if (!McpId.IsEmpty())
        {
            NewActor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:%s"), *McpId)));
        }

        // Apply additional tags if provided
        if (Params->HasField(TEXT("tags")))
        {
            const TArray<TSharedPtr<FJsonValue>>* TagsJsonArray;
            if (Params->TryGetArrayField(TEXT("tags"), TagsJsonArray))
            {
                for (const TSharedPtr<FJsonValue>& TagValue : *TagsJsonArray)
                {
                    if (TagValue->Type == EJson::String)
                    {
                        FString TagStr = TagValue->AsString();
                        if (!TagStr.IsEmpty())
                        {
                            NewActor->Tags.AddUnique(FName(*TagStr));
                        }
                    }
                }
            }
        }

        // Add to index for O(1) lookup
        GetActorIndex().AddActor(NewActor);

        // Return the created actor's details
        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

// --- P4.3: Pre-parse and execute helpers for batch scene delta ---

FParsedCreateParams FEpicUnrealMCPEditorCommands::ParseCreateParams(const TSharedPtr<FJsonObject>& Params) const
{
    FParsedCreateParams Parsed;

    if (!Params->TryGetStringField(TEXT("name"), Parsed.Name) || Parsed.Name.IsEmpty())
    {
        Parsed.ErrorString = TEXT("Missing or empty 'name' parameter");
        return Parsed;
    }

    if (!Params->TryGetStringField(TEXT("type"), Parsed.Type) || Parsed.Type.IsEmpty())
    {
        Parsed.ErrorString = TEXT("Missing or empty 'type' parameter");
        return Parsed;
    }

    // Validate actor type (hardcoded list for common types, others resolved dynamically)
    static const TSet<FString> HardcodedTypes = {
        TEXT("StaticMeshActor"), TEXT("PointLight"), TEXT("SpotLight"),
        TEXT("DirectionalLight"), TEXT("SkyLight"), TEXT("RectLight"),
        TEXT("SkyAtmosphere"), TEXT("ExponentialHeightFog"), TEXT("CameraActor")
    };
    
    // If it's not a hardcoded type, we'll try to resolve it during execution.
    // We only fail here if it's empty.
    if (Parsed.Type.IsEmpty())
    {
        Parsed.ErrorString = TEXT("Missing or empty 'type' parameter");
        return Parsed;
    }

    Params->TryGetStringField(TEXT("mcp_id"), Parsed.McpId);
    Params->TryGetStringField(TEXT("static_mesh"), Parsed.StaticMeshPath);

    // Parse transform
    FString ParamError;
    if (Params->HasField(TEXT("location")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Parsed.Location, ParamError))
        {
            Parsed.ErrorString = FString::Printf(TEXT("Invalid 'location': %s"), *ParamError);
            return Parsed;
        }
    }
    if (Params->HasField(TEXT("rotation")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), Parsed.Rotation, ParamError))
        {
            Parsed.ErrorString = FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError);
            return Parsed;
        }
    }
    if (Params->HasField(TEXT("scale")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("scale"), Parsed.Scale, ParamError))
        {
            Parsed.ErrorString = FString::Printf(TEXT("Invalid 'scale': %s"), *ParamError);
            return Parsed;
        }
    }

    // Parse tags
    if (Params->HasField(TEXT("tags")))
    {
        const TArray<TSharedPtr<FJsonValue>>* TagsJsonArray;
        if (Params->TryGetArrayField(TEXT("tags"), TagsJsonArray))
        {
            for (const TSharedPtr<FJsonValue>& TagValue : *TagsJsonArray)
            {
                if (TagValue->Type == EJson::String)
                {
                    FString TagStr = TagValue->AsString();
                    if (!TagStr.IsEmpty())
                    {
                        Parsed.Tags.Add(TagStr);
                    }
                }
            }
        }
    }

    Parsed.bValid = true;
    return Parsed;
}

FParsedUpdateParams FEpicUnrealMCPEditorCommands::ParseUpdateParams(const TSharedPtr<FJsonObject>& Params) const
{
    FParsedUpdateParams Parsed;

    if (!Params->TryGetStringField(TEXT("mcp_id"), Parsed.McpId) || Parsed.McpId.IsEmpty())
    {
        Parsed.ErrorString = TEXT("Missing or empty 'mcp_id' parameter");
        return Parsed;
    }

    FString ParamError;
    if (Params->HasField(TEXT("location")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Parsed.Location, ParamError))
        {
            Parsed.ErrorString = FString::Printf(TEXT("Invalid 'location': %s"), *ParamError);
            return Parsed;
        }
        Parsed.bHasLocation = true;
    }
    if (Params->HasField(TEXT("rotation")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), Parsed.Rotation, ParamError))
        {
            Parsed.ErrorString = FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError);
            return Parsed;
        }
        Parsed.bHasRotation = true;
    }
    if (Params->HasField(TEXT("scale")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("scale"), Parsed.Scale, ParamError))
        {
            Parsed.ErrorString = FString::Printf(TEXT("Invalid 'scale': %s"), *ParamError);
            return Parsed;
        }
        Parsed.bHasScale = true;
    }

    Parsed.bValid = true;
    return Parsed;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::ExecuteCreateActor(const FParsedCreateParams& Parsed, bool bSuppressTransaction)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists (O(1) via index)
    if (GetActorIndex().FindByName(FName(*Parsed.Name)))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor with name '%s' already exists"), *Parsed.Name));
    }

    // Also check the actual world for actors with this FName
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetFName() == FName(*Parsed.Name))
        {
            GetActorIndex().AddActor(*It);
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Actor with name '%s' already exists in the level"), *Parsed.Name));
        }
    }

    // Wrap in a transaction unless the caller already provides one (batch mode).
    TUniquePtr<FScopedTransaction> Transaction;
    if (!bSuppressTransaction)
    {
        Transaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("UnrealMCP: Spawn Actor")));
    }

    // Use deferred spawning so scale is injected before component registration,
    // avoiding a redundant SetActorTransform + re-registration cycle.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *Parsed.Name;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = nullptr;
    FTransform SpawnTransform(Parsed.Rotation, Parsed.Location, Parsed.Scale);

    if (Parsed.Type == TEXT("StaticMeshActor"))
    {
        AStaticMeshActor* NewMeshActor = World->SpawnActorDeferred<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewMeshActor)
        {
            if (!Parsed.StaticMeshPath.IsEmpty())
            {
                UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(Parsed.StaticMeshPath));
                if (Mesh)
                {
                    FlushRenderingCommands();
                    NewMeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
                }
            }
            NewMeshActor->FinishSpawning(SpawnTransform);
        }
        NewActor = NewMeshActor;
    }
    else if (Parsed.Type == TEXT("PointLight"))
    {
        NewActor = World->SpawnActorDeferred<APointLight>(
            APointLight::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else if (Parsed.Type == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActorDeferred<ASpotLight>(
            ASpotLight::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else if (Parsed.Type == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActorDeferred<ADirectionalLight>(
            ADirectionalLight::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else if (Parsed.Type == TEXT("SkyLight"))
    {
        NewActor = World->SpawnActorDeferred<ASkyLight>(
            ASkyLight::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else if (Parsed.Type == TEXT("RectLight"))
    {
        NewActor = World->SpawnActorDeferred<ARectLight>(
            ARectLight::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else if (Parsed.Type == TEXT("SkyAtmosphere"))
    {
        NewActor = World->SpawnActorDeferred<ASkyAtmosphere>(
            ASkyAtmosphere::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else if (Parsed.Type == TEXT("ExponentialHeightFog"))
    {
        NewActor = World->SpawnActorDeferred<AExponentialHeightFog>(
            AExponentialHeightFog::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else if (Parsed.Type == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActorDeferred<ACameraActor>(
            ACameraActor::StaticClass(), SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
    }
    else
    {
        // Try to resolve generic class by name
        UClass* FoundClass = nullptr;
        
        TArray<FString> Candidates;
        Candidates.Add(Parsed.Type);
        if (!Parsed.Type.StartsWith(TEXT("A"))) Candidates.Add(TEXT("A") + Parsed.Type);
        
        for (const FString& ClassName : Candidates)
        {
            // Try FlopperamUnrealMCP
            FoundClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/FlopperamUnrealMCP.%s"), *ClassName));
            if (FoundClass) break;
            
            // Try Engine
            FoundClass = LoadClass<AActor>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
            if (FoundClass) break;
            
            // Try generic FindObject
            FoundClass = FindObject<UClass>(nullptr, *ClassName);
            if (FoundClass && FoundClass->IsChildOf(AActor::StaticClass())) break;
        }

        if (FoundClass)
        {
            NewActor = World->SpawnActorDeferred<AActor>(
                FoundClass, SpawnTransform, nullptr, nullptr,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (NewActor) { NewActor->FinishSpawning(SpawnTransform); }
        }
    }

    if (NewActor)
    {
        // Apply mcp_id tag
        NewActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
        if (!Parsed.McpId.IsEmpty())
        {
            NewActor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:%s"), *Parsed.McpId)));
        }

        // Apply additional tags
        for (const FString& Tag : Parsed.Tags)
        {
            NewActor->Tags.AddUnique(FName(*Tag));
        }

        // Add to index
        GetActorIndex().AddActor(NewActor);

        return FEpicUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::ExecuteUpdateActor(const FParsedUpdateParams& Parsed)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* Actor = FindActorByMcpId(World, Parsed.McpId);
    if (!Actor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor with mcp_id '%s' not found"), *Parsed.McpId));
    }

    FTransform Transform = Actor->GetTransform();
    if (Parsed.bHasLocation)
    {
        Transform.SetLocation(Parsed.Location);
    }
    if (Parsed.bHasRotation)
    {
        Transform.SetRotation(Parsed.Rotation.Quaternion());
    }
    if (Parsed.bHasScale)
    {
        Transform.SetScale3D(Parsed.Scale);
    }
    Actor->SetActorTransform(Transform);

    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* Actor = GetActorIndex().FindByName(FName(*ActorName));
    if (!Actor)
    {
        // Fallback: scan the world for actors not in the index (e.g. from previous sessions)
        UWorld* World = GetEditorWorld();
        if (World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetFName() == FName(*ActorName))
                {
                    Actor = *It;
                    break;
                }
            }
        }
    }
    if (!Actor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor);
    GetActorIndex().RemoveActor(Actor);

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Delete Actor")));
    Actor->Modify();
    Actor->Destroy();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AActor* TargetActor = GetActorIndex().FindByName(FName(*ActorName));
    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Actor Transform")));
    TargetActor->Modify();

    FString ParamError;

    if (Params->HasField(TEXT("location")))
    {
        FVector NewLocation;
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), NewLocation, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
        }
        NewTransform.SetLocation(NewLocation);
    }
    if (Params->HasField(TEXT("rotation")))
    {
        FRotator NewRotation;
        if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), NewRotation, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError));
        }
        NewTransform.SetRotation(FQuat(NewRotation));
    }
    if (Params->HasField(TEXT("scale")))
    {
        FVector NewScale;
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("scale"), NewScale, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'scale': %s"), *ParamError));
        }
        NewTransform.SetScale3D(NewScale);
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

AActor* FEpicUnrealMCPEditorCommands::FindActorByMcpId(UWorld* World, const FString& McpId) const
{
    if (!World || McpId.IsEmpty())
    {
        return nullptr;
    }

    AActor* FoundActor = GetActorIndex().FindByMcpId(McpId);
    if (FoundActor)
    {
        return FoundActor;
    }

    // Fallback: linear scan for actors not yet in the index
    const FName TargetTag(*FString::Printf(TEXT("mcp_id:%s"), *McpId));
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->Tags.Contains(TargetTag))
        {
            if (FoundActor)
            {
                UE_LOG(LogTemp, Warning, TEXT("FindActorByMcpId: Multiple actors found with mcp_id '%s'"), *McpId);
                return FoundActor; // Return first found
            }
            FoundActor = Actor;
        }
    }

    return FoundActor;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindActorByMcpId(const TSharedPtr<FJsonObject>& Params)
{
    FString McpId;
    if (!Params->TryGetStringField(TEXT("mcp_id"), McpId) || McpId.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'mcp_id' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Index lookup + fallback world scan for consistency with other handlers
    AActor* FoundActor = FindActorByMcpId(World, McpId);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    if (!FoundActor)
    {
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("message"), TEXT("No actor found with the given mcp_id"));
    }
    else
    {
        ResultObj->SetBoolField(TEXT("success"), true);
        TSharedPtr<FJsonValue> ActorJsonValue = FEpicUnrealMCPCommonUtils::ActorToJson(FoundActor);
        if (ActorJsonValue.IsValid() && ActorJsonValue->Type == EJson::Object)
        {
            ResultObj->SetObjectField(TEXT("actor"), ActorJsonValue->AsObject());
        }
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorTransformByMcpId(const TSharedPtr<FJsonObject>& Params)
{
    FString McpId;
    if (!Params->TryGetStringField(TEXT("mcp_id"), McpId) || McpId.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'mcp_id' parameter"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* TargetActor = FindActorByMcpId(World, McpId);
    if (!TargetActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with mcp_id '%s' not found"), *McpId));
    }

    FTransform NewTransform = TargetActor->GetTransform();

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Set Actor Transform By McpId")));
    TargetActor->Modify();

    FString ParamError;

    if (Params->HasField(TEXT("location")))
    {
        FVector NewLocation;
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), NewLocation, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
        }
        NewTransform.SetLocation(NewLocation);
    }
    if (Params->HasField(TEXT("rotation")))
    {
        FRotator NewRotation;
        if (!FEpicUnrealMCPCommonUtils::TryGetRotatorFromJson(Params, TEXT("rotation"), NewRotation, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'rotation': %s"), *ParamError));
        }
        NewTransform.SetRotation(FQuat(NewRotation));
    }
    if (Params->HasField(TEXT("scale")))
    {
        FVector NewScale;
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("scale"), NewScale, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'scale': %s"), *ParamError));
        }
        NewTransform.SetScale3D(NewScale);
    }

    TargetActor->SetActorTransform(NewTransform);

    return FEpicUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteActorByMcpId(const TSharedPtr<FJsonObject>& Params)
{
    FString McpId;
    if (!Params->TryGetStringField(TEXT("mcp_id"), McpId) || McpId.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'mcp_id' parameter"));
    }

    AActor* Actor = GetActorIndex().FindByMcpId(McpId);

    if (!Actor)
    {
        // Fallback linear scan for actors not in the index
        UWorld* World = GetEditorWorld();
        if (!World)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
        }
        const FName TargetTag(*FString::Printf(TEXT("mcp_id:%s"), *McpId));
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
        for (AActor* A : AllActors)
        {
            if (A && A->Tags.Contains(TargetTag))
            {
                Actor = A;
                break;
            }
        }
    }

    if (!Actor)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("message"), FString::Printf(TEXT("No actor with mcp_id '%s' found (already deleted)"), *McpId));
        ResultObj->SetBoolField(TEXT("deleted"), false);
        return ResultObj;
    }

    TSharedPtr<FJsonObject> ActorInfo = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Actor);
    GetActorIndex().RemoveActor(Actor);

    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Delete Actor By McpId")));
    Actor->Modify();
    Actor->Destroy();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
    ResultObj->SetBoolField(TEXT("deleted"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCloneActor(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceActorName;
    if (!Params->TryGetStringField(TEXT("source_actor_name"), SourceActorName) || SourceActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_actor_name' parameter"));
    }

    FString NewActorName;
    if (!Params->TryGetStringField(TEXT("new_actor_name"), NewActorName) || NewActorName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_actor_name' parameter"));
    }

    FString McpId;
    Params->TryGetStringField(TEXT("mcp_id"), McpId);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Find source actor
    AActor* SourceActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName() == SourceActorName)
        {
            SourceActor = *It;
            break;
        }
    }

    if (!SourceActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source actor '%s' not found in level"), *SourceActorName));
    }

    // Parse new transform (default to source actor's current values)
    FVector NewLocation = SourceActor->GetActorLocation();
    FRotator NewRotation = SourceActor->GetActorRotation();
    FVector NewScale = SourceActor->GetActorScale3D();

    auto TryGetVec = [](const TSharedPtr<FJsonObject>& Obj, FVector& Out) -> bool
    {
        if (!Obj.IsValid()) return false;
        double X = 0, Y = 0, Z = 0;
        if (!Obj->TryGetNumberField(TEXT("x"), X)) return false;
        if (!Obj->TryGetNumberField(TEXT("y"), Y)) return false;
        if (!Obj->TryGetNumberField(TEXT("z"), Z)) return false;
        Out = FVector(X, Y, Z);
        return true;
    };

    const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("location"), ObjPtr))
    {
        TryGetVec(*ObjPtr, NewLocation);
    }
    if (Params->TryGetObjectField(TEXT("rotation"), ObjPtr))
    {
        FVector RotVec;
        if (TryGetVec(*ObjPtr, RotVec))
        {
            NewRotation = FRotator(RotVec.Y, RotVec.Z, RotVec.X); // pitch,yaw,roll
        }
    }
    if (Params->TryGetObjectField(TEXT("scale"), ObjPtr))
    {
        TryGetVec(*ObjPtr, NewScale);
    }

    // Clone via deferred spawn with template 驕ｯ・ｶ郢晢ｽｻmuch faster than full SpawnActor
    // because property values are copied from the template instead of CDO init.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *NewActorName;
    SpawnParams.Template = SourceActor;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FTransform NewTransform(NewRotation, NewLocation, NewScale);
    AActor* Clone = World->SpawnActorDeferred<AActor>(
        SourceActor->GetClass(), NewTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Clone)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to clone actor"));
    }
    Clone->FinishSpawning(NewTransform);

    // Update tags: copy non-system tags, add new mcp_identity
    Clone->Tags.Empty();
    for (const FName& Tag : SourceActor->Tags)
    {
        FString TagStr = Tag.ToString();
        if (!TagStr.StartsWith(TEXT("mcp_id:")))
        {
            Clone->Tags.AddUnique(Tag);
        }
    }
    Clone->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
    if (!McpId.IsEmpty())
    {
        Clone->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:%s"), *McpId)));
    }

    // Apply custom tags from params
    const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
    if (Params->TryGetArrayField(TEXT("tags"), TagsArray))
    {
        for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
        {
            FString TagStr;
            if (TagValue->TryGetString(TagStr))
            {
                Clone->Tags.AddUnique(FName(*TagStr));
            }
        }
    }

    // Index the clone
    GetActorIndex().AddActor(Clone);

    TSharedPtr<FJsonObject> ResultObj = FEpicUnrealMCPCommonUtils::ActorToJsonObject(Clone, true);
    ResultObj->SetBoolField(TEXT("cloned"), true);
    ResultObj->SetStringField(TEXT("source_actor_name"), SourceActorName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateNavMeshVolume(const TSharedPtr<FJsonObject>& Params)
{
    // Parse parameters
    FString VolumeName = TEXT("NavMeshVolume");
    Params->TryGetStringField(TEXT("volume_name"), VolumeName);

    // Parse location
    FVector Location = FVector::ZeroVector;
    FString ParamError;
    if (Params->HasField(TEXT("location")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("location"), Location, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'location': %s"), *ParamError));
        }
    }

    // Parse extent (default 500,500,500)
    FVector Extent(500.0f, 500.0f, 500.0f);
    if (Params->HasField(TEXT("extent")))
    {
        if (!FEpicUnrealMCPCommonUtils::TryGetVectorFromJson(Params, TEXT("extent"), Extent, ParamError))
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid 'extent': %s"), *ParamError));
        }
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Create NavMeshBoundsVolume
    FScopedTransaction Transaction(FText::FromString(TEXT("UnrealMCP: Create NavMesh Volume")));
    ANavMeshBoundsVolume* NavMeshVolume = World->SpawnActor<ANavMeshBoundsVolume>(Location, FRotator::ZeroRotator);
    if (!NavMeshVolume)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn NavMeshBoundsVolume"));
    }

    // UE 5.7 no longer exposes the old brush-building helper used here. Scaling
    // the spawned volume keeps this command usable while preserving the extent
    // in the response for callers.
    NavMeshVolume->SetActorScale3D(FVector(
        FMath::Max(Extent.X / 100.0f, 0.01f),
        FMath::Max(Extent.Y / 100.0f, 0.01f),
        FMath::Max(Extent.Z / 100.0f, 0.01f)
    ));

    // Set name and folder
    NavMeshVolume->SetActorLabel(*VolumeName);
    NavMeshVolume->SetFolderPath(FName(TEXT("NavMesh")));

    NavMeshVolume->Tags.AddUnique(FName(TEXT("managed_by_mcp")));

    // Add to actor index
    GetActorIndex().AddActor(NavMeshVolume);

    // Request NavMesh rebuild
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    if (NavSys)
    {
        NavSys->Build();
    }

    // Build result JSON
    auto MakeVecJson = [](const FVector& V) -> TSharedPtr<FJsonObject> {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("x"), V.X);
        Obj->SetNumberField(TEXT("y"), V.Y);
        Obj->SetNumberField(TEXT("z"), V.Z);
        return Obj;
    };

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("volume_name"), VolumeName);
    ResultObj->SetStringField(TEXT("actor_name"), NavMeshVolume->GetName());
    ResultObj->SetObjectField(TEXT("location"), MakeVecJson(Location));
    ResultObj->SetObjectField(TEXT("extent"), MakeVecJson(Extent));
    ResultObj->SetBoolField(TEXT("navmesh_rebuilt"), NavSys != nullptr);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreatePatrolRoute(const TSharedPtr<FJsonObject>& Params)
{
    FString RouteName = TEXT("PatrolRoute");
    Params->TryGetStringField(TEXT("patrol_route_name"), RouteName);

    // Parse patrol points
    const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("points"), PointsArray) || !PointsArray || PointsArray->Num() < 2)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Patrol route requires at least 2 points"));
    }

    bool bClosedLoop = false;
    Params->TryGetBoolField(TEXT("closed_loop"), bClosedLoop);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Create a spline-based actor for the patrol route
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *RouteName;
    AActor* RouteActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!RouteActor)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn patrol route actor"));
    }

    RouteActor->SetActorLabel(*RouteName);

    // Bare AActor has no RootComponent by default; create one so the
    // SplineComponent has something to attach to.
    if (!RouteActor->GetRootComponent())
    {
        USceneComponent* RootComp = NewObject<USceneComponent>(RouteActor, USceneComponent::StaticClass(), TEXT("DefaultSceneRoot"));
        RootComp->RegisterComponent();
        RouteActor->SetRootComponent(RootComp);
    }

    // Add SplineComponent
    USplineComponent* SplineComp = NewObject<USplineComponent>(RouteActor, USplineComponent::StaticClass(), *FString::Printf(TEXT("PatrolSpline_%s"), *RouteName));
    if (!SplineComp)
    {
        RouteActor->Destroy();
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SplineComponent"));
    }

    SplineComp->RegisterComponent();
    SplineComp->AttachToComponent(RouteActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

    // Set spline points from params
    SplineComp->ClearSplinePoints();
    auto MakeVecJson = [](const FVector& V) {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("x"), V.X);
        Obj->SetNumberField(TEXT("y"), V.Y);
        Obj->SetNumberField(TEXT("z"), V.Z);
        return Obj;
    };

    TArray<TSharedPtr<FJsonValue>> PointJsonArray;
    for (int32 i = 0; i < PointsArray->Num(); ++i)
    {
        const TSharedPtr<FJsonObject>* PointObj = nullptr;
        if (!(*PointsArray)[i]->TryGetObject(PointObj) || !PointObj)
        {
            continue;
        }
        double X = 0.0, Y = 0.0, Z = 0.0;
        (*PointObj)->TryGetNumberField(TEXT("x"), X);
        (*PointObj)->TryGetNumberField(TEXT("y"), Y);
        (*PointObj)->TryGetNumberField(TEXT("z"), Z);
        FVector Point(X, Y, Z);
        SplineComp->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
        PointJsonArray.Add(MakeShared<FJsonValueObject>(MakeVecJson(Point)));
    }

    SplineComp->SetClosedLoop(bClosedLoop);
    SplineComp->UpdateSpline();

    // Add mcp_id tag
    RouteActor->Tags.Add(FName(TEXT("managed_by_mcp")));
    RouteActor->Tags.Add(FName(*FString::Printf(TEXT("mcp_id:%s"), *RouteName)));

    // Register in actor index
    GetActorIndex().AddActor(RouteActor);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("route_name"), RouteName);
    ResultObj->SetStringField(TEXT("actor_name"), RouteActor->GetName());
    ResultObj->SetArrayField(TEXT("points"), PointJsonArray);
    ResultObj->SetBoolField(TEXT("closed_loop"), bClosedLoop);
    ResultObj->SetNumberField(TEXT("point_count"), PointsArray->Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetAIBehavior(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: actor_name"));
    }

    FString BehaviorTreePath;
    Params->TryGetStringField(TEXT("behavior_tree_path"), BehaviorTreePath);

    double PerceptionRadius = 1000.0;
    Params->TryGetNumberField(TEXT("perception_radius"), PerceptionRadius);

    FString Faction = TEXT("neutral");
    Params->TryGetStringField(TEXT("faction"), Faction);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Find the actor
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
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Store AI behavior configuration as tags on the actor for runtime lookup
    TargetActor->Tags.Add(FName(*FString::Printf(TEXT("ai_faction:%s"), *Faction)));
    TargetActor->Tags.Add(FName(*FString::Printf(TEXT("ai_perception_radius:%.1f"), PerceptionRadius)));

    if (!BehaviorTreePath.IsEmpty())
    {
        TargetActor->Tags.Add(FName(*FString::Printf(TEXT("ai_behavior_tree:%s"), *BehaviorTreePath)));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), TargetActor->GetName());
    ResultObj->SetStringField(TEXT("faction"), Faction);
    ResultObj->SetNumberField(TEXT("perception_radius"), PerceptionRadius);
    if (!BehaviorTreePath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("behavior_tree_path"), BehaviorTreePath);
    }
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// ============================================================================
// Behavior Tree / Blackboard Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateBehaviorTree(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetName;
    if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'asset_name' parameter"));
    }

    FString PackagePath = TEXT("/Game/AI/");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);
    if (!PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath += TEXT("/");
    }

    FString FullPackageName = PackagePath + AssetName;
    UPackage* Package = CreatePackage(*FullPackageName);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    UBehaviorTree* BehaviorTree = NewObject<UBehaviorTree>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!BehaviorTree)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create BehaviorTree asset"));
    }

    FAssetRegistryModule::AssetCreated(BehaviorTree);
    Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_name"), AssetName);
    ResultObj->SetStringField(TEXT("path"), FullPackageName);
    ResultObj->SetStringField(TEXT("type"), TEXT("UBehaviorTree"));
    ResultObj->SetStringField(TEXT("note"), TEXT("Use the Behavior Tree Editor to add Blackboard and Composite nodes."));
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateBlackboard(
    const TSharedPtr<FJsonObject>& Params)
{
    FString AssetName;
    if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'asset_name' parameter"));
    }

    FString PackagePath = TEXT("/Game/AI/");
    Params->TryGetStringField(TEXT("package_path"), PackagePath);
    if (!PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath += TEXT("/");
    }

    FString FullPackageName = PackagePath + AssetName;
    UPackage* Package = CreatePackage(*FullPackageName);
    if (!Package)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
    }

    UBlackboardData* Blackboard = NewObject<UBlackboardData>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!Blackboard)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Blackboard asset"));
    }

    FAssetRegistryModule::AssetCreated(Blackboard);
    Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("asset_name"), AssetName);
    ResultObj->SetStringField(TEXT("path"), FullPackageName);
    ResultObj->SetStringField(TEXT("type"), TEXT("UBlackboardData"));
    ResultObj->SetStringField(TEXT("note"), TEXT("Use the Blackboard Editor to add keys and their types."));
    return ResultObj;
}

// ============================================================================
// Navigation Commands
// ============================================================================

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateNavModifierVolume(
    const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active world found"));
    }

    FString VolumeName = TEXT("NavModifierVolume");
    Params->TryGetStringField(TEXT("name"), VolumeName);

    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocationObj = nullptr;
    if (Params->TryGetObjectField(TEXT("location"), LocationObj))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*LocationObj, TEXT("value"));
    }

    FVector Extent = FVector(500.0f, 500.0f, 250.0f);
    const TSharedPtr<FJsonObject>* ExtentObj = nullptr;
    if (Params->TryGetObjectField(TEXT("extent"), ExtentObj))
    {
        Extent = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*ExtentObj, TEXT("value"));
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*VolumeName);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ANavModifierVolume* Volume = World->SpawnActor<ANavModifierVolume>(
        ANavModifierVolume::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);

    if (!Volume)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn NavModifierVolume"));
    }

    Volume->SetActorScale3D(Extent / 50.0f);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("name"), Volume->GetName());
    ResultObj->SetStringField(TEXT("location"), Location.ToString());
    ResultObj->SetStringField(TEXT("extent"), Extent.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateNavLinkProxy(
    const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active world found"));
    }

    FString ProxyName = TEXT("NavLinkProxy");
    Params->TryGetStringField(TEXT("name"), ProxyName);

    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocationObj = nullptr;
    if (Params->TryGetObjectField(TEXT("location"), LocationObj))
    {
        Location = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*LocationObj, TEXT("value"));
    }

    FVector LeftPoint = FVector(-100.0f, 0.0f, 0.0f);
    const TSharedPtr<FJsonObject>* LeftObj = nullptr;
    if (Params->TryGetObjectField(TEXT("left"), LeftObj))
    {
        LeftPoint = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*LeftObj, TEXT("value"));
    }

    FVector RightPoint = FVector(100.0f, 0.0f, 0.0f);
    const TSharedPtr<FJsonObject>* RightObj = nullptr;
    if (Params->TryGetObjectField(TEXT("right"), RightObj))
    {
        RightPoint = FEpicUnrealMCPCommonUtils::GetVectorFromJson(*RightObj, TEXT("value"));
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*ProxyName);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ANavLinkProxy* Proxy = World->SpawnActor<ANavLinkProxy>(
        ANavLinkProxy::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);

    if (!Proxy)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn NavLinkProxy"));
    }

    Proxy->SetActorLocation(Location);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("name"), Proxy->GetName());
    ResultObj->SetStringField(TEXT("location"), Location.ToString());
    ResultObj->SetStringField(TEXT("left"), LeftPoint.ToString());
    ResultObj->SetStringField(TEXT("right"), RightPoint.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateSplineFromPoints(const TSharedPtr<FJsonObject>& Params)
{
    FString SplineName = TEXT("SplineFromPoints");
    Params->TryGetStringField(TEXT("spline_name"), SplineName);

    FString McpId = SplineName;
    Params->TryGetStringField(TEXT("mcp_id"), McpId);

    bool bClosedLoop = false;
    Params->TryGetBoolField(TEXT("closed_loop"), bClosedLoop);

    bool bFocusViewport = true;
    Params->TryGetBoolField(TEXT("focus_viewport"), bFocusViewport);

    double MergeTolerance = 0.01;
    Params->TryGetNumberField(TEXT("point_merge_tolerance"), MergeTolerance);
    const double MergeToleranceSq = FMath::Square(FMath::Max(MergeTolerance, 0.0));

    FString TangentModeStr = TEXT("curve");
    Params->TryGetStringField(TEXT("tangent_mode"), TangentModeStr);
    const ESplinePointType::Type PointType = TangentModeStr.Equals(TEXT("linear"), ESearchCase::IgnoreCase)
        ? ESplinePointType::Linear
        : ESplinePointType::Curve;

    auto TryParsePointObject = [](const TSharedPtr<FJsonObject>& Obj, FVector& OutPoint, FString& OutError) -> bool
    {
        if (!Obj.IsValid())
        {
            OutError = TEXT("point must be an object");
            return false;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!Obj->TryGetNumberField(TEXT("x"), X) ||
            !Obj->TryGetNumberField(TEXT("y"), Y) ||
            !Obj->TryGetNumberField(TEXT("z"), Z))
        {
            OutError = TEXT("point requires numeric x, y, and z fields");
            return false;
        }

        OutPoint = FVector(X, Y, Z);
        if (!FMath::IsFinite(OutPoint.X) || !FMath::IsFinite(OutPoint.Y) || !FMath::IsFinite(OutPoint.Z))
        {
            OutError = TEXT("point contains NaN or Infinity");
            return false;
        }
        return true;
    };

    auto TryParsePointValue = [&TryParsePointObject](const TSharedPtr<FJsonValue>& Value, FVector& OutPoint, FString& OutError) -> bool
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (Value.IsValid() && Value->TryGetObject(Obj) && Obj && Obj->IsValid())
        {
            return TryParsePointObject(*Obj, OutPoint, OutError);
        }
        OutError = TEXT("point value must be an object");
        return false;
    };

    auto MakeVecJson = [](const FVector& V)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("x"), V.X);
        Obj->SetNumberField(TEXT("y"), V.Y);
        Obj->SetNumberField(TEXT("z"), V.Z);
        return Obj;
    };

    auto PointsNearlyEqual = [MergeToleranceSq](const FVector& A, const FVector& B) -> bool
    {
        return FVector::DistSquared(A, B) <= MergeToleranceSq;
    };

    TArray<TArray<FVector>> PointChains;
    int32 SegmentCount = 0;

    const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
    if (Params->TryGetArrayField(TEXT("points"), PointsArray) && PointsArray)
    {
        if (PointsArray->Num() < 2)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("create_spline_from_points requires at least 2 points"));
        }

        TArray<FVector> Chain;
        Chain.Reserve(PointsArray->Num());
        for (int32 i = 0; i < PointsArray->Num(); ++i)
        {
            FVector Point;
            FString Error;
            if (!TryParsePointValue((*PointsArray)[i], Point, Error))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid points[%d]: %s"), i, *Error));
            }
            Chain.Add(Point);
        }
        SegmentCount = FMath::Max(0, Chain.Num() - 1);
        PointChains.Add(MoveTemp(Chain));
    }
    else
    {
        const TArray<TSharedPtr<FJsonValue>>* SegmentsArray = nullptr;
        if (!Params->TryGetArrayField(TEXT("segments"), SegmentsArray) || !SegmentsArray || SegmentsArray->Num() < 1)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("create_spline_from_points requires 'points' or at least 1 segment"));
        }

        TArray<FVector> CurrentChain;
        for (int32 i = 0; i < SegmentsArray->Num(); ++i)
        {
            const TSharedPtr<FJsonObject>* SegObj = nullptr;
            if (!(*SegmentsArray)[i].IsValid() || !(*SegmentsArray)[i]->TryGetObject(SegObj) || !SegObj || !SegObj->IsValid())
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid segments[%d]: segment must be an object"), i));
            }

            const TSharedPtr<FJsonObject>* StartObj = nullptr;
            const TSharedPtr<FJsonObject>* EndObj = nullptr;
            if (!(*SegObj)->TryGetObjectField(TEXT("start"), StartObj) || !(*SegObj)->TryGetObjectField(TEXT("end"), EndObj))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid segments[%d]: requires start and end"), i));
            }

            FVector StartPt;
            FVector EndPt;
            FString Error;
            if (!TryParsePointObject(*StartObj, StartPt, Error))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid segments[%d].start: %s"), i, *Error));
            }
            if (!TryParsePointObject(*EndObj, EndPt, Error))
            {
                return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid segments[%d].end: %s"), i, *Error));
            }
            if (PointsNearlyEqual(StartPt, EndPt))
            {
                continue;
            }

            if (CurrentChain.Num() == 0)
            {
                CurrentChain.Add(StartPt);
                CurrentChain.Add(EndPt);
            }
            else if (PointsNearlyEqual(CurrentChain.Last(), StartPt))
            {
                CurrentChain.Add(EndPt);
            }
            else
            {
                PointChains.Add(MoveTemp(CurrentChain));
                CurrentChain.Add(StartPt);
                CurrentChain.Add(EndPt);
            }
            ++SegmentCount;
        }

        if (CurrentChain.Num() > 0)
        {
            PointChains.Add(MoveTemp(CurrentChain));
        }
    }

    int32 TotalPointCount = 0;
    for (const TArray<FVector>& Chain : PointChains)
    {
        TotalPointCount += Chain.Num();
    }
    if (TotalPointCount < 2 || PointChains.Num() == 0)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("create_spline_from_points produced no drawable spline points"));
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Create Spline %s"), *SplineName)));
    AActor* SplineActor = FindActorByMcpId(World, McpId);
    bool bCreatedActor = false;
    if (!SplineActor)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(*SplineName));
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SplineActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!SplineActor)
        {
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn spline actor"));
        }
        bCreatedActor = true;
    }

    SplineActor->SetFlags(RF_Transactional);
    SplineActor->Modify();
    SplineActor->SetActorLabel(*SplineName);

    if (!SplineActor->GetRootComponent())
    {
        USceneComponent* RootComp = NewObject<USceneComponent>(SplineActor, USceneComponent::StaticClass(), TEXT("DefaultSceneRoot"));
        RootComp->RegisterComponent();
        SplineActor->SetRootComponent(RootComp);
    }

    TArray<USplineComponent*> ExistingSplineComponents;
    SplineActor->GetComponents<USplineComponent>(ExistingSplineComponents);
    for (USplineComponent* ExistingComp : ExistingSplineComponents)
    {
        if (ExistingComp)
        {
            ExistingComp->DestroyComponent();
        }
    }

    TArray<TSharedPtr<FJsonValue>> PointJsonArray;
    TArray<TSharedPtr<FJsonValue>> ChainLengthsJson;
    int32 ComponentCount = 0;
    for (int32 ChainIndex = 0; ChainIndex < PointChains.Num(); ++ChainIndex)
    {
        const TArray<FVector>& Chain = PointChains[ChainIndex];
        if (Chain.Num() < 2)
        {
            continue;
        }

        const FName ComponentName(*FString::Printf(TEXT("ProceduralSpline_%d"), ChainIndex));
        USplineComponent* SplineComp = NewObject<USplineComponent>(SplineActor, USplineComponent::StaticClass(), ComponentName, RF_Transactional);
        if (!SplineComp)
        {
            if (bCreatedActor)
            {
                SplineActor->Destroy();
            }
            return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create SplineComponent"));
        }

        SplineComp->SetFlags(RF_Transactional);
        SplineComp->AttachToComponent(SplineActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
        SplineActor->AddInstanceComponent(SplineComp);
        SplineComp->RegisterComponent();
        SplineComp->ClearSplinePoints(false);

        for (int32 PointIndex = 0; PointIndex < Chain.Num(); ++PointIndex)
        {
            SplineComp->AddSplinePoint(Chain[PointIndex], ESplineCoordinateSpace::World, false);
            SplineComp->SetSplinePointType(PointIndex, PointType, false);
            PointJsonArray.Add(MakeShared<FJsonValueObject>(MakeVecJson(Chain[PointIndex])));
        }

        SplineComp->SetClosedLoop(bClosedLoop && PointChains.Num() == 1);
        SplineComp->UpdateSpline();
        ChainLengthsJson.Add(MakeShared<FJsonValueNumber>(Chain.Num()));
        ++ComponentCount;
    }

    SplineActor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
    if (!McpId.IsEmpty())
    {
        SplineActor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:%s"), *McpId)));
    }
    GetActorIndex().AddActor(SplineActor);
    SplineActor->MarkPackageDirty();

    if (bFocusViewport && GEditor)
    {
        GEditor->SelectNone(false, true, false);
        GEditor->SelectActor(SplineActor, true, true, true, true);
        GEditor->MoveViewportCamerasToActor(*SplineActor, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("spline_name"), SplineName);
    ResultObj->SetStringField(TEXT("mcp_id"), McpId);
    ResultObj->SetStringField(TEXT("actor_name"), SplineActor->GetName());
    ResultObj->SetStringField(TEXT("actor_label"), SplineActor->GetActorLabel());
    ResultObj->SetArrayField(TEXT("points"), PointJsonArray);
    ResultObj->SetArrayField(TEXT("chain_lengths"), ChainLengthsJson);
    ResultObj->SetBoolField(TEXT("closed_loop"), bClosedLoop && PointChains.Num() == 1);
    ResultObj->SetStringField(TEXT("tangent_mode"), TangentModeStr);
    ResultObj->SetNumberField(TEXT("point_count"), TotalPointCount);
    ResultObj->SetNumberField(TEXT("segment_count"), SegmentCount);
    ResultObj->SetNumberField(TEXT("component_count"), ComponentCount);
    ResultObj->SetBoolField(TEXT("created_actor"), bCreatedActor);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleApplySceneDelta(const TSharedPtr<FJsonObject>& Params)
{
    FString TransactionId = TEXT("batch");
    Params->TryGetStringField(TEXT("transaction_id"), TransactionId);

    const TArray<TSharedPtr<FJsonValue>>* CreatesArray = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* UpdatesArray = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* DeletesArray = nullptr;

    Params->TryGetArrayField(TEXT("creates"), CreatesArray);
    Params->TryGetArrayField(TEXT("updates"), UpdatesArray);
    Params->TryGetArrayField(TEXT("deletes"), DeletesArray);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Apply Scene Delta %s"), *TransactionId)));

    int32 CreatedCount = 0;
    int32 UpdatedCount = 0;
    int32 DeletedCount = 0;
    TArray<TSharedPtr<FJsonValue>> CreatedActors;
    TArray<TSharedPtr<FJsonValue>> UpdatedActors;
    TArray<TSharedPtr<FJsonValue>> DeletedActors;
    TArray<TSharedPtr<FJsonValue>> Errors;

    // --- P4.3: Pre-parse, pre-validate, then execute ------------------------

    // Phase 1: Parse all create params and validate for duplicates
    TArray<FParsedCreateParams> ParsedCreates;
    if (CreatesArray)
    {
        ParsedCreates.Reserve(CreatesArray->Num());
        TSet<FString> CreateNames;
        for (const TSharedPtr<FJsonValue>& Value : *CreatesArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                FParsedCreateParams Invalid;
                Invalid.bValid = false;
                Invalid.ErrorString = TEXT("Invalid JSON object in creates array");
                ParsedCreates.Add(Invalid);
                continue;
            }
            FParsedCreateParams Parsed = ParseCreateParams(Value->AsObject());
            // Check for duplicate names within this batch
            if (Parsed.bValid && CreateNames.Contains(Parsed.Name))
            {
                Parsed.bValid = false;
                Parsed.ErrorString = FString::Printf(TEXT("Duplicate actor name in batch: %s"), *Parsed.Name);
            }
            if (Parsed.bValid)
            {
                CreateNames.Add(Parsed.Name);
            }
            ParsedCreates.Add(Parsed);
        }
        CreatedActors.Reserve(ParsedCreates.Num());
    }

    // Phase 2: Parse all update params
    TArray<FParsedUpdateParams> ParsedUpdates;
    if (UpdatesArray)
    {
        ParsedUpdates.Reserve(UpdatesArray->Num());
        for (const TSharedPtr<FJsonValue>& Value : *UpdatesArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                FParsedUpdateParams Invalid;
                Invalid.bValid = false;
                Invalid.ErrorString = TEXT("Invalid JSON object in updates array");
                ParsedUpdates.Add(Invalid);
                continue;
            }
            ParsedUpdates.Add(ParseUpdateParams(Value->AsObject()));
        }
        UpdatedActors.Reserve(ParsedUpdates.Num());
    }

    // Phase 3: Execute creates
    for (const FParsedCreateParams& Parsed : ParsedCreates)
    {
        if (!Parsed.bValid)
        {
            Errors.Add(MakeShared<FJsonValueObject>(
                FEpicUnrealMCPCommonUtils::CreateErrorResponse(Parsed.ErrorString)));
            continue;
        }
        TSharedPtr<FJsonObject> Result = ExecuteCreateActor(Parsed, true);
        if (Result.IsValid() && Result->HasField(TEXT("success")) && Result->GetBoolField(TEXT("success")))
        {
            CreatedCount++;
            TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
            Summary->SetStringField(TEXT("name"), Result->GetStringField(TEXT("name")));
            CreatedActors.Add(MakeShared<FJsonValueObject>(Summary));
        }
        else
        {
            Errors.Add(MakeShared<FJsonValueObject>(Result));
        }
    }

    // Phase 4: Execute updates
    for (const FParsedUpdateParams& Parsed : ParsedUpdates)
    {
        if (!Parsed.bValid)
        {
            Errors.Add(MakeShared<FJsonValueObject>(
                FEpicUnrealMCPCommonUtils::CreateErrorResponse(Parsed.ErrorString)));
            continue;
        }
        TSharedPtr<FJsonObject> Result = ExecuteUpdateActor(Parsed);
        if (Result.IsValid() && Result->HasField(TEXT("success")) && Result->GetBoolField(TEXT("success")))
        {
            UpdatedCount++;
            TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
            Summary->SetStringField(TEXT("name"), Result->GetStringField(TEXT("name")));
            UpdatedActors.Add(MakeShared<FJsonValueObject>(Summary));
        }
        else
        {
            Errors.Add(MakeShared<FJsonValueObject>(Result));
        }
    }

    // Phase 5: Execute deletes (simple, no pre-parse needed)
    if (DeletesArray)
    {
        DeletedActors.Reserve(DeletesArray->Num());
        for (const TSharedPtr<FJsonValue>& Value : *DeletesArray)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                continue;
            }
            TSharedPtr<FJsonObject> Result = HandleDeleteActorByMcpId(Value->AsObject());
            if (Result.IsValid() && Result->HasField(TEXT("deleted")) && Result->GetBoolField(TEXT("deleted")))
            {
                DeletedCount++;
                TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
                const TSharedPtr<FJsonObject>* DeletedActorObj = nullptr;
                if (Result->TryGetObjectField(TEXT("deleted_actor"), DeletedActorObj) && DeletedActorObj)
                {
                    Summary->SetStringField(TEXT("name"), (*DeletedActorObj)->GetStringField(TEXT("name")));
                }
                DeletedActors.Add(MakeShared<FJsonValueObject>(Summary));
            }
            else if (Result.IsValid() && Result->HasField(TEXT("success")) && Result->GetBoolField(TEXT("success")))
            {
                DeletedCount++;
            }
            else
            {
                Errors.Add(MakeShared<FJsonValueObject>(Result));
            }
        }
    }

    // Rebuild index to ensure consistency after batch mutations
    GetActorIndex().RebuildFromWorld(World);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("transaction_id"), TransactionId);
    ResultObj->SetNumberField(TEXT("created_count"), CreatedCount);
    ResultObj->SetNumberField(TEXT("updated_count"), UpdatedCount);
    ResultObj->SetNumberField(TEXT("deleted_count"), DeletedCount);
    ResultObj->SetNumberField(TEXT("error_count"), Errors.Num());
    ResultObj->SetArrayField(TEXT("created"), CreatedActors);
    ResultObj->SetArrayField(TEXT("updated"), UpdatedActors);
    ResultObj->SetArrayField(TEXT("deleted"), DeletedActors);
    ResultObj->SetArrayField(TEXT("errors"), Errors);
    return ResultObj;
}

// ------------------------------------------------------------------
// Draft Proxy commands (HISM-based lightweight visualization)
// ------------------------------------------------------------------

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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreateDraftProxy(const TSharedPtr<FJsonObject>& Params)
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

    GetActorIndex().AddActor(ProxyActor);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("proxy_name"), ProxyName);
    ResultObj->SetStringField(TEXT("actor_name"), ProxyActor->GetName());
    ResultObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleUpdateDraftProxy(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteDraftProxy(const TSharedPtr<FJsonObject>& Params)
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

    GetActorIndex().RemoveActor(ProxyActor);

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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnInstanceSet(const TSharedPtr<FJsonObject>& Params)
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
    GetActorIndex().AddActor(SetActor);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("set_id"), SetId);
    ResultObj->SetStringField(TEXT("actor_name"), SetActor->GetName());
    ResultObj->SetNumberField(TEXT("instance_count"), InstanceCount);
    ResultObj->SetBoolField(TEXT("use_hism"), bUseHism);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleUpdateInstanceSet(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleDeleteInstanceSet(const TSharedPtr<FJsonObject>& Params)
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

    GetActorIndex().RemoveActor(SetActor);

    FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnrealMCP: Delete Instance Set %s"), *SetId)));
    SetActor->Modify();
    SetActor->Destroy();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("deleted"), true);
    ResultObj->SetStringField(TEXT("set_id"), SetId);
    return ResultObj;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleGetInstanceSetState(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleListInstanceSets(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorCollisionPreset(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSetActorPhysics(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCreatePhysicalMaterial(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnRadialForce(const TSharedPtr<FJsonObject>& Params)
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

    GetActorIndex().AddActor(ForceActor);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ForceActor->GetName());
    Result->SetNumberField(TEXT("radius"), Radius);
    Result->SetNumberField(TEXT("strength"), Strength);
    return Result;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnPhysicsConstraint(const TSharedPtr<FJsonObject>& Params)
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

    GetActorIndex().AddActor(ConstraintActor);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor_name"), ConstraintActor->GetName());
    return Result;
}

// ------------------------------------------------------------------
// Validation commands
// ------------------------------------------------------------------

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleCompileAllBlueprints(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRunMapCheck(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleFindBrokenReferences(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleRequestCognitiveProcessing(const TSharedPtr<FJsonObject>& Params)
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
        // Try mcp_id lookup
        TargetActor = FindActorByMcpId(World, ActorName);
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
TSharedPtr<FJsonObject> FEpicUnrealMCPEditorCommands::HandleSpawnTileGrid(const TSharedPtr<FJsonObject>& Params)
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

        GetActorIndex().AddActor(TileActor);

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
