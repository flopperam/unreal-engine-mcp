"""L1 Unit tests for audio tools.

Verifies payload serialization and input validation without a live Unreal Editor.
"""

import pytest
from unittest.mock import patch, MagicMock

import server.audio_tools as audio_tools


def _mock_ue_conn():
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    return mock


class TestCreateSoundCue:
    def test_sends_minimal_payload(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = audio_tools.create_sound_cue(cue_path="/Game/Audio/MyCue")

        mock_ue.return_value.send_command.assert_called_once()
        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "create_sound_cue"
        assert args[0][1]["cue_path"] == "/Game/Audio/MyCue"
        assert "sound_wave_path" not in args[0][1]
        assert result["success"] is True

    def test_sends_with_sound_wave(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = audio_tools.create_sound_cue(
                cue_path="/Game/Audio/MyCue",
                sound_wave_path="/Game/Audio/MyWave",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][1]["sound_wave_path"] == "/Game/Audio/MyWave"

    def test_rejects_empty_path(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = audio_tools.create_sound_cue(cue_path="")
        assert result.get("success") is False

    def test_returns_error_when_no_connection(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=None):
            result = audio_tools.create_sound_cue(cue_path="/Game/Audio/MyCue")
        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()


class TestAddAudioComponent:
    def test_sends_required_params(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = audio_tools.add_audio_component(
                actor_name="Player", sound_path="/Game/Audio/MyCue"
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "add_audio_component"
        payload = args[0][1]
        assert payload["actor_name"] == "Player"
        assert payload["sound_path"] == "/Game/Audio/MyCue"
        assert payload["volume"] == 1.0
        assert payload["pitch"] == 1.0
        assert payload["auto_activate"] is False
        assert payload["loop"] is False
        assert result["success"] is True

    def test_sends_custom_params(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = audio_tools.add_audio_component(
                actor_name="Player",
                sound_path="/Game/Audio/MyCue",
                volume=0.5,
                pitch=1.2,
                auto_activate=True,
                loop=True,
            )

        payload = mock_ue.return_value.send_command.call_args[0][1]
        assert payload["volume"] == 0.5
        assert payload["pitch"] == 1.2
        assert payload["auto_activate"] is True
        assert payload["loop"] is True

    def test_rejects_empty_actor_name(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = audio_tools.add_audio_component(actor_name="", sound_path="/Game/Audio/MyCue")
        assert result.get("success") is False

    def test_rejects_empty_sound_path(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = audio_tools.add_audio_component(actor_name="Player", sound_path="")
        assert result.get("success") is False


class TestSetSoundAttenuation:
    def test_sends_minimal_payload(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = audio_tools.set_sound_attenuation(attenuation_path="/Game/Audio/MyAtt")

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "set_sound_attenuation"
        payload = args[0][1]
        assert payload["attenuation_path"] == "/Game/Audio/MyAtt"
        assert "radius" not in payload
        assert "spatialization" not in payload
        assert result["success"] is True

    def test_sends_full_payload(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = audio_tools.set_sound_attenuation(
                attenuation_path="/Game/Audio/MyAtt",
                radius=500.0,
                spatialization=True,
                cone_attenuation=True,
                cone_inner_angle=30.0,
                cone_outer_angle=60.0,
                reverb_send=0.3,
            )

        payload = mock_ue.return_value.send_command.call_args[0][1]
        assert payload["radius"] == 500.0
        assert payload["spatialization"] is True
        assert payload["cone_attenuation"] is True
        assert payload["cone_inner_angle"] == 30.0
        assert payload["cone_outer_angle"] == 60.0
        assert payload["reverb_send"] == 0.3

    def test_rejects_empty_path(self):
        with patch("server.audio_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = audio_tools.set_sound_attenuation(attenuation_path="")
        assert result.get("success") is False
