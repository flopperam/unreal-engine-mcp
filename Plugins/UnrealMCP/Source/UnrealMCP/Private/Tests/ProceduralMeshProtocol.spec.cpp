#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"

#include "Commands/ProceduralMeshBuilder.h"

#include <limits>

namespace
{
constexpr uint32 MCPM_MAGIC = 0x4D43504D;
constexpr uint32 MCPM_VERSION = 1;
constexpr uint32 MCPM_HEADER_SIZE = 104;
constexpr uint32 FLAG_HAS_UV = 0x01;
constexpr uint32 FLAG_HAS_COLOR = 0x04;

void WriteU32(TArray<uint8>& Buffer, int32 Offset, uint32 Value)
{
	FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
}

void WriteU64(TArray<uint8>& Buffer, int32 Offset, uint64 Value)
{
	FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
}

void AppendF32(TArray<uint8>& Buffer, float Value)
{
	const int32 Offset = Buffer.AddUninitialized(sizeof(float));
	FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(float));
}

void AppendU32(TArray<uint8>& Buffer, uint32 Value)
{
	const int32 Offset = Buffer.AddUninitialized(sizeof(uint32));
	FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
}

uint32 CalculateCrc32(const uint8* Data, int64 Size)
{
	return FCrc::MemCrc32(Data, static_cast<int32>(Size));
}

void RecomputePayloadCrc(TArray<uint8>& Buffer)
{
	const uint32 Crc = CalculateCrc32(Buffer.GetData() + MCPM_HEADER_SIZE, Buffer.Num() - MCPM_HEADER_SIZE);
	WriteU32(Buffer, 24, Crc);
}

void WriteF32(TArray<uint8>& Buffer, int32 Offset, float Value)
{
	FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(float));
}

TArray<uint8> MakeTrianglePayload(uint32 Flags = 0)
{
	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(MCPM_HEADER_SIZE);

	WriteU32(Buffer, 0, MCPM_MAGIC);
	WriteU32(Buffer, 4, MCPM_VERSION);
	WriteU32(Buffer, 8, MCPM_HEADER_SIZE);
	WriteU32(Buffer, 12, Flags);
	WriteU32(Buffer, 16, 3);
	WriteU32(Buffer, 20, 3);
	WriteU64(Buffer, 32, 42);
	const FTCHARToUTF8 McpIdUtf8(TEXT("parser_test_mesh"));
	FMemory::Memcpy(Buffer.GetData() + 40, McpIdUtf8.Get(), FMath::Min(63, McpIdUtf8.Length()));

	const float Positions[9] = {
		0.0f, 0.0f, 100.0f,
		100.0f, 0.0f, 100.0f,
		0.0f, 100.0f, 100.0f,
	};
	for (float Value : Positions)
	{
		AppendF32(Buffer, Value);
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		AppendF32(Buffer, 0.0f);
		AppendF32(Buffer, 0.0f);
		AppendF32(Buffer, 1.0f);
	}

	if ((Flags & FLAG_HAS_UV) != 0)
	{
		const float UVs[6] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
		for (float Value : UVs)
		{
			AppendF32(Buffer, Value);
		}
	}

	if ((Flags & FLAG_HAS_COLOR) != 0)
	{
		const uint8 Colors[12] = {
			255, 0, 0, 255,
			0, 255, 0, 255,
			0, 0, 255, 255,
		};
		Buffer.Append(Colors, UE_ARRAY_COUNT(Colors));
	}

	AppendU32(Buffer, 0);
	AppendU32(Buffer, 1);
	AppendU32(Buffer, 2);

	RecomputePayloadCrc(Buffer);
	return Buffer;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserValidTest, "UnrealMCP.L1.ProceduralMesh.Parser.Valid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserValidTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload(FLAG_HAS_UV | FLAG_HAS_COLOR);
	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	Payload.ActorName = TEXT("ParserTestMesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestTrue(TEXT("valid payload parses"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("request_id is parsed"), static_cast<int64>(Payload.RequestId), static_cast<int64>(42));
	TestEqual(TEXT("vertex count is parsed"), Payload.VertexCount, static_cast<uint32>(3));
	TestEqual(TEXT("index count is parsed"), Payload.IndexCount, static_cast<uint32>(3));
	TestEqual(TEXT("UV count matches vertices"), Payload.UVs.Num(), 3);
	TestEqual(TEXT("color count matches vertices"), Payload.VertexColors.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserRejectsBadCrcTest, "UnrealMCP.L1.ProceduralMesh.Parser.BadCrc", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserRejectsBadCrcTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	WriteU32(Buffer, 24, 0xDEADBEEF);
	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestFalse(TEXT("bad CRC payload is rejected"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("bad CRC error code is stable"), ErrorCode, FString(TEXT("BAD_CRC32")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserRejectsBadIndexTest, "UnrealMCP.L1.ProceduralMesh.Parser.BadIndex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserRejectsBadIndexTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	const int32 LastIndexOffset = Buffer.Num() - static_cast<int32>(sizeof(uint32));
	WriteU32(Buffer, LastIndexOffset, 99);
	RecomputePayloadCrc(Buffer);

	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestFalse(TEXT("out of range index is rejected"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("bad index error code is stable"), ErrorCode, FString(TEXT("INDEX_OUT_OF_RANGE")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserRejectsBadIndexCountTest, "UnrealMCP.L1.ProceduralMesh.Parser.BadIndexCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserRejectsBadIndexCountTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	WriteU32(Buffer, 20, 4);

	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestFalse(TEXT("index_count not divisible by 3 is rejected"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("bad index count error code is stable"), ErrorCode, FString(TEXT("BAD_INDEX_COUNT")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserRejectsNanPositionTest, "UnrealMCP.L1.ProceduralMesh.Parser.NaNPosition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserRejectsNanPositionTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	WriteF32(Buffer, MCPM_HEADER_SIZE + sizeof(float), std::numeric_limits<float>::quiet_NaN());
	RecomputePayloadCrc(Buffer);

	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestFalse(TEXT("NaN position is rejected"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("NaN position error code is stable"), ErrorCode, FString(TEXT("INVALID_POSITION")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserRejectsInfNormalTest, "UnrealMCP.L1.ProceduralMesh.Parser.InfNormal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserRejectsInfNormalTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	const int32 FirstNormalOffset = MCPM_HEADER_SIZE + static_cast<int32>(sizeof(float) * 9);
	WriteF32(Buffer, FirstNormalOffset, std::numeric_limits<float>::infinity());
	RecomputePayloadCrc(Buffer);

	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestFalse(TEXT("Inf normal is rejected"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("Inf normal error code is stable"), ErrorCode, FString(TEXT("INVALID_NORMAL")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserRejectsBoundsTest, "UnrealMCP.L1.ProceduralMesh.Parser.BoundsTooLarge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserRejectsBoundsTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	WriteF32(Buffer, MCPM_HEADER_SIZE, 10000001.0f);
	RecomputePayloadCrc(Buffer);

	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestFalse(TEXT("out of bounds coordinate is rejected"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("bounds error code is stable"), ErrorCode, FString(TEXT("BOUNDS_TOO_LARGE")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserWarnsDegenerateDuplicateTest, "UnrealMCP.L1.ProceduralMesh.Parser.DegenerateDuplicateWarnings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserWarnsDegenerateDuplicateTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	WriteU32(Buffer, 20, 9);
	const int32 FirstIndexOffset = Buffer.Num() - static_cast<int32>(sizeof(uint32) * 3);
	WriteU32(Buffer, FirstIndexOffset, 0);
	WriteU32(Buffer, FirstIndexOffset + 4, 0);
	WriteU32(Buffer, FirstIndexOffset + 8, 1);
	AppendU32(Buffer, 0);
	AppendU32(Buffer, 1);
	AppendU32(Buffer, 2);
	AppendU32(Buffer, 2);
	AppendU32(Buffer, 1);
	AppendU32(Buffer, 0);
	RecomputePayloadCrc(Buffer);

	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestTrue(TEXT("degenerate/duplicate triangles are accepted with warnings"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestTrue(TEXT("degenerate triangle warning is present"), Payload.Warnings.Contains(TEXT("DEGENERATE_TRIANGLE:0")));
	TestTrue(TEXT("duplicate triangle warning is present"), Payload.Warnings.Contains(TEXT("DUPLICATE_TRIANGLE:2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPProceduralMeshParserRejectsShortPayloadTest, "UnrealMCP.L1.ProceduralMesh.Parser.ShortPayload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPProceduralMeshParserRejectsShortPayloadTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Buffer = MakeTrianglePayload();
	Buffer.RemoveAt(Buffer.Num() - 1);

	FProceduralMeshPayload Payload;
	Payload.McpId = TEXT("parser_test_mesh");
	FString ErrorCode;
	FString ErrorMessage;

	TestFalse(TEXT("short payload is rejected"), FProceduralMeshBuilder::ParseBinaryPayload(Buffer, Payload, ErrorCode, ErrorMessage));
	TestEqual(TEXT("short payload error code is stable"), ErrorCode, FString(TEXT("PAYLOAD_SIZE_MISMATCH")));
	return true;
}

#endif
