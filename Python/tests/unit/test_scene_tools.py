"""L1 Unit tests for scene_tools MCP tools.

Verifies that scene-syncd-facing tools build the correct payloads
and perform input validation without requiring a live Unreal Editor
or scene-syncd service.
"""

import pytest
from unittest.mock import patch, MagicMock

import server.scene_tools as scene_tools


class TestSceneCreate:
    def test_sends_correct_payload(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_call:
            mock_call.return_value = {"success": True}
            result = scene_tools.scene_create(scene_id="test_scene", name="Test", description="A scene")

        mock_call.assert_called_once()
        args = mock_call.call_args
        assert args[0][0] == "/scenes/create"
        assert args[0][1]["scene_id"] == "test_scene"
        assert args[0][1]["name"] == "Test"
        assert args[0][1]["description"] == "A scene"

    def test_missing_optional_fields(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_call:
            mock_call.return_value = {"success": True}
            result = scene_tools.scene_create()

        args = mock_call.call_args
        payload = args[0][1]
        assert payload["scene_id"] == "main"
        assert "name" not in payload
        assert "description" not in payload


class TestSceneUpsertActor:
    def test_sends_full_payload(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_call:
            mock_call.return_value = {"success": True}
            result = scene_tools.scene_upsert_actor(
                scene_id="main",
                mcp_id="actor_01",
                desired_name="MyActor",
                actor_type="StaticMeshActor",
                asset_ref={"path": "/Engine/BasicShapes/Cube"},
                transform={"location": {"x": 1, "y": 2, "z": 3}},
                tags=["wall"],
            )

        mock_call.assert_called_once()
        args = mock_call.call_args
        payload = args[0][1]
        assert payload["scene_id"] == "main"
        assert payload["mcp_id"] == "actor_01"
        assert payload["desired_name"] == "MyActor"
        assert payload["actor_type"] == "StaticMeshActor"
        assert payload["asset_ref"]["path"] == "/Engine/BasicShapes/Cube"
        assert payload["transform"]["location"]["x"] == 1
        assert payload["tags"] == ["wall"]

    def test_rejects_empty_mcp_id(self):
        with patch("server.scene_tools.call_scene_syncd"):
            result = scene_tools.scene_upsert_actor(scene_id="main", mcp_id="")
        assert result.get("success") is False
        assert "mcp_id" in result.get("error", "").lower() or "validation" in result.get("error", "").lower()


class TestSceneDeleteActor:
    def test_sends_delete_payload(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_call:
            mock_call.return_value = {"success": True}
            result = scene_tools.scene_delete_actor(scene_id="main", mcp_id="actor_01")

        mock_call.assert_called_once_with("/objects/delete", {"scene_id": "main", "mcp_id": "actor_01"})

    def test_rejects_empty_mcp_id(self):
        with patch("server.scene_tools.call_scene_syncd"):
            result = scene_tools.scene_delete_actor(scene_id="main", mcp_id="")
        assert result.get("success") is False


class TestScenePlanSync:
    def test_sends_plan_payload(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_call:
            mock_call.return_value = {"success": True, "data": {"summary": {}}}
            result = scene_tools.scene_plan_sync(scene_id="main", mode="plan_only")

        mock_call.assert_called_once()
        args = mock_call.call_args
        assert args[0][0] == "/sync/plan"
        payload = args[0][1]
        assert payload["scene_id"] == "main"
        assert payload["mode"] == "plan_only"

    def test_orphan_policy_passed(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_call:
            mock_call.return_value = {"success": True, "data": {"summary": {}}}
            result = scene_tools.scene_plan_sync(scene_id="main", orphan_policy="adopt")

        payload = mock_call.call_args[0][1]
        assert payload["orphan_policy"] == "adopt"


class TestSceneSync:
    def test_sends_apply_payload(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_call:
            mock_call.return_value = {"success": True}
            result = scene_tools.scene_sync(scene_id="main", mode="apply_safe", allow_delete=True)

        args = mock_call.call_args
        payload = args[0][1]
        assert payload["scene_id"] == "main"
        assert payload["mode"] == "apply_safe"
        assert payload["allow_delete"] is True
        assert payload["max_operations"] == 500


class TestSceneCreateWall:
    def test_rejects_zero_segments(self):
        with patch("server.scene_tools.call_scene_syncd"):
            result = scene_tools.scene_create_wall(segments=0)
        assert result.get("success") is False
        assert "segments" in result.get("error", "").lower()

    def test_rejects_bad_axis(self):
        with patch("server.scene_tools.call_scene_syncd"):
            result = scene_tools.scene_create_wall(axis="z")
        assert result.get("success") is False
        assert "axis" in result.get("error", "").lower()


class TestSceneCreatePyramid:
    def test_rejects_zero_levels(self):
        with patch("server.scene_tools.call_scene_syncd"):
            result = scene_tools.scene_create_pyramid(levels=0)
        assert result.get("success") is False
        assert "levels" in result.get("error", "").lower()

    def test_rejects_zero_block_size(self):
        with patch("server.scene_tools.call_scene_syncd"):
            result = scene_tools.scene_create_pyramid(block_size=0)
        assert result.get("success") is False
        assert "block_size" in result.get("error", "").lower()


class TestSceneNavMeshAndPatrol:
    def test_navmesh_sends_component_payload(self):
        with patch("server.scene_tools.call_scene_syncd") as mock_ss, \
             patch("server.core.get_unreal_connection") as mock_ue:
            mock_ue.return_value = MagicMock()
            mock_ue.return_value.send_command.return_value = {"success": True}
            mock_ss.return_value = {"success": True}

            result = scene_tools.scene_create_navmesh_volume(
                scene_id="main", volume_name="Nav1",
                location={"x": 0, "y": 0, "z": 0},
                extent={"x": 500, "y": 500, "z": 500},
            )

        assert result["success"] is True
        ue_call = mock_ue.return_value.send_command.call_args
        assert ue_call[0][1]["volume_name"] == "Nav1"
        assert ue_call[0][1]["extent"] == [500.0, 500.0, 500.0]

    def test_patrol_requires_two_points(self):
        with patch("server.scene_tools.call_scene_syncd"):
            result = scene_tools.scene_create_patrol_route(points=[])
        assert result.get("success") is False
        assert "at least 2" in result.get("error", "").lower()


class TestDraftProxyHelpers:
    def test_extract_layout_kind_from_proxy_group(self):
        obj = {"visual": {"draft": {"proxy_group": "towers"}}, "tags": []}
        kind = scene_tools._extract_layout_kind(obj)
        assert kind == "towers"

    def test_extract_layout_kind_from_tag(self):
        obj = {"visual": {}, "tags": ["layout_kind:bridges"]}
        kind = scene_tools._extract_layout_kind(obj)
        assert kind == "bridges"

    def test_extract_layout_kind_default(self):
        obj = {"visual": {}, "tags": []}
        kind = scene_tools._extract_layout_kind(obj)
        assert kind == "layout"
