#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Dom/JsonObject.h"
#include "Tests/MCPAutomationTestUtils.h"
#include "EditorAssetLibrary.h"

using namespace UnrealMCP::Tests;

namespace
{
    void DeleteAssetIfExists(const FString& AssetPath)
    {
        if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            UEditorAssetLibrary::DeleteAsset(AssetPath);
        }
    }
}

// --- CreateMaterialInstance ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPCreateMaterialInstanceTest, "UnrealMCP.L3.Material.CreateInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPCreateMaterialInstanceTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString InstanceName = MakeUniqueName(TEXT("MCPTestMIC"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + InstanceName;

    DeleteAssetIfExists(AssetPath);

    const TSharedPtr<FJsonObject> Params = MakeObject({
        {TEXT("parent_material"), MakeStringValue(TEXT("/Engine/BasicShapes/BasicShapeMaterial"))},
        {TEXT("instance_name"), MakeStringValue(InstanceName)},
        {TEXT("package_path"), MakeStringValue(PackagePath)}
    });

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("create_material_instance"), Params);
    TestTrue(TEXT("create_material_instance should succeed"), IsSuccessResponse(Result));

    if (IsSuccessResponse(Result))
    {
        FString OutPath;
        TestTrue(TEXT("Result should contain path"), Result->TryGetStringField(TEXT("path"), OutPath));
        TestEqual(TEXT("path should match"), OutPath, AssetPath);
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

// --- BatchUpdateParameters ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPBatchUpdateParametersTest, "UnrealMCP.L3.Material.BatchUpdateParameters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPBatchUpdateParametersTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString InstanceName = MakeUniqueName(TEXT("MCPTestBatchMIC"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + InstanceName;

    DeleteAssetIfExists(AssetPath);

    // 1. Create MIC
    {
        const TSharedPtr<FJsonObject> CreateParams = MakeObject({
            {TEXT("parent_material"), MakeStringValue(TEXT("/Engine/BasicShapes/BasicShapeMaterial"))},
            {TEXT("instance_name"), MakeStringValue(InstanceName)},
            {TEXT("package_path"), MakeStringValue(PackagePath)}
        });
        TSharedPtr<FJsonObject> CreateResult = Commands.HandleCommand(TEXT("create_material_instance"), CreateParams);
        TestTrue(TEXT("Setup: create_material_instance should succeed"), IsSuccessResponse(CreateResult));
    }

    // 2. Batch update scalar + vector
    {
        TArray<TSharedPtr<FJsonValue>> ParamsArray;
        {
            TSharedPtr<FJsonObject> ScalarParam = MakeObject({
                {TEXT("name"), MakeStringValue(TEXT("TestScalar"))},
                {TEXT("type"), MakeStringValue(TEXT("scalar"))},
                {TEXT("value"), MakeNumberValue(0.75)}
            });
            ParamsArray.Add(MakeObjectValue(ScalarParam));
        }
        {
            TArray<TSharedPtr<FJsonValue>> ColorArray;
            ColorArray.Add(MakeNumberValue(1.0));
            ColorArray.Add(MakeNumberValue(0.5));
            ColorArray.Add(MakeNumberValue(0.25));
            ColorArray.Add(MakeNumberValue(1.0));
            TSharedPtr<FJsonObject> VectorParam = MakeObject({
                {TEXT("name"), MakeStringValue(TEXT("TestVector"))},
                {TEXT("type"), MakeStringValue(TEXT("vector"))},
                {TEXT("value"), MakeArrayValueJson(ColorArray)}
            });
            ParamsArray.Add(MakeObjectValue(VectorParam));
        }

        const TSharedPtr<FJsonObject> UpdateParams = MakeObject({
            {TEXT("instance_path"), MakeStringValue(AssetPath)},
            {TEXT("parameters"), MakeArrayValueJson(ParamsArray)}
        });

        TSharedPtr<FJsonObject> UpdateResult = Commands.HandleCommand(TEXT("batch_update_material_parameters"), UpdateParams);
        TestTrue(TEXT("batch_update_material_parameters should succeed"), IsSuccessResponse(UpdateResult));

        if (IsSuccessResponse(UpdateResult))
        {
            double UpdatedCount = 0;
            TestTrue(TEXT("Result should contain updated_count"), UpdateResult->TryGetNumberField(TEXT("updated_count"), UpdatedCount));
            TestEqual(TEXT("updated_count should be 2"), static_cast<int32>(UpdatedCount), 2);
        }
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

// --- StaticSwitchParameter ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPStaticSwitchParameterTest, "UnrealMCP.L3.Material.StaticSwitchParameter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPStaticSwitchParameterTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString InstanceName = MakeUniqueName(TEXT("MCPTestSwitchMIC"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + InstanceName;

    DeleteAssetIfExists(AssetPath);

    // Create MIC
    {
        const TSharedPtr<FJsonObject> CreateParams = MakeObject({
            {TEXT("parent_material"), MakeStringValue(TEXT("/Engine/BasicShapes/BasicShapeMaterial"))},
            {TEXT("instance_name"), MakeStringValue(InstanceName)},
            {TEXT("package_path"), MakeStringValue(PackagePath)}
        });
        Commands.HandleCommand(TEXT("create_material_instance"), CreateParams);
    }

    // Set static switch
    {
        const TSharedPtr<FJsonObject> Params = MakeObject({
            {TEXT("instance_path"), MakeStringValue(AssetPath)},
            {TEXT("parameter_name"), MakeStringValue(TEXT("TestSwitch"))},
            {TEXT("value"), MakeBoolValue(true)}
        });

        TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("set_material_static_switch_parameter"), Params);
        TestTrue(TEXT("set_material_static_switch_parameter should succeed"), IsSuccessResponse(Result));
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

// --- ParameterCollection ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPParameterCollectionTest, "UnrealMCP.L3.Material.ParameterCollection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPParameterCollectionTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString CollectionName = MakeUniqueName(TEXT("MCPTestParamCollection"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + CollectionName;

    DeleteAssetIfExists(AssetPath);

    // Create collection
    {
        const TSharedPtr<FJsonObject> CreateParams = MakeObject({
            {TEXT("name"), MakeStringValue(CollectionName)},
            {TEXT("package_path"), MakeStringValue(PackagePath)}
        });
        TSharedPtr<FJsonObject> CreateResult = Commands.HandleCommand(TEXT("create_material_parameter_collection"), CreateParams);
        TestTrue(TEXT("create_material_parameter_collection should succeed"), IsSuccessResponse(CreateResult));

        if (IsSuccessResponse(CreateResult))
        {
            FString OutPath;
            TestTrue(TEXT("Result should contain path"), CreateResult->TryGetStringField(TEXT("path"), OutPath));
            TestEqual(TEXT("path should match"), OutPath, AssetPath);
        }
    }

    // Edit collection: add scalar + vector
    {
        TArray<TSharedPtr<FJsonValue>> Scalars;
        Scalars.Add(MakeStringValue(TEXT("GlobalOpacity")));

        TArray<TSharedPtr<FJsonValue>> Vectors;
        Vectors.Add(MakeStringValue(TEXT("GlobalTint")));

        const TSharedPtr<FJsonObject> EditParams = MakeObject({
            {TEXT("collection_path"), MakeStringValue(AssetPath)},
            {TEXT("add_scalars"), MakeArrayValueJson(Scalars)},
            {TEXT("add_vectors"), MakeArrayValueJson(Vectors)}
        });

        TSharedPtr<FJsonObject> EditResult = Commands.HandleCommand(TEXT("edit_material_parameter_collection"), EditParams);
        TestTrue(TEXT("edit_material_parameter_collection should succeed"), IsSuccessResponse(EditResult));

        if (IsSuccessResponse(EditResult))
        {
            double AddedScalars = 0;
            double AddedVectors = 0;
            TestTrue(TEXT("Result should contain added_scalars"), EditResult->TryGetNumberField(TEXT("added_scalars"), AddedScalars));
            TestTrue(TEXT("Result should contain added_vectors"), EditResult->TryGetNumberField(TEXT("added_vectors"), AddedVectors));
            TestEqual(TEXT("added_scalars should be 1"), static_cast<int32>(AddedScalars), 1);
            TestEqual(TEXT("added_vectors should be 1"), static_cast<int32>(AddedVectors), 1);
        }
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

// --- BatchUpdateInvalidType ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPBatchUpdateInvalidTypeTest, "UnrealMCP.L3.Material.BatchUpdateInvalidType", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPBatchUpdateInvalidTypeTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString InstanceName = MakeUniqueName(TEXT("MCPTestInvalidMIC"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + InstanceName;

    DeleteAssetIfExists(AssetPath);

    // Create MIC
    {
        const TSharedPtr<FJsonObject> CreateParams = MakeObject({
            {TEXT("parent_material"), MakeStringValue(TEXT("/Engine/BasicShapes/BasicShapeMaterial"))},
            {TEXT("instance_name"), MakeStringValue(InstanceName)},
            {TEXT("package_path"), MakeStringValue(PackagePath)}
        });
        Commands.HandleCommand(TEXT("create_material_instance"), CreateParams);
    }

    // Attempt batch with invalid type
    {
        TArray<TSharedPtr<FJsonValue>> ParamsArray;
        TSharedPtr<FJsonObject> BadParam = MakeObject({
            {TEXT("name"), MakeStringValue(TEXT("BadParam"))},
            {TEXT("type"), MakeStringValue(TEXT("unknown_type"))},
            {TEXT("value"), MakeNumberValue(1.0)}
        });
        ParamsArray.Add(MakeObjectValue(BadParam));

        const TSharedPtr<FJsonObject> UpdateParams = MakeObject({
            {TEXT("instance_path"), MakeStringValue(AssetPath)},
            {TEXT("parameters"), MakeArrayValueJson(ParamsArray)}
        });

        TSharedPtr<FJsonObject> UpdateResult = Commands.HandleCommand(TEXT("batch_update_material_parameters"), UpdateParams);
        TestFalse(TEXT("batch_update_material_parameters with invalid type should fail"), IsSuccessResponse(UpdateResult));
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

// --- BatchUpdateMissingInstance ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPBatchUpdateMissingInstanceTest, "UnrealMCP.L3.Material.BatchUpdateMissingInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPBatchUpdateMissingInstanceTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;

    TArray<TSharedPtr<FJsonValue>> ParamsArray;
    TSharedPtr<FJsonObject> ScalarParam = MakeObject({
        {TEXT("name"), MakeStringValue(TEXT("TestScalar"))},
        {TEXT("type"), MakeStringValue(TEXT("scalar"))},
        {TEXT("value"), MakeNumberValue(0.5)}
    });
    ParamsArray.Add(MakeObjectValue(ScalarParam));

    const TSharedPtr<FJsonObject> UpdateParams = MakeObject({
        {TEXT("instance_path"), MakeStringValue(TEXT("/Game/Materials/NonExistentInstance"))},
        {TEXT("parameters"), MakeArrayValueJson(ParamsArray)}
    });

    TSharedPtr<FJsonObject> UpdateResult = Commands.HandleCommand(TEXT("batch_update_material_parameters"), UpdateParams);
    TestFalse(TEXT("batch_update_material_parameters with missing instance should fail"), IsSuccessResponse(UpdateResult));

    return true;
}

// --- AdvancedMaterial: DeferredDecal ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPAdvancedMaterialDecalTest, "UnrealMCP.L3.Material.AdvancedDecal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPAdvancedMaterialDecalTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString MaterialName = MakeUniqueName(TEXT("MCPTestDecalMat"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + MaterialName;

    DeleteAssetIfExists(AssetPath);

    const TSharedPtr<FJsonObject> Params = MakeObject({
        {TEXT("name"), MakeStringValue(MaterialName)},
        {TEXT("material_domain"), MakeStringValue(TEXT("DeferredDecal"))},
        {TEXT("package_path"), MakeStringValue(PackagePath)}
    });

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("create_advanced_material"), Params);
    TestTrue(TEXT("create_advanced_material (DeferredDecal) should succeed"), IsSuccessResponse(Result));

    if (IsSuccessResponse(Result))
    {
        FString OutPath;
        TestTrue(TEXT("Result should contain path"), Result->TryGetStringField(TEXT("path"), OutPath));
        TestEqual(TEXT("path should match"), OutPath, AssetPath);

        FString OutDomain;
        TestTrue(TEXT("Result should contain material_domain"), Result->TryGetStringField(TEXT("material_domain"), OutDomain));
        TestEqual(TEXT("material_domain should be DeferredDecal"), OutDomain, TEXT("DeferredDecal"));
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

// --- AdvancedMaterial: LightFunction ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPAdvancedMaterialLightFunctionTest, "UnrealMCP.L3.Material.AdvancedLightFunction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPAdvancedMaterialLightFunctionTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString MaterialName = MakeUniqueName(TEXT("MCPTestLightFuncMat"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + MaterialName;

    DeleteAssetIfExists(AssetPath);

    const TSharedPtr<FJsonObject> Params = MakeObject({
        {TEXT("name"), MakeStringValue(MaterialName)},
        {TEXT("material_domain"), MakeStringValue(TEXT("LightFunction"))},
        {TEXT("package_path"), MakeStringValue(PackagePath)}
    });

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("create_advanced_material"), Params);
    TestTrue(TEXT("create_advanced_material (LightFunction) should succeed"), IsSuccessResponse(Result));

    if (IsSuccessResponse(Result))
    {
        FString OutDomain;
        TestTrue(TEXT("Result should contain material_domain"), Result->TryGetStringField(TEXT("material_domain"), OutDomain));
        TestEqual(TEXT("material_domain should be LightFunction"), OutDomain, TEXT("LightFunction"));
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

// --- AdvancedMaterial: PostProcess ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealMCPAdvancedMaterialPostProcessTest, "UnrealMCP.L3.Material.AdvancedPostProcess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealMCPAdvancedMaterialPostProcessTest::RunTest(const FString& Parameters)
{
    FEpicUnrealMCPMaterialCommands Commands;
    const FString MaterialName = MakeUniqueName(TEXT("MCPTestPPMat"));
    const FString PackagePath = TEXT("/Game/Materials/");
    const FString AssetPath = PackagePath + MaterialName;

    DeleteAssetIfExists(AssetPath);

    const TSharedPtr<FJsonObject> Params = MakeObject({
        {TEXT("name"), MakeStringValue(MaterialName)},
        {TEXT("material_domain"), MakeStringValue(TEXT("PostProcess"))},
        {TEXT("package_path"), MakeStringValue(PackagePath)}
    });

    TSharedPtr<FJsonObject> Result = Commands.HandleCommand(TEXT("create_advanced_material"), Params);
    TestTrue(TEXT("create_advanced_material (PostProcess) should succeed"), IsSuccessResponse(Result));

    if (IsSuccessResponse(Result))
    {
        FString OutDomain;
        TestTrue(TEXT("Result should contain material_domain"), Result->TryGetStringField(TEXT("material_domain"), OutDomain));
        TestEqual(TEXT("material_domain should be PostProcess"), OutDomain, TEXT("PostProcess"));
    }

    DeleteAssetIfExists(AssetPath);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
