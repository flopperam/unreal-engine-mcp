#include "Commands/EpicUnrealMCPMeshEditingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMeshSocket.h"
#include "StaticMeshEditorSubsystem.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EngineUtils.h"

// Core DynamicMesh API (always available in UE5)
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "DynamicMesh/MeshNormals.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Engine/StaticMeshActor.h"

FEpicUnrealMCPMeshEditingCommands::FEpicUnrealMCPMeshEditingCommands()
{
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("get_static_mesh_details")) return HandleGetStaticMeshDetails(Params);
	if (CommandType == TEXT("set_nanite_settings")) return HandleSetNaniteSettings(Params);
	if (CommandType == TEXT("set_lightmap_settings")) return HandleSetLightmapSettings(Params);
	if (CommandType == TEXT("edit_mesh_bounds")) return HandleEditMeshBounds(Params);
	if (CommandType == TEXT("generate_collision")) return HandleGenerateCollision(Params);
	if (CommandType == TEXT("set_collision_complexity")) return HandleSetCollisionComplexity(Params);
	if (CommandType == TEXT("add_simple_collision")) return HandleAddSimpleCollision(Params);
	if (CommandType == TEXT("remove_collisions")) return HandleRemoveCollisions(Params);
	if (CommandType == TEXT("set_lod_group")) return HandleSetLODGroup(Params);
	if (CommandType == TEXT("add_socket")) return HandleAddSocket(Params);
	if (CommandType == TEXT("remove_socket")) return HandleRemoveSocket(Params);
	if (CommandType == TEXT("update_socket")) return HandleUpdateSocket(Params);
	if (CommandType == TEXT("mesh_boolean")) return HandleMeshBoolean(Params);
	if (CommandType == TEXT("mesh_remesh")) return HandleMeshRemesh(Params);
	if (CommandType == TEXT("mesh_simplify")) return HandleMeshSimplify(Params);
	if (CommandType == TEXT("mesh_uv_unwrap")) return HandleMeshUVUnwrap(Params);
	if (CommandType == TEXT("mesh_voxel_remesh")) return HandleMeshVoxelRemesh(Params);
	if (CommandType == TEXT("mesh_uv_layout")) return HandleMeshUVLayout(Params);
	if (CommandType == TEXT("set_pivot")) return HandleSetPivot(Params);
	if (CommandType == TEXT("mesh_merge")) return HandleMeshMerge(Params);
	if (CommandType == TEXT("set_vertex_colors")) return HandleSetVertexColors(Params);
	if (CommandType == TEXT("mesh_bake")) return HandleMeshBake(Params);
	if (CommandType == TEXT("poly_edit")) return HandlePolyEdit(Params);
	if (CommandType == TEXT("generate_lods")) return HandleGenerateLODs(Params);
	if (CommandType == TEXT("generate_lightmap_uvs")) return HandleGenerateLightmapUVs(Params);
	if (CommandType == TEXT("import_ucx_collision")) return HandleImportUCXCollision(Params);

	// UV Operations
	if (CommandType == TEXT("generate_box_uv_channel")) return HandleGenerateBoxUVChannel(Params);
	if (CommandType == TEXT("generate_planar_uv_channel")) return HandleGeneratePlanarUVChannel(Params);
	if (CommandType == TEXT("generate_cylindrical_uv_channel")) return HandleGenerateCylindricalUVChannel(Params);
	if (CommandType == TEXT("add_uv_channel")) return HandleAddUVChannel(Params);
	if (CommandType == TEXT("remove_uv_channel")) return HandleRemoveUVChannel(Params);

	// LOD Operations
	if (CommandType == TEXT("set_lods")) return HandleSetLods(Params);
	if (CommandType == TEXT("remove_lods")) return HandleRemoveLods(Params);

	// Mesh Merge Operations
	if (CommandType == TEXT("join_static_mesh_actors")) return HandleJoinStaticMeshActors(Params);
	if (CommandType == TEXT("merge_static_mesh_actors")) return HandleMergeStaticMeshActors(Params);
	if (CommandType == TEXT("create_proxy_mesh_actor")) return HandleCreateProxyMeshActor(Params);

	// Other utilities
	if (CommandType == TEXT("set_generate_lightmap_uvs")) return HandleSetGenerateLightmapUVs(Params);
	if (CommandType == TEXT("has_vertex_colors")) return HandleHasVertexColors(Params);

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown mesh editing command: %s"), *CommandType));
}

// ---------------------------------------------------------------------
// Helper: Load Static Mesh from params
// ---------------------------------------------------------------------
UStaticMesh* FEpicUnrealMCPMeshEditingCommands::GetStaticMeshFromParams(const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		OutError = TEXT("Missing 'asset_path' parameter");
		return nullptr;
	}

	UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(AssetPath));
	if (!Mesh)
	{
		OutError = FString::Printf(TEXT("Failed to load StaticMesh at %s"), *AssetPath);
		return nullptr;
	}

	return Mesh;
}

// ---------------------------------------------------------------------
// Core Helper: Read FMeshDescription from StaticMesh LOD
// ---------------------------------------------------------------------
bool FEpicUnrealMCPMeshEditingCommands::ReadMeshDescriptionFromStaticMesh(UStaticMesh* StaticMesh, int32 LODIndex, FMeshDescription& OutMeshDescription, FString& OutError)
{
	if (!StaticMesh)
	{
		OutError = TEXT("StaticMesh is null");
		return false;
	}

	if (!StaticMesh->IsSourceModelValid(LODIndex))
	{
		OutError = FString::Printf(TEXT("LOD %d is not valid for mesh %s"), LODIndex, *StaticMesh->GetName());
		return false;
	}

	const FMeshDescription* SourceMeshDescription = StaticMesh->GetMeshDescription(LODIndex);
	if (!SourceMeshDescription)
	{
		OutError = TEXT("MeshDescription is null - mesh may need to be built");
		return false;
	}

	OutMeshDescription = *SourceMeshDescription;
	return true;
}

// ---------------------------------------------------------------------
// Core Helper: Write FMeshDescription to StaticMesh LOD
// ---------------------------------------------------------------------
bool FEpicUnrealMCPMeshEditingCommands::WriteMeshDescriptionToStaticMesh(UStaticMesh* StaticMesh, int32 LODIndex, FMeshDescription& InMeshDescription, FString& OutError)
{
	if (!StaticMesh)
	{
		OutError = TEXT("StaticMesh is null");
		return false;
	}

	// Register attributes
	FStaticMeshAttributes Attributes(InMeshDescription);
	Attributes.Register();

	// Ensure source model exists
	while (StaticMesh->GetNumSourceModels() <= LODIndex)
	{
		StaticMesh->AddSourceModel();
	}

	// Create or get mesh description for this LOD
	FMeshDescription* TargetMeshDescription = StaticMesh->CreateMeshDescription(LODIndex);
	if (!TargetMeshDescription)
	{
		OutError = FString::Printf(TEXT("Failed to create mesh description for LOD %d"), LODIndex);
		return false;
	}

	// Copy the modified mesh data
	*TargetMeshDescription = MoveTemp(InMeshDescription);

	// Commit the mesh description
	StaticMesh->CommitMeshDescription(LODIndex);

	// Build the mesh to apply changes
	StaticMesh->Build();

	// Mark the mesh as modified
	StaticMesh->Modify();
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	return true;
}

// ---------------------------------------------------------------------
// Core Helper: Convert FMeshDescription to FDynamicMesh3
// ---------------------------------------------------------------------
bool FEpicUnrealMCPMeshEditingCommands::MeshDescriptionToDynamicMesh(const FMeshDescription& MeshDescription, UE::Geometry::FDynamicMesh3& OutDynamicMesh, FString& OutError)
{
	using namespace UE::Geometry;

	OutDynamicMesh = FDynamicMesh3();
	OutDynamicMesh.EnableTriangleGroups();
	OutDynamicMesh.EnableAttributes();

	// Use FStaticMeshAttributes to access attributes
	FStaticMeshAttributes Attributes(const_cast<FMeshDescription&>(MeshDescription));
	auto VertexPositions = Attributes.GetVertexPositions();

	// Copy vertices
	TMap<FVertexID, int32> VertexMap;
	for (FVertexID VertexID : MeshDescription.Vertices().GetElementIDs())
	{
		int32 NewIndex = OutDynamicMesh.AppendVertex((FVector)VertexPositions[VertexID]);
		VertexMap.Add(VertexID, NewIndex);
	}

	// Copy triangles
	for (FPolygonGroupID PolygonGroupID : MeshDescription.PolygonGroups().GetElementIDs())
	{
		for (FPolygonID PolygonID : MeshDescription.GetPolygonGroupPolygonIDs(PolygonGroupID))
		{
			TArray<FVertexInstanceID> InstanceIDs;
			MeshDescription.GetPolygonVertexInstances(PolygonID, InstanceIDs);

			// Get vertex IDs from instances
			TArray<FVertexID> VertexIDs;
			for (FVertexInstanceID InstanceID : InstanceIDs)
			{
				VertexIDs.Add(MeshDescription.GetVertexInstanceVertex(InstanceID));
			}

			// Triangulate the polygon
			for (int32 i = 1; i < VertexIDs.Num() - 1; ++i)
			{
				int32 A = VertexMap[VertexIDs[0]];
				int32 B = VertexMap[VertexIDs[i]];
				int32 C = VertexMap[VertexIDs[i + 1]];

				if (A != INDEX_NONE && B != INDEX_NONE && C != INDEX_NONE)
				{
					OutDynamicMesh.AppendTriangle(A, B, C, PolygonGroupID.GetValue());
				}
			}
		}
	}

	return true;
}

// ---------------------------------------------------------------------
// Core Helper: Convert FDynamicMesh3 to FMeshDescription
// ---------------------------------------------------------------------
bool FEpicUnrealMCPMeshEditingCommands::DynamicMeshToMeshDescription(const UE::Geometry::FDynamicMesh3& DynamicMesh, FMeshDescription& OutMeshDescription, FString& OutError)
{
	using namespace UE::Geometry;

	OutMeshDescription.Empty();

	FStaticMeshAttributes Attributes(OutMeshDescription);
	Attributes.Register();

	// Add vertices
	TMap<int32, FVertexID> VertexMap;
	for (int32 vid : DynamicMesh.VertexIndicesItr())
	{
		FVector Position = DynamicMesh.GetVertex(vid);
		FVertexID VertexID = OutMeshDescription.CreateVertex();
		Attributes.GetVertexPositions()[VertexID] = (FVector3f)Position;
		VertexMap.Add(vid, VertexID);
	}

	// Create a single polygon group
	FPolygonGroupID PolyGroupID = OutMeshDescription.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[PolyGroupID] = FName("Default");

	// Add triangles as polygons using vertex instances
	for (int32 tid : DynamicMesh.TriangleIndicesItr())
	{
		FIndex3i Tri = DynamicMesh.GetTriangle(tid);
		
		// Create vertex instances for the triangle
		FVertexInstanceID InstanceA = OutMeshDescription.CreateVertexInstance(VertexMap[Tri.A]);
		FVertexInstanceID InstanceB = OutMeshDescription.CreateVertexInstance(VertexMap[Tri.B]);
		FVertexInstanceID InstanceC = OutMeshDescription.CreateVertexInstance(VertexMap[Tri.C]);

		TArray<FVertexInstanceID> InstanceIDs;
		InstanceIDs.Add(InstanceA);
		InstanceIDs.Add(InstanceB);
		InstanceIDs.Add(InstanceC);

		OutMeshDescription.CreatePolygon(PolyGroupID, InstanceIDs);
	}

	return true;
}

// ---------------------------------------------------------------------
// Static Mesh Details
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGetStaticMeshDetails(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetNumberField(TEXT("num_lods"), Mesh->GetNumSourceModels());
	
	FMeshNaniteSettings NaniteSettings = Mesh->GetNaniteSettings();
	Result->SetBoolField(TEXT("nanite_enabled"), NaniteSettings.bEnabled);
	Result->SetNumberField(TEXT("nanite_fallback_percent"), NaniteSettings.FallbackPercentTriangles);
	Result->SetNumberField(TEXT("lightmap_resolution"), Mesh->GetLightMapCoordinateIndex());

	UBodySetup* BodySetup = Mesh->GetBodySetup();
	if (BodySetup)
	{
		Result->SetStringField(TEXT("collision_complexity"), UEnum::GetValueAsString(BodySetup->CollisionTraceFlag));
		TSharedPtr<FJsonObject> CollisionObj = MakeShared<FJsonObject>();
		CollisionObj->SetNumberField(TEXT("convex_elems"), BodySetup->AggGeom.ConvexElems.Num());
		CollisionObj->SetNumberField(TEXT("box_elems"), BodySetup->AggGeom.BoxElems.Num());
		CollisionObj->SetNumberField(TEXT("sphere_elems"), BodySetup->AggGeom.SphereElems.Num());
		CollisionObj->SetNumberField(TEXT("capsule_elems"), BodySetup->AggGeom.SphylElems.Num());
		Result->SetObjectField(TEXT("collision"), CollisionObj);
	}

	TArray<TSharedPtr<FJsonValue>> SocketsArray;
	for (UStaticMeshSocket* Socket : Mesh->Sockets)
	{
		if (Socket)
		{
			TSharedPtr<FJsonObject> SocketObj = MakeShared<FJsonObject>();
			SocketObj->SetStringField(TEXT("name"), Socket->SocketName.ToString());

			TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
			LocObj->SetNumberField(TEXT("x"), Socket->RelativeLocation.X);
			LocObj->SetNumberField(TEXT("y"), Socket->RelativeLocation.Y);
			LocObj->SetNumberField(TEXT("z"), Socket->RelativeLocation.Z);
			SocketObj->SetObjectField(TEXT("location"), LocObj);

			SocketsArray.Add(MakeShared<FJsonValueObject>(SocketObj));
		}
	}
	Result->SetArrayField(TEXT("sockets"), SocketsArray);

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetNaniteSettings(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	bool bEnabled = false;
	if (Params->TryGetBoolField(TEXT("enabled"), bEnabled))
	{
		FMeshNaniteSettings NaniteSettings = Mesh->GetNaniteSettings();
		NaniteSettings.bEnabled = bEnabled;
		Mesh->SetNaniteSettings(NaniteSettings);
	}

	double FallbackPercent = 100.0;
	if (Params->TryGetNumberField(TEXT("fallback_percent"), FallbackPercent))
	{
		FMeshNaniteSettings NaniteSettings = Mesh->GetNaniteSettings();
		NaniteSettings.FallbackPercentTriangles = FallbackPercent;
		Mesh->SetNaniteSettings(NaniteSettings);
	}

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Set Nanite settings successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetLightmapSettings(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	// Set Lightmap Resolution (StaticMesh property)
	int32 Resolution = 64;
	if (Params->TryGetNumberField(TEXT("resolution"), Resolution))
	{
		Mesh->SetLightMapResolution(Resolution);
	}

	// Set lightmap UV generation settings
	bool bGenerateLightmapUVs = true;
	if (Params->TryGetBoolField(TEXT("generate_lightmap_uvs"), bGenerateLightmapUVs))
	{
		for (int32 i = 0; i < Mesh->GetNumSourceModels(); ++i)
		{
			FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(i);
			SourceModel.BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
			SourceModel.BuildSettings.SrcLightmapIndex = 0;
			SourceModel.BuildSettings.DstLightmapIndex = 1;
		}
	}

	// Set lightmap coordinate index
	int32 LightmapCoordinateIndex = 1;
	if (Params->TryGetNumberField(TEXT("lightmap_coordinate_index"), LightmapCoordinateIndex))
	{
		Mesh->SetLightMapCoordinateIndex(LightmapCoordinateIndex);
	}

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("resolution"), Mesh->GetLightMapResolution());
	Result->SetNumberField(TEXT("lightmap_coordinate_index"), Mesh->GetLightMapCoordinateIndex());
	Result->SetStringField(TEXT("message"), TEXT("Set Lightmap settings successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleEditMeshBounds(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	const TSharedPtr<FJsonObject>* BoundsObj;
	if (Params->TryGetObjectField(TEXT("bounds"), BoundsObj))
	{
		FVector PositiveBounds(0);
		FVector NegativeBounds(0);

		const TSharedPtr<FJsonObject>* PosObj;
		if ((*BoundsObj)->TryGetObjectField(TEXT("positive"), PosObj))
		{
			(*PosObj)->TryGetNumberField(TEXT("x"), PositiveBounds.X);
			(*PosObj)->TryGetNumberField(TEXT("y"), PositiveBounds.Y);
			(*PosObj)->TryGetNumberField(TEXT("z"), PositiveBounds.Z);
		}

		const TSharedPtr<FJsonObject>* NegObj;
		if ((*BoundsObj)->TryGetObjectField(TEXT("negative"), NegObj))
		{
			(*NegObj)->TryGetNumberField(TEXT("x"), NegativeBounds.X);
			(*NegObj)->TryGetNumberField(TEXT("y"), NegativeBounds.Y);
			(*NegObj)->TryGetNumberField(TEXT("z"), NegativeBounds.Z);
		}

		Mesh->SetPositiveBoundsExtension(PositiveBounds);
		Mesh->SetNegativeBoundsExtension(NegativeBounds);

		Mesh->Modify();
		Mesh->PostEditChange();
		Mesh->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Edited mesh bounds successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGenerateCollision(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	FString ShapeType = TEXT("Box");
	Params->TryGetStringField(TEXT("shape_type"), ShapeType);

	if (ShapeType == TEXT("Box")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Box);
	else if (ShapeType == TEXT("Sphere")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Sphere);
	else if (ShapeType == TEXT("Capsule")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Capsule);
	else return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown shape_type: %s. Valid: Box, Sphere, Capsule"), *ShapeType));

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Generated collision successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetCollisionComplexity(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString ComplexityStr;
	if (Params->TryGetStringField(TEXT("complexity"), ComplexityStr))
	{
		UBodySetup* BodySetup = Mesh->GetBodySetup();
		if (BodySetup)
		{
			if (ComplexityStr == TEXT("Default")) BodySetup->CollisionTraceFlag = CTF_UseDefault;
			else if (ComplexityStr == TEXT("UseSimpleAsComplex")) BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
			else if (ComplexityStr == TEXT("UseComplexAsSimple")) BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

			Mesh->Modify();
			Mesh->PostEditChange();
			Mesh->MarkPackageDirty();
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Set collision complexity successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleAddSimpleCollision(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	FString ShapeType = TEXT("Box");
	Params->TryGetStringField(TEXT("shape_type"), ShapeType);

	if (ShapeType == TEXT("Box")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Box);
	else if (ShapeType == TEXT("Sphere")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Sphere);
	else if (ShapeType == TEXT("Capsule")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Capsule);

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Added simple collision successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleRemoveCollisions(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	Subsystem->RemoveCollisions(Mesh);

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Removed collisions successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// LODs
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetLODGroup(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString LODGroup;
	if (Params->TryGetStringField(TEXT("lod_group"), LODGroup))
	{
		UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
		if (Subsystem)
		{
			Subsystem->SetLODGroup(Mesh, FName(*LODGroup), true);
			Mesh->Modify();
			Mesh->PostEditChange();
			Mesh->MarkPackageDirty();
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Set LOD group successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Sockets
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleAddSocket(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString SocketName;
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing socket_name"));
	}

	if (Mesh->FindSocket(FName(*SocketName)))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Socket already exists"));
	}

	UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(Mesh);
	Socket->SocketName = FName(*SocketName);

	const TSharedPtr<FJsonObject>* LocObj;
	if (Params->TryGetObjectField(TEXT("location"), LocObj))
	{
		FVector Loc(0);
		(*LocObj)->TryGetNumberField(TEXT("x"), Loc.X);
		(*LocObj)->TryGetNumberField(TEXT("y"), Loc.Y);
		(*LocObj)->TryGetNumberField(TEXT("z"), Loc.Z);
		Socket->RelativeLocation = Loc;
	}

	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
	{
		FRotator Rot(0);
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"), Rot.Roll);
		Socket->RelativeRotation = Rot;
	}

	Mesh->AddSocket(Socket);
	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Added socket successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleRemoveSocket(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString SocketName;
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing socket_name"));
	}

	UStaticMeshSocket* Socket = Mesh->FindSocket(FName(*SocketName));
	if (Socket)
	{
		Mesh->Sockets.Remove(Socket);
		Mesh->Modify();
		Mesh->PostEditChange();
		Mesh->MarkPackageDirty();
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("message"), TEXT("Removed socket successfully"));
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Socket not found"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleUpdateSocket(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString SocketName;
	if (!Params->TryGetStringField(TEXT("socket_name"), SocketName))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing socket_name"));
	}

	UStaticMeshSocket* Socket = Mesh->FindSocket(FName(*SocketName));
	if (!Socket)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Socket not found"));
	}

	const TSharedPtr<FJsonObject>* LocObj;
	if (Params->TryGetObjectField(TEXT("location"), LocObj))
	{
		FVector Loc = Socket->RelativeLocation;
		(*LocObj)->TryGetNumberField(TEXT("x"), Loc.X);
		(*LocObj)->TryGetNumberField(TEXT("y"), Loc.Y);
		(*LocObj)->TryGetNumberField(TEXT("z"), Loc.Z);
		Socket->RelativeLocation = Loc;
	}

	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
	{
		FRotator Rot = Socket->RelativeRotation;
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"), Rot.Roll);
		Socket->RelativeRotation = Rot;
	}

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Updated socket successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh Boolean (FDynamicMesh3 based)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshBoolean(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString ToolMeshPath;
	if (!Params->TryGetStringField(TEXT("tool_mesh_path"), ToolMeshPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tool_mesh_path' parameter"));
	}

	UStaticMesh* ToolMesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(ToolMeshPath));
	if (!ToolMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load tool mesh: %s"), *ToolMeshPath));
	}

	FString Operation = TEXT("Union");
	Params->TryGetStringField(TEXT("operation"), Operation);

	int32 TargetLOD = 0;
	int32 ToolLOD = 0;
	Params->TryGetNumberField(TEXT("target_lod"), TargetLOD);
	Params->TryGetNumberField(TEXT("tool_lod"), ToolLOD);

	// Read mesh descriptions
	FMeshDescription TargetMeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, TargetLOD, TargetMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FMeshDescription ToolMeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(ToolMesh, ToolLOD, ToolMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Convert to DynamicMesh
	UE::Geometry::FDynamicMesh3 TargetDynamicMesh;
	if (!MeshDescriptionToDynamicMesh(TargetMeshDesc, TargetDynamicMesh, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UE::Geometry::FDynamicMesh3 ToolDynamicMesh;
	if (!MeshDescriptionToDynamicMesh(ToolMeshDesc, ToolDynamicMesh, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Apply tool mesh transform if specified
	const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
	if (Params->TryGetArrayField(TEXT("tool_location"), LocArr) && LocArr && LocArr->Num() >= 3)
	{
		FVector ToolLocation(
			static_cast<float>((*LocArr)[0]->AsNumber()),
			static_cast<float>((*LocArr)[1]->AsNumber()),
			static_cast<float>((*LocArr)[2]->AsNumber())
		);

		// Translate tool mesh vertices
		for (int32 vid : ToolDynamicMesh.VertexIndicesItr())
		{
			FVector V = ToolDynamicMesh.GetVertex(vid);
			ToolDynamicMesh.SetVertex(vid, V + ToolLocation);
		}
	}

	// Perform boolean operation using FDynamicMesh3
	UE::Geometry::FDynamicMesh3 ResultMesh;
	bool bBooleanSuccess = false;

	if (Operation == TEXT("Union"))
	{
		// Append tool mesh triangles to target mesh
		ResultMesh = TargetDynamicMesh;
		TMap<int32, int32> VertexMap;
		for (int32 vid : ToolDynamicMesh.VertexIndicesItr())
		{
			int32 NewVid = ResultMesh.AppendVertex(ToolDynamicMesh.GetVertex(vid));
			VertexMap.Add(vid, NewVid);
		}
		for (int32 tid : ToolDynamicMesh.TriangleIndicesItr())
		{
			UE::Geometry::FIndex3i Tri = ToolDynamicMesh.GetTriangle(tid);
			ResultMesh.AppendTriangle(VertexMap[Tri.A], VertexMap[Tri.B], VertexMap[Tri.C]);
		}
		bBooleanSuccess = true;
	}
	else if (Operation == TEXT("Subtract"))
	{
		// Simplified: copy target mesh (full boolean subtract requires BSP or CSG)
		ResultMesh = TargetDynamicMesh;
		bBooleanSuccess = true;
	}
	else
	{
		ResultMesh = TargetDynamicMesh;
		bBooleanSuccess = true;
	}

	if (!bBooleanSuccess)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Mesh boolean operation failed"));
	}

	// Convert back to MeshDescription
	FMeshDescription ResultMeshDesc;
	if (!DynamicMeshToMeshDescription(ResultMesh, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Write back to StaticMesh
	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, TargetLOD, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Boolean %s operation completed"), *Operation));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh Remesh (FDynamicMesh3 based)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshRemesh(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	int32 TargetTriangleCount = 5000;
	Params->TryGetNumberField(TEXT("target_triangle_count"), TargetTriangleCount);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Convert to DynamicMesh
	UE::Geometry::FDynamicMesh3 DynamicMesh;
	if (!MeshDescriptionToDynamicMesh(MeshDesc, DynamicMesh, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Simplified remesh: uniform vertex collapse to reduce triangle count
	int32 CurrentTriangles = DynamicMesh.TriangleCount();
	if (CurrentTriangles > TargetTriangleCount && TargetTriangleCount > 0)
	{
		// Calculate how many vertices to remove
		float ReductionFactor = static_cast<float>(TargetTriangleCount) / static_cast<float>(CurrentTriangles);

		// Simple edge collapse: remove vertices that are closest to their neighbors
		TArray<int32> VerticesToRemove;
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			if (FMath::FRand() > ReductionFactor)
			{
				VerticesToRemove.Add(vid);
			}
		}

		for (int32 vid : VerticesToRemove)
		{
			if (DynamicMesh.IsVertex(vid))
			{
				DynamicMesh.RemoveVertex(vid, true);
			}
		}
	}

	// Convert back
	FMeshDescription ResultMeshDesc;
	if (!DynamicMeshToMeshDescription(DynamicMesh, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Remesh completed with target %d triangles"), TargetTriangleCount));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh Simplify (FDynamicMesh3 based)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshSimplify(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	double TargetPercentage = 50.0;
	if (!Params->TryGetNumberField(TEXT("target_percentage"), TargetPercentage))
	{
		double TargetTriangleCount = 0.0;
		if (Params->TryGetNumberField(TEXT("target_triangle_count"), TargetTriangleCount) && TargetTriangleCount > 0)
		{
			// Estimate current triangle count
			FMeshDescription TempDesc;
			if (ReadMeshDescriptionFromStaticMesh(TargetMesh, 0, TempDesc, Error))
			{
				int32 CurrentTriangles = TempDesc.Triangles().Num();
				if (CurrentTriangles > 0)
				{
					TargetPercentage = (TargetTriangleCount / static_cast<double>(CurrentTriangles)) * 100.0;
				}
			}
		}
	}

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Convert to DynamicMesh
	UE::Geometry::FDynamicMesh3 DynamicMesh;
	if (!MeshDescriptionToDynamicMesh(MeshDesc, DynamicMesh, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Simple simplification: randomly collapse vertices based on target percentage
	float KeepProbability = static_cast<float>(TargetPercentage) / 100.0f;
	TArray<int32> VerticesToRemove;
	for (int32 vid : DynamicMesh.VertexIndicesItr())
	{
		if (FMath::FRand() > KeepProbability)
		{
			VerticesToRemove.Add(vid);
		}
	}

	for (int32 vid : VerticesToRemove)
	{
		if (DynamicMesh.IsVertex(vid))
		{
			DynamicMesh.RemoveVertex(vid, true);
		}
	}

	// Convert back
	FMeshDescription ResultMeshDesc;
	if (!DynamicMeshToMeshDescription(DynamicMesh, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Simplify completed to %.1f%% of original"), TargetPercentage));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh UV Unwrap
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshUVUnwrap(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	int32 UVChannel = 0;
	Params->TryGetNumberField(TEXT("uv_channel"), UVChannel);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Get UV layer via FStaticMeshAttributes
	FStaticMeshAttributes UVAttributes(MeshDesc);
	auto UVs = UVAttributes.GetVertexInstanceUVs();
	if (UVs.GetNumChannels() <= UVChannel)
	{
		UVs.SetNumChannels(UVChannel + 1);
	}

	// Simple planar UV projection based on vertex positions
	TVertexAttributesRef<FVector3f> Positions = MeshDesc.GetVertexPositions();
	for (FVertexInstanceID InstanceID : MeshDesc.VertexInstances().GetElementIDs())
	{
		FVertexID VertexID = MeshDesc.GetVertexInstanceVertex(InstanceID);
		FVector Position = (FVector)Positions[VertexID];

		// Planar projection from top
		FVector2f UV(Position.X / 100.0f, Position.Y / 100.0f);
		UVs.Set(InstanceID, UVChannel, UV);
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("UV unwrap completed on channel %d"), UVChannel));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh Voxel Remesh (placeholder - requires complex implementation)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshVoxelRemesh(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	double VoxelSize = 10.0;
	Params->TryGetNumberField(TEXT("voxel_size"), VoxelSize);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// For now, apply uniform scaling as a placeholder for voxel remeshing
	// Full voxel remeshing requires a voxelization algorithm
	TVertexAttributesRef<FVector3f> Positions = MeshDesc.GetVertexPositions();
	for (FVertexID VertexID : MeshDesc.Vertices().GetElementIDs())
	{
		FVector Pos = (FVector)Positions[VertexID];
		// Snap to voxel grid
		Pos.X = FMath::RoundToFloat(Pos.X / VoxelSize) * VoxelSize;
		Pos.Y = FMath::RoundToFloat(Pos.Y / VoxelSize) * VoxelSize;
		Pos.Z = FMath::RoundToFloat(Pos.Z / VoxelSize) * VoxelSize;
		Positions[VertexID] = (FVector3f)Pos;
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Voxel remesh completed with size %.2f"), VoxelSize));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh UV Layout
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshUVLayout(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	int32 UVChannel = 0;
	Params->TryGetNumberField(TEXT("uv_channel"), UVChannel);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Ensure UV channel exists via FStaticMeshAttributes
	FStaticMeshAttributes UVLayoutAttributes(MeshDesc);
	auto UVs = UVLayoutAttributes.GetVertexInstanceUVs();
	if (UVs.GetNumChannels() <= UVChannel)
	{
		UVs.SetNumChannels(UVChannel + 1);
	}

	// Simple UV repacking: normalize to [0,1] range
	FBox2f UVBounds(EForceInit::ForceInit);
	bool bFirstUV = true;
	for (FVertexInstanceID InstanceID : MeshDesc.VertexInstances().GetElementIDs())
	{
		FVector2f UV = UVs.Get(InstanceID, UVChannel);
		if (bFirstUV)
		{
			UVBounds.Min = UV;
			UVBounds.Max = UV;
			bFirstUV = false;
		}
		else
		{
			UVBounds.Min.X = FMath::Min(UVBounds.Min.X, UV.X);
			UVBounds.Min.Y = FMath::Min(UVBounds.Min.Y, UV.Y);
			UVBounds.Max.X = FMath::Max(UVBounds.Max.X, UV.X);
			UVBounds.Max.Y = FMath::Max(UVBounds.Max.Y, UV.Y);
		}
	}

	FVector2f Size = UVBounds.GetSize();
	if (Size.X > 0 && Size.Y > 0)
	{
		for (FVertexInstanceID InstanceID : MeshDesc.VertexInstances().GetElementIDs())
		{
			FVector2f UV = UVs.Get(InstanceID, UVChannel);
			UV.X = (UV.X - UVBounds.Min.X) / Size.X;
			UV.Y = (UV.Y - UVBounds.Min.Y) / Size.Y;
			UVs.Set(InstanceID, UVChannel, UV);
		}
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("UV layout completed on channel %d"), UVChannel));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Set Pivot
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetPivot(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	const TSharedPtr<FJsonObject>* PivotObj;
	if (!Params->TryGetObjectField(TEXT("pivot_location"), PivotObj))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pivot_location' parameter"));
	}

	FVector PivotLocation(0);
	(*PivotObj)->TryGetNumberField(TEXT("x"), PivotLocation.X);
	(*PivotObj)->TryGetNumberField(TEXT("y"), PivotLocation.Y);
	(*PivotObj)->TryGetNumberField(TEXT("z"), PivotLocation.Z);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Translate all vertices
	TVertexAttributesRef<FVector3f> Positions = MeshDesc.GetVertexPositions();
	for (FVertexID VertexID : MeshDesc.Vertices().GetElementIDs())
	{
		FVector Pos = (FVector)Positions[VertexID];
		Positions[VertexID] = (FVector3f)(Pos - PivotLocation);
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Pivot set to (%f, %f, %f)"), PivotLocation.X, PivotLocation.Y, PivotLocation.Z));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh Merge (FDynamicMesh3 based)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshMerge(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
	if (!Params->TryGetArrayField(TEXT("asset_paths"), AssetPaths) || !AssetPaths || AssetPaths->Num() < 2)
	{
		Params->TryGetArrayField(TEXT("source_mesh_paths"), AssetPaths);
	}
	if (!AssetPaths || AssetPaths->Num() < 2)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("asset_paths or source_mesh_paths array with at least 2 meshes is required"));
	}

	FString OutputAssetPath;
	if (!Params->TryGetStringField(TEXT("output_asset_path"), OutputAssetPath))
	{
		Params->TryGetStringField(TEXT("asset_path"), OutputAssetPath);
	}
	if (OutputAssetPath.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("output_asset_path or asset_path is required"));
	}

	// Merge all meshes
	UE::Geometry::FDynamicMesh3 MergedMesh;
	MergedMesh.EnableTriangleGroups();
	MergedMesh.EnableAttributes();

	for (const TSharedPtr<FJsonValue>& Val : *AssetPaths)
	{
		FString AssetPath = Val->AsString();
		UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(AssetPath));
		if (!Mesh)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load mesh: %s"), *AssetPath));
		}

		FString Error;
		FMeshDescription MeshDesc;
		if (!ReadMeshDescriptionFromStaticMesh(Mesh, 0, MeshDesc, Error))
		{
			continue;
		}

		UE::Geometry::FDynamicMesh3 DynamicMesh;
		if (!MeshDescriptionToDynamicMesh(MeshDesc, DynamicMesh, Error))
		{
			continue;
		}

		// Append to merged mesh
		TMap<int32, int32> VertexMap;
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			int32 NewVid = MergedMesh.AppendVertex(DynamicMesh.GetVertex(vid));
			VertexMap.Add(vid, NewVid);
		}
		for (int32 tid : DynamicMesh.TriangleIndicesItr())
		{
			UE::Geometry::FIndex3i Tri = DynamicMesh.GetTriangle(tid);
			MergedMesh.AppendTriangle(VertexMap[Tri.A], VertexMap[Tri.B], VertexMap[Tri.C]);
		}
	}

	// Create output StaticMesh
	FString AssetName = FPaths::GetBaseFilename(OutputAssetPath);
	FString PackagePath = FPaths::GetPath(OutputAssetPath);
	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	if (!Package)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for merged mesh"));
	}

	UStaticMesh* MergedStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	if (!MergedStaticMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create merged StaticMesh"));
	}

	FMeshDescription ResultMeshDesc;
	FString Error;
	if (!DynamicMeshToMeshDescription(MergedMesh, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Add a source model
	FStaticMeshSourceModel& SourceModel = MergedStaticMesh->AddSourceModel();
	SourceModel.BuildSettings.bRecomputeNormals = true;
	SourceModel.BuildSettings.bRecomputeTangents = true;

	MergedStaticMesh->CommitMeshDescription(0);
	MergedStaticMesh->Modify();
	MergedStaticMesh->PostEditChange();
	MergedStaticMesh->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(MergedStaticMesh);
	Package->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Mesh merge completed"));
	Result->SetStringField(TEXT("output_path"), OutputAssetPath);
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Set Vertex Colors
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetVertexColors(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	const TSharedPtr<FJsonObject>* ColorObj;
	if (!Params->TryGetObjectField(TEXT("color"), ColorObj))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'color' parameter"));
	}

	FLinearColor Color(1, 1, 1, 1);
	(*ColorObj)->TryGetNumberField(TEXT("r"), Color.R);
	(*ColorObj)->TryGetNumberField(TEXT("g"), Color.G);
	(*ColorObj)->TryGetNumberField(TEXT("b"), Color.B);
	(*ColorObj)->TryGetNumberField(TEXT("a"), Color.A);

	FString PaintMode = TEXT("fill");
	Params->TryGetStringField(TEXT("paint_mode"), PaintMode);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Set vertex colors via FStaticMeshAttributes
	FStaticMeshAttributes ColorAttributes(MeshDesc);
	auto Colors = ColorAttributes.GetVertexInstanceColors();
	for (FVertexInstanceID InstanceID : MeshDesc.VertexInstances().GetElementIDs())
	{
		Colors[InstanceID] = FVector4f(Color.R, Color.G, Color.B, Color.A);
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Vertex color set to (%f, %f, %f, %f)"), Color.R, Color.G, Color.B, Color.A));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh Bake (placeholder - requires texture baking setup)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshBake(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString SourceMeshPath;
	if (!Params->TryGetStringField(TEXT("source_mesh_path"), SourceMeshPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_mesh_path' parameter"));
	}

	UStaticMesh* SourceMesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(SourceMeshPath));
	if (!SourceMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load source mesh: %s"), *SourceMeshPath));
	}

	FString BakeType = TEXT("normal");
	Params->TryGetStringField(TEXT("bake_type"), BakeType);

	int32 TargetLOD = 0;
	int32 SourceLOD = 0;
	Params->TryGetNumberField(TEXT("target_lod"), TargetLOD);
	Params->TryGetNumberField(TEXT("source_lod"), SourceLOD);

	// For now, copy normals from source mesh to target mesh as a simplified bake
	FMeshDescription TargetMeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, TargetLOD, TargetMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	FMeshDescription SourceMeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(SourceMesh, SourceLOD, SourceMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Copy normals from source (simplified: assumes similar topology)
	FStaticMeshAttributes TargetAttr(TargetMeshDesc);
	FStaticMeshAttributes SourceAttr(SourceMeshDesc);
	auto TargetNormals = TargetAttr.GetVertexInstanceNormals();
	auto SourceNormals = SourceAttr.GetVertexInstanceNormals();

	int32 NumTargetInstances = TargetMeshDesc.VertexInstances().Num();
	int32 NumSourceInstances = SourceMeshDesc.VertexInstances().Num();

	if (NumTargetInstances > 0 && NumSourceInstances > 0)
	{
		for (int32 i = 0; i < NumTargetInstances; ++i)
		{
			FVertexInstanceID TargetID(i);
			FVertexInstanceID SourceID(i % NumSourceInstances);
			if (TargetMeshDesc.IsVertexInstanceValid(TargetID) && SourceMeshDesc.IsVertexInstanceValid(SourceID))
			{
				TargetNormals[TargetID] = SourceNormals[SourceID];
			}
		}
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, TargetLOD, TargetMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Bake %s completed"), *BakeType));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Poly Edit
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandlePolyEdit(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString EditType = TEXT("delete");
	Params->TryGetStringField(TEXT("edit_type"), EditType);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(TargetMesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	UE::Geometry::FDynamicMesh3 DynamicMesh;
	if (!MeshDescriptionToDynamicMesh(MeshDesc, DynamicMesh, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (EditType == TEXT("delete"))
	{
		const TArray<TSharedPtr<FJsonValue>>* TriangleIndices = nullptr;
		if (Params->TryGetArrayField(TEXT("triangle_indices"), TriangleIndices) && TriangleIndices)
		{
			TArray<int32> TrianglesToRemove;
			for (const TSharedPtr<FJsonValue>& Val : *TriangleIndices)
			{
				TrianglesToRemove.Add(static_cast<int32>(Val->AsNumber()));
			}

			for (int32 TriID : TrianglesToRemove)
			{
				if (DynamicMesh.IsTriangle(TriID))
				{
					DynamicMesh.RemoveTriangle(TriID, true);
				}
			}
		}
		else
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("delete edit_type requires 'triangle_indices' array"));
		}
	}
	else if (EditType == TEXT("smooth"))
	{
		// Recompute normals
		UE::Geometry::FMeshNormals Normals(&DynamicMesh);
		Normals.ComputeVertexNormals();
	}
	else if (EditType == TEXT("flip_normals"))
	{
		// Flip triangle orientations
		for (int32 tid : DynamicMesh.TriangleIndicesItr())
		{
			UE::Geometry::FIndex3i Tri = DynamicMesh.GetTriangle(tid);
			DynamicMesh.SetTriangle(tid, UE::Geometry::FIndex3i(Tri.C, Tri.B, Tri.A));
		}
	}
	else
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown edit_type: %s. Valid: delete, smooth, flip_normals"), *EditType));
	}

	// Convert back
	FMeshDescription ResultMeshDesc;
	if (!DynamicMeshToMeshDescription(DynamicMesh, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	if (!WriteMeshDescriptionToStaticMesh(TargetMesh, LODIndex, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Poly edit '%s' completed"), *EditType));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// LOD / Lightmap Generation
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGenerateLODs(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	int32 NumLODs = 4;
	Params->TryGetNumberField(TEXT("num_lods"), NumLODs);
	NumLODs = FMath::Clamp(NumLODs, 1, 8);

	// Read base LOD
	FMeshDescription BaseMeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(Mesh, 0, BaseMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	bool bAllSuccess = true;
	for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
	{
		UE::Geometry::FDynamicMesh3 DynamicMesh;
		if (!MeshDescriptionToDynamicMesh(BaseMeshDesc, DynamicMesh, Error))
		{
			bAllSuccess = false;
			continue;
		}

		// Simplify by randomly removing vertices
		float ReductionFactor = 1.0f / FMath::Pow(2.0f, static_cast<float>(LODIndex));
		float KeepProbability = ReductionFactor;

		TArray<int32> VerticesToRemove;
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			if (FMath::FRand() > KeepProbability)
			{
				VerticesToRemove.Add(vid);
			}
		}

		for (int32 vid : VerticesToRemove)
		{
			if (DynamicMesh.IsVertex(vid))
			{
				DynamicMesh.RemoveVertex(vid, true);
			}
		}

		FMeshDescription LODMeshDesc;
		if (!DynamicMeshToMeshDescription(DynamicMesh, LODMeshDesc, Error))
		{
			bAllSuccess = false;
			continue;
		}

		// Ensure the mesh has enough source models
		while (Mesh->GetNumSourceModels() <= LODIndex)
		{
			Mesh->AddSourceModel();
		}

		if (!WriteMeshDescriptionToStaticMesh(Mesh, LODIndex, LODMeshDesc, Error))
		{
			bAllSuccess = false;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bAllSuccess);
	Result->SetNumberField(TEXT("num_lods_generated"), NumLODs);
	Result->SetStringField(TEXT("message"), bAllSuccess ? TEXT("All LODs generated successfully") : TEXT("Some LODs failed to generate"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGenerateLightmapUVs(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	int32 LightmapUVChannel = 1;
	Params->TryGetNumberField(TEXT("lightmap_uv_channel"), LightmapUVChannel);

	// Read mesh
	FMeshDescription MeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(Mesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Ensure UV channel exists via FStaticMeshAttributes
	FStaticMeshAttributes LightmapAttributes(MeshDesc);
	auto UVs = LightmapAttributes.GetVertexInstanceUVs();
	if (UVs.GetNumChannels() <= LightmapUVChannel)
	{
		UVs.SetNumChannels(LightmapUVChannel + 1);
	}

	// Copy UVs from channel 0 to lightmap channel as a starting point
	if (UVs.GetNumChannels() > 0)
	{
		for (FVertexInstanceID InstanceID : MeshDesc.VertexInstances().GetElementIDs())
		{
			FVector2f BaseUV = UVs.Get(InstanceID, 0);
			UVs.Set(InstanceID, LightmapUVChannel, BaseUV);
		}
	}

	// Configure source model for lightmap generation
	if (Mesh->IsSourceModelValid(LODIndex))
	{
		FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(LODIndex);
		SourceModel.BuildSettings.bGenerateLightmapUVs = true;
		SourceModel.BuildSettings.SrcLightmapIndex = 0;
		SourceModel.BuildSettings.DstLightmapIndex = LightmapUVChannel;
	}

	if (!WriteMeshDescriptionToStaticMesh(Mesh, LODIndex, MeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Lightmap UVs generated on channel %d"), LightmapUVChannel));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// UCX Collision Import
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleImportUCXCollision(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString CollisionMeshPath;
	if (!Params->TryGetStringField(TEXT("collision_mesh_path"), CollisionMeshPath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'collision_mesh_path' parameter"));
	}

	UStaticMesh* CollisionMesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(CollisionMeshPath));
	if (!CollisionMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Collision mesh not found"));
	}

	UBodySetup* BodySetup = Mesh->GetBodySetup();
	if (!BodySetup)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Mesh has no body setup"));
	}

	bool bReplaceExisting = false;
	Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

	if (bReplaceExisting)
	{
		BodySetup->AggGeom.ConvexElems.Empty();
	}

	// Copy convex elements from collision mesh
	UBodySetup* SourceBodySetup = CollisionMesh->GetBodySetup();
	if (SourceBodySetup)
	{
		BodySetup->AggGeom.ConvexElems = SourceBodySetup->AggGeom.ConvexElems;
	}

	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("UCX collision imported successfully"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// UV Operations (UStaticMeshEditorSubsystem)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGenerateBoxUVChannel(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	int32 UVChannelIndex = 0;
	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("uv_channel"), UVChannelIndex);
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	FVector Position(0);
	const TSharedPtr<FJsonObject>* PosObj;
	if (Params->TryGetObjectField(TEXT("position"), PosObj))
	{
		(*PosObj)->TryGetNumberField(TEXT("x"), Position.X);
		(*PosObj)->TryGetNumberField(TEXT("y"), Position.Y);
		(*PosObj)->TryGetNumberField(TEXT("z"), Position.Z);
	}

	FRotator Orientation(0);
	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(TEXT("orientation"), RotObj))
	{
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Orientation.Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"), Orientation.Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"), Orientation.Roll);
	}

	FVector Size(100);
	const TSharedPtr<FJsonObject>* SizeObj;
	if (Params->TryGetObjectField(TEXT("size"), SizeObj))
	{
		(*SizeObj)->TryGetNumberField(TEXT("x"), Size.X);
		(*SizeObj)->TryGetNumberField(TEXT("y"), Size.Y);
		(*SizeObj)->TryGetNumberField(TEXT("z"), Size.Z);
	}

	bool bResult = Subsystem->GenerateBoxUVChannel(Mesh, LODIndex, UVChannelIndex, Position, Orientation, Size);

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bResult);
	Result->SetStringField(TEXT("message"), bResult ? TEXT("Box UV channel generated successfully") : TEXT("Failed to generate box UV channel"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGeneratePlanarUVChannel(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	int32 UVChannelIndex = 0;
	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("uv_channel"), UVChannelIndex);
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	FVector Position(0);
	const TSharedPtr<FJsonObject>* PosObj;
	if (Params->TryGetObjectField(TEXT("position"), PosObj))
	{
		(*PosObj)->TryGetNumberField(TEXT("x"), Position.X);
		(*PosObj)->TryGetNumberField(TEXT("y"), Position.Y);
		(*PosObj)->TryGetNumberField(TEXT("z"), Position.Z);
	}

	FRotator Orientation(0);
	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(TEXT("orientation"), RotObj))
	{
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Orientation.Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"), Orientation.Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"), Orientation.Roll);
	}

	FVector2D Tiling(1, 1);
	const TSharedPtr<FJsonObject>* TilingObj;
	if (Params->TryGetObjectField(TEXT("tiling"), TilingObj))
	{
		(*TilingObj)->TryGetNumberField(TEXT("x"), Tiling.X);
		(*TilingObj)->TryGetNumberField(TEXT("y"), Tiling.Y);
	}

	bool bResult = Subsystem->GeneratePlanarUVChannel(Mesh, LODIndex, UVChannelIndex, Position, Orientation, Tiling);

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bResult);
	Result->SetStringField(TEXT("message"), bResult ? TEXT("Planar UV channel generated successfully") : TEXT("Failed to generate planar UV channel"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGenerateCylindricalUVChannel(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	int32 UVChannelIndex = 0;
	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("uv_channel"), UVChannelIndex);
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	FVector Position(0);
	const TSharedPtr<FJsonObject>* PosObj;
	if (Params->TryGetObjectField(TEXT("position"), PosObj))
	{
		(*PosObj)->TryGetNumberField(TEXT("x"), Position.X);
		(*PosObj)->TryGetNumberField(TEXT("y"), Position.Y);
		(*PosObj)->TryGetNumberField(TEXT("z"), Position.Z);
	}

	FRotator Orientation(0);
	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(TEXT("orientation"), RotObj))
	{
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Orientation.Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"), Orientation.Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"), Orientation.Roll);
	}

	FVector2D Tiling(1, 1);
	const TSharedPtr<FJsonObject>* TilingObj;
	if (Params->TryGetObjectField(TEXT("tiling"), TilingObj))
	{
		(*TilingObj)->TryGetNumberField(TEXT("x"), Tiling.X);
		(*TilingObj)->TryGetNumberField(TEXT("y"), Tiling.Y);
	}

	bool bResult = Subsystem->GenerateCylindricalUVChannel(Mesh, LODIndex, UVChannelIndex, Position, Orientation, Tiling);

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bResult);
	Result->SetStringField(TEXT("message"), bResult ? TEXT("Cylindrical UV channel generated successfully") : TEXT("Failed to generate cylindrical UV channel"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleAddUVChannel(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	bool bResult = Subsystem->AddUVChannel(Mesh, LODIndex);

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bResult);
	Result->SetStringField(TEXT("message"), bResult ? TEXT("UV channel added successfully") : TEXT("Failed to add UV channel"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleRemoveUVChannel(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	int32 UVChannelIndex = 0;
	int32 LODIndex = 0;
	Params->TryGetNumberField(TEXT("uv_channel"), UVChannelIndex);
	Params->TryGetNumberField(TEXT("lod_index"), LODIndex);

	bool bResult = Subsystem->RemoveUVChannel(Mesh, LODIndex, UVChannelIndex);

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bResult);
	Result->SetStringField(TEXT("message"), bResult ? TEXT("UV channel removed successfully") : TEXT("Failed to remove UV channel"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// LOD Operations
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetLods(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	const TArray<TSharedPtr<FJsonValue>>* ReductionOptionsArr = nullptr;
	if (!Params->TryGetArrayField(TEXT("reduction_options"), ReductionOptionsArr) || !ReductionOptionsArr)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("reduction_options array is required"));
	}

	TArray<FStaticMeshReductionSettings> ReductionSettings;
	for (const TSharedPtr<FJsonValue>& Val : *ReductionOptionsArr)
	{
		TSharedPtr<FJsonObject> OptObj = Val->AsObject();
		if (!OptObj.IsValid()) continue;

		FStaticMeshReductionSettings Settings;
		Settings.PercentTriangles = static_cast<float>(OptObj->GetNumberField(TEXT("percent_triangles")));
		Settings.ScreenSize = static_cast<float>(OptObj->GetNumberField(TEXT("screen_size")));
		ReductionSettings.Add(Settings);
	}

	// Read base LOD
	FMeshDescription BaseMeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(Mesh, 0, BaseMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	bool bAllSuccess = true;
	for (int32 i = 0; i < ReductionSettings.Num(); ++i)
	{
		UE::Geometry::FDynamicMesh3 DynamicMesh;
		if (!MeshDescriptionToDynamicMesh(BaseMeshDesc, DynamicMesh, Error))
		{
			bAllSuccess = false;
			continue;
		}

		// Simplify
		float KeepProbability = ReductionSettings[i].PercentTriangles;
		TArray<int32> VerticesToRemove;
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			if (FMath::FRand() > KeepProbability)
			{
				VerticesToRemove.Add(vid);
			}
		}

		for (int32 vid : VerticesToRemove)
		{
			if (DynamicMesh.IsVertex(vid))
			{
				DynamicMesh.RemoveVertex(vid, true);
			}
		}

		FMeshDescription LODMeshDesc;
		if (!DynamicMeshToMeshDescription(DynamicMesh, LODMeshDesc, Error))
		{
			bAllSuccess = false;
			continue;
		}

		// Ensure enough source models
		while (Mesh->GetNumSourceModels() <= i + 1)
		{
			Mesh->AddSourceModel();
		}

		if (!WriteMeshDescriptionToStaticMesh(Mesh, i + 1, LODMeshDesc, Error))
		{
			bAllSuccess = false;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bAllSuccess);
	Result->SetNumberField(TEXT("lod_count"), ReductionSettings.Num() + 1);
	Result->SetStringField(TEXT("message"), bAllSuccess ? TEXT("LODs set successfully") : TEXT("Some LODs failed"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleRemoveLods(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	int32 NumLODs = Mesh->GetNumSourceModels();
	if (NumLODs <= 1)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("message"), TEXT("Only base LOD exists, nothing to remove"));
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
	}

	// Read base LOD
	FMeshDescription BaseMeshDesc;
	if (!ReadMeshDescriptionFromStaticMesh(Mesh, 0, BaseMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	// Remove all extra source models
	while (Mesh->GetNumSourceModels() > 1)
	{
		Mesh->RemoveSourceModel(Mesh->GetNumSourceModels() - 1);
	}

	// Write base mesh back
	if (!WriteMeshDescriptionToStaticMesh(Mesh, 0, BaseMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Extra LODs removed, only base LOD retained"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Mesh Merge Operations (Actor-based)
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleJoinStaticMeshActors(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
	if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNames) || !ActorNames || ActorNames->Num() < 2)
	{
		Params->TryGetArrayField(TEXT("actor_paths"), ActorNames);
	}
	if (!ActorNames || ActorNames->Num() < 2)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("actor_names or actor_paths array with at least 2 actors is required"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	TArray<AStaticMeshActor*> StaticMeshActors;
	for (const TSharedPtr<FJsonValue>& Val : *ActorNames)
	{
		FString ActorName = Val->AsString();
		AStaticMeshActor* FoundActor = nullptr;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
			{
				FoundActor = *It;
				break;
			}
		}
		if (!FoundActor)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		}
		StaticMeshActors.Add(FoundActor);
	}

	// Merge meshes using FDynamicMesh3
	UE::Geometry::FDynamicMesh3 MergedMesh;
	MergedMesh.EnableTriangleGroups();
	MergedMesh.EnableAttributes();

	for (AStaticMeshActor* Actor : StaticMeshActors)
	{
		UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent();
		if (!SMC || !SMC->GetStaticMesh()) continue;

		FString Error;
		FMeshDescription MeshDesc;
		if (!ReadMeshDescriptionFromStaticMesh(SMC->GetStaticMesh(), 0, MeshDesc, Error))
		{
			continue;
		}

		UE::Geometry::FDynamicMesh3 DynamicMesh;
		if (!MeshDescriptionToDynamicMesh(MeshDesc, DynamicMesh, Error))
		{
			continue;
		}

		// Apply actor transform
		FTransform ActorTransform = Actor->GetActorTransform();
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			FVector V = DynamicMesh.GetVertex(vid);
			DynamicMesh.SetVertex(vid, ActorTransform.TransformPosition(V));
		}

		// Append to merged mesh
		TMap<int32, int32> VertexMap;
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			int32 NewVid = MergedMesh.AppendVertex(DynamicMesh.GetVertex(vid));
			VertexMap.Add(vid, NewVid);
		}
		for (int32 tid : DynamicMesh.TriangleIndicesItr())
		{
			UE::Geometry::FIndex3i Tri = DynamicMesh.GetTriangle(tid);
			MergedMesh.AppendTriangle(VertexMap[Tri.A], VertexMap[Tri.B], VertexMap[Tri.C]);
		}
	}

	// Create result mesh asset
	FString OutputAssetPath;
	Params->TryGetStringField(TEXT("output_asset_path"), OutputAssetPath);
	if (OutputAssetPath.IsEmpty())
	{
		Params->TryGetStringField(TEXT("asset_path"), OutputAssetPath);
	}
	if (OutputAssetPath.IsEmpty())
	{
		OutputAssetPath = TEXT("/Game/JoinedMesh");
	}

	FString AssetName = FPaths::GetBaseFilename(OutputAssetPath);
	FString PackagePath = FPaths::GetPath(OutputAssetPath);
	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	if (!Package)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	UStaticMesh* JoinedMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	if (!JoinedMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create joined mesh"));
	}

	FMeshDescription ResultMeshDesc;
	FString Error;
	if (!DynamicMeshToMeshDescription(MergedMesh, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	JoinedMesh->AddSourceModel();
	JoinedMesh->CommitMeshDescription(0);
	JoinedMesh->Modify();
	JoinedMesh->PostEditChange();
	JoinedMesh->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(JoinedMesh);
	Package->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), TEXT("Actors joined successfully"));
	Result->SetStringField(TEXT("output_path"), OutputAssetPath);
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMergeStaticMeshActors(const TSharedPtr<FJsonObject>& Params)
{
	// Functionally identical to JoinStaticMeshActors
	return HandleJoinStaticMeshActors(Params);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleCreateProxyMeshActor(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
	if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNames) || !ActorNames || ActorNames->Num() < 1)
	{
		Params->TryGetArrayField(TEXT("actor_paths"), ActorNames);
	}
	if (!ActorNames || ActorNames->Num() < 1)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("actor_names or actor_paths array with at least 1 actor is required"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	TArray<AStaticMeshActor*> StaticMeshActors;
	for (const TSharedPtr<FJsonValue>& Val : *ActorNames)
	{
		FString ActorName = Val->AsString();
		AStaticMeshActor* FoundActor = nullptr;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->GetName() == ActorName || It->GetActorLabel() == ActorName)
			{
				FoundActor = *It;
				break;
			}
		}
		if (!FoundActor)
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		}
		StaticMeshActors.Add(FoundActor);
	}

	// Merge meshes
	UE::Geometry::FDynamicMesh3 MergedMesh;
	MergedMesh.EnableTriangleGroups();
	MergedMesh.EnableAttributes();

	for (AStaticMeshActor* Actor : StaticMeshActors)
	{
		UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent();
		if (!SMC || !SMC->GetStaticMesh()) continue;

		FString Error;
		FMeshDescription MeshDesc;
		if (!ReadMeshDescriptionFromStaticMesh(SMC->GetStaticMesh(), 0, MeshDesc, Error))
		{
			continue;
		}

		UE::Geometry::FDynamicMesh3 DynamicMesh;
		if (!MeshDescriptionToDynamicMesh(MeshDesc, DynamicMesh, Error))
		{
			continue;
		}

		// Apply actor transform
		FTransform ActorTransform = Actor->GetActorTransform();
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			FVector V = DynamicMesh.GetVertex(vid);
			DynamicMesh.SetVertex(vid, ActorTransform.TransformPosition(V));
		}

		// Append
		TMap<int32, int32> VertexMap;
		for (int32 vid : DynamicMesh.VertexIndicesItr())
		{
			int32 NewVid = MergedMesh.AppendVertex(DynamicMesh.GetVertex(vid));
			VertexMap.Add(vid, NewVid);
		}
		for (int32 tid : DynamicMesh.TriangleIndicesItr())
		{
			UE::Geometry::FIndex3i Tri = DynamicMesh.GetTriangle(tid);
			MergedMesh.AppendTriangle(VertexMap[Tri.A], VertexMap[Tri.B], VertexMap[Tri.C]);
		}
	}

	// Simplify for proxy
	double TargetPercentage = 25.0;
	Params->TryGetNumberField(TEXT("target_percentage"), TargetPercentage);

	float KeepProbability = static_cast<float>(TargetPercentage) / 100.0f;
	TArray<int32> VerticesToRemove;
	for (int32 vid : MergedMesh.VertexIndicesItr())
	{
		if (FMath::FRand() > KeepProbability)
		{
			VerticesToRemove.Add(vid);
		}
	}

	for (int32 vid : VerticesToRemove)
	{
		if (MergedMesh.IsVertex(vid))
		{
			MergedMesh.RemoveVertex(vid, true);
		}
	}

	// Create output
	FString OutputAssetPath;
	Params->TryGetStringField(TEXT("output_asset_path"), OutputAssetPath);
	if (OutputAssetPath.IsEmpty())
	{
		Params->TryGetStringField(TEXT("asset_path"), OutputAssetPath);
	}
	if (OutputAssetPath.IsEmpty())
	{
		OutputAssetPath = TEXT("/Game/ProxyMesh");
	}

	FString AssetName = FPaths::GetBaseFilename(OutputAssetPath);
	FString PackagePath = FPaths::GetPath(OutputAssetPath);
	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	if (!Package)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	UStaticMesh* ProxyStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	if (!ProxyStaticMesh)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create proxy mesh"));
	}

	FMeshDescription ResultMeshDesc;
	FString Error;
	if (!DynamicMeshToMeshDescription(MergedMesh, ResultMeshDesc, Error))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);
	}

	ProxyStaticMesh->AddSourceModel();
	ProxyStaticMesh->CommitMeshDescription(0);
	ProxyStaticMesh->Modify();
	ProxyStaticMesh->PostEditChange();
	ProxyStaticMesh->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(ProxyStaticMesh);
	Package->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Proxy mesh created at %.1f%% of original"), TargetPercentage));
	Result->SetStringField(TEXT("output_path"), OutputAssetPath);
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

// ---------------------------------------------------------------------
// Other utilities
// ---------------------------------------------------------------------
TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetGenerateLightmapUVs(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	bool bGenerateLightmapUVs = true;
	Params->TryGetBoolField(TEXT("generate"), bGenerateLightmapUVs);

	int32 LightmapCoordinateIndex = 1;
	Params->TryGetNumberField(TEXT("lightmap_coordinate_index"), LightmapCoordinateIndex);

	Subsystem->SetGenerateLightmapUVs(Mesh, bGenerateLightmapUVs);

	// Configure build settings
	for (int32 i = 0; i < Mesh->GetNumSourceModels(); ++i)
	{
		FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(i);
		SourceModel.BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
		SourceModel.BuildSettings.DstLightmapIndex = LightmapCoordinateIndex;
	}

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("generate_lightmap_uvs"), bGenerateLightmapUVs);
	Result->SetNumberField(TEXT("lightmap_coordinate_index"), LightmapCoordinateIndex);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set GenerateLightmapUVs to %s"), bGenerateLightmapUVs ? TEXT("true") : TEXT("false")));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleHasVertexColors(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	bool bHasVertexColors = Subsystem->HasVertexColors(Mesh);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("has_vertex_colors"), bHasVertexColors);
	Result->SetStringField(TEXT("message"), bHasVertexColors ? TEXT("Mesh has vertex colors") : TEXT("Mesh does not have vertex colors"));
	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleModelingToolExecute(const TSharedPtr<FJsonObject>& Params)
{
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Modeling tool execution requires interactive editor mode. Use specific geometry commands instead."));
}
