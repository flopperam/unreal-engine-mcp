#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Pre-parsed create parameters for batch scene delta processing.
 * Parsed once before entering the game-thread loop to reduce per-item overhead.
 *
 * Phase 2 refactor: moved here from EpicUnrealMCPEditorCommands.h together
 * with the Actor CRUD handlers.  Kept in a header so apply_scene_delta and
 * the other batch tooling can share the parsed representation.
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
 * Handler class for Actor CRUD MCP commands.
 *
 * Phase 2 refactor: split out from FEpicUnrealMCPEditorCommands so the
 * EditorCommands file stays focused on viewport / level / misc editor
 * operations while bulk Actor lifecycle work lives here.  Routed via the
 * dedicated route id assigned in EpicUnrealMCPRouter.
 */
class UNREALMCP_API FEpicUnrealMCPActorCommands
{
public:
    FEpicUnrealMCPActorCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    UWorld* GetEditorWorld() const;

    // Actor manipulation commands (label / index based)
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);

    // MCP identity commands (mcp_id based)
    TSharedPtr<FJsonObject> HandleFindActorByMcpId(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransformByMcpId(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActorByMcpId(const TSharedPtr<FJsonObject>& Params);

    // Helper
    AActor* FindActorByMcpId(UWorld* World, const FString& McpId) const;

    // Pre-parse helpers used by HandleApplySceneDelta and individual handlers
    FParsedCreateParams ParseCreateParams(const TSharedPtr<FJsonObject>& Params) const;
    FParsedUpdateParams ParseUpdateParams(const TSharedPtr<FJsonObject>& Params) const;
    TSharedPtr<FJsonObject> ExecuteCreateActor(const FParsedCreateParams& Parsed, bool bSuppressTransaction = false);
    TSharedPtr<FJsonObject> ExecuteUpdateActor(const FParsedUpdateParams& Parsed);

    // Template clone (fast duplication of identical-mesh actors)
    TSharedPtr<FJsonObject> HandleCloneActor(const TSharedPtr<FJsonObject>& Params);

    // Batch scene delta (P4)
    TSharedPtr<FJsonObject> HandleApplySceneDelta(const TSharedPtr<FJsonObject>& Params);
};