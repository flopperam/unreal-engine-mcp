"""Unit tests for P7 MCP tools (NavMesh, Patrol, AI, Blueprint, Component).

These tests use FakeUnrealConnection and mock scene-syncd responses
to verify tool behavior without external services.
"""

import json
import pytest
from unittest.mock import patch, MagicMock

from server.specs.component_spec import CollisionSpec, NavSpec, AISpec, LightConfigSpec, MeshComponentSpec


class TestComponentSpecs:
    """Test P6/P7 component spec dataclasses."""

    def test_collision_spec_defaults(self):
        spec = CollisionSpec()
        assert spec.profile == "BlockAllDynamic"
        assert spec.shape == "simple_box"

    def test_collision_spec_custom(self):
        spec = CollisionSpec(profile="BlockAll", shape="complex_as_simple")
        assert spec.profile == "BlockAll"
        assert spec.shape == "complex_as_simple"

    def test_nav_spec_defaults(self):
        spec = NavSpec()
        assert spec.behavior == "walkable"

    def test_nav_spec_blocked(self):
        spec = NavSpec(behavior="blocked")
        assert spec.behavior == "blocked"

    def test_nav_spec_jump_link(self):
        spec = NavSpec(behavior="jump_link")
        assert spec.behavior == "jump_link"

    def test_ai_spec_defaults(self):
        spec = AISpec()
        assert spec.faction == "neutral"
        assert spec.behavior_tree is None
        assert spec.patrol_points == []
        assert spec.perception_radius == 1000.0

    def test_ai_spec_custom(self):
        points = [[0, 0, 0], [100, 0, 0], [100, 100, 0]]
        spec = AISpec(
            faction="hostile",
            behavior_tree="/Game/AI/BT_Guard",
            patrol_points=points,
            perception_radius=2000.0,
        )
        assert spec.faction == "hostile"
        assert spec.behavior_tree == "/Game/AI/BT_Guard"
        assert len(spec.patrol_points) == 3
        assert spec.perception_radius == 2000.0

    def test_light_config_spec_defaults(self):
        spec = LightConfigSpec()
        assert spec.light_type == "point"
        assert spec.intensity == 5000.0
        assert spec.color == [1.0, 1.0, 1.0]
        assert spec.attenuation_radius == 1000.0
        assert spec.mobility == "Stationary"
        assert spec.cast_shadows is True

    def test_mesh_component_spec(self):
        spec = MeshComponentSpec(mesh_path="/Game/Meshes/SM_Rock")
        assert spec.mesh_path == "/Game/Meshes/SM_Rock"
        assert spec.material_path is None
        assert spec.lod_level == 0


class TestP7ToolRegistration:
    """Verify P7 tools are registered with the MCP server."""

    def test_navmesh_tool_registered(self):
        from server.scene_tools import scene_create_navmesh_volume
        assert scene_create_navmesh_volume is not None

    def test_patrol_route_tool_registered(self):
        from server.scene_tools import scene_create_patrol_route
        assert scene_create_patrol_route is not None

    def test_ai_behavior_tool_registered(self):
        from server.scene_tools import scene_set_ai_behavior
        assert scene_set_ai_behavior is not None

    def test_blueprint_spawn_tool_registered(self):
        from server.scene_tools import scene_spawn_blueprint
        assert scene_spawn_blueprint is not None

    def test_component_upsert_tool_registered(self):
        from server.scene_tools import scene_component_upsert
        assert scene_component_upsert is not None


class TestSceneCreateNavmeshVolume:
    """Test scene_create_navmesh_volume tool logic."""

    @patch("server.scene_tools.call_scene_syncd")
    @patch("server.core.get_unreal_connection")
    def test_navmesh_volume_calls_unreal_and_db(self, mock_get_conn, mock_syncd):
        mock_conn = MagicMock()
        mock_conn.send_command.return_value = {"success": True, "actor_name": "NavMeshVolume_0"}
        mock_get_conn.return_value = mock_conn
        mock_syncd.return_value = {"success": True, "data": {"id": "comp_01"}}

        from server.scene_tools import scene_create_navmesh_volume
        result = scene_create_navmesh_volume(
            scene_id="test_scene",
            volume_name="TestNavVolume",
            location={"x": 100.0, "y": 200.0, "z": 300.0},
            extent={"x": 500.0, "y": 500.0, "z": 500.0},
        )

        assert result["success"] is True
        mock_conn.send_command.assert_called_once_with(
            "create_nav_mesh_volume",
            {
                "volume_name": "TestNavVolume",
                "location": [100.0, 200.0, 300.0],
                "extent": [500.0, 500.0, 500.0],
            },
        )
        mock_syncd.assert_called_once()
        call_args = mock_syncd.call_args
        assert call_args[0][0] == "/components/upsert"
        assert call_args[0][1]["component_type"] == "navmesh"

    @patch("server.scene_tools.call_scene_syncd")
    @patch("server.core.get_unreal_connection")
    def test_navmesh_volume_default_params(self, mock_get_conn, mock_syncd):
        mock_conn = MagicMock()
        mock_conn.send_command.return_value = {"success": True}
        mock_get_conn.return_value = mock_conn
        mock_syncd.return_value = {"success": True}

        from server.scene_tools import scene_create_navmesh_volume
        result = scene_create_navmesh_volume(scene_id="test_scene")

        assert result["success"] is True
        call_args = mock_conn.send_command.call_args[0][1]
        assert call_args["volume_name"] == "NavMeshVolume"
        assert call_args["location"] == [0.0, 0.0, 0.0]
        assert call_args["extent"] == [500.0, 500.0, 500.0]


class TestSceneCreatePatrolRoute:
    """Test scene_create_patrol_route tool logic."""

    @patch("server.scene_tools.call_scene_syncd")
    @patch("server.core.get_unreal_connection")
    def test_patrol_route_creates_route(self, mock_get_conn, mock_syncd):
        mock_conn = MagicMock()
        mock_conn.send_command.return_value = {"success": True, "actor_name": "PatrolRoute_0"}
        mock_get_conn.return_value = mock_conn
        mock_syncd.return_value = {"success": True}

        from server.scene_tools import scene_create_patrol_route
        points = [{"x": 0.0, "y": 0.0, "z": 0.0}, {"x": 100.0, "y": 0.0, "z": 0.0}]
        result = scene_create_patrol_route(
            scene_id="test_scene",
            route_name="Patrol_Alpha",
            points=points,
            closed_loop=True,
        )

        assert result["success"] is True
        mock_conn.send_command.assert_called_once()
        call_args = mock_conn.send_command.call_args[0][1]
        assert call_args["patrol_route_name"] == "Patrol_Alpha"
        assert call_args["closed_loop"] is True

    def test_patrol_route_requires_two_points(self):
        from server.scene_tools import scene_create_patrol_route
        result = scene_create_patrol_route(
            scene_id="test_scene",
            route_name="BadRoute",
            points=[{"x": 0.0, "y": 0.0, "z": 0.0}],
        )
        assert "error" in result or result.get("success") is False


class TestSceneSetAIBehavior:
    """Test scene_set_ai_behavior tool logic."""

    @patch("server.scene_tools.call_scene_syncd")
    @patch("server.core.get_unreal_connection")
    def test_set_ai_behavior(self, mock_get_conn, mock_syncd):
        mock_conn = MagicMock()
        mock_conn.send_command.return_value = {"success": True}
        mock_get_conn.return_value = mock_conn
        mock_syncd.return_value = {"success": True}

        from server.scene_tools import scene_set_ai_behavior
        result = scene_set_ai_behavior(
            scene_id="test_scene",
            entity_id="guard_01",
            actor_name="Guard_Enemy",
            behavior_tree="/Game/AI/BT_Guard",
            perception_radius=1500.0,
        )

        assert result["success"] is True
        call_args = mock_conn.send_command.call_args[0][1]
        assert call_args["actor_name"] == "Guard_Enemy"
        assert call_args["behavior_tree_path"] == "/Game/AI/BT_Guard"
        assert call_args["perception_radius"] == 1500.0


class TestSceneSpawnBlueprint:
    """Test scene_spawn_blueprint tool logic."""

    @patch("server.scene_tools.call_scene_syncd")
    @patch("server.core.get_unreal_connection")
    def test_spawn_blueprint(self, mock_get_conn, mock_syncd):
        mock_conn = MagicMock()
        mock_conn.send_command.return_value = {"success": True, "actor_name": "BP_Tower_C_0"}
        mock_get_conn.return_value = mock_conn
        mock_syncd.return_value = {"success": True}

        from server.scene_tools import scene_spawn_blueprint
        result = scene_spawn_blueprint(
            scene_id="test_scene",
            entity_id="tower_01",
            blueprint_path="/Game/Blueprints/BP_Tower.BP_Tower",
            actor_name="Tower_01",
            location={"x": 100.0, "y": 200.0, "z": 0.0},
        )

        assert result["success"] is True
        call_args = mock_conn.send_command.call_args[0][1]
        assert call_args["blueprint_name"] == "/Game/Blueprints/BP_Tower.BP_Tower"
        assert call_args["actor_name"] == "Tower_01"

        # Check realization stored in DB
        syncd_call = mock_syncd.call_args[0]
        assert syncd_call[0] == "/realizations/upsert"
        assert syncd_call[1]["policy"] == "blueprint"


class TestSceneComponentUpsert:
    """Test scene_component_upsert tool logic."""

    @patch("server.scene_tools.call_scene_syncd")
    def test_component_upsert_collision(self, mock_syncd):
        mock_syncd.return_value = {"success": True, "data": {"id": "comp_01"}}

        from server.scene_tools import scene_component_upsert
        result = scene_component_upsert(
            scene_id="test_scene",
            entity_id="wall_01",
            component_type="collision",
            name="collision_block_all",
            properties={"profile": "BlockAllDynamic", "shape": "complex_as_simple"},
        )

        mock_syncd.assert_called_once()
        call_args = mock_syncd.call_args[0]
        assert call_args[0] == "/components/upsert"
        assert call_args[1]["component_type"] == "collision"
        assert call_args[1]["properties"]["profile"] == "BlockAllDynamic"

    @patch("server.scene_tools.call_scene_syncd")
    def test_component_upsert_navmesh(self, mock_syncd):
        mock_syncd.return_value = {"success": True}

        from server.scene_tools import scene_component_upsert
        result = scene_component_upsert(
            scene_id="test_scene",
            entity_id="ground_01",
            component_type="navmesh",
            name="nav_walkable",
            properties={"behavior": "walkable"},
        )

        call_args = mock_syncd.call_args[0]
        assert call_args[1]["component_type"] == "navmesh"

    @patch("server.scene_tools.call_scene_syncd")
    def test_component_upsert_missing_required(self, mock_syncd):
        from server.scene_tools import scene_component_upsert
        result = scene_component_upsert(
            scene_id="test_scene",
            entity_id="",
            component_type="navmesh",
            name="test",
        )
        assert "error" in result or result.get("success") is False


class TestP7ToolErrorCases:
    """Error and boundary case tests for P7 tools."""

    @patch("server.scene_tools.call_scene_syncd")
    @patch("server.core.get_unreal_connection")
    def test_navmesh_volume_unreal_connection_failure(self, mock_get_conn, mock_syncd):
        """When Unreal connection fails, tool should return an error response."""
        mock_get_conn.side_effect = RuntimeError("Unreal bridge unreachable")
        mock_syncd.return_value = {"success": True}

        from server.scene_tools import scene_create_navmesh_volume
        result = scene_create_navmesh_volume(
            scene_id="test_scene",
            volume_name="TestNavVolume",
            location={"x": 100.0, "y": 200.0, "z": 300.0},
            extent={"x": 500.0, "y": 500.0, "z": 500.0},
        )

        assert result.get("success") is False or "error" in result
        assert "Unreal" in str(result.get("error", "")) or "bridge" in str(result.get("error", ""))

    @patch("server.scene_tools.call_scene_syncd")
    @patch("server.core.get_unreal_connection")
    def test_spawn_blueprint_invalid_path(self, mock_get_conn, mock_syncd):
        """When Unreal reports blueprint spawn failure, tool should return error."""
        mock_conn = MagicMock()
        mock_conn.send_command.return_value = {"success": False, "error": "Blueprint not found"}
        mock_get_conn.return_value = mock_conn
        mock_syncd.return_value = {"success": True}

        from server.scene_tools import scene_spawn_blueprint
        result = scene_spawn_blueprint(
            scene_id="test_scene",
            entity_id="tower_01",
            blueprint_path="/Game/Blueprints/BP_NonExistent",
            actor_name="Tower_01",
        )

        assert result.get("success") is False
        assert "Blueprint not found" in result.get("error", "")

    @patch("server.scene_tools.call_scene_syncd")
    def test_component_upsert_empty_properties(self, mock_syncd):
        """Component upsert with empty properties should still succeed."""
        mock_syncd.return_value = {"success": True, "data": {"id": "comp_empty"}}

        from server.scene_tools import scene_component_upsert
        result = scene_component_upsert(
            scene_id="test_scene",
            entity_id="entity_01",
            component_type="collision",
            name="collision_empty",
            properties={},
        )

        assert result.get("success") is True
        call_args = mock_syncd.call_args[0]
        assert call_args[0] == "/components/upsert"
        assert call_args[1]["properties"] == {}