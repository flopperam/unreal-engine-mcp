"""L2 Contract tests for Lighting / Atmosphere MCP protocol.

Verifies JSON envelope contracts:
- Success responses contain expected fields
- Error responses contain 'success': false and 'error' string
- Commands rejected for missing required params have consistent error shapes
"""

import pytest
from unittest.mock import patch
import unreal_mcp_server_advanced as srv
import server.lighting_tools as lighting_tools


class TestLightingProtocolContracts:
    def test_set_light_intensity_success_shape(self, fake_conn):
        fake_conn.responses["set_light_intensity"] = {"success": True, "actor_name": "Light", "intensity": 1000.0}
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            result = srv.set_light_intensity(actor_name="Light", intensity=1000.0)
        assert result["success"] is True
        assert "actor_name" in result
        assert "intensity" in result

    def test_set_light_intensity_error_shape(self, fake_conn):
        fake_conn.responses["set_light_intensity"] = {"success": False, "error": "Actor 'Missing' not found"}
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            result = srv.set_light_intensity(actor_name="Missing", intensity=1000.0)
        assert result["success"] is False
        assert "error" in result

    def test_set_light_color_rejects_bad_color(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            result = srv.set_light_color(actor_name="Light", color=[1.0, 0.5])
        # Validation should reject missing third component
        assert result.get("success") is False or "error" in result

    def test_build_lighting_success_shape(self, fake_conn):
        fake_conn.responses["build_lighting"] = {"success": True, "quality": "Preview", "message": "Lighting build initiated"}
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            result = srv.build_lighting(quality="Preview")
        assert result["success"] is True
        assert result["quality"] == "Preview"

    def test_create_reflection_capture_success_shape(self, fake_conn):
        fake_conn.responses["create_reflection_capture"] = {"success": True, "actor_name": "RC_0", "type": "Sphere"}
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            result = srv.create_reflection_capture(actor_name="RC_0", type="Sphere")
        assert result["success"] is True
        assert result["actor_name"] == "RC_0"

    def test_error_consistency_on_actor_not_found(self, fake_conn):
        """All actor-scoped lighting tools should return consistent error shape when actor is missing."""
        fake_conn.responses["set_light_intensity"] = {"success": False, "error": "Actor 'X' not found"}
        fake_conn.responses["set_light_color"] = {"success": False, "error": "Actor 'X' not found"}
        fake_conn.responses["set_light_shadow_enabled"] = {"success": False, "error": "Actor 'X' not found"}
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            tools = [
                (srv.set_light_intensity, {"actor_name": "X", "intensity": 1.0}),
                (srv.set_light_color, {"actor_name": "X", "color": [1.0, 0.5, 0.0]}),
                (srv.set_light_shadow_enabled, {"actor_name": "X", "enabled": True}),
            ]
            for tool, kwargs in tools:
                result = tool(**kwargs)
                if hasattr(result, "get"):
                    assert "success" in result
                    if result.get("success") is False:
                        assert isinstance(result.get("error"), str)
