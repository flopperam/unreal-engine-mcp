"""E2E tests for Material & Rendering MCP commands (Phase 1-3).

Requires:
    - Unreal Editor with MCP Bridge on 127.0.0.1:55557

Run:
    pytest tests/e2e/test_material_e2e.py --skip-unreal   # Skip tests requiring Unreal
    pytest tests/e2e/test_material_e2e.py                 # Full E2E with Unreal
"""

import pytest

from .conftest import unreal_command


@pytest.mark.requires_unreal
class TestMaterialInstanceLifecycle:
    """End-to-end material instance creation and parameter updates."""

    def test_create_material_instance(self):
        result = unreal_command("create_material_instance", {
            "parent_material": "/Engine/BasicShapes/BasicShapeMaterial",
            "instance_name": "E2E_TestMIC",
            "package_path": "/Game/Materials/",
        })
        assert result.get("success") is True, f"Failed to create MIC: {result}"
        assert "path" in result

    def test_batch_update_parameters(self):
        # Ensure MIC exists
        unreal_command("create_material_instance", {
            "parent_material": "/Engine/BasicShapes/BasicShapeMaterial",
            "instance_name": "E2E_BatchMIC",
            "package_path": "/Game/Materials/",
        })

        result = unreal_command("batch_update_material_parameters", {
            "instance_path": "/Game/Materials/E2E_BatchMIC",
            "parameters": [
                {"name": "TestScalar", "type": "scalar", "value": 0.75},
                {"name": "TestVector", "type": "vector", "value": [1.0, 0.5, 0.25, 1.0]},
            ],
        })
        assert result.get("success") is True, f"Batch update failed: {result}"
        assert result.get("updated_count") == 2

    def test_set_static_switch_parameter(self):
        unreal_command("create_material_instance", {
            "parent_material": "/Engine/BasicShapes/BasicShapeMaterial",
            "instance_name": "E2E_SwitchMIC",
            "package_path": "/Game/Materials/",
        })

        result = unreal_command("set_material_static_switch_parameter", {
            "instance_path": "/Game/Materials/E2E_SwitchMIC",
            "parameter_name": "TestSwitch",
            "value": True,
        })
        assert result.get("success") is True, f"Static switch update failed: {result}"


@pytest.mark.requires_unreal
class TestAdvancedMaterialCreation:
    """End-to-end advanced material creation with different domains."""

    def test_create_deferred_decal_material(self):
        result = unreal_command("create_advanced_material", {
            "name": "E2E_DecalMat",
            "material_domain": "DeferredDecal",
            "package_path": "/Game/Materials/",
        })
        assert result.get("success") is True, f"Failed to create decal material: {result}"
        assert result.get("material_domain") == "DeferredDecal"

    def test_create_post_process_material(self):
        result = unreal_command("create_advanced_material", {
            "name": "E2E_PPMat",
            "material_domain": "PostProcess",
            "package_path": "/Game/Materials/",
        })
        assert result.get("success") is True, f"Failed to create post process material: {result}"
        assert result.get("material_domain") == "PostProcess"


@pytest.mark.requires_unreal
class TestParameterCollection:
    """End-to-end material parameter collection operations."""

    def test_create_and_edit_collection(self):
        result = unreal_command("create_material_parameter_collection", {
            "name": "E2E_ParamCollection",
            "package_path": "/Game/Materials/",
        })
        assert result.get("success") is True, f"Failed to create collection: {result}"

        edit_result = unreal_command("edit_material_parameter_collection", {
            "collection_path": "/Game/Materials/E2E_ParamCollection",
            "add_scalars": ["GlobalOpacity"],
            "add_vectors": ["GlobalTint"],
        })
        assert edit_result.get("success") is True, f"Failed to edit collection: {edit_result}"
        assert edit_result.get("added_scalars") == 1
        assert edit_result.get("added_vectors") == 1


@pytest.mark.requires_unreal
class TestRenderingSettings:
    """End-to-end rendering CVar adjustments."""

    def test_set_anti_aliasing(self):
        result = unreal_command("set_anti_aliasing", {"method": "TSR"})
        assert result.get("success") is True, f"Failed to set AA: {result}"

    def test_set_nanite_visualization(self):
        result = unreal_command("set_nanite_visualization", {"mode": "Clusters"})
        assert result.get("success") is True, f"Failed to set Nanite viz: {result}"

        # Reset
        unreal_command("set_nanite_visualization", {"mode": "Off"})

    def test_get_shader_compile_status(self):
        result = unreal_command("get_shader_compile_status", {})
        assert result.get("success") is True, f"Failed to get shader status: {result}"
        assert "remaining_jobs" in result
        assert "is_compiling" in result
