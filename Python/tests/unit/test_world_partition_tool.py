"""
Unit tests for the world_partition_tool command dispatch.
Uses FakeUnrealConnection to verify command routing without an engine.
"""

import sys
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

import pytest
from server.project_editor_tools import world_partition_tool
from tests.conftest import FakeUnrealConnection


@pytest.fixture(autouse=True)
def patch_unreal_connection(monkeypatch):
    """Inject a FakeUnrealConnection so tests never touch the network."""
    fake = FakeUnrealConnection()
    monkeypatch.setattr(
        "server.project_editor_tools.get_unreal_connection", lambda: fake
    )
    yield fake


class TestWorldPartitionToolDispatch:
    def test_enable(self, patch_unreal_connection):
        result = world_partition_tool(action="enable", enable=True)
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "enable_world_partition",
            "params": {"enable": True},
        }

    def test_set_grid(self, patch_unreal_connection):
        result = world_partition_tool(
            action="set_grid",
            placement_grid_size=12800,
            foliage_grid_size=25600,
            minimap_threshold=512,
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_world_partition_grid",
            "params": {
                "placement_grid_size": 12800,
                "foliage_grid_size": 25600,
                "minimap_threshold": 512,
            },
        }

    def test_get_cells(self, patch_unreal_connection):
        result = world_partition_tool(action="get_cells")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "get_world_partition_cells",
            "params": {},
        }

    def test_load_cell(self, patch_unreal_connection):
        result = world_partition_tool(
            action="load_cell",
            min_x=0.0,
            min_y=0.0,
            min_z=0.0,
            max_x=1000.0,
            max_y=1000.0,
            max_z=500.0,
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "load_world_partition_cell",
            "params": {
                "min_x": 0.0,
                "min_y": 0.0,
                "min_z": 0.0,
                "max_x": 1000.0,
                "max_y": 1000.0,
                "max_z": 500.0,
            },
        }

    def test_unload_cell(self, patch_unreal_connection):
        result = world_partition_tool(action="unload_cell")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "unload_world_partition_cell",
            "params": {},
        }

    def test_missing_enable(self, patch_unreal_connection):
        result = world_partition_tool(action="enable")
        assert "error" in result
        assert "enable is required" in result["error"]

    def test_unknown_action(self, patch_unreal_connection):
        result = world_partition_tool(action="fly")
        assert "error" in result
        assert "Unknown world_partition_tool action" in result["error"]
