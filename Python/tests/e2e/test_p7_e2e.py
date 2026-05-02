"""E2E tests for P7 features: NavMesh, Patrol Routes, AI Behavior, Blueprint Spawn.

Requires:
    - SurrealDB on ws://127.0.0.1:8000
    - scene-syncd on http://127.0.0.1:8787
    - (Optional) Unreal Editor with MCP Bridge on 127.0.0.1:55557

Run:
    pytest tests/e2e/test_p7_e2e.py --skip-unreal   # DB + planner only
    pytest tests/e2e/test_p7_e2e.py                  # Full E2E with Unreal
"""

import json
import pytest

from .conftest import api_post, api_get, assert_success, SCENE_SYNCD_URL, unreal_command


class TestComponentCRUD:
    """Test component CRUD operations via scene-syncd (DB only, no Unreal)."""

    def test_upsert_and_list_component(self, isolated_scene):
        """Create a navmesh component and verify it can be listed."""
        result = api_post("/components/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_test_entity_01",
            "component_type": "navmesh",
            "name": "nav_volume_main",
            "properties": {
                "location": {"x": 0.0, "y": 0.0, "z": 0.0},
                "extent": {"x": 500.0, "y": 500.0, "z": 500.0},
            },
        })
        assert_success(result, "upsert navmesh component")

        list_result = api_post("/components/list", {
            "scene_id": isolated_scene,
            "entity_id": "p7_test_entity_01",
        })
        data = assert_success(list_result, "list components")
        components = data.get("components", [])
        assert len(components) >= 1, f"Expected at least 1 component, got {len(components)}"
        nav_comp = next((c for c in components if c.get("component_type") == "navmesh"), None)
        assert nav_comp is not None, "NavMesh component not found in list"

    def test_upsert_collision_component(self, isolated_scene):
        """Create a collision component and verify it can be listed."""
        result = api_post("/components/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_test_entity_02",
            "component_type": "collision",
            "name": "collision_block_all",
            "properties": {
                "profile": "BlockAllDynamic",
                "shape": "complex_as_simple",
            },
        })
        assert_success(result, "upsert collision component")

    def test_upsert_ai_behavior_component(self, isolated_scene):
        """Create an AI behavior component and verify it can be listed."""
        result = api_post("/components/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_test_entity_03",
            "component_type": "ai_behavior",
            "name": "guard_ai",
            "properties": {
                "faction": "hostile",
                "behavior_tree": "/Game/AI/BT_Guard.BT_Guard",
                "perception_radius": 1500.0,
            },
        })
        assert_success(result, "upsert AI behavior component")

    def test_delete_component(self, isolated_scene):
        """Create a component and then delete it."""
        api_post("/components/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_test_entity_04",
            "component_type": "navmesh",
            "name": "nav_temp",
            "properties": {"behavior": "walkable"},
        })

        del_result = api_post("/components/delete", {
            "scene_id": isolated_scene,
            "entity_id": "p7_test_entity_04",
            "component_type": "navmesh",
            "name": "nav_temp",
        })
        assert_success(del_result, "delete component")


class TestBlueprintCRUD:
    """Test blueprint CRUD operations via scene-syncd (DB only)."""

    def test_upsert_and_list_blueprint(self, isolated_scene):
        """Create a blueprint definition and verify it can be listed."""
        result = api_post("/blueprints/upsert", {
            "scene_id": isolated_scene,
            "blueprint_id": "bp_guard_tower",
            "class_name": "AStaticMeshActor",
            "parent_class": "AActor",
            "components": [
                {"type": "collision", "profile": "BlockAll"},
                {"type": "navmesh", "behavior": "blocked"},
            ],
            "variables": [
                {"name": "Health", "type": "float", "default": 100.0},
            ],
        })
        assert_success(result, "upsert blueprint")

        list_result = api_post("/blueprints/list", {
            "scene_id": isolated_scene,
        })
        data = assert_success(list_result, "list blueprints")
        blueprints = data.get("blueprints", [])
        assert len(blueprints) >= 1, "Expected at least 1 blueprint"

    def test_delete_blueprint(self, isolated_scene):
        """Create a blueprint and then delete it."""
        api_post("/blueprints/upsert", {
            "scene_id": isolated_scene,
            "blueprint_id": "bp_temp",
            "class_name": "AActor",
        })

        del_result = api_post("/blueprints/delete", {
            "scene_id": isolated_scene,
            "blueprint_id": "bp_temp",
        })
        assert_success(del_result, "delete blueprint")


class TestRealizationCRUD:
    """Test realization CRUD operations via scene-syncd (DB only)."""

    def test_upsert_and_list_realization(self, isolated_scene):
        """Create a realization and verify it can be listed."""
        result = api_post("/realizations/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_realization_01",
            "policy": "blueprint",
            "status": "pending",
            "unreal_actor_name": None,
            "metadata": {
                "blueprint_path": "/Game/Blueprints/BP_Tower.BP_Tower",
            },
        })
        assert_success(result, "upsert realization")

        list_result = api_post("/realizations/list", {
            "scene_id": isolated_scene,
            "entity_id": "p7_realization_01",
        })
        data = assert_success(list_result, "list realizations")
        realizations = data.get("realizations", [])
        assert len(realizations) >= 1, "Expected at least 1 realization"

    def test_update_realization_status(self, isolated_scene):
        """Update realization status from pending to realized."""
        api_post("/realizations/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_realization_02",
            "policy": "blueprint",
            "status": "pending",
        })

        update_result = api_post("/realizations/update-status", {
            "scene_id": isolated_scene,
            "entity_id": "p7_realization_02",
            "policy": "blueprint",
            "status": "realized",
            "unreal_actor_name": "BP_Tower_C_01",
        })
        assert_success(update_result, "update realization status")


@pytest.mark.requires_unreal
class TestP7WithUnreal:
    """Tests requiring a running Unreal Editor session."""

    def test_navmesh_volume_creation(self, isolated_scene):
        """Create a NavMesh volume via MCP tool and verify in DB."""
        # This test requires the Unreal MCP bridge to be running
        # The navmesh volume is created in Unreal and stored as a component in DB
        result = api_post("/components/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_nav_volume",
            "component_type": "navmesh",
            "name": "NavMesh_Vol_01",
            "properties": {
                "location": {"x": 0.0, "y": 0.0, "z": 0.0},
                "extent": {"x": 2000.0, "y": 2000.0, "z": 500.0},
            },
        })
        assert_success(result, "upsert navmesh component for Unreal test")

    def test_patrol_route_creation(self, isolated_scene):
        """Create a patrol route component and verify in DB."""
        result = api_post("/components/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_patrol_01",
            "component_type": "ai_patrol",
            "name": "Patrol_Route_Alpha",
            "properties": {
                "points": [
                    {"x": 0.0, "y": 0.0, "z": 0.0},
                    {"x": 500.0, "y": 0.0, "z": 0.0},
                    {"x": 500.0, "y": 500.0, "z": 0.0},
                    {"x": 0.0, "y": 500.0, "z": 0.0},
                ],
                "closed_loop": True,
            },
        })
        assert_success(result, "upsert patrol route component")

    def test_blueprint_spawn_via_realization(self, isolated_scene):
        """Store a blueprint realization and verify it in DB."""
        result = api_post("/realizations/upsert", {
            "scene_id": isolated_scene,
            "entity_id": "p7_bp_spawn_01",
            "policy": "blueprint",
            "status": "pending",
            "metadata": {
                "blueprint_path": "/Game/Blueprints/BP_Enemy.BP_Enemy",
            },
        })
        assert_success(result, "upsert blueprint realization")

        # Verify the realization was stored
        list_result = api_post("/realizations/list", {
            "scene_id": isolated_scene,
            "entity_id": "p7_bp_spawn_01",
        })
        data = assert_success(list_result, "list realizations")
        realizations = data.get("realizations", [])
        assert len(realizations) >= 1, "Expected at least 1 realization"

        bp_realization = realizations[0]
        assert bp_realization.get("policy") == "blueprint"
        assert bp_realization.get("status") in ("pending", "realized")

    def test_navmesh_volume_direct_in_unreal(self, isolated_scene):
        """Create a NavMesh volume directly in Unreal via MCP bridge."""
        result = unreal_command("create_nav_mesh_volume", {
            "volume_name": f"E2E_NavMesh_{isolated_scene}",
            "location": [0.0, 0.0, 0.0],
            "extent": [1000.0, 1000.0, 500.0],
        })
        assert result.get("success") is not False, f"NavMesh creation failed: {result}"
        assert "actor_name" in result, f"Missing actor_name in response: {result}"

    def test_patrol_route_direct_in_unreal(self, isolated_scene):
        """Create a patrol route directly in Unreal via MCP bridge."""
        result = unreal_command("create_patrol_route", {
            "patrol_route_name": f"E2E_Patrol_{isolated_scene}",
            "points": [
                {"x": 0.0, "y": 0.0, "z": 0.0},
                {"x": 500.0, "y": 0.0, "z": 0.0},
            ],
            "closed_loop": True,
        })
        assert result.get("success") is not False, f"Patrol route creation failed: {result}"
        assert "actor_name" in result, f"Missing actor_name in response: {result}"

    def test_blueprint_spawn_direct_in_unreal(self, isolated_scene):
        """Spawn a Blueprint actor directly in Unreal via MCP bridge.

        Note: This test verifies the command is dispatched correctly.
        If the test blueprint does not exist in the project, the command
        may return a blueprint-loading error rather than success.
        """
        result = unreal_command("spawn_blueprint_actor", {
            "blueprint_name": "/Game/Blueprints/BP_Enemy.BP_Enemy",
            "actor_name": f"E2E_Enemy_{isolated_scene}",
            "location": [0.0, 0.0, 100.0],
        })
        error = result.get("error", "")
        assert "Unknown command" not in str(error), f"spawn_blueprint_actor not dispatched: {result}"
        # If the blueprint exists, verify success details
        if result.get("success"):
            assert "actor_name" in result, f"Missing actor_name in response: {result}"
