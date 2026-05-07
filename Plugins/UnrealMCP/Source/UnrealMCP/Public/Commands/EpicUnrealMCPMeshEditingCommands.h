#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations for DynamicMesh
namespace UE::Geometry
{
	class FDynamicMesh3;
}

/**
 * Handler class for Static Mesh Editing MCP commands
 */
class UNREALMCP_API FEpicUnrealMCPMeshEditingCommands
{
public:
	FEpicUnrealMCPMeshEditingCommands();

	// Handle mesh editing commands
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Static Mesh details and basic properties
	TSharedPtr<FJsonObject> HandleGetStaticMeshDetails(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetNaniteSettings(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetLightmapSettings(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleEditMeshBounds(const TSharedPtr<FJsonObject>& Params);

	// Collisions
	TSharedPtr<FJsonObject> HandleGenerateCollision(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetCollisionComplexity(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddSimpleCollision(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveCollisions(const TSharedPtr<FJsonObject>& Params);

	// LODs
	TSharedPtr<FJsonObject> HandleSetLODGroup(const TSharedPtr<FJsonObject>& Params);

	// Sockets
	TSharedPtr<FJsonObject> HandleAddSocket(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveSocket(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleUpdateSocket(const TSharedPtr<FJsonObject>& Params);

	// Geometry / Mesh Editing (FDynamicMesh3 based)
	TSharedPtr<FJsonObject> HandleMeshBoolean(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshRemesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshSimplify(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshUVUnwrap(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshVoxelRemesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshUVLayout(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetPivot(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshMerge(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetVertexColors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshBake(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePolyEdit(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleModelingToolExecute(const TSharedPtr<FJsonObject>& Params);

	// LOD / Lightmap
	TSharedPtr<FJsonObject> HandleGenerateLODs(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGenerateLightmapUVs(const TSharedPtr<FJsonObject>& Params);

	// UCX Collision Import
	TSharedPtr<FJsonObject> HandleImportUCXCollision(const TSharedPtr<FJsonObject>& Params);

	// UV Operations (UStaticMeshEditorSubsystem alternatives)
	TSharedPtr<FJsonObject> HandleGenerateBoxUVChannel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGeneratePlanarUVChannel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGenerateCylindricalUVChannel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddUVChannel(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveUVChannel(const TSharedPtr<FJsonObject>& Params);

	// LOD Operations (UStaticMeshEditorSubsystem alternatives)
	TSharedPtr<FJsonObject> HandleSetLods(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveLods(const TSharedPtr<FJsonObject>& Params);

	// Mesh Merge Operations (UStaticMeshEditorSubsystem alternatives)
	TSharedPtr<FJsonObject> HandleJoinStaticMeshActors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMergeStaticMeshActors(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateProxyMeshActor(const TSharedPtr<FJsonObject>& Params);

	// Other utilities
	TSharedPtr<FJsonObject> HandleSetGenerateLightmapUVs(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleHasVertexColors(const TSharedPtr<FJsonObject>& Params);

	// ------------------------------------------------------------------
	// Core Mesh Editing Helpers (FDynamicMesh3 based)
	// ------------------------------------------------------------------
	bool ReadMeshDescriptionFromStaticMesh(UStaticMesh* StaticMesh, int32 LODIndex, FMeshDescription& OutMeshDescription, FString& OutError);
	bool WriteMeshDescriptionToStaticMesh(UStaticMesh* StaticMesh, int32 LODIndex, FMeshDescription& InMeshDescription, FString& OutError);
	bool MeshDescriptionToDynamicMesh(const FMeshDescription& MeshDescription, UE::Geometry::FDynamicMesh3& OutDynamicMesh, FString& OutError);
	bool DynamicMeshToMeshDescription(const UE::Geometry::FDynamicMesh3& DynamicMesh, FMeshDescription& OutMeshDescription, FString& OutError);

	// Helper
	class UStaticMesh* GetStaticMeshFromParams(const TSharedPtr<FJsonObject>& Params, FString& OutError) const;
};
