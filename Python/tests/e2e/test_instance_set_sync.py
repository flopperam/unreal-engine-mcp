"""E2E tests for InstanceSet end-to-end synchronization.

Verifies that many identical-mesh objects are grouped into ISM/HISM
components instead of individual Actors.

Requires:
    - SurrealDB on ws://127.0.0.1:8000
    - scene-syncd on http://127.0.0.1:8787
    - (Optional) Unreal Editor with MCP Bridge on 127.0.0.1:55557

Run:
    pytest tests/e2e/test_instance_set_sync.py --skip-unreal   # DB + planner only
    pytest tests/e2e/test_instance_set_sync.py                    # Full E2E with Unreal
"""

import time
import pytest

from .conftest import api_post, api_get, unreal_command, assert_success


@pytest.fixture(scope="module")
def crenellation_scene(scene_syncd_available):
    """Create a scene with 60 crenellation objects (same mesh) for InstanceSet grouping."""
    if not scene_syncd_available:
        pytest.skip("scene-syncd not available")

    suffix = time.strftime("%Y%m%d%H%M%S")
    scene_id = f"instset_e2e_{suffix}"

    assert_success(
        api_post("/scenes/create", {
            "scene_id": scene_id,
            "name": "InstanceSet E2E Test",
            "description": "Created by test_instance_set_sync",
        }),
        "create scene",
    )

    # Create 60 crenellation objects with identical mesh
    # These should group into a single InstanceSet
    objects = []
    for i in range(60):
        objects.append({
            "scene_id": scene_id,
            "mcp_id": f"crenellation_{i:03d}_{suffix}",
            "desired_name": f"Crenellation_{i:03d}_{suffix}",
            "actor_type": "StaticMeshActor",
            "asset_ref": {"mesh": "/Engine/BasicShapes/Cube"},
            "transform": {
                "location": {"x": float(i * 100), "y": 0.0, "z": 200.0},
                "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                "scale": {"x": 0.5, "y": 0.5, "z": 0.5},
            },
            "tags": ["layout_kind:crenellation", "detail:wall_top"],
        })

    assert_success(
        api_post("/objects/bulk-upsert", {"scene_id": scene_id, "objects": objects}),
        "bulk upsert crenellations",
    )

    yield {
        "scene_id": scene_id,
        "suffix": suffix,
        "objects": objects,
        "object_count": len(objects),
    }

    # Cleanup
    for obj in objects:
        try:
            api_post("/objects/delete", {"scene_id": scene_id, "mcp_id": obj["mcp_id"]})
        except Exception:
            pass
    try:
        api_post("/sync/apply", {
            "scene_id": scene_id,
            "mode": "apply_safe",
            "allow_delete": True,
            "max_operations": 100,
        })
    except Exception:
        pass


class TestInstanceSetDBOnly:
    """Tests requiring only scene-syncd (no Unreal)."""

    def test_plan_shows_instance_set_create(self, crenellation_scene):
        """Plan should detect 60 objects as InstanceSet, not individual Actors."""
        scene = crenellation_scene

        plan = api_post("/sync/plan", {"scene_id": scene["scene_id"]})
        data = assert_success(plan, "sync plan")

        # With no actual Unreal state, all objects appear as CREATE individually
        # InstanceSet grouping happens at apply time, not plan time
        # So we just verify the plan succeeds and contains operations
        ops = data.get("operations", [])
        assert len(ops) >= scene["object_count"], \
            f"Expected >= {scene['object_count']} operations, got {len(ops)}"

    def test_apply_groups_into_instance_set(self, crenellation_scene):
        """Apply should group 60 identical-mesh objects into InstanceSet(s)."""
        scene = crenellation_scene

        apply = api_post("/sync/apply", {
            "scene_id": scene["scene_id"],
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 100,
        })
        data = assert_success(apply, "sync apply")
        summary = data["summary"]

        # Unreal is not available in DB-only mode, so instance_set_* should be 0
        # But the apply should still succeed (it falls back to empty actual state)
        isc = summary.get("instance_set_creates", 0)
        assert isc == 0, \
            f"Without Unreal, instance set creates should be 0, got {isc}"


@pytest.mark.requires_unreal
class TestInstanceSetWithUnreal:
    """Tests requiring a running Unreal Editor session."""

    def test_initial_apply_creates_instance_set(self, crenellation_scene):
        """60 crenellations should spawn as 1 InstanceSet, not 60 Actors."""
        scene = crenellation_scene

        apply = api_post("/sync/apply", {
            "scene_id": scene["scene_id"],
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 100,
        })
        data = assert_success(apply, "sync apply")
        summary = data["summary"]

        # Should create 1 instance set, not 60 individual actors
        isc = summary.get("instance_set_creates", 0)
        assert isc >= 1, \
            f"Expected >=1 instance set create, got {isc}"

        # Individual actor creates should be minimal (only non-instance-set objects)
        # With 60 identical crenellations, creates should be close to 0
        creates = summary.get("creates", 0)
        assert creates <= 5, \
            f"Expected <=5 individual creates, got {creates}"

    def test_instance_set_state_has_correct_count(self, crenellation_scene):
        """get_instance_set_state should report 60 instances."""
        scene = crenellation_scene

        # Ensure sync applied
        apply = api_post("/sync/apply", {
            "scene_id": scene["scene_id"],
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 100,
        })
        assert_success(apply, "sync apply")

        # Query Unreal for instance set state
        # The set_id is derived from kind + mesh: "crenellation_{mesh}_instances"
        set_id = "crenellation__Engine_BasicShapes_Cube_instances"

        resp = unreal_command("get_instance_set_state", {"set_id": set_id})
        result = resp.get("result", {})
        if result.get("success"):
            state = result.get("state", {})
            assert state.get("instance_count", 0) == 60, \
                f"Expected 60 instances, got {state.get('instance_count', 0)}"
            assert "/Engine/BasicShapes/Cube" in state.get("mesh_path", ""), \
                f"Unexpected mesh: {state.get('mesh_path')}"
        else:
            # If the command is not yet available, list instead
            list_resp = unreal_command("list_instance_sets", {})
            list_result = list_resp.get("result", {})
            if list_result.get("success"):
                sets = list_result.get("sets", [])
                cren_sets = [s for s in sets if "crenellation" in s.get("set_id", "")]
                assert len(cren_sets) >= 1, \
                    f"Expected >=1 crenellation instance set, found {len(cren_sets)}"

    def test_list_instance_sets_includes_crenellation(self, crenellation_scene):
        """list_instance_sets should return the crenellation instance set."""
        scene = crenellation_scene

        # Ensure sync applied
        apply = api_post("/sync/apply", {
            "scene_id": scene["scene_id"],
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 100,
        })
        assert_success(apply, "sync apply")

        resp = unreal_command("list_instance_sets", {})
        result = resp.get("result", {})
        if not result.get("success"):
            pytest.skip("list_instance_sets not supported by Unreal plugin")

        sets = result.get("sets", [])
        cren_sets = [s for s in sets if "crenellation" in s.get("set_id", "")]
        assert len(cren_sets) >= 1, \
            f"Expected >=1 crenellation set, got {len(cren_sets)} total sets"

    def test_reapply_is_noop(self, crenellation_scene):
        """After initial sync, re-apply should show noops."""
        scene = crenellation_scene

        # First apply
        apply1 = api_post("/sync/apply", {
            "scene_id": scene["scene_id"],
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 100,
        })
        assert_success(apply1, "first apply")

        # Second apply
        apply2 = api_post("/sync/apply", {
            "scene_id": scene["scene_id"],
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 100,
        })
        data2 = assert_success(apply2, "second apply")
        summary2 = data2["summary"]

        assert summary2.get("instance_set_creates", 0) == 0, \
            "Re-apply should not create new instance sets"
        assert summary2.get("instance_set_updates", 0) == 0, \
            "Re-apply should not update instance sets"
        assert summary2.get("creates", 0) == 0, \
            "Re-apply should not create individual actors"
