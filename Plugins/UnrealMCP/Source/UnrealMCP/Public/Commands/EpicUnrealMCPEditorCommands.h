#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for generic / catch-all Editor MCP commands.
 *
 * Refactor history:
 *   Phase 1 - Procedural, Physics, Validation, and Cognitive Processing
 *             commands were moved to FEpicUnrealMCPProceduralCommands.
 *   Phase 2 - Actor CRUD (spawn / delete / transform / clone /
 *             find-by-name / find-by-mcp-id / apply_scene_delta) was
 *             moved to FEpicUnrealMCPActorCommands.
 *   Phase 3 - NavAI + Spline (NavMesh, Behavior Tree, Blackboard,
 *             NavLinkProxy, NavModifier, PatrolRoute, SplineFromPoints)
 *             was moved to FEpicUnrealMCPNavigationCommands.
 *
 * This class is intentionally kept as a small shell so that:
 *   - the Bridge subsystem registration pattern stays uniform,
 *   - the legacy "route 1" lane in EpicUnrealMCPRouter still resolves to *   - future generic editor commands have a clear, low-friction landing
 *     spot.
 *
 * If a new feature warrants more than one or two handlers, prefer
 * creating a new FEpicUnrealMCP<Domain>Commands class instead of growing
 * this file back.
 */
class UNREALMCP_API FEpicUnrealMCPEditorCommands
{
public:
    FEpicUnrealMCPEditorCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    UWorld* GetEditorWorld() const;
};