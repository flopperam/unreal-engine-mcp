#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for procedural generation and instancing commands.
 * Split from FEpicUnrealMCPEditorCommands to reduce file size and
 * isolate procedural/instancing logic.
 */
class UNREALMCP_API FEpicUnrealMCPProceduralCommands
{
public:
    FEpicUnrealMCPProceduralCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    UWorld* GetEditorWorld() const;

    // Procedural generation
    TSharedPtr<FJsonObject> HandleSpawnTileGrid(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnProceduralActorBatch(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateSplineMeshFromSegments(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateDataLayerForGeneration(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleClearGeneratedGroup(const TSharedPtr<FJsonObject>& Params);

    // Draft proxy (HISM visualization)
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

    // Physics commands (collision, physics body, materials, forces, constraints)
    TSharedPtr<FJsonObject> HandleSetActorCollisionPreset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorPhysics(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreatePhysicalMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnRadialForce(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnPhysicsConstraint(const TSharedPtr<FJsonObject>& Params);

    // Validation commands
    TSharedPtr<FJsonObject> HandleCompileAllBlueprints(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRunMapCheck(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindBrokenReferences(const TSharedPtr<FJsonObject>& Params);
};
