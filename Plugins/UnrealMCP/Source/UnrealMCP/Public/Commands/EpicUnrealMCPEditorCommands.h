#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Pre-parsed create parameters for batch scene delta processing.
 * Parsed once before entering the game-thread loop to reduce per-item overhead.
 */
struct FParsedCreateParams
{
    bool bValid = false;
    FString ErrorString;
    FString Name;
    FString Type;
    FString McpId;
    FString StaticMeshPath;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    TArray<FString> Tags;
};

/**
 * Pre-parsed update parameters for batch scene delta processing.
 */
struct FParsedUpdateParams
{
    bool bValid = false;
    FString ErrorString;
    FString McpId;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    bool bHasLocation = false;
    bool bHasRotation = false;
    bool bHasScale = false;
};

/**
 * Handler class for Editor-related MCP commands
 * Handles viewport control, actor manipulation, and level management
 */
class UNREALMCP_API FEpicUnrealMCPEditorCommands
{
public:
    	FEpicUnrealMCPEditorCommands();

    // Handle editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    UWorld* GetEditorWorld() const;

    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);

    // MCP identity commands
    TSharedPtr<FJsonObject> HandleFindActorByMcpId(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransformByMcpId(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActorByMcpId(const TSharedPtr<FJsonObject>& Params);

    // Helper
    AActor* FindActorByMcpId(UWorld* World, const FString& McpId) const;

    // P4.3: Pre-parse helpers for batch scene delta
    FParsedCreateParams ParseCreateParams(const TSharedPtr<FJsonObject>& Params) const;
    FParsedUpdateParams ParseUpdateParams(const TSharedPtr<FJsonObject>& Params) const;
    TSharedPtr<FJsonObject> ExecuteCreateActor(const FParsedCreateParams& Parsed, bool bSuppressTransaction = false);
    TSharedPtr<FJsonObject> ExecuteUpdateActor(const FParsedUpdateParams& Parsed);

    // Template clone (fast duplication of identical-mesh actors)
    TSharedPtr<FJsonObject> HandleCloneActor(const TSharedPtr<FJsonObject>& Params);

    // NavMesh commands
    TSharedPtr<FJsonObject> HandleCreateNavMeshVolume(const TSharedPtr<FJsonObject>& Params);

    // AI commands
    TSharedPtr<FJsonObject> HandleCreatePatrolRoute(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetAIBehavior(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateBehaviorTree(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateBlackboard(const TSharedPtr<FJsonObject>& Params);

    // Navigation commands
    TSharedPtr<FJsonObject> HandleCreateNavModifierVolume(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateNavLinkProxy(const TSharedPtr<FJsonObject>& Params);

    // Procedural spline from points (L-System output)
    TSharedPtr<FJsonObject> HandleCreateSplineFromPoints(const TSharedPtr<FJsonObject>& Params);

    // Batch scene delta (P4)
    TSharedPtr<FJsonObject> HandleApplySceneDelta(const TSharedPtr<FJsonObject>& Params);

    // Draft proxy commands (HISM visualization)
    TSharedPtr<FJsonObject> HandleCreateDraftProxy(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleUpdateDraftProxy(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteDraftProxy(const TSharedPtr<FJsonObject>& Params);

    // InstanceSet commands (HISM/ISM bulk instancing)
    TSharedPtr<FJsonObject> HandleSpawnInstanceSet(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleUpdateInstanceSet(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteInstanceSet(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetInstanceSetState(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListInstanceSets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRequestCognitiveProcessing(const TSharedPtr<FJsonObject>& Params);
    }; 