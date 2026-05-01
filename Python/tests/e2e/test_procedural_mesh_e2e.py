"""E2E tests for procedural mesh pipeline.

Requires:
    - SurrealDB on ws://127.0.0.1:8000
    - scene-syncd on http://127.0.0.1:8787
    - (Optional) Unreal Editor with MCP Bridge on 127.0.0.1:55557

Run:
    pytest tests/e2e/test_procedural_mesh_e2e.py --skip-unreal   # API only
    pytest tests/e2e/test_procedural_mesh_e2e.py                 # Full E2E with Unreal
"""

import pytest

from .conftest import api_post, assert_success


@pytest.mark.requires_unreal
class TestProceduralMeshWithUnreal:
    """Tests that require a running Unreal Editor session.

    The /procedural/create-mesh endpoint always forwards to Unreal via TCP,
    so these tests require a live Unreal MCP bridge.
    """

    def test_create_procedural_mesh_triangle(self, scene_syncd_available):
        """Send a simple triangle through the HTTP API to Unreal."""
        if not scene_syncd_available:
            pytest.skip("scene-syncd not available")

        result = api_post("/procedural/create-mesh", {
            "vertex_count": 3,
            "index_count": 6,
            "positions": [
                [0.0, 0.0, 700.0],
                [2200.0, 0.0, 700.0],
                [0.0, 2200.0, 700.0],
            ],
            "normals": [
                [0.0, 0.0, 1.0],
                [0.0, 0.0, 1.0],
                [0.0, 0.0, 1.0],
            ],
            "indices": [0, 1, 2, 2, 1, 0],
            "actor_name": "E2E_Triangle",
            "focus_viewport": True,
        })
        data = assert_success(result, "create procedural mesh triangle")
        assert "unreal_response" in data
        assert data["unreal_response"].get("success") is True
        assert data["unreal_response"].get("actor_name") == "E2E_Triangle"

    def test_create_procedural_mesh_with_uvs(self, scene_syncd_available):
        """Send a triangle with UVs through the HTTP API to Unreal."""
        if not scene_syncd_available:
            pytest.skip("scene-syncd not available")

        result = api_post("/procedural/create-mesh", {
            "vertex_count": 3,
            "index_count": 6,
            "positions": [
                [0.0, 0.0, 750.0],
                [2200.0, 0.0, 750.0],
                [0.0, 2200.0, 750.0],
            ],
            "normals": [
                [0.0, 0.0, 1.0],
                [0.0, 0.0, 1.0],
                [0.0, 0.0, 1.0],
            ],
            "uvs": [
                [0.0, 0.0],
                [1.0, 0.0],
                [0.0, 1.0],
            ],
            "indices": [0, 1, 2, 2, 1, 0],
            "actor_name": "E2E_Triangle_UVs",
            "flags": 1,
            "focus_viewport": True,
        })
        data = assert_success(result, "create procedural mesh with UVs")
        assert "unreal_response" in data
        assert data["unreal_response"].get("success") is True

    def test_create_procedural_mesh_in_unreal(self, scene_syncd_available):
        """Create a procedural mesh and verify it appears in Unreal."""
        if not scene_syncd_available:
            pytest.skip("scene-syncd not available")

        result = api_post("/procedural/create-mesh", {
            "vertex_count": 3,
            "index_count": 6,
            "positions": [
                [0.0, 0.0, 800.0],
                [2200.0, 0.0, 800.0],
                [0.0, 2200.0, 800.0],
            ],
            "normals": [
                [0.0, 0.0, 1.0],
                [0.0, 0.0, 1.0],
                [0.0, 0.0, 1.0],
            ],
            "indices": [0, 1, 2, 2, 1, 0],
            "actor_name": "E2E_Unreal_Triangle",
            "scale": [1.0, 1.0, 1.0],
            "focus_viewport": True,
        })
        data = assert_success(result, "create procedural mesh in Unreal")
        assert "unreal_response" in data
        assert data["unreal_response"].get("success") is True
        assert data["unreal_response"].get("actor_name") == "E2E_Unreal_Triangle"
        assert data["unreal_response"].get("bounds_extent")
        # Verify elapsed time is present
        assert "elapsed_ms" in data
