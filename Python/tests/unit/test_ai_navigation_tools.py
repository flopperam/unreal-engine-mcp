"""Unit tests for AI and Navigation tools."""

from unittest.mock import MagicMock, patch

import pytest

import server.ai_navigation_tools as ai_navigation_tools


class TestCreateBehaviorTree:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=None):
            result = ai_navigation_tools.create_behavior_tree(asset_name="BT_Enemy")
        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()

    def test_rejects_empty_asset_name(self):
        result = ai_navigation_tools.create_behavior_tree(asset_name="")
        assert result.get("success") is False

    def test_sends_command_with_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = ai_navigation_tools.create_behavior_tree(
                asset_name="BT_Patrol",
                package_path="/Game/AI/Enemies/",
            )
        assert result["success"] is True
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "create_behavior_tree"
        assert call_args[1]["asset_name"] == "BT_Patrol"
        assert call_args[1]["package_path"] == "/Game/AI/Enemies/"


class TestCreateBlackboard:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=None):
            result = ai_navigation_tools.create_blackboard(asset_name="BB_Enemy")
        assert result.get("success") is False

    def test_rejects_empty_asset_name(self):
        result = ai_navigation_tools.create_blackboard(asset_name="")
        assert result.get("success") is False

    def test_sends_command_with_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = ai_navigation_tools.create_blackboard(asset_name="BB_Patrol")
        assert result["success"] is True
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "create_blackboard"
        assert call_args[1]["asset_name"] == "BB_Patrol"


class TestCreateNavModifierVolume:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=None):
            result = ai_navigation_tools.create_nav_modifier_volume()
        assert result.get("success") is False

    def test_rejects_empty_name(self):
        result = ai_navigation_tools.create_nav_modifier_volume(name="")
        assert result.get("success") is False

    def test_sends_optional_location_and_extent(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = ai_navigation_tools.create_nav_modifier_volume(
                name="NavMod_Water",
                location=[0.0, 0.0, 100.0],
                extent=[500.0, 500.0, 100.0],
            )
        assert result["success"] is True
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "create_nav_modifier_volume"
        params = call_args[1]
        assert params["name"] == "NavMod_Water"
        assert params["location"]["value"] == [0.0, 0.0, 100.0]
        assert params["extent"]["value"] == [500.0, 500.0, 100.0]

    def test_omits_none_location(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = ai_navigation_tools.create_nav_modifier_volume(name="NavMod_Default")
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert "location" not in params
        assert "extent" not in params


class TestCreateNavLinkProxy:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=None):
            result = ai_navigation_tools.create_nav_link_proxy()
        assert result.get("success") is False

    def test_rejects_empty_name(self):
        result = ai_navigation_tools.create_nav_link_proxy(name="")
        assert result.get("success") is False

    def test_sends_all_optional_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = ai_navigation_tools.create_nav_link_proxy(
                name="Link_Jump",
                location=[100.0, 200.0, 0.0],
                left=[-50.0, 0.0, 0.0],
                right=[50.0, 0.0, 0.0],
            )
        assert result["success"] is True
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "create_nav_link_proxy"
        params = call_args[1]
        assert params["name"] == "Link_Jump"
        assert params["location"]["value"] == [100.0, 200.0, 0.0]
        assert params["left"]["value"] == [-50.0, 0.0, 0.0]
        assert params["right"]["value"] == [50.0, 0.0, 0.0]

    def test_omits_none_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(ai_navigation_tools, "get_unreal_connection", return_value=fake_unreal):
            result = ai_navigation_tools.create_nav_link_proxy(name="Link_Default")
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert "location" not in params
        assert "left" not in params
        assert "right" not in params
