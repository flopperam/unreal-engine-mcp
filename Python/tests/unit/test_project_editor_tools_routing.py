from unittest.mock import MagicMock

import pytest

from server.project_editor_tools import (
    editor_control_tool,
    play_tool,
    plugin_tool,
    project_settings_tool,
    viewport_tool,
)
from server.enhanced_input_tools import enhanced_input_tool


@pytest.fixture
def mock_unreal(monkeypatch):
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    monkeypatch.setattr(
        "server.project_editor_tools.get_unreal_connection",
        lambda: mock,
    )
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


def test_enhanced_input_create_action_routing(mock_unreal, monkeypatch):
    monkeypatch.setattr(
        "server.enhanced_input_tools.get_unreal_connection",
        lambda: mock_unreal,
    )
    enhanced_input_tool(
        action="create_input_action",
        asset_path="/Game/Input/IA_Jump",
        value_type="Boolean",
    )
    mock_unreal.send_command.assert_called_with(
        "create_input_action",
        {"asset_path": "/Game/Input/IA_Jump", "value_type": "Boolean"},
    )


def test_enhanced_input_mapping_dead_zone_routing(mock_unreal, monkeypatch):
    monkeypatch.setattr(
        "server.enhanced_input_tools.get_unreal_connection",
        lambda: mock_unreal,
    )
    enhanced_input_tool(
        action="set_dead_zone",
        mapping_context_path="/Game/Input/IMC_Default",
        input_action_path="/Game/Input/IA_Move",
        key="Gamepad_Left2D",
        lower_threshold=0.15,
        upper_threshold=0.95,
        dead_zone_type="radial",
    )
    mock_unreal.send_command.assert_called_with(
        "configure_enhanced_input_mapping",
        {
            "mapping_context_path": "/Game/Input/IMC_Default",
            "input_action_path": "/Game/Input/IA_Move",
            "key": "Gamepad_Left2D",
            "modifiers": [
                {
                    "type": "dead_zone",
                    "lower_threshold": 0.15,
                    "upper_threshold": 0.95,
                    "dead_zone_type": "radial",
                }
            ],
        },
    )
