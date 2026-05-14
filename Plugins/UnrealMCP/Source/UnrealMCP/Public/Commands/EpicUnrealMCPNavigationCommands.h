#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Navigation, AI, and Spline-related MCP commands.
 *
 * Phase 3 refactor: split out from FEpicUnrealMCPEditorCommands so the
 * navigation/AI/spline surface lives in one focused file.  The grouping
 * follows the original Phase 3 plan ("NavAI + Spline") because the
 * patrol-route handler bridges both AI and spline use cases.
 *
 * Owns:
 *   NavMesh        - create_nav_mesh_volume, create_nav_modifier_volume,
 *                    create_nav_link_proxy
 *   AI             - create_patrol_route, set_ai_behavior,
 *                    create_behavior_tree, create_blackboard
 *   Spline         - create_spline_from_points (L-System output)
 *
 * Routed via the dedicated route id assigned in EpicUnrealMCPRouter.
 */
class UNREALMCP_API FEpicUnrealMCPNavigationCommands
{
public:
    FEpicUnrealMCPNavigationCommands();

    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    UWorld* GetEditorWorld() const;

    // NavMesh / Navigation
    TSharedPtr<FJsonObject> HandleCreateNavMeshVolume(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateNavModifierVolume(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateNavLinkProxy(const TSharedPtr<FJsonObject>& Params);

    // AI
    TSharedPtr<FJsonObject> HandleCreatePatrolRoute(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetAIBehavior(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateBehaviorTree(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateBlackboard(const TSharedPtr<FJsonObject>& Params);

    // Spline
    TSharedPtr<FJsonObject> HandleCreateSplineFromPoints(const TSharedPtr<FJsonObject>& Params);
};