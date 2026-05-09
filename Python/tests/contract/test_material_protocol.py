"""L2 contract tests for Phase 1-3 material/rendering commands.

Verifies JSON envelope structure and command routing contracts.
"""

import pytest
from tests.conftest import FakeUnrealConnection


class TestMaterialCommandEnvelopes:
    """Verify material commands send and receive properly structured envelopes."""

    def test_create_material_instance_envelope(self):
        conn = FakeUnrealConnection()
        conn.responses["create_material_instance"] = {"success": True, "path": "/Game/Materials/MI_Test"}
        result = conn.send_command("create_material_instance", {
            "parent_material": "/Engine/BasicShapes/BasicShapeMaterial",
            "instance_name": "MI_Test",
        })
        assert "success" in result
        assert result["success"] is True
        assert "path" in result

    def test_batch_update_parameters_envelope(self):
        conn = FakeUnrealConnection()
        conn.responses["batch_update_material_parameters"] = {
            "success": True,
            "updated_count": 2,
            "instance_path": "/Game/Materials/MI_Test",
        }
        result = conn.send_command("batch_update_material_parameters", {
            "instance_path": "/Game/Materials/MI_Test",
            "parameters": [
                {"name": "Roughness", "type": "scalar", "value": 0.5},
                {"name": "BaseColor", "type": "vector", "value": [1.0, 0.0, 0.0, 1.0]},
            ],
        })
        assert result["success"] is True
        assert result["updated_count"] == 2

    def test_batch_update_error_envelope(self):
        conn = FakeUnrealConnection()
        conn.responses["batch_update_material_parameters"] = {
            "success": False,
            "error": "Material instance not found: /Game/Materials/MI_Missing",
        }
        result = conn.send_command("batch_update_material_parameters", {
            "instance_path": "/Game/Materials/MI_Missing",
            "parameters": [],
        })
        assert result["success"] is False
        assert "error" in result

    def test_set_static_switch_envelope(self):
        conn = FakeUnrealConnection()
        conn.responses["set_material_static_switch_parameter"] = {
            "success": True,
            "parameter_name": "UseDetail",
            "value": True,
        }
        result = conn.send_command("set_material_static_switch_parameter", {
            "instance_path": "/Game/Materials/MI_Test",
            "parameter_name": "UseDetail",
            "value": True,
        })
        assert result["success"] is True
        assert result["value"] is True


class TestRenderingCommandEnvelopes:
    """Verify rendering commands send and receive properly structured envelopes."""

    def test_set_anti_aliasing_envelope(self):
        conn = FakeUnrealConnection()
        conn.responses["set_anti_aliasing"] = {"success": True, "cvar": "r.AntiAliasingMethod"}
        result = conn.send_command("set_anti_aliasing", {"method": "TSR"})
        assert result["success"] is True
        assert "cvar" in result

    def test_set_lumen_enabled_envelope(self):
        conn = FakeUnrealConnection()
        conn.responses["set_lumen_enabled"] = {"success": True, "cvar": "r.Lumen.DiffuseIndirect.Allow"}
        result = conn.send_command("set_lumen_enabled", {"enabled": True})
        assert result["success"] is True

    def test_get_shader_compile_status_envelope(self):
        conn = FakeUnrealConnection()
        conn.responses["get_shader_compile_status"] = {
            "success": True,
            "remaining_jobs": 0,
            "is_compiling": False,
            "status": "idle",
        }
        result = conn.send_command("get_shader_compile_status", {})
        assert result["success"] is True
        assert "remaining_jobs" in result
        assert "is_compiling" in result
        assert "status" in result

    def test_unknown_rendering_command_error(self):
        conn = FakeUnrealConnection()
        conn.responses["unknown_rendering_cmd"] = {"success": False, "error": "Unknown rendering command"}
        result = conn.send_command("unknown_rendering_cmd", {})
        assert result["success"] is False
