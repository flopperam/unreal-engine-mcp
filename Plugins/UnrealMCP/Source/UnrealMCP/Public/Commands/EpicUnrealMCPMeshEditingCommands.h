#pragma once

#include "CoreMinimal.h"
#include "Json.h"

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

	// Geometry Script / Modeling
	TSharedPtr<FJsonObject> HandleMeshBoolean(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshRemesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshSimplify(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleMeshUVUnwrap(const TSharedPtr<FJsonObject>& Params);

	// Helper
	class UStaticMesh* GetStaticMeshFromParams(const TSharedPtr<FJsonObject>& Params, FString& OutError) const;
};
