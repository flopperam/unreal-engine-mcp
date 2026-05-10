"""L1 Unit tests for rendering camera tools.

Verifies payload serialization and input validation without a live Unreal Editor.
"""

import pytest
from unittest.mock import patch, MagicMock

import server.rendering_tools as rendering_tools


def _mock_ue_conn():
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    return mock


class TestSpawnCameraActor:
    def test_sends_minimal_payload(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = rendering_tools.spawn_camera_actor(name="MainCamera")

        mock_ue.return_value.send_command.assert_called_once()
        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "spawn_camera_actor"
        assert args[0][1]["name"] == "MainCamera"
        assert "location" not in args[0][1]
        assert "rotation" not in args[0][1]
        assert result["success"] is True

    def test_sends_full_payload(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = rendering_tools.spawn_camera_actor(
                name="Cam1",
                location=[100.0, 200.0, 300.0],
                rotation=[10.0, 20.0, 30.0],
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["location"] == [100.0, 200.0, 300.0]
        assert args[0][1]["rotation"] == [10.0, 20.0, 30.0]

    def test_rejects_empty_name(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = rendering_tools.spawn_camera_actor(name="")
        assert result.get("success") is False

    def test_returns_error_when_no_connection(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=None):
            result = rendering_tools.spawn_camera_actor(name="Cam1")
        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()


class TestSpawnCineCameraActor:
    def test_sends_minimal_payload(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = rendering_tools.spawn_cine_camera_actor(name="CineCam1")

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "spawn_cine_camera_actor"
        assert args[0][1]["name"] == "CineCam1"
        assert "focal_length" not in args[0][1]

    def test_sends_full_payload(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = rendering_tools.spawn_cine_camera_actor(
                name="CineCam1",
                location=[0.0, 0.0, 150.0],
                rotation=[-10.0, 45.0, 0.0],
                focal_length=35.0,
                aperture=2.8,
                focus_distance=1000.0,
            )

        args = mock_ue.return_value.send_command.call_args
        payload = args[0][1]
        assert payload["name"] == "CineCam1"
        assert payload["location"] == [0.0, 0.0, 150.0]
        assert payload["rotation"] == [-10.0, 45.0, 0.0]
        assert payload["focal_length"] == 35.0
        assert payload["aperture"] == 2.8
        assert payload["focus_distance"] == 1000.0

    def test_rejects_empty_name(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = rendering_tools.spawn_cine_camera_actor(name="")
        assert result.get("success") is False


class TestSetCameraProperties:
    def test_sends_partial_payload(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = rendering_tools.set_camera_properties(
                name="CineCam1",
                focal_length=50.0,
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "set_camera_properties"
        payload = args[0][1]
        assert payload["name"] == "CineCam1"
        assert payload["focal_length"] == 50.0
        assert "aperture" not in payload
        assert "focus_distance" not in payload

    def test_sends_full_payload(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = rendering_tools.set_camera_properties(
                name="CineCam1",
                focal_length=85.0,
                aperture=1.4,
                focus_distance=2000.0,
            )

        payload = mock_ue.return_value.send_command.call_args[0][1]
        assert payload["focal_length"] == 85.0
        assert payload["aperture"] == 1.4
        assert payload["focus_distance"] == 2000.0

    def test_rejects_empty_name(self):
        with patch("server.rendering_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = rendering_tools.set_camera_properties(name="")
        assert result.get("success") is False
