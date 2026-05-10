"""L1 Unit tests for sequencer_tools MCP tools.

Verifies payload serialization and input validation without a live Unreal Editor.
"""

import pytest
from unittest.mock import patch, MagicMock

import server.sequencer_tools as sequencer_tools


def _mock_ue_conn():
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    return mock


class TestCreateLevelSequence:
    def test_sends_required_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.create_level_sequence(
                sequence_path="/Game/Cinematics/MySeq",
                duration_frames=300,
                frame_rate_numerator=24,
                frame_rate_denominator=1,
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "create_level_sequence"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["duration_frames"] == 300
        assert payload["frame_rate_numerator"] == 24
        assert payload["frame_rate_denominator"] == 1
        assert result["success"] is True

    def test_uses_defaults(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.create_level_sequence(sequence_path="/Game/Cinematics/MySeq")

        payload = mock_ue.return_value.send_command.call_args[0][1]
        assert payload["duration_frames"] == 150
        assert payload["frame_rate_numerator"] == 30
        assert payload["frame_rate_denominator"] == 1

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.create_level_sequence(sequence_path="")
        assert result.get("success") is False


class TestAddActorBinding:
    def test_sends_required_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.add_actor_binding(
                sequence_path="/Game/Cinematics/MySeq",
                actor_name="MyActor",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "add_actor_binding"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["actor_name"] == "MyActor"
        assert result["success"] is True

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_actor_binding(sequence_path="", actor_name="MyActor")
        assert result.get("success") is False

    def test_rejects_empty_actor_name(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_actor_binding(
                sequence_path="/Game/Cinematics/MySeq", actor_name=""
            )
        assert result.get("success") is False


class TestAddTransformTrack:
    def test_sends_required_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.add_transform_track(
                sequence_path="/Game/Cinematics/MySeq",
                binding_guid="A1B2C3D4",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "add_transform_track"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["binding_guid"] == "A1B2C3D4"
        assert result["success"] is True

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_transform_track(sequence_path="", binding_guid="A1")
        assert result.get("success") is False

    def test_rejects_empty_binding_guid(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_transform_track(
                sequence_path="/Game/Cinematics/MySeq", binding_guid=""
            )
        assert result.get("success") is False


class TestAddCameraCutTrack:
    def test_sends_required_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.add_camera_cut_track(
                sequence_path="/Game/Cinematics/MySeq",
                camera_binding_guid="CAM123",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "add_camera_cut_track"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["camera_binding_guid"] == "CAM123"
        assert result["success"] is True

    def test_allows_no_camera_guid(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.add_camera_cut_track(
                sequence_path="/Game/Cinematics/MySeq",
            )

        payload = mock_ue.return_value.send_command.call_args[0][1]
        assert "camera_binding_guid" not in payload
        assert result["success"] is True

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_camera_cut_track(sequence_path="")
        assert result.get("success") is False


class TestAddEventTrack:
    def test_sends_required_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.add_event_track(
                sequence_path="/Game/Cinematics/MySeq",
                binding_guid="A1B2C3D4",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "add_event_track"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["binding_guid"] == "A1B2C3D4"
        assert result["success"] is True

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_event_track(sequence_path="", binding_guid="A1")
        assert result.get("success") is False

    def test_rejects_empty_binding_guid(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_event_track(
                sequence_path="/Game/Cinematics/MySeq", binding_guid=""
            )
        assert result.get("success") is False


class TestAddKeyframe:
    def test_sends_minimal_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.add_keyframe(
                sequence_path="/Game/Cinematics/MySeq",
                binding_guid="A1B2C3D4",
                frame=60,
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "add_keyframe"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["binding_guid"] == "A1B2C3D4"
        assert payload["frame"] == 60
        assert result["success"] is True

    def test_sends_full_transform(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.add_keyframe(
                sequence_path="/Game/Cinematics/MySeq",
                binding_guid="A1B2C3D4",
                frame=30,
                location={"x": 1, "y": 2, "z": 3},
                rotation={"x": 0, "y": 90, "z": 0},
                scale={"x": 1, "y": 1, "z": 1},
            )

        payload = mock_ue.return_value.send_command.call_args[0][1]
        assert payload["location"] == {"x": 1, "y": 2, "z": 3}
        assert payload["rotation"] == {"x": 0, "y": 90, "z": 0}
        assert payload["scale"] == {"x": 1, "y": 1, "z": 1}

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_keyframe(sequence_path="", binding_guid="A1")
        assert result.get("success") is False

    def test_rejects_empty_binding_guid(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.add_keyframe(
                sequence_path="/Game/Cinematics/MySeq", binding_guid=""
            )
        assert result.get("success") is False


class TestSetPlaybackRange:
    def test_sends_required_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.set_playback_range(
                sequence_path="/Game/Cinematics/MySeq",
                start_frame=0,
                end_frame=300,
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "set_playback_range"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["start_frame"] == 0
        assert payload["end_frame"] == 300
        assert result["success"] is True

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.set_playback_range(sequence_path="")
        assert result.get("success") is False


class TestSetFrameRate:
    def test_sends_required_params(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = sequencer_tools.set_frame_rate(
                sequence_path="/Game/Cinematics/MySeq",
                numerator=24,
                denominator=1,
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "set_frame_rate"
        payload = args[0][1]
        assert payload["sequence_path"] == "/Game/Cinematics/MySeq"
        assert payload["numerator"] == 24
        assert payload["denominator"] == 1
        assert result["success"] is True

    def test_rejects_zero_denominator(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.set_frame_rate(
                sequence_path="/Game/Cinematics/MySeq", numerator=30, denominator=0
            )
        assert result.get("success") is False

    def test_rejects_empty_sequence_path(self):
        with patch("server.sequencer_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = sequencer_tools.set_frame_rate(sequence_path="")
        assert result.get("success") is False
