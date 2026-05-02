"""Full Phase 1 procedural generation E2E tests.

Requires scene-syncd, SurrealDB, and Unreal MCP Bridge.
"""

import pytest

from .conftest import api_post, assert_success, unreal_command


@pytest.mark.requires_unreal
class TestPhase1ProceduralGeneration:
    def test_sdf_composite_mesh_reaches_unreal(self, scene_syncd_available):
        if not scene_syncd_available:
            pytest.skip("scene-syncd not available")

        result = api_post("/procedural/sdf-mesh", {
            "mcp_id": "E2E_Phase1_SDF_Composite",
            "actor_name": "E2E_Phase1_SDF_Composite",
            "sdf": {
                "type": "union",
                "smoothness": 25.0,
                "children": [
                    {"type": "sphere", "center": [-60.0, 0.0, 900.0], "radius": 120.0},
                    {"type": "sphere", "center": [60.0, 0.0, 900.0], "radius": 120.0},
                ],
            },
            "bounds": {
                "min": [-220.0, -180.0, 720.0],
                "max": [220.0, 180.0, 1080.0],
            },
            "resolution": 24,
            "focus_viewport": False,
        })

        data = assert_success(result, "create composite SDF mesh")
        unreal = data["unreal_response"]
        assert unreal["success"] is True
        assert unreal["vertex_count"] > 0
        assert unreal["triangle_count"] > 0

    def test_superformula_mesh_reaches_unreal(self, scene_syncd_available):
        if not scene_syncd_available:
            pytest.skip("scene-syncd not available")

        result = api_post("/procedural/superformula-mesh", {
            "mcp_id": "E2E_Phase1_Superformula",
            "actor_name": "E2E_Phase1_Superformula",
            "m1": 8.0,
            "n1_1": 0.45,
            "n2_1": 0.8,
            "n3_1": 0.8,
            "m2": 5.0,
            "n1_2": 0.7,
            "n2_2": 1.2,
            "n3_2": 1.2,
            "resolution": 24,
            "scale": 130.0,
            "location": [500.0, 0.0, 900.0],
            "focus_viewport": False,
        })

        data = assert_success(result, "create superformula mesh")
        unreal = data["unreal_response"]
        assert unreal["success"] is True
        assert unreal["vertex_count"] > 0
        assert unreal["triangle_count"] > 0

    def test_lsystem_spline_reaches_unreal(self, scene_syncd_available):
        if not scene_syncd_available:
            pytest.skip("scene-syncd not available")

        result = api_post("/procedural/lsystem-spline", {
            "mcp_id": "E2E_Phase1_LSystemSpline",
            "spline_name": "E2E_Phase1_LSystemSpline",
            "axiom": "F",
            "rules": [["F", "F[+F]F[-F]F"]],
            "iterations": 2,
            "step_length": 70.0,
            "angle_degrees": 28.0,
            "origin": [900.0, 0.0, 700.0],
            "heading": [0.0, 0.0, 1.0],
            "up": [0.0, 1.0, 0.0],
            "tangent_mode": "curve",
            "focus_viewport": False,
        })

        data = assert_success(result, "create L-System spline")
        assert data["segment_count"] > 0
        unreal = data["unreal_response"]
        assert unreal["success"] is True
        assert unreal["component_count"] >= 1
        assert unreal["point_count"] >= data["segment_count"]

        found = unreal_command("find_actor_by_mcp_id", {"mcp_id": "E2E_Phase1_LSystemSpline"})
        assert found.get("result", {}).get("success") is True
