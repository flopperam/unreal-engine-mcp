"""
Unit tests for the level_tool command dispatch.
Uses FakeUnrealConnection to verify command routing without an engine.
"""

import sys
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

import pytest
from server.project_editor_tools import level_tool
from tests.conftest import FakeUnrealConnection


@pytest.fixture(autouse=True)
def patch_unreal_connection(monkeypatch):
    """Inject a FakeUnrealConnection so tests never touch the network."""
    fake = FakeUnrealConnection()
    monkeypatch.setattr(
        "server.project_editor_tools.get_unreal_connection", lambda: fake
    )
    yield fake


class TestLevelToolDispatch:
    def test_create(self, patch_unreal_connection):
        result = level_tool(action="create", asset_path="/Game/Maps/TestLevel")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "create_level",
            "params": {"asset_path": "/Game/Maps/TestLevel"},
        }

    def test_save(self, patch_unreal_connection):
        result = level_tool(action="save", asset_path="/Game/Maps/TestLevel")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "save_level",
            "params": {"asset_path": "/Game/Maps/TestLevel"},
        }

    def test_load(self, patch_unreal_connection):
        result = level_tool(action="load", asset_path="/Game/Maps/TestLevel")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "load_level",
            "params": {"asset_path": "/Game/Maps/TestLevel"},
        }

    def test_duplicate(self, patch_unreal_connection):
        result = level_tool(
            action="duplicate",
            source_path="/Game/Maps/Source",
            dest_path="/Game/Maps/Dest",
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "duplicate_level",
            "params": {
                "source_path": "/Game/Maps/Source",
                "dest_path": "/Game/Maps/Dest",
            },
        }

    def test_rename(self, patch_unreal_connection):
        result = level_tool(
            action="rename",
            source_path="/Game/Maps/Old",
            dest_path="/Game/Maps/New",
        )
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "rename_level",
            "params": {
                "source_path": "/Game/Maps/Old",
                "dest_path": "/Game/Maps/New",
            },
        }

    def test_delete(self, patch_unreal_connection):
        result = level_tool(action="delete", asset_path="/Game/Maps/TestLevel")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "delete_level",
            "params": {"asset_path": "/Game/Maps/TestLevel"},
        }

    def test_get_current(self, patch_unreal_connection):
        result = level_tool(action="get_current")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "get_current_level",
            "params": {},
        }

    def test_list(self, patch_unreal_connection):
        result = level_tool(action="list")
        assert result["success"] is True
        assert patch_unreal_connection.history[-1] == {
            "command": "list_levels",
            "params": {},
        }

    def test_missing_asset_path(self, patch_unreal_connection):
        result = level_tool(action="create")
        assert "error" in result
        assert "asset_path is required" in result["error"]

    def test_missing_source_dest(self, patch_unreal_connection):
        result = level_tool(action="duplicate", source_path="/Game/Maps/A")
        assert "error" in result
        assert "dest_path" in result["error"]

    def test_unknown_action(self, patch_unreal_connection):
        result = level_tool(action="fly")
        assert "error" in result
        assert "Unknown level_tool action" in result["error"]

    def test_no_connection(self, monkeypatch):
        monkeypatch.setattr(
            "server.project_editor_tools.get_unreal_connection", lambda: None
        )
        result = level_tool(action="list")
        assert "error" in result
        assert "Failed to connect" in result["error"]
