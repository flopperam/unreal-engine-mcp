"""
Unit tests for the sublevel_tool command dispatch.
Uses FakeUnrealConnection to verify command routing without an engine.
"""

import sys
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

import pytest
from server.project_editor_tools import sublevel_tool
from tests.conftest import FakeUnrealConnection


@pytest.fixture(autouse=True)
def patch_unreal_connection(monkeypatch):
    """Inject a FakeUnrealConnection so tests never touch the network."""
    fake = FakeUnrealConnection()
    monkeypatch.setattr(
        "server.project_editor_tools.get_unreal_connection", lambda: fake
    )
    yield fake


class TestSublevelToolDispatch:
    def test_get_persistent(self, patch_unreal_connection):
        result = sublevel_tool(action="get_persistent")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "get_persistent_level",
            "params": {},
        }

    def test_add(self, patch_unreal_connection):
        result = sublevel_tool(action="add", level_path="/Game/Maps/Sub")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "add_sublevel",
            "params": {"level_path": "/Game/Maps/Sub"},
        }

    def test_remove(self, patch_unreal_connection):
        result = sublevel_tool(action="remove", level_name="Sub")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "remove_sublevel",
            "params": {"level_name": "Sub"},
        }

    def test_set_visible(self, patch_unreal_connection):
        result = sublevel_tool(action="set_visible", level_name="Sub", visible=False)
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_sublevel_visible",
            "params": {"level_name": "Sub", "visible": False},
        }

    def test_set_loaded(self, patch_unreal_connection):
        result = sublevel_tool(action="set_loaded", level_name="Sub", loaded=True)
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_sublevel_loaded",
            "params": {"level_name": "Sub", "loaded": True},
        }

    def test_create_volume(self, patch_unreal_connection):
        result = sublevel_tool(
            action="create_volume",
            location=[0.0, 0.0, 0.0],
            extent=[500.0, 500.0, 500.0],
            streaming_levels=["/Game/Maps/A", "/Game/Maps/B"],
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "create_streaming_volume",
            "params": {
                "location": [0.0, 0.0, 0.0],
                "extent": [500.0, 500.0, 500.0],
                "streaming_levels": ["/Game/Maps/A", "/Game/Maps/B"],
            },
        }

    def test_set_streaming(self, patch_unreal_connection):
        result = sublevel_tool(
            action="set_streaming",
            level_name="Sub",
            should_be_loaded=True,
            should_be_visible=False,
            priority=1,
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "set_level_streaming_settings",
            "params": {
                "level_name": "Sub",
                "should_be_loaded": True,
                "should_be_visible": False,
                "priority": 1,
            },
        }

    def test_missing_level_path(self, patch_unreal_connection):
        result = sublevel_tool(action="add")
        assert "error" in result
        assert "level_path is required" in result["error"]

    def test_missing_level_name(self, patch_unreal_connection):
        result = sublevel_tool(action="remove")
        assert "error" in result
        assert "level_name is required" in result["error"]

    def test_unknown_action(self, patch_unreal_connection):
        result = sublevel_tool(action="fly")
        assert "error" in result
        assert "Unknown sublevel_tool action" in result["error"]
