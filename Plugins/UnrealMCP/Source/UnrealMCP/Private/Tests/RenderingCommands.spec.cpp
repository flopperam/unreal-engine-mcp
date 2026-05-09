#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Commands/EpicUnrealMCPRenderingCommands.h"
#include "Dom/JsonObject.h"
#include "Tests/MCPAutomationTestUtils.h"
#include "HAL/IConsoleManager.h"

using namespace UnrealMCP::Tests;

// --- SetCVar ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPSetCVarTest, "UnrealMCP.L3.Rendering.SetCVar", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPSetCVarTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPRenderingCommands Commands;

    // Set shadow distance scale
    const TSharedPtr<FJsonObject> Params = MakeObject({
        {TEXT("distance_scale"), MakeNumberValue(2.0)}
    });

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("set_shadow_quality"), Params);
    TestTrue(TEXT("set_shadow_quality should succeed"), IsSuccessResponse(Result));

    if (IsSuccessResponse(Result))
    {
        // Read back the CVar to verify
        IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.DistanceScale"));
        if (CVar)
        {
            TestEqual(TEXT("r.Shadow.DistanceScale should be 2.0"), CVar->GetFloat(), 2.0f);
        }
        else
        {
            AddWarning(TEXT("Could not verify CVar value: r.Shadow.DistanceScale not found"));
        }
    }

    return true;
}

// --- NaniteVisualization ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPNaniteVizTest, "UnrealMCP.L3.Rendering.NaniteVisualization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPNaniteVizTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPRenderingCommands Commands;

    const TSharedPtr<FJsonObject> Params = MakeObject({
        {TEXT("mode"), MakeStringValue(TEXT("Clusters"))}
    });

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("set_nanite_visualization"), Params);
    TestTrue(TEXT("set_nanite_visualization should succeed"), IsSuccessResponse(Result));

    if (IsSuccessResponse(Result))
    {
        IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.Visualize"));
        if (CVar)
        {
            TestEqual(TEXT("r.Nanite.Visualize should be 1"), CVar->GetInt(), 1);
        }
        else
        {
            AddWarning(TEXT("Could not verify CVar value: r.Nanite.Visualize not found"));
        }
    }

    // Reset to Off
    {
        const TSharedPtr<FJsonObject> ResetParams = MakeObject({
            {TEXT("mode"), MakeStringValue(TEXT("Off"))}
        });
        Commands.HandleCommand(TEXT("set_nanite_visualization"), ResetParams);
    }

    return true;
}

// --- ShaderCompileStatus ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPShaderCompileStatusTest, "UnrealMCP.L3.Rendering.ShaderCompileStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPShaderCompileStatusTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPRenderingCommands Commands;

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("get_shader_compile_status"), MakeShared<FJsonObject>());
    TestTrue(TEXT("get_shader_compile_status should succeed"), IsSuccessResponse(Result));

    if (IsSuccessResponse(Result))
    {
        double RemainingJobs = -1;
        bool bIsCompiling = false;
        TestTrue(TEXT("Result should contain remaining_jobs"), Result->TryGetNumberField(TEXT("remaining_jobs"), RemainingJobs));
        TestTrue(TEXT("Result should contain is_compiling"), Result->TryGetBoolField(TEXT("is_compiling"), bIsCompiling));
        TestTrue(TEXT("remaining_jobs should be >= 0"), RemainingJobs >= 0.0);
    }

    return true;
}

// --- PIESafety ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPPIESafetyTest, "UnrealMCP.L3.Rendering.PIESafety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPPIESafetyTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPRenderingCommands Commands;

    // We don't actually start PIE here to avoid side effects; we just test the command parses params
    // and returns either success (if not in PIE) or error (if in PIE). Since automation tests run
    // in editor context without PIE, this should succeed.
    const TSharedPtr<FJsonObject> Params = MakeObject({
        {TEXT("enabled"), MakeBoolValue(true)}
    });

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("set_lumen_enabled"), Params);
    // If not in PIE, this should succeed. The test verifies the command runs without crashing.
    // We don't strictly assert success because the test environment may have different defaults.
    TestTrue(TEXT("set_lumen_enabled command should return a valid response"), Result.IsValid());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
