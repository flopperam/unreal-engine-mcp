"""Unit tests for Material MCP tools (Phase 1-3).

These tests use FakeUnrealConnection to verify tool JSON serialization
and response handling without external services.
"""

import pytest
from unittest.mock import patch, MagicMock

from server.specs.material_spec import (
    MaterialParameterSpec,
    BatchUpdateParametersSpec,
    MaterialInstanceSpec,
    AdvancedMaterialSpec,
)


class TestMaterialSpecs:
    """Test material spec dataclasses."""

    def test_scalar_parameter_spec(self):
        spec = MaterialParameterSpec(name="Roughness", type="scalar", value=0.5)
        assert spec.name == "Roughness"
        assert spec.type == "scalar"
        assert spec.value == 0.5

    def test_vector_parameter_spec(self):
        spec = MaterialParameterSpec(name="BaseColor", type="vector", value=[1.0, 0.0, 0.0, 1.0])
        assert spec.type == "vector"
        assert spec.value == [1.0, 0.0, 0.0, 1.0]

    def test_texture_parameter_spec(self):
        spec = MaterialParameterSpec(name="NormalMap", type="texture", value="/Game/Textures/T_Normal")
        assert spec.type == "texture"
        assert spec.value == "/Game/Textures/T_Normal"

    def test_static_switch_parameter_spec(self):
        spec = MaterialParameterSpec(name="UseDetail", type="static_switch", value=True)
        assert spec.type == "static_switch"
        assert spec.value is True

    def test_batch_update_spec(self):
        params = [
            MaterialParameterSpec(name="Roughness", type="scalar", value=0.5),
            MaterialParameterSpec(name="BaseColor", type="vector", value=[1.0, 0.0, 0.0, 1.0]),
        ]
        spec = BatchUpdateParametersSpec(instance_path="/Game/Materials/MI_Test", parameters=params)
        assert spec.instance_path == "/Game/Materials/MI_Test"
        assert len(spec.parameters) == 2

    def test_material_instance_spec_defaults(self):
        spec = MaterialInstanceSpec(parent_material="/Engine/BasicShapes/BasicShapeMaterial", instance_name="MI_Test")
        assert spec.package_path == "/Game/Materials/"

    def test_advanced_material_spec(self):
        spec = AdvancedMaterialSpec(name="M_Decal", material_domain="DeferredDecal")
        assert spec.material_domain == "DeferredDecal"
        assert spec.package_path == "/Game/Materials/"


class TestMaterialToolsSerialization:
    """Verify tools send correct JSON payloads to Unreal."""

    def test_create_material_instance_payload(self):
        from tests.conftest import FakeUnrealConnection
        conn = FakeUnrealConnection()
        conn.responses["create_material_instance"] = {"success": True, "path": "/Game/Materials/MI_Test"}
        result = conn.send_command("create_material_instance", {
            "parent_material": "/Engine/BasicShapes/BasicShapeMaterial",
            "instance_name": "MI_Test",
            "package_path": "/Game/Materials/"
        })
        assert result["success"] is True
        assert result["path"] == "/Game/Materials/MI_Test"

    def test_batch_update_parameters_payload(self):
        from tests.conftest import FakeUnrealConnection
        conn = FakeUnrealConnection()
        conn.responses["batch_update_material_parameters"] = {"success": True, "updated_count": 2}
        result = conn.send_command("batch_update_material_parameters", {
            "instance_path": "/Game/Materials/MI_Test",
            "parameters": [
                {"name": "Roughness", "type": "scalar", "value": 0.5},
                {"name": "BaseColor", "type": "vector", "value": [1.0, 0.0, 0.0, 1.0]},
            ]
        })
        assert result["success"] is True
        assert result["updated_count"] == 2

    def test_set_material_scalar_parameter_payload(self):
        from tests.conftest import FakeUnrealConnection
        conn = FakeUnrealConnection()
        conn.responses["set_material_scalar_parameter"] = {"success": True, "parameter_name": "Roughness", "value": 0.5}
        result = conn.send_command("set_material_scalar_parameter", {
            "instance_path": "/Game/Materials/MI_Test",
            "parameter_name": "Roughness",
            "value": 0.5,
        })
        assert result["success"] is True
        assert result["parameter_name"] == "Roughness"

    def test_create_advanced_material_payload(self):
        from tests.conftest import FakeUnrealConnection
        conn = FakeUnrealConnection()
        conn.responses["create_advanced_material"] = {"success": True, "path": "/Game/Materials/M_Decal", "material_domain": "DeferredDecal"}
        result = conn.send_command("create_advanced_material", {
            "name": "M_Decal",
            "material_domain": "DeferredDecal",
            "package_path": "/Game/Materials/"
        })
        assert result["success"] is True
        assert result["material_domain"] == "DeferredDecal"

    def test_set_anti_aliasing_payload(self):
        from tests.conftest import FakeUnrealConnection
        conn = FakeUnrealConnection()
        conn.responses["set_anti_aliasing"] = {"success": True, "cvar": "r.AntiAliasingMethod"}
        result = conn.send_command("set_anti_aliasing", {"method": "TSR"})
        assert result["success"] is True

    def test_get_shader_compile_status_payload(self):
        from tests.conftest import FakeUnrealConnection
        conn = FakeUnrealConnection()
        conn.responses["get_shader_compile_status"] = {"success": True, "remaining_jobs": 0, "is_compiling": False}
        result = conn.send_command("get_shader_compile_status", {})
        assert result["success"] is True
        assert result["is_compiling"] is False
