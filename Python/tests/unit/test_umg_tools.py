"""L1 Unit tests for UMG dedicated tools.

Verifies payload serialization and input validation without a live Unreal Editor.
"""

import pytest
from unittest.mock import patch, MagicMock

import server.umg_tools as umg_tools


def _mock_ue_conn():
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    return mock


class TestCreateWidgetInstance:
    def test_sends_correct_payload(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = umg_tools.create_widget_instance(
                widget_blueprint="/Game/UI/WBP_HUD",
                instance_name="HUD_Instance",
            )

        mock_ue.return_value.send_command.assert_called_once()
        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "create_widget_instance"
        assert args[0][1]["widget_blueprint"] == "/Game/UI/WBP_HUD"
        assert args[0][1]["instance_name"] == "HUD_Instance"
        assert result["success"] is True

    def test_omits_optional_instance_name(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = umg_tools.create_widget_instance(widget_blueprint="/Game/UI/WBP_Menu")

        args = mock_ue.return_value.send_command.call_args
        assert "instance_name" not in args[0][1]

    def test_rejects_empty_blueprint(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = umg_tools.create_widget_instance(widget_blueprint="")

        assert result.get("success") is False
        assert "widget_blueprint" in result.get("error", "").lower() or "validation" in result.get("error", "").lower()

    def test_returns_error_when_no_connection(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=None):
            result = umg_tools.create_widget_instance(widget_blueprint="/Game/UI/WBP_Test")

        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()


class TestBindWidgetHealthBar:
    def test_sends_default_payload(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = umg_tools.bind_widget_health_bar(
                widget_blueprint="/Game/UI/WBP_HUD",
                widget_name="HealthBar",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "bind_widget_property"
        payload = args[0][1]
        assert payload["widget_blueprint"] == "/Game/UI/WBP_HUD"
        assert payload["widget_name"] == "HealthBar"
        assert payload["property_name"] == "Percent"
        assert payload["function_name"] == "GetHealthPercent"
        assert result["success"] is True

    def test_sends_custom_function_name(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = umg_tools.bind_widget_health_bar(
                widget_blueprint="/Game/UI/WBP_HUD",
                widget_name="HealthBar",
                function_name="GetPlayerHealth",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["function_name"] == "GetPlayerHealth"

    def test_rejects_empty_inputs(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = umg_tools.bind_widget_health_bar(widget_blueprint="", widget_name="HealthBar")
        assert result.get("success") is False

        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = umg_tools.bind_widget_health_bar(widget_blueprint="/Game/UI/WBP_HUD", widget_name="")
        assert result.get("success") is False

        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = umg_tools.bind_widget_health_bar(widget_blueprint="/Game/UI/WBP_HUD", widget_name="Bar", function_name="")
        assert result.get("success") is False


class TestBindWidgetScoreText:
    def test_sends_default_payload(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = umg_tools.bind_widget_score_text(
                widget_blueprint="/Game/UI/WBP_HUD",
                widget_name="ScoreText",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "bind_widget_property"
        payload = args[0][1]
        assert payload["widget_blueprint"] == "/Game/UI/WBP_HUD"
        assert payload["widget_name"] == "ScoreText"
        assert payload["property_name"] == "Text"
        assert payload["function_name"] == "GetScoreText"
        assert result["success"] is True

    def test_sends_custom_function_name(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = umg_tools.bind_widget_score_text(
                widget_blueprint="/Game/UI/WBP_HUD",
                widget_name="ScoreText",
                function_name="GetPlayerScore",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["function_name"] == "GetPlayerScore"

    def test_rejects_empty_inputs(self):
        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = umg_tools.bind_widget_score_text(widget_blueprint="", widget_name="ScoreText")
        assert result.get("success") is False

        with patch("server.umg_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = umg_tools.bind_widget_score_text(widget_blueprint="/Game/UI/WBP_HUD", widget_name="")
        assert result.get("success") is False
