"""Unit tests for validation tools."""

from unittest.mock import MagicMock, patch

import pytest

import server.validation_tools as validation_tools


class TestCompileAllBlueprints:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(validation_tools, "get_unreal_connection", return_value=None):
            result = validation_tools.compile_all_blueprints()
        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()

    def test_sends_command(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True, "compiled_count": 42}
        with patch.object(validation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = validation_tools.compile_all_blueprints()
        assert result["success"] is True
        assert result["compiled_count"] == 42
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "compile_all_blueprints"


class TestRunMapCheck:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(validation_tools, "get_unreal_connection", return_value=None):
            result = validation_tools.run_map_check()
        assert result.get("success") is False

    def test_sends_command(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True, "errors": 0, "warnings": 2}
        with patch.object(validation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = validation_tools.run_map_check()
        assert result["success"] is True
        assert result["warnings"] == 2
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "run_map_check"


class TestFindBrokenReferences:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(validation_tools, "get_unreal_connection", return_value=None):
            result = validation_tools.find_broken_references()
        assert result.get("success") is False

    def test_sends_command(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True, "broken_actor_count": 0}
        with patch.object(validation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = validation_tools.find_broken_references()
        assert result["success"] is True
        assert result["broken_actor_count"] == 0
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "find_broken_references"
