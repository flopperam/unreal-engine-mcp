#include "Commands/ProceduralMeshBuilder.h"
#include "Editor.h"
#include "Engine/World.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTangents.h"
#include "Components/DynamicMeshComponent.h"
#include "Dom/JsonValue.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"
#include "Algo/Unique.h"
#include "RenderingThread.h"

namespace
{
constexpr uint32 MCPM_MAGIC = 0x4D43504D;
constexpr uint32 MCPM_VERSION = 1;
constexpr uint32 MCPM_HEADER_SIZE = 104;
constexpr uint32 FLAG_HAS_UV = 0x01;
constexpr uint32 FLAG_HAS_TANGENT = 0x02;
constexpr uint32 FLAG_HAS_COLOR = 0x04;
constexpr uint32 FLAG_HAS_MATERIAL_ID = 0x08;
constexpr uint32 SUPPORTED_FLAGS = FLAG_HAS_UV | FLAG_HAS_TANGENT | FLAG_HAS_COLOR | FLAG_HAS_MATERIAL_ID;
constexpr uint32 MAX_VERTEX_COUNT = 1000000;
constexpr uint32 MAX_INDEX_COUNT = 6000000;
constexpr int64 MAX_PAYLOAD_BYTES = 256LL * 1024LL * 1024LL;
constexpr double MAX_ABS_COORDINATE = 10000000.0;

uint32 ReadUInt32LE(const uint8* Data)
{
	uint32 Value = 0;
	FMemory::Memcpy(&Value, Data, sizeof(uint32));
	return Value;
}

uint64 ReadUInt64LE(const uint8* Data)
{
	uint64 Value = 0;
	FMemory::Memcpy(&Value, Data, sizeof(uint64));
	return Value;
}

float ReadFloatLE(const uint8* Data)
{
	float Value = 0.0f;
	FMemory::Memcpy(&Value, Data, sizeof(float));
	return Value;
}

bool FailParse(const TCHAR* Code, const FString& Message, FString& OutErrorCode, FString& OutErrorMessage)
{
	OutErrorCode = Code;
	OutErrorMessage = Message;
	UE_LOG(LogTemp, Warning, TEXT("ProceduralMeshBuilder: %s: %s"), Code, *Message);
	return false;
}

bool IsFiniteVector(const FVector& Vector)
{
	return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z);
}

bool IsFiniteVector2(const FVector2D& Vector)
{
	return FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y);
}

FString TriangleKey(uint32 A, uint32 B, uint32 C)
{
	uint32 Values[3] = {A, B, C};
	Algo::Sort(Values);
	return FString::Printf(TEXT("%u:%u:%u"), Values[0], Values[1], Values[2]);
}

TArray<TSharedPtr<FJsonValue>> MakeVectorJsonArray(const FVector& Vector)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
	Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
	Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
	return Values;
}

TArray<TSharedPtr<FJsonValue>> MakeStringJsonArray(const TArray<FString>& Strings)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FString& String : Strings)
	{
		Values.Add(MakeShared<FJsonValueString>(String));
	}
	return Values;
}

TSharedPtr<FJsonObject> MakeEnvelopeBase(const FProceduralMeshPayload& Payload, const TArray<FString>& Warnings, bool bSuccess)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetNumberField(TEXT("request_id"), static_cast<double>(Payload.RequestId));
	Result->SetStringField(TEXT("mcp_id"), Payload.McpId);
	Result->SetArrayField(TEXT("warnings"), MakeStringJsonArray(Warnings));
	return Result;
}

TSharedPtr<FJsonObject> MakeErrorResponse(const FProceduralMeshPayload& Payload, const TArray<FString>& Warnings, const FString& Code, const FString& Message)
{
	TSharedPtr<FJsonObject> Result = MakeEnvelopeBase(Payload, Warnings, false);
	Result->SetStringField(TEXT("error_code"), Code);
	Result->SetStringField(TEXT("error"), Message);
	return Result;
}

TSharedPtr<FJsonObject> MakeSuccessResponse(const FProceduralMeshPayload& Payload, const TArray<FString>& Warnings)
{
	TSharedPtr<FJsonObject> Result = MakeEnvelopeBase(Payload, Warnings, true);
	Result->SetStringField(TEXT("error_code"), TEXT(""));
	Result->SetStringField(TEXT("error"), TEXT(""));
	return Result;
}

UMaterialInterface* ResolveMaterial(const FProceduralMeshPayload& Payload, TArray<FString>& InOutWarnings)
{
	UMaterialInterface* AppliedMaterial = nullptr;
	if (!Payload.MaterialPath.IsEmpty())
	{
		AppliedMaterial = LoadObject<UMaterialInterface>(nullptr, *Payload.MaterialPath);
		if (!AppliedMaterial)
		{
			InOutWarnings.Add(FString::Printf(TEXT("MATERIAL_NOT_FOUND:%s"), *Payload.MaterialPath));
			UE_LOG(LogTemp, Warning, TEXT("ProceduralMeshBuilder: Failed to load material %s"), *Payload.MaterialPath);
		}
	}

	if (!AppliedMaterial && Payload.VertexColors.Num() == static_cast<int32>(Payload.VertexCount))
	{
		AppliedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
		if (!AppliedMaterial)
		{
			InOutWarnings.Add(TEXT("VERTEX_COLOR_MATERIAL_NOT_FOUND"));
		}
	}
	if (!AppliedMaterial)
	{
		AppliedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	}
	if (!AppliedMaterial)
	{
		AppliedMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	return AppliedMaterial;
}

void ApplyNaniteIfSupported(UDynamicMeshComponent* MeshComp, bool bEnableNanite, TArray<FString>& InOutWarnings)
{
	if (!MeshComp)
	{
		return;
	}

	if (FBoolProperty* NaniteProperty = FindFProperty<FBoolProperty>(MeshComp->GetClass(), TEXT("bEnableNanite")))
	{
		NaniteProperty->SetPropertyValue_InContainer(MeshComp, bEnableNanite);
		return;
	}

	if (bEnableNanite)
	{
		InOutWarnings.Add(TEXT("NANITE_NOT_SUPPORTED_ON_DYNAMIC_MESH_COMPONENT"));
	}
}
}

bool FProceduralMeshBuilder::ParseBinaryPayload(const TArray<uint8>& RawBuffer, FProceduralMeshPayload& OutPayload, FString& OutErrorCode, FString& OutErrorMessage)
{
	if (RawBuffer.Num() < static_cast<int32>(MCPM_HEADER_SIZE))
	{
		return FailParse(TEXT("SHORT_HEADER"), FString::Printf(TEXT("buffer too small for header: %d bytes"), RawBuffer.Num()), OutErrorCode, OutErrorMessage);
	}
	if (RawBuffer.Num() > MAX_PAYLOAD_BYTES)
	{
		return FailParse(TEXT("PAYLOAD_TOO_LARGE"), FString::Printf(TEXT("payload exceeded maximum size: %d > %lld bytes"), RawBuffer.Num(), MAX_PAYLOAD_BYTES), OutErrorCode, OutErrorMessage);
	}

	const uint8* Data = RawBuffer.GetData();
	uint32 Magic = ReadUInt32LE(Data + 0);
	if (Magic != MCPM_MAGIC)
	{
		return FailParse(TEXT("BAD_MAGIC"), FString::Printf(TEXT("invalid magic 0x%08X"), Magic), OutErrorCode, OutErrorMessage);
	}

	OutPayload.Version = ReadUInt32LE(Data + 4);
	if (OutPayload.Version != MCPM_VERSION)
	{
		return FailParse(TEXT("UNSUPPORTED_VERSION"), FString::Printf(TEXT("unsupported version %u"), OutPayload.Version), OutErrorCode, OutErrorMessage);
	}

	uint32 HeaderSize = ReadUInt32LE(Data + 8);
	if (HeaderSize != MCPM_HEADER_SIZE)
	{
		return FailParse(TEXT("BAD_HEADER_SIZE"), FString::Printf(TEXT("invalid header size %u"), HeaderSize), OutErrorCode, OutErrorMessage);
	}

	OutPayload.Flags = ReadUInt32LE(Data + 12);
	if ((OutPayload.Flags & ~SUPPORTED_FLAGS) != 0)
	{
		return FailParse(TEXT("UNSUPPORTED_FLAGS"), FString::Printf(TEXT("unsupported flags 0x%08X"), OutPayload.Flags & ~SUPPORTED_FLAGS), OutErrorCode, OutErrorMessage);
	}

	OutPayload.VertexCount = ReadUInt32LE(Data + 16);
	OutPayload.IndexCount = ReadUInt32LE(Data + 20);
	OutPayload.PayloadCrc32 = ReadUInt32LE(Data + 24);
	// reserved at 28
	OutPayload.RequestId = ReadUInt64LE(Data + 32);
	// mcp_id at 40
	
	char McpIdBuf[65] = {0};
	FMemory::Memcpy(McpIdBuf, Data + 40, 64);
	FString ParsedMcpId = UTF8_TO_TCHAR(McpIdBuf);
	if (OutPayload.McpId.IsEmpty())
	{
		OutPayload.McpId = ParsedMcpId;
	}
	else if (OutPayload.McpId != ParsedMcpId && !ParsedMcpId.IsEmpty())
	{
		return FailParse(TEXT("MCP_ID_MISMATCH"), FString::Printf(TEXT("mcp_id mismatch. JSON='%s', Binary='%s'"), *OutPayload.McpId, *ParsedMcpId), OutErrorCode, OutErrorMessage);
	}

	const uint32 V = OutPayload.VertexCount;
	const uint32 I = OutPayload.IndexCount;
	if (V == 0)
	{
		return FailParse(TEXT("EMPTY_VERTEX_BUFFER"), TEXT("vertex_count must be greater than zero"), OutErrorCode, OutErrorMessage);
	}
	if (V > MAX_VERTEX_COUNT)
	{
		return FailParse(TEXT("VERTEX_COUNT_TOO_LARGE"), FString::Printf(TEXT("vertex_count %u exceeds maximum %u"), V, MAX_VERTEX_COUNT), OutErrorCode, OutErrorMessage);
	}
	if (I == 0 || I % 3 != 0)
	{
		return FailParse(TEXT("BAD_INDEX_COUNT"), FString::Printf(TEXT("index_count %u must be a non-zero multiple of 3"), I), OutErrorCode, OutErrorMessage);
	}
	if (I > MAX_INDEX_COUNT)
	{
		return FailParse(TEXT("INDEX_COUNT_TOO_LARGE"), FString::Printf(TEXT("index_count %u exceeds maximum %u"), I, MAX_INDEX_COUNT), OutErrorCode, OutErrorMessage);
	}

	int64 ExpectedSize = MCPM_HEADER_SIZE
		+ static_cast<int64>(V) * sizeof(float) * 3  // positions (xyz)
		+ static_cast<int64>(V) * sizeof(float) * 3  // normals (nxnynz)
		+ static_cast<int64>(I) * sizeof(uint32);    // indices

	if (OutPayload.Flags & FLAG_HAS_UV) ExpectedSize += static_cast<int64>(V) * sizeof(float) * 2; // UVs
	if (OutPayload.Flags & FLAG_HAS_TANGENT) ExpectedSize += static_cast<int64>(V) * sizeof(float) * 4; // Tangents
	if (OutPayload.Flags & FLAG_HAS_COLOR) ExpectedSize += static_cast<int64>(V) * sizeof(uint8) * 4;  // Colors
	if (OutPayload.Flags & FLAG_HAS_MATERIAL_ID) ExpectedSize += static_cast<int64>(I / 3) * sizeof(uint16); // Material IDs

	if (ExpectedSize > MAX_PAYLOAD_BYTES)
	{
		return FailParse(TEXT("PAYLOAD_TOO_LARGE"), FString::Printf(TEXT("expected payload size %lld exceeds maximum %lld bytes"), ExpectedSize, MAX_PAYLOAD_BYTES), OutErrorCode, OutErrorMessage);
	}

	if (RawBuffer.Num() != ExpectedSize)
	{
		return FailParse(TEXT("PAYLOAD_SIZE_MISMATCH"), FString::Printf(TEXT("payload size mismatch. expected=%lld, actual=%d"), ExpectedSize, RawBuffer.Num()), OutErrorCode, OutErrorMessage);
	}

	const uint32 ActualCrc32 = FCrc::MemCrc32(Data + MCPM_HEADER_SIZE, static_cast<int32>(ExpectedSize - MCPM_HEADER_SIZE));
	if (ActualCrc32 != OutPayload.PayloadCrc32)
	{
		return FailParse(TEXT("BAD_CRC32"), FString::Printf(TEXT("payload_crc32 mismatch. expected=0x%08X, actual=0x%08X"), OutPayload.PayloadCrc32, ActualCrc32), OutErrorCode, OutErrorMessage);
	}

	OutPayload.Positions.SetNumUninitialized(V);
	OutPayload.Normals.SetNumUninitialized(V);
	OutPayload.PayloadByteSize = ExpectedSize;

	int64 Offset = MCPM_HEADER_SIZE;
	for (uint32 Index = 0; Index < V; ++Index)
	{
		const float X = ReadFloatLE(Data + Offset);
		const float Y = ReadFloatLE(Data + Offset + sizeof(float));
		const float Z = ReadFloatLE(Data + Offset + sizeof(float) * 2);
		OutPayload.Positions[Index] = FVector(X, Y, Z);
		if (!IsFiniteVector(OutPayload.Positions[Index]))
		{
			return FailParse(TEXT("INVALID_POSITION"), FString::Printf(TEXT("position %u contains NaN or Infinity"), Index), OutErrorCode, OutErrorMessage);
		}
		if (FMath::Abs(OutPayload.Positions[Index].X) > MAX_ABS_COORDINATE ||
			FMath::Abs(OutPayload.Positions[Index].Y) > MAX_ABS_COORDINATE ||
			FMath::Abs(OutPayload.Positions[Index].Z) > MAX_ABS_COORDINATE)
		{
			return FailParse(TEXT("BOUNDS_TOO_LARGE"), FString::Printf(TEXT("position %u exceeds maximum coordinate extent %.0f"), Index, MAX_ABS_COORDINATE), OutErrorCode, OutErrorMessage);
		}
		Offset += sizeof(float) * 3;
	}

	for (uint32 Index = 0; Index < V; ++Index)
	{
		const float X = ReadFloatLE(Data + Offset);
		const float Y = ReadFloatLE(Data + Offset + sizeof(float));
		const float Z = ReadFloatLE(Data + Offset + sizeof(float) * 2);
		OutPayload.Normals[Index] = FVector(X, Y, Z);
		if (!IsFiniteVector(OutPayload.Normals[Index]))
		{
			return FailParse(TEXT("INVALID_NORMAL"), FString::Printf(TEXT("normal %u contains NaN or Infinity"), Index), OutErrorCode, OutErrorMessage);
		}
		Offset += sizeof(float) * 3;
	}

	if (OutPayload.Flags & FLAG_HAS_UV)
	{
		OutPayload.UVs.SetNumUninitialized(V);
		for (uint32 Index = 0; Index < V; ++Index)
		{
			const float U = ReadFloatLE(Data + Offset);
			const float VCoord = ReadFloatLE(Data + Offset + sizeof(float));
			OutPayload.UVs[Index] = FVector2D(U, VCoord);
			if (!IsFiniteVector2(OutPayload.UVs[Index]))
			{
				return FailParse(TEXT("INVALID_UV"), FString::Printf(TEXT("uv %u contains NaN or Infinity"), Index), OutErrorCode, OutErrorMessage);
			}
			Offset += sizeof(float) * 2;
		}
	}

	if (OutPayload.Flags & FLAG_HAS_TANGENT)
	{
		OutPayload.Warnings.Add(TEXT("TANGENTS_IGNORED_RECOMPUTED"));
		for (uint32 Index = 0; Index < V; ++Index)
		{
			for (int32 Component = 0; Component < 4; ++Component)
			{
				const float TangentValue = ReadFloatLE(Data + Offset + sizeof(float) * Component);
				if (!FMath::IsFinite(TangentValue))
				{
					return FailParse(TEXT("INVALID_TANGENT"), FString::Printf(TEXT("tangent %u contains NaN or Infinity"), Index), OutErrorCode, OutErrorMessage);
				}
			}
			Offset += sizeof(float) * 4;
		}
	}

	if (OutPayload.Flags & FLAG_HAS_COLOR)
	{
		OutPayload.VertexColors.SetNumUninitialized(V);
		for (uint32 Index = 0; Index < V; ++Index)
		{
			const uint8 R = Data[Offset + 0];
			const uint8 G = Data[Offset + 1];
			const uint8 B = Data[Offset + 2];
			const uint8 A = Data[Offset + 3];
			OutPayload.VertexColors[Index] = FColor(R, G, B, A);
			Offset += sizeof(uint8) * 4;
		}
	}

	if (OutPayload.Flags & FLAG_HAS_MATERIAL_ID)
	{
		OutPayload.Warnings.Add(TEXT("MATERIAL_IDS_IGNORED"));
		Offset += static_cast<int64>(I / 3) * sizeof(uint16);
	}

	OutPayload.Indices.SetNumUninitialized(I);
	TSet<FString> UniqueTriangles;
	for (uint32 Index = 0; Index < I; ++Index)
	{
		OutPayload.Indices[Index] = ReadUInt32LE(Data + Offset);
		if (OutPayload.Indices[Index] >= V)
		{
			return FailParse(TEXT("INDEX_OUT_OF_RANGE"), FString::Printf(TEXT("index %u has vertex id %u but vertex_count is %u"), Index, OutPayload.Indices[Index], V), OutErrorCode, OutErrorMessage);
		}
		Offset += sizeof(uint32);
	}

	for (uint32 Index = 0; Index < I; Index += 3)
	{
		const uint32 A = OutPayload.Indices[Index];
		const uint32 B = OutPayload.Indices[Index + 1];
		const uint32 C = OutPayload.Indices[Index + 2];
		if (A == B || B == C || A == C)
		{
			OutPayload.Warnings.Add(FString::Printf(TEXT("DEGENERATE_TRIANGLE:%u"), Index / 3));
			continue;
		}

		const FString Key = TriangleKey(A, B, C);
		if (UniqueTriangles.Contains(Key))
		{
			OutPayload.Warnings.Add(FString::Printf(TEXT("DUPLICATE_TRIANGLE:%u"), Index / 3));
		}
		else
		{
			UniqueTriangles.Add(Key);
		}
	}

	if (Offset != ExpectedSize)
	{
		return FailParse(TEXT("PAYLOAD_OFFSET_MISMATCH"), FString::Printf(TEXT("parser consumed %lld bytes but expected %lld"), Offset, ExpectedSize), OutErrorCode, OutErrorMessage);
	}

	UE_LOG(LogTemp, Log, TEXT("ProceduralMeshBuilder: request_id=%llu mcp_id=%s parsed payload bytes=%lld flags=%u verts=%u triangles=%u warnings=%d"),
		OutPayload.RequestId, *OutPayload.McpId, ExpectedSize, OutPayload.Flags, V, I / 3, OutPayload.Warnings.Num());
	return true;
}

TSharedPtr<FJsonObject> FProceduralMeshBuilder::BuildAndSpawnOnGameThread(const FProceduralMeshPayload& Payload)
{
	TArray<FString> Warnings = Payload.Warnings;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMeshBuilder: No editor world"));
		return MakeErrorResponse(Payload, Warnings, TEXT("NO_EDITOR_WORLD"), TEXT("No editor world is available"));
	}

	FDynamicMesh3 DynMesh;
	DynMesh.EnableTriangleGroups();
	DynMesh.EnableAttributes();
	if (!DynMesh.Attributes())
	{
		return MakeErrorResponse(Payload, Warnings, TEXT("MESH_ATTRIBUTE_INIT_FAILED"), TEXT("failed to initialize DynamicMesh attributes"));
	}

	TArray<int32> VertexIds;
	VertexIds.SetNumUninitialized(Payload.VertexCount);
	for (uint32 i = 0; i < Payload.VertexCount; ++i)
	{
		VertexIds[i] = DynMesh.AppendVertex((FVector3d)Payload.Positions[i]);
	}

	TArray<int32> UVElementIds;
	if (Payload.UVs.Num() == static_cast<int32>(Payload.VertexCount))
	{
		DynMesh.Attributes()->SetNumUVLayers(1);
		if (UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynMesh.Attributes()->PrimaryUV())
		{
			UVElementIds.SetNumUninitialized(Payload.VertexCount);
			for (uint32 i = 0; i < Payload.VertexCount; ++i)
			{
				UVElementIds[i] = UVOverlay->AppendElement(FVector2f((float)Payload.UVs[i].X, (float)Payload.UVs[i].Y));
			}
		}
	}

	TArray<int32> NormalElementIds;
	if (Payload.Normals.Num() == static_cast<int32>(Payload.VertexCount))
	{
		DynMesh.Attributes()->SetNumNormalLayers(1);
		if (UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynMesh.Attributes()->PrimaryNormals())
		{
			NormalElementIds.SetNumUninitialized(Payload.VertexCount);
			for (uint32 i = 0; i < Payload.VertexCount; ++i)
			{
				NormalElementIds[i] = NormalOverlay->AppendElement(FVector3f((float)Payload.Normals[i].X, (float)Payload.Normals[i].Y, (float)Payload.Normals[i].Z));
			}
		}
	}

	TArray<int32> ColorElementIds;
	if (Payload.VertexColors.Num() == static_cast<int32>(Payload.VertexCount))
	{
		DynMesh.Attributes()->EnablePrimaryColors();
		if (UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = DynMesh.Attributes()->PrimaryColors())
		{
			ColorElementIds.SetNumUninitialized(Payload.VertexCount);
			for (uint32 i = 0; i < Payload.VertexCount; ++i)
			{
				const FLinearColor LinearColor = FLinearColor(Payload.VertexColors[i]);
				ColorElementIds[i] = ColorOverlay->AppendElement(FVector4f(LinearColor.R, LinearColor.G, LinearColor.B, LinearColor.A));
			}
		}
	}

	int32 BuiltTriangleCount = 0;
	TSet<FString> BuiltTriangles;
	for (uint32 i = 0; i < Payload.IndexCount; i += 3)
	{
		const int32 A = VertexIds[(int32)Payload.Indices[i]];
		const int32 B = VertexIds[(int32)Payload.Indices[i + 1]];
		const int32 C = VertexIds[(int32)Payload.Indices[i + 2]];
		if (A == B || B == C || A == C)
		{
			continue;
		}

		const FString Key = TriangleKey((uint32)A, (uint32)B, (uint32)C);
		if (BuiltTriangles.Contains(Key))
		{
			continue;
		}
		BuiltTriangles.Add(Key);

		const int32 TriangleId = DynMesh.AppendTriangle(A, B, C);
		if (TriangleId < 0)
		{
			return MakeErrorResponse(Payload, Warnings, TEXT("MESH_BUILD_FAILED"), FString::Printf(TEXT("failed to append triangle %u"), i / 3));
		}
		++BuiltTriangleCount;

		if (UVElementIds.Num() == static_cast<int32>(Payload.VertexCount))
		{
			if (UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = DynMesh.Attributes()->PrimaryUV())
			{
				if (UVOverlay->SetTriangle(TriangleId, UE::Geometry::FIndex3i(UVElementIds[Payload.Indices[i]], UVElementIds[Payload.Indices[i + 1]], UVElementIds[Payload.Indices[i + 2]])) != UE::Geometry::EMeshResult::Ok)
				{
					return MakeErrorResponse(Payload, Warnings, TEXT("UV_OVERLAY_FAILED"), FString::Printf(TEXT("failed to assign UV overlay for triangle %u"), i / 3));
				}
			}
		}
		if (NormalElementIds.Num() == static_cast<int32>(Payload.VertexCount))
		{
			if (UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynMesh.Attributes()->PrimaryNormals())
			{
				if (NormalOverlay->SetTriangle(TriangleId, UE::Geometry::FIndex3i(NormalElementIds[Payload.Indices[i]], NormalElementIds[Payload.Indices[i + 1]], NormalElementIds[Payload.Indices[i + 2]])) != UE::Geometry::EMeshResult::Ok)
				{
					return MakeErrorResponse(Payload, Warnings, TEXT("NORMAL_OVERLAY_FAILED"), FString::Printf(TEXT("failed to assign normal overlay for triangle %u"), i / 3));
				}
			}
		}
		if (ColorElementIds.Num() == static_cast<int32>(Payload.VertexCount))
		{
			if (UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = DynMesh.Attributes()->PrimaryColors())
			{
				if (ColorOverlay->SetTriangle(TriangleId, UE::Geometry::FIndex3i(ColorElementIds[Payload.Indices[i]], ColorElementIds[Payload.Indices[i + 1]], ColorElementIds[Payload.Indices[i + 2]])) != UE::Geometry::EMeshResult::Ok)
				{
					return MakeErrorResponse(Payload, Warnings, TEXT("COLOR_OVERLAY_FAILED"), FString::Printf(TEXT("failed to assign color overlay for triangle %u"), i / 3));
				}
			}
		}
	}
	if (BuiltTriangleCount == 0)
	{
		return MakeErrorResponse(Payload, Warnings, TEXT("EMPTY_MESH"), TEXT("mesh contains no valid triangles"));
	}
	UE::Geometry::FMeshNormals::QuickComputeVertexNormals(DynMesh);
	if (UVElementIds.Num() == static_cast<int32>(Payload.VertexCount))
	{
		if (!UE::Geometry::FMeshTangentsd::ComputeDefaultOverlayTangents(DynMesh))
		{
			Warnings.Add(TEXT("TANGENT_RECOMPUTE_SKIPPED"));
		}
	}

	AActor* Actor = nullptr;
	int32 DuplicateCount = 0;
	if (!Payload.McpId.IsEmpty())
	{
		const FName SearchTag(*FString::Printf(TEXT("mcp_id:%s"), *Payload.McpId));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(SearchTag))
			{
				++DuplicateCount;
				if (!Actor)
				{
					Actor = *It;
				}
			}
		}
	}
	if (DuplicateCount > 1)
	{
		return MakeErrorResponse(Payload, Warnings, TEXT("MCP_ID_CONFLICT"), FString::Printf(TEXT("found %d actors with mcp_id '%s'"), DuplicateCount, *Payload.McpId));
	}

	const FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("Upsert Procedural Mesh %s"), *Payload.ActorName)));
	const FTransform SpawnTransform(Payload.Rotation, Payload.Location, Payload.Scale);
	bool bCreatedActor = false;

	if (!Actor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(*Payload.ActorName));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Actor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, SpawnParams);
		if (!Actor)
		{
			return MakeErrorResponse(Payload, Warnings, TEXT("SPAWN_FAILED"), TEXT("failed to spawn actor"));
		}
		bCreatedActor = true;
		Actor->SetFlags(RF_Transactional);
		Actor->Modify();
		Actor->SetActorLabel(Payload.ActorName);
		Actor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
		if (!Payload.McpId.IsEmpty())
		{
			Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:%s"), *Payload.McpId)));
		}
	}
	else
	{
		Actor->SetFlags(RF_Transactional);
		Actor->Modify();
		Actor->SetActorTransform(SpawnTransform);
		if (Actor->GetActorLabel() != Payload.ActorName)
		{
			Actor->SetActorLabel(Payload.ActorName);
		}
		Actor->Tags.AddUnique(FName(TEXT("managed_by_mcp")));
		if (!Payload.McpId.IsEmpty())
		{
			Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("mcp_id:%s"), *Payload.McpId)));
		}
	}

	UDynamicMeshComponent* MeshComp = Actor->FindComponentByClass<UDynamicMeshComponent>();
	if (!MeshComp)
	{
		MeshComp = NewObject<UDynamicMeshComponent>(Actor, UDynamicMeshComponent::StaticClass(), FName(*Payload.ActorName), RF_Transactional);
		if (MeshComp)
		{
			MeshComp->SetFlags(RF_Transactional);
			MeshComp->Modify();
			Actor->SetRootComponent(MeshComp);
			Actor->AddInstanceComponent(MeshComp);
			MeshComp->RegisterComponent();
		}
	}
	else
	{
		MeshComp->SetFlags(RF_Transactional);
		MeshComp->Modify();
	}

	if (!MeshComp)
	{
		if (bCreatedActor)
		{
			World->DestroyActor(Actor);
		}
		return MakeErrorResponse(Payload, Warnings, TEXT("COMPONENT_CREATE_FAILED"), TEXT("failed to create or find mesh component"));
	}

	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetVisibility(true, true);
	MeshComp->SetHiddenInGame(false);

	FlushRenderingCommands();

	UMaterialInterface* AppliedMaterial = ResolveMaterial(Payload, Warnings);
	ApplyNaniteIfSupported(MeshComp, Payload.bEnableNanite, Warnings);
	MeshComp->SetMesh(MoveTemp(DynMesh));
	MeshComp->NotifyMeshUpdated();
	if (AppliedMaterial)
	{
		MeshComp->SetMaterial(0, AppliedMaterial);
	}
	if (Payload.VertexColors.Num() == static_cast<int32>(Payload.VertexCount))
	{
		const TArray<FColor> Colors = Payload.VertexColors;
		MeshComp->SetTriangleColorFunction(
			[Colors](const FDynamicMesh3* Mesh, int TriangleID)
			{
				if (!Mesh || !Mesh->IsTriangle(TriangleID))
				{
					return FColor(255, 120, 0, 255);
				}
				const UE::Geometry::FIndex3i Tri = Mesh->GetTriangle(TriangleID);
				const FColor A = Colors.IsValidIndex(Tri.A) ? Colors[Tri.A] : FColor(255, 120, 0, 255);
				const FColor B = Colors.IsValidIndex(Tri.B) ? Colors[Tri.B] : FColor(255, 120, 0, 255);
				const FColor C = Colors.IsValidIndex(Tri.C) ? Colors[Tri.C] : FColor(255, 120, 0, 255);
				return FColor(
					(uint8)(((int32)A.R + (int32)B.R + (int32)C.R) / 3),
					(uint8)(((int32)A.G + (int32)B.G + (int32)C.G) / 3),
					(uint8)(((int32)A.B + (int32)B.B + (int32)C.B) / 3),
					(uint8)(((int32)A.A + (int32)B.A + (int32)C.A) / 3)
				);
			}
		);
	}
	else
	{
		MeshComp->SetTriangleColorFunction(
			[](const FDynamicMesh3*, int)
			{
				return FColor(255, 120, 0, 255);
			}
		);
	}

	MeshComp->MarkRenderStateDirty();
	Actor->MarkPackageDirty();
	if (ULevel* CurrentLevel = World->GetCurrentLevel())
	{
		CurrentLevel->MarkPackageDirty();
	}

	if (Payload.bFocusViewport && GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Actor, true, true, true, true);
		GEditor->MoveViewportCamerasToActor(*Actor, false);
	}

	TSharedPtr<FJsonObject> Result = MakeSuccessResponse(Payload, Warnings);
	Result->SetStringField(TEXT("actor_name"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("actor_object_name"), Actor->GetName());
	Result->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
	Result->SetStringField(TEXT("component_name"), MeshComp->GetName());
	Result->SetNumberField(TEXT("bytes"), static_cast<double>(Payload.PayloadByteSize));
	Result->SetNumberField(TEXT("vertex_count"), Payload.VertexCount);
	Result->SetNumberField(TEXT("index_count"), Payload.IndexCount);
	Result->SetNumberField(TEXT("triangle_count"), Payload.IndexCount / 3);
	Result->SetNumberField(TEXT("built_triangle_count"), BuiltTriangleCount);
	Result->SetArrayField(TEXT("location"), MakeVectorJsonArray(Actor->GetActorLocation()));
	Result->SetArrayField(TEXT("scale"), MakeVectorJsonArray(Actor->GetActorScale3D()));
	const FBox Bounds = Actor->GetComponentsBoundingBox(true);
	Result->SetArrayField(TEXT("bounds_origin"), MakeVectorJsonArray(Bounds.GetCenter()));
	Result->SetArrayField(TEXT("bounds_extent"), MakeVectorJsonArray(Bounds.GetExtent()));
	return Result;
}
