from unittest.mock import MagicMock

import pytest

from server.project_editor_tools import (
    editor_control_tool,
    play_tool,
    plugin_tool,
    project_settings_tool,
    viewport_tool,
)


@pytest.fixture
def mock_unreal(mocker):
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    mocker.patch("server.project_editor_tools.get_unreal_connection", return_value=mock)
    return mock


def test_project_settings_default_map_routing(mock_unreal):
    project_settings_tool(action="set_default_map", map_path="/Game/Maps/Test")
    mock_unreal.send_command.assert_called_with("set_default_map", {"map_path": "/Game/Maps/Test"})


def test_project_settings_maps_and_modes_routing(mock_unreal):
    project_settings_tool(
        action="set_maps_and_modes",
        game_mode="/Game/BP_GM.BP_GM_C",
        game_instance="/Game/BP_GI.BP_GI_C",
        transition_map="/Game/Maps/Transition",
    )
    mock_unreal.send_command.assert_called_with(
        "set_maps_and_modes",
        {
            "action": "set_maps_and_modes",
            "game_mode": "/Game/BP_GM.BP_GM_C",
            "game_instance": "/Game/BP_GI.BP_GI_C",
            "transition_map": "/Game/Maps/Transition",
        },
    )


def test_plugin_tool_routing(mock_unreal):
    plugin_tool(action="set_enabled", plugin_name="ModelingToolsEditorMode", enabled=True)
    mock_unreal.send_command.assert_called_with(
        "set_plugin_enabled", {"plugin_name": "ModelingToolsEditorMode", "enabled": True}
    )


def test_editor_control_routing(mock_unreal):
    editor_control_tool(action="save_asset", asset_path="/Game/Maps/Test")
    mock_unreal.send_command.assert_called_with("save_asset", {"asset_path": "/Game/Maps/Test"})


def test_play_tool_routing(mock_unreal):
    play_tool(action="start_pie")
    mock_unreal.send_command.assert_called_with("start_pie", {})


def test_viewport_camera_routing(mock_unreal):
    viewport_tool(action="set_camera_position", location=[1, 2, 3], rotation=[4, 5, 6])
    mock_unreal.send_command.assert_called_with(
        "set_camera_position", {"location": [1, 2, 3], "rotation": [4, 5, 6]}
    )
