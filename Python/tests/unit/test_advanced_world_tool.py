"""
Unit tests for the advanced_world_tool command dispatch.
Uses FakeUnrealConnection to verify command routing without an engine.
"""

import sys
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

import pytest
from server.project_editor_tools import advanced_world_tool
from tests.conftest import FakeUnrealConnection


@pytest.fixture(autouse=True)
def patch_unreal_connection(monkeypatch):
    """Inject a FakeUnrealConnection so tests never touch the network."""
    fake = FakeUnrealConnection()
    monkeypatch.setattr(
        "server.project_editor_tools.get_unreal_connection", lambda: fake
    )
    yield fake


class TestAdvancedWorldToolDispatch:
    def test_create_data_layer(self, patch_unreal_connection):
        result = advanced_world_tool(action="create_data_layer", name="MyLayer")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "create_data_layer",
            "params": {"name": "MyLayer"},
        }

    def test_add_actors_to_data_layer(self, patch_unreal_connection):
        result = advanced_world_tool(
            action="add_actors_to_data_layer",
            data_layer_name="MyLayer",
            actor_names=["Actor1", "Actor2"],
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "add_actors_to_data_layer",
            "params": {
                "data_layer_name": "MyLayer",
                "actor_names": ["Actor1", "Actor2"],
            },
        }

    def test_remove_actors_from_data_layer(self, patch_unreal_connection):
        result = advanced_world_tool(
            action="remove_actors_from_data_layer",
            data_layer_name="MyLayer",
            actor_names=["Actor1"],
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "remove_actors_from_data_layer",
            "params": {
                "data_layer_name": "MyLayer",
                "actor_names": ["Actor1"],
            },
        }

    def test_set_data_layer_enabled(self, patch_unreal_connection):
        result = advanced_world_tool(
            action="set_data_layer_enabled",
            data_layer_name="MyLayer",
            enabled=False,
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_data_layer_enabled",
            "params": {"data_layer_name": "MyLayer", "enabled": False},
        }

    def test_create_hlod_layer(self, patch_unreal_connection):
        result = advanced_world_tool(action="create_hlod_layer", name="MyHLOD")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "create_hlod_layer",
            "params": {"name": "MyHLOD"},
        }

    def test_build_hlod(self, patch_unreal_connection):
        result = advanced_world_tool(action="build_hlod", map_path="/Game/Maps/Test")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "build_hlod",
            "params": {"map_path": "/Game/Maps/Test"},
        }

    def test_rebuild_hlod(self, patch_unreal_connection):
        result = advanced_world_tool(action="rebuild_hlod", map_path="/Game/Maps/Test")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "rebuild_hlod",
            "params": {"map_path": "/Game/Maps/Test"},
        }

    def test_set_one_file_per_actor(self, patch_unreal_connection):
        result = advanced_world_tool(action="set_one_file_per_actor", enabled=True)
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_one_file_per_actor",
            "params": {"enable": True},
        }

    def test_set_level_bounds(self, patch_unreal_connection):
        result = advanced_world_tool(
            action="set_level_bounds",
            min_extent=[0.0, 0.0, 0.0],
            max_extent=[1000.0, 1000.0, 500.0],
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_level_bounds",
            "params": {
                "min": [0.0, 0.0, 0.0],
                "max": [1000.0, 1000.0, 500.0],
            },
        }

    def test_set_world_origin_rebasing(self, patch_unreal_connection):
        result = advanced_world_tool(
            action="set_world_origin_rebasing", enabled=True
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_world_origin_rebasing",
            "params": {"enable": True},
        }

    def test_missing_name(self, patch_unreal_connection):
        result = advanced_world_tool(action="create_data_layer")
        assert "error" in result
        assert "name is required" in result["error"]

    def test_missing_data_layer_name(self, patch_unreal_connection):
        result = advanced_world_tool(
            action="add_actors_to_data_layer", actor_names=["Actor1"]
        )
        assert "error" in result
        assert "data_layer_name and actor_names are required" in result["error"]

    def test_unknown_action(self, patch_unreal_connection):
        result = advanced_world_tool(action="fly")
        assert "error" in result
        assert "Unknown advanced_world_tool action" in result["error"]
