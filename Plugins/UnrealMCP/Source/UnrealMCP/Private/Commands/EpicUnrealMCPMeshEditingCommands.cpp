#include "Commands/EpicUnrealMCPMeshEditingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMeshSocket.h"
#include "StaticMeshEditorSubsystem.h"
#include "EditorAssetLibrary.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshRemeshFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/StaticMeshFunctions.h"
#include "DynamicMesh/DynamicMesh3.h"

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

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown mesh editing command: %s"), *CommandType));
}

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

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGetStaticMeshDetails(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetNumberField(TEXT("num_lods"), Mesh->GetNumLODs());
	Result->SetBoolField(TEXT("nanite_enabled"), Mesh->NaniteSettings.bEnabled);
	Result->SetNumberField(TEXT("nanite_fallback_percent"), Mesh->NaniteSettings.FallbackPercentTriangles);
	Result->SetNumberField(TEXT("lightmap_resolution"), Mesh->LightMapResolution);

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
		Mesh->NaniteSettings.bEnabled = bEnabled;
	}

	double FallbackPercent = 100.0;
	if (Params->TryGetNumberField(TEXT("fallback_percent"), FallbackPercent))
	{
		Mesh->NaniteSettings.FallbackPercentTriangles = FallbackPercent;
	}

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Set Nanite settings successfully"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleSetLightmapSettings(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	int32 Resolution = 64;
	if (Params->TryGetNumberField(TEXT("resolution"), Resolution))
	{
		Mesh->LightMapResolution = Resolution;
	}

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Set Lightmap settings successfully"));
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

		Mesh->PositiveBoundsExtension = PositiveBounds;
		Mesh->NegativeBoundsExtension = NegativeBounds;

		Mesh->Modify();
	Mesh->PostEditChange();
		Mesh->MarkPackageDirty();
	}

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Edited mesh bounds successfully"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleGenerateCollision(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* Mesh = GetStaticMeshFromParams(Params, Error);
	if (!Mesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	UStaticMeshEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();
	if (!Subsystem) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get UStaticMeshEditorSubsystem"));

	FString ShapeType = TEXT("Auto");
	Params->TryGetStringField(TEXT("shape_type"), ShapeType); // Auto, Box, Sphere, Capsule, 10DOP, 18DOP, 26DOP

	if (ShapeType == TEXT("Box")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Box);
	else if (ShapeType == TEXT("Sphere")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Sphere);
	else if (ShapeType == TEXT("Capsule")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Capsule);
	else if (ShapeType == TEXT("10DOPX")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Ndop10X);
	else if (ShapeType == TEXT("10DOPY")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Ndop10Y);
	else if (ShapeType == TEXT("10DOPZ")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Ndop10Z);
	else if (ShapeType == TEXT("18DOP")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Ndop18);
	else if (ShapeType == TEXT("26DOP")) Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Ndop26);
	else Subsystem->AddSimpleCollisions(Mesh, EScriptCollisionShapeType::Box); // Default

	Mesh->Modify();
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Generated collision successfully"));
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

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Set collision complexity successfully"));
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

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Added simple collision successfully"));
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

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Removed collisions successfully"));
}

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

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Set LOD group successfully"));
}

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

	// Check if exists
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

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Added socket successfully"));
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
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Removed socket successfully"));
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

	return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Updated socket successfully"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshBoolean(const TSharedPtr<FJsonObject>& Params)
{
	// NOTE: GeometryScript requires DynamicMesh components usually, or operates on UDynamicMesh.
	// For StaticMesh, you copy to DynamicMesh, perform operation, then copy back.
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	FString ToolMeshPath;
	if (!Params->TryGetStringField(TEXT("tool_mesh_path"), ToolMeshPath))
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing tool_mesh_path"));

	UStaticMesh* ToolMesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(ToolMeshPath));
	if (!ToolMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Tool mesh not found"));

	FString OperationStr;
	Params->TryGetStringField(TEXT("operation"), OperationStr);
	EGeometryScriptBooleanOperation Operation = EGeometryScriptBooleanOperation::Subtract;
	if (OperationStr == TEXT("Union")) Operation = EGeometryScriptBooleanOperation::Union;
	else if (OperationStr == TEXT("Intersect")) Operation = EGeometryScriptBooleanOperation::Intersect;

	EGeometryScriptOutcomePins Outcome;
	UDynamicMesh* DynTarget = NewObject<UDynamicMesh>();
	DynTarget = UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(TargetMesh, DynTarget, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), Outcome, nullptr);
	EGeometryScriptOutcomePins ToolOutcome;
	UDynamicMesh* DynTool = NewObject<UDynamicMesh>();
	DynTool = UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(ToolMesh, DynTool, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), ToolOutcome, nullptr);

	if (DynTarget && DynTool)
	{
		UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(DynTarget, FTransform::Identity, DynTool, FTransform::Identity, Operation, FGeometryScriptMeshBooleanOptions());

		FGeometryScriptCopyMeshToAssetOptions CopyOptions;
		CopyOptions.bEnableRecomputeNormals = true;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(DynTarget, TargetMesh, CopyOptions, FGeometryScriptMeshWriteLOD(), Outcome, nullptr);

		TargetMesh->Modify();
	TargetMesh->PostEditChange();
		TargetMesh->MarkPackageDirty();
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Boolean operation successful"));
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to setup dynamic meshes for boolean"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshRemesh(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	EGeometryScriptOutcomePins Outcome;
	UDynamicMesh* DynTarget = NewObject<UDynamicMesh>();
	DynTarget = UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(TargetMesh, DynTarget, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), Outcome, nullptr);

	if (DynTarget)
	{
		int32 TargetTriangleCount = 5000;
		Params->TryGetNumberField(TEXT("target_triangle_count"), TargetTriangleCount);

		FGeometryScriptRemeshOptions Options;
		UGeometryScriptLibrary_MeshRemeshFunctions::ApplyUniformRemesh(DynTarget, Options, FGeometryScriptUniformRemeshParameters());

		FGeometryScriptCopyMeshToAssetOptions CopyOptions;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(DynTarget, TargetMesh, CopyOptions, FGeometryScriptMeshWriteLOD(), Outcome, nullptr);

		TargetMesh->Modify();
	TargetMesh->PostEditChange();
		TargetMesh->MarkPackageDirty();
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Remesh successful"));
	}
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to setup dynamic mesh"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshSimplify(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	EGeometryScriptOutcomePins Outcome;
	UDynamicMesh* DynTarget = NewObject<UDynamicMesh>();
	DynTarget = UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(TargetMesh, DynTarget, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), Outcome, nullptr);

	if (DynTarget)
	{
		int32 TargetTriangleCount = 1000;
		Params->TryGetNumberField(TEXT("target_triangle_count"), TargetTriangleCount);

		FGeometryScriptPolygroupSimplifyOptions Options;
		UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(DynTarget, TargetTriangleCount, Options);

		FGeometryScriptCopyMeshToAssetOptions CopyOptions;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(DynTarget, TargetMesh, CopyOptions, FGeometryScriptMeshWriteLOD(), Outcome, nullptr);

		TargetMesh->Modify();
	TargetMesh->PostEditChange();
		TargetMesh->MarkPackageDirty();
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("Simplify successful"));
	}
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to setup dynamic mesh"));
}

TSharedPtr<FJsonObject> FEpicUnrealMCPMeshEditingCommands::HandleMeshUVUnwrap(const TSharedPtr<FJsonObject>& Params)
{
	FString Error;
	UStaticMesh* TargetMesh = GetStaticMeshFromParams(Params, Error);
	if (!TargetMesh) return FEpicUnrealMCPCommonUtils::CreateErrorResponse(Error);

	EGeometryScriptOutcomePins Outcome;
	UDynamicMesh* DynTarget = NewObject<UDynamicMesh>();
	DynTarget = UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(TargetMesh, DynTarget, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), Outcome, nullptr);

	if (DynTarget)
	{
		FGeometryScriptRepackUVsOptions Options;
		UGeometryScriptLibrary_MeshUVFunctions::RepackUVs(DynTarget, 0, Options);

		FGeometryScriptCopyMeshToAssetOptions CopyOptions;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(DynTarget, TargetMesh, CopyOptions, FGeometryScriptMeshWriteLOD(), Outcome, nullptr);

		TargetMesh->Modify();
	TargetMesh->PostEditChange();
		TargetMesh->MarkPackageDirty();
		return FEpicUnrealMCPCommonUtils::CreateSuccessResponse(TEXT("UV unwrap/repack successful"));
	}
	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to setup dynamic mesh"));
}
