"""L1 Unit tests for gameplay framework save/load tools.

Verifies payload serialization and input validation without a live Unreal Editor.
"""

import pytest
from unittest.mock import patch, MagicMock

import server.gameplay_framework_tools as gfw_tools


def _mock_ue_conn():
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    return mock


class TestSaveGameToSlot:
    def test_sends_default_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.save_game_to_slot()

        mock_ue.return_value.send_command.assert_called_once()
        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "save_game_to_slot"
        assert args[0][1]["slot_name"] == "save0"
        assert args[0][1]["user_index"] == 0
        assert result["success"] is True

    def test_sends_custom_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.save_game_to_slot(slot_name="slot1", user_index=1)

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["slot_name"] == "slot1"
        assert args[0][1]["user_index"] == 1

    def test_returns_error_when_no_connection(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=None):
            result = gfw_tools.save_game_to_slot()
        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()


class TestLoadGameFromSlot:
    def test_sends_default_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.load_game_from_slot()

        mock_ue.return_value.send_command.assert_called_once()
        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "load_game_from_slot"
        assert args[0][1]["slot_name"] == "save0"
        assert args[0][1]["user_index"] == 0

    def test_sends_custom_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.load_game_from_slot(slot_name="autosave", user_index=2)

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["slot_name"] == "autosave"
        assert args[0][1]["user_index"] == 2

    def test_returns_error_when_no_connection(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=None):
            result = gfw_tools.load_game_from_slot()
        assert result.get("success") is False


class TestDeleteSaveSlot:
    def test_sends_default_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.delete_save_slot()

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "delete_save_slot"
        assert args[0][1]["slot_name"] == "save0"
        assert args[0][1]["user_index"] == 0

    def test_sends_custom_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.delete_save_slot(slot_name="quicksave", user_index=0)

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["slot_name"] == "quicksave"

    def test_returns_error_when_no_connection(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=None):
            result = gfw_tools.delete_save_slot()
        assert result.get("success") is False


class TestHasSaveGame:
    def test_sends_default_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.has_save_game()

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "has_save_game"
        assert args[0][1]["slot_name"] == "save0"
        assert args[0][1]["user_index"] == 0

    def test_sends_custom_payload(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = gfw_tools.has_save_game(slot_name="chapter3", user_index=0)

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["slot_name"] == "chapter3"

    def test_returns_error_when_no_connection(self):
        with patch("server.gameplay_framework_tools.get_unreal_connection", return_value=None):
            result = gfw_tools.has_save_game()
        assert result.get("success") is False
