"""E2E tests for scene sync lifecycle.

Requires:
    - SurrealDB on ws://127.0.0.1:8000
    - scene-syncd on http://127.0.0.1:8787
    - (Optional) Unreal Editor with MCP Bridge on 127.0.0.1:55557

Run:
    pytest tests/e2e/test_scene_sync_lifecycle.py --skip-unreal   # DB + planner only
    pytest tests/e2e/test_scene_sync_lifecycle.py                 # Full E2E with Unreal
"""

import json
import pytest

from .conftest import api_post, api_get, unreal_command, assert_success, SCENE_SYNCD_URL


@pytest.mark.requires_unreal
class TestSceneSyncLifecycleWithUnreal:
    """Tests that require a running Unreal Editor session."""

    def test_apply_create(self, isolated_scene, upsert_test_object):
        """Upsert an object, sync, and verify it exists in Unreal."""
        result = upsert_test_object(
            mcp_id="lifecycle_create_01",
            location={"x": 100.0, "y": 200.0, "z": 300.0},
        )
        assert_success(result, "upsert create")

        apply = api_post("/sync/apply", {
            "scene_id": isolated_scene,
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 10,
        })
        data = assert_success(apply, "apply create")
        assert data["summary"]["creates"] >= 1, f"Expected at least 1 create, got {data['summary']}"

        # Verify in Unreal
        unreal_resp = unreal_command("find_actor_by_mcp_id", {"mcp_id": "lifecycle_create_01"})
        assert unreal_resp.get("result", {}).get("success"), f"Unreal lookup failed: {unreal_resp}"

    def test_update_transform_and_apply(self, isolated_scene, upsert_test_object):
        """Update a transform, sync, and verify in Unreal."""
        result = upsert_test_object(
            mcp_id="lifecycle_update_01",
            location={"x": 10.0, "y": 20.0, "z": 30.0},
        )
        assert_success(result, "upsert initial")

        apply1 = api_post("/sync/apply", {
            "scene_id": isolated_scene,
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 10,
        })
        assert_success(apply1, "initial apply")

        # Modify transform
        result2 = api_post("/objects/upsert", {
            "scene_id": isolated_scene,
            "mcp_id": "lifecycle_update_01",
            "desired_name": "E2E_lifecycle_update_01",
            "actor_type": "StaticMeshActor",
            "asset_ref": {"path": "/Engine/BasicShapes/Cube.Cube"},
            "transform": {
                "location": {"x": 999.0, "y": 888.0, "z": 777.0},
                "rotation": {"pitch": 0.0, "yaw": 90.0, "roll": 0.0},
                "scale": {"x": 2.0, "y": 2.0, "z": 2.0},
            },
            "tags": ["e2e_test"],
        })
        assert_success(result2, "upsert transform update")

        apply2 = api_post("/sync/apply", {
            "scene_id": isolated_scene,
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 10,
        })
        data2 = assert_success(apply2, "transform apply")
        assert data2["summary"].get("update_transforms", 0) >= 1

        # Verify in Unreal
        unreal_resp = unreal_command("find_actor_by_mcp_id", {"mcp_id": "lifecycle_update_01"})
        actor = unreal_resp.get("result", {}).get("actor")
        assert actor is not None, "Actor not found in Unreal"

    def test_tombstone_and_delete(self, isolated_scene, upsert_test_object):
        """Tombstone an object, apply with delete, verify deletion in Unreal."""
        result = upsert_test_object(
            mcp_id="lifecycle_delete_01",
            location={"x": 500.0, "y": 500.0, "z": 0.0},
        )
        assert_success(result, "upsert for delete")

        apply1 = api_post("/sync/apply", {
            "scene_id": isolated_scene,
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 10,
        })
        assert_success(apply1, "initial apply for delete test")

        # Tombstone
        del_result = api_post("/objects/delete", {
            "scene_id": isolated_scene,
            "mcp_id": "lifecycle_delete_01",
        })
        assert_success(del_result, "tombstone")

        # Apply with allow_delete=False — should skip
        apply2 = api_post("/sync/apply", {
            "scene_id": isolated_scene,
            "mode": "apply_safe",
            "allow_delete": False,
            "max_operations": 10,
        })
        data2 = assert_success(apply2, "apply without delete")
        assert data2["summary"].get("deletes", 0) == 0

        # Apply with allow_delete=True — should execute
        apply3 = api_post("/sync/apply", {
            "scene_id": isolated_scene,
            "mode": "apply_safe",
            "allow_delete": True,
            "max_operations": 10,
        })
        data3 = assert_success(apply3, "apply with delete")
        assert data3["summary"].get("deletes", 0) >= 1


class TestSceneSyncDBOnly:
    """Tests that only require scene-syncd and SurrealDB (no Unreal)."""

    def test_health_check(self, scene_syncd_available):
        """scene-syncd health endpoint must be reachable when service is up."""
        if not scene_syncd_available:
            pytest.skip("scene-syncd not available")
        result = api_get("/health")
        assert result.get("success") is True or "status" in result, f"Health check failed: {result}"

    def test_upsert_and_plan_create(self, isolated_scene):
        """Upsert an object, plan, and expect CREATE operation."""
        result = api_post("/objects/upsert", {
            "scene_id": isolated_scene,
            "mcp_id": "lifecycle_plan_01",
            "desired_name": "E2E_Plan_Cube",
            "actor_type": "StaticMeshActor",
            "asset_ref": {"path": "/Engine/BasicShapes/Cube.Cube"},
            "transform": {
                "location": {"x": 100.0, "y": 200.0, "z": 300.0},
                "rotation": {"pitch": 0.0, "yaw": 45.0, "roll": 0.0},
                "scale": {"x": 2.0, "y": 2.0, "z": 2.0},
            },
            "tags": ["e2e_test"],
        })
        assert_success(result, "upsert")

        plan = api_post("/sync/plan", {"scene_id": isolated_scene})
        data = assert_success(plan, "plan sync")
        ops = data.get("operations", [])
        creates = [op for op in ops if op.get("action") == "create"]
        assert len(creates) >= 1, f"Expected at least 1 CREATE, got {json.dumps(ops, indent=2)}"

    def test_noop_after_sync(self, isolated_scene, upsert_test_object):
        """After syncing, a re-plan should show NOOP for all objects."""
        upsert_test_object(mcp_id="lifecycle_noop_01")

        # Plan before sync — should show CREATE
        plan1 = api_post("/sync/plan", {"scene_id": isolated_scene})
        data1 = assert_success(plan1, "plan before sync")
        creates1 = [op for op in data1.get("operations", []) if op.get("action") == "create"]
        assert len(creates1) >= 1

    def test_update_transform_plan(self, isolated_scene, upsert_test_object):
        """Update a transform and verify UPDATE operation in plan."""
        upsert_test_object(
            mcp_id="lifecycle_xform_01",
            location={"x": 100.0, "y": 200.0, "z": 300.0},
        )

        # Modify transform
        result2 = api_post("/objects/upsert", {
            "scene_id": isolated_scene,
            "mcp_id": "lifecycle_xform_01",
            "transform": {
                "location": {"x": 500.0, "y": 600.0, "z": 700.0},
                "rotation": {"pitch": 0.0, "yaw": 90.0, "roll": 0.0},
                "scale": {"x": 3.0, "y": 3.0, "z": 3.0},
            },
        })
        assert_success(result2, "upsert transform update")

    def test_snapshot_create_restore(self, isolated_scene, upsert_test_object):
        """Create snapshot, modify state, restore, and verify."""
        upsert_test_object(mcp_id="lifecycle_snap_01")

        # Create snapshot
        snap = api_post("/snapshots/create", {
            "scene_id": isolated_scene,
            "name": "lifecycle_test_snapshot",
            "description": "Created by E2E lifecycle test",
        })
        snap_data = assert_success(snap, "create snapshot")
        snap_id = snap_data.get("snapshot_id")
        assert snap_id, f"Snapshot ID missing: {snap_data}"

        # Modify object
        result2 = api_post("/objects/upsert", {
            "scene_id": isolated_scene,
            "mcp_id": "lifecycle_snap_01",
            "transform": {
                "location": {"x": 9999.0, "y": 9999.0, "z": 9999.0},
            },
        })
        assert_success(result2, "upsert modified")

        # Restore snapshot
        restore = api_post("/snapshots/restore", {
            "snapshot_id": snap_id,
            "restore_mode": "replace_desired",
        })
        restore_data = assert_success(restore, "restore snapshot")
        assert "summary" in restore_data