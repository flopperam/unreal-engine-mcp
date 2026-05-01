#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Payload structure for procedural mesh binary transfer.
 * Header format (104 bytes, little-endian):
 *   u32 magic ('MCPM' / 0x4D43504D)
 *   u32 version
 *   u32 header_size (104)
 *   u32 flags (bitfield: 0x01=UV, 0x02=Tangent, 0x04=Color, 0x08=MaterialID)
 *   u32 vertex_count
 *   u32 index_count
 *   u32 payload_crc32 (IEEE CRC32 of bytes after the header)
 *   u32 reserved
 *   u64 request_id
 *   u8[64] mcp_id (UTF-8, null-terminated when shorter than 64 bytes)
 */
struct FProceduralMeshPayload
{
	uint32 Version = 1;
	uint32 Flags = 0;
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 PayloadCrc32 = 0;
	uint64 RequestId = 0;
	int64 PayloadByteSize = 0;
	TArray<FVector> Positions;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<uint32> Indices;
	TArray<FString> Warnings;
	FString McpId;
	FString ActorName;
	FString MaterialPath;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	bool bFocusViewport = true;
	bool bEnableNanite = false;
};

/**
 * Utility class for parsing binary payload and building UDynamicMesh on GameThread.
 */
class FProceduralMeshBuilder
{
public:
	/** Parse raw binary buffer into structured payload. Returns false on invalid data. */
	static bool ParseBinaryPayload(const TArray<uint8>& RawBuffer, FProceduralMeshPayload& OutPayload, FString& OutErrorCode, FString& OutErrorMessage);

	/** Build mesh and spawn Actor on GameThread. Must be called from GameThread. */
	static TSharedPtr<FJsonObject> BuildAndSpawnOnGameThread(const FProceduralMeshPayload& Payload);
};
