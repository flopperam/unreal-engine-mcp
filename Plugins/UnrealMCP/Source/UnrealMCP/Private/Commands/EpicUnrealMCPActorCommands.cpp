// =====================================================================
// EpicUnrealMCPActorCommands
//
// Phase 2 refactor: split out from EpicUnrealMCPEditorCommands.cpp.
//
// Owns the Actor CRUD surface area:
//   - get_actors_in_level / find_actors_by_name
//   - spawn_actor / delete_actor / set_actor_transform / clone_actor
//   - find_actor_by_mcp_id / set_actor_transform_by_mcp_id /
//     delete_actor_by_mcp_id
//   - apply_scene_delta (batch)
//
// Helpers ParseCreateParams / ParseUpdateParams / ExecuteCreateActor /
// ExecuteUpdateActor / FindActorByMcpId are kept private to this class so
// that the batch path and the individual handlers share the same code.
// =====================================================================

#include "Commands/EpicUnrealMCPActorCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPBridge.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/SkyLight.h"
#include "Engine/RectLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Selection.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Camera/CameraActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "Subsystems/EditorActorSubsystem.h"
#include "EditorAssetLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    // Bridge ActorIndex accessor (file-local helper).
    FActorIndex& GetActorIndex()
    {
        UEpicUnrealMCPBridge* Bridge = GEditor->GetEditorSubsystem<UEpicUnrealMCPBridge>();
        check(Bridge);
        return Bridge->ActorIndex;
    }
}

FEpicUnrealMCPActorCommands::FEpicUnrealMCPActorCommands()
{
}

UWorld* FEpicUnrealMCPActorCommands::GetEditorWorld() const
{
    if (!GEditor)
    {
        return nullptr;
    }
    return GEditor->GetEditorWorldContext().World();
}

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    using Handler = TSharedPtr<FJsonObject>(FEpicUnrealMCPActorCommands::*)(const TSharedPtr<FJsonObject>&);
    static const TMap<FString, Handler> Dispatch = {
        {TEXT("get_actors_in_level"),            &FEpicUnrealMCPActorCommands::HandleGetActorsInLevel},
        {TEXT("find_actors_by_name"),            &FEpicUnrealMCPActorCommands::HandleFindActorsByName},
        {TEXT("spawn_actor"),                    &FEpicUnrealMCPActorCommands::HandleSpawnActor},
        {TEXT("delete_actor"),                   &FEpicUnrealMCPActorCommands::HandleDeleteActor},
        {TEXT("set_actor_transform"),            &FEpicUnrealMCPActorCommands::HandleSetActorTransform},
        {TEXT("find_actor_by_mcp_id"),           &FEpicUnrealMCPActorCommands::HandleFindActorByMcpId},
        {TEXT("set_actor_transform_by_mcp_id"),  &FEpicUnrealMCPActorCommands::HandleSetActorTransformByMcpId},
        {TEXT("delete_actor_by_mcp_id"),         &FEpicUnrealMCPActorCommands::HandleDeleteActorByMcpId},
        {TEXT("apply_scene_delta"),              &FEpicUnrealMCPActorCommands::HandleApplySceneDelta},
        {TEXT("clone_actor"),                    &FEpicUnrealMCPActorCommands::HandleCloneActor},
    };

    const Handler* H = Dispatch.Find(CommandType);
    if (H)
    {
        return (this->*(*H))(Params);
    }

    return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
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

FParsedCreateParams FEpicUnrealMCPActorCommands::ParseCreateParams(const TSharedPtr<FJsonObject>& Params) const
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

FParsedUpdateParams FEpicUnrealMCPActorCommands::ParseUpdateParams(const TSharedPtr<FJsonObject>& Params) const
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::ExecuteCreateActor(const FParsedCreateParams& Parsed, bool bSuppressTransaction)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::ExecuteUpdateActor(const FParsedUpdateParams& Parsed)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
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

AActor* FEpicUnrealMCPActorCommands::FindActorByMcpId(UWorld* World, const FString& McpId) const
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleFindActorByMcpId(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleSetActorTransformByMcpId(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleDeleteActorByMcpId(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleCloneActor(const TSharedPtr<FJsonObject>& Params)
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

    // Clone via deferred spawn with template 鬯ｩ蛹・ｽｽ・ｯ郢晢ｽｻ繝ｻ・ｶ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｻmuch faster than full SpawnActor
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

TSharedPtr<FJsonObject> FEpicUnrealMCPActorCommands::HandleApplySceneDelta(const TSharedPtr<FJsonObject>& Params)
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
