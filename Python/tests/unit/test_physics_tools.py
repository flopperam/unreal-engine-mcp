"""Unit tests for physics tools."""

from unittest.mock import MagicMock, patch

import pytest

import server.physics_tools as physics_tools


class TestSetActorCollisionPreset:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(physics_tools, "get_unreal_connection", return_value=None):
            result = physics_tools.set_actor_collision_preset(actor_name="Cube", preset="BlockAll")
        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()

    def test_rejects_empty_actor_name(self):
        result = physics_tools.set_actor_collision_preset(actor_name="", preset="BlockAll")
        assert result.get("success") is False

    def test_rejects_empty_preset(self):
        result = physics_tools.set_actor_collision_preset(actor_name="Cube", preset="")
        assert result.get("success") is False

    def test_sends_command_with_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.set_actor_collision_preset(actor_name="Cube", preset="PhysicsActor")
        assert result["success"] is True
        call_args = fake_unreal.send_command.call_args[0]
        assert call_args[0] == "set_actor_collision_preset"
        assert call_args[1]["actor_name"] == "Cube"
        assert call_args[1]["preset"] == "PhysicsActor"


class TestSetActorPhysics:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(physics_tools, "get_unreal_connection", return_value=None):
            result = physics_tools.set_actor_physics(actor_name="Cube")
        assert result.get("success") is False

    def test_rejects_empty_actor_name(self):
        result = physics_tools.set_actor_physics(actor_name="")
        assert result.get("success") is False

    def test_sends_all_optional_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.set_actor_physics(
                actor_name="Cube",
                simulate_physics=True,
                gravity_enabled=True,
                mass_scale=2.0,
                linear_damping=0.5,
                angular_damping=0.2,
            )
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert params["actor_name"] == "Cube"
        assert params["simulate_physics"] is True
        assert params["gravity_enabled"] is True
        assert params["mass_scale"] == 2.0
        assert params["linear_damping"] == 0.5
        assert params["angular_damping"] == 0.2

    def test_omits_none_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.set_actor_physics(actor_name="Cube")
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert "simulate_physics" not in params
        assert "gravity_enabled" not in params


class TestCreatePhysicalMaterial:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(physics_tools, "get_unreal_connection", return_value=None):
            result = physics_tools.create_physical_material(asset_path="/Game/PM_Test")
        assert result.get("success") is False

    def test_rejects_empty_asset_path(self):
        result = physics_tools.create_physical_material(asset_path="")
        assert result.get("success") is False

    def test_sends_default_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.create_physical_material(asset_path="/Game/PM_Test")
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert params["asset_path"] == "/Game/PM_Test"
        assert params["friction"] == 0.7
        assert params["restitution"] == 0.3


class TestSpawnRadialForce:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(physics_tools, "get_unreal_connection", return_value=None):
            result = physics_tools.spawn_radial_force()
        assert result.get("success") is False

    def test_sends_default_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.spawn_radial_force()
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert params["actor_name"] == "RadialForceActor"
        assert params["radius"] == 500.0
        assert params["strength"] == 1000.0
        assert "location" not in params

    def test_sends_location(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.spawn_radial_force(location={"x": 100.0, "y": 200.0, "z": 0.0})
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert params["location"] == {"x": 100.0, "y": 200.0, "z": 0.0}


class TestSpawnPhysicsConstraint:
    def test_returns_error_when_unreal_not_connected(self):
        with patch.object(physics_tools, "get_unreal_connection", return_value=None):
            result = physics_tools.spawn_physics_constraint()
        assert result.get("success") is False

    def test_sends_default_params(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.spawn_physics_constraint()
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert params["actor_name"] == "PhysicsConstraintActor"
        assert "location" not in params

    def test_sends_location(self):
        fake_unreal = MagicMock()
        fake_unreal.send_command.return_value = {"success": True}
        with patch.object(physics_tools, "get_unreal_connection", return_value=fake_unreal):
            result = physics_tools.spawn_physics_constraint(location={"x": 0.0, "y": 0.0, "z": 100.0})
        assert result["success"] is True
        params = fake_unreal.send_command.call_args[0][1]
        assert params["location"] == {"x": 0.0, "y": 0.0, "z": 100.0}
