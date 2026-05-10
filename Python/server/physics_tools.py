"""Physics and Collision tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def set_actor_collision_preset(
    actor_name: str,
    preset: str,
) -> Dict[str, Any]:
    """Set the collision preset on an actor's root primitive component.

    actor_name: Name or label of the actor
    preset: Collision preset name (e.g. "BlockAll", "OverlapAll", "Pawn", "PhysicsActor")
    """
    try:
        validate_string(actor_name, "actor_name")
        validate_string(preset, "preset")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command(
            "set_actor_collision_preset",
            {"actor_name": actor_name, "preset": preset},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_actor_collision_preset error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_actor_physics(
    actor_name: str,
    simulate_physics: Optional[bool] = None,
    gravity_enabled: Optional[bool] = None,
    mass_scale: Optional[float] = None,
    linear_damping: Optional[float] = None,
    angular_damping: Optional[float] = None,
) -> Dict[str, Any]:
    """Set physics properties on an actor's root primitive component.

    actor_name: Name or label of the actor
    simulate_physics: Enable physics simulation
    gravity_enabled: Enable gravity
    mass_scale: Multiplier for mass
    linear_damping: Linear damping (0-1)
    angular_damping: Angular damping (0-1)
    """
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    params: Dict[str, Any] = {"actor_name": actor_name}
    if simulate_physics is not None:
        params["simulate_physics"] = simulate_physics
    if gravity_enabled is not None:
        params["gravity_enabled"] = gravity_enabled
    if mass_scale is not None:
        params["mass_scale"] = mass_scale
    if linear_damping is not None:
        params["linear_damping"] = linear_damping
    if angular_damping is not None:
        params["angular_damping"] = angular_damping

    try:
        response = unreal.send_command("set_actor_physics", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_actor_physics error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_physical_material(
    asset_path: str,
    friction: float = 0.7,
    restitution: float = 0.3,
) -> Dict[str, Any]:
    """Create a PhysicalMaterial asset.

    asset_path: Full asset path (e.g. /Game/Physics/PM_MyMaterial)
    friction: Surface friction (0-1)
    restitution: Bounciness (0-1)
    """
    try:
        validate_string(asset_path, "asset_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command(
            "create_physical_material",
            {
                "asset_path": asset_path,
                "friction": friction,
                "restitution": restitution,
            },
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_physical_material error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def spawn_radial_force(
    actor_name: str = "RadialForceActor",
    location: Optional[Dict[str, float]] = None,
    radius: float = 500.0,
    strength: float = 1000.0,
) -> Dict[str, Any]:
    """Spawn a RadialForceActor in the level.

    actor_name: Actor name
    location: {"x": 0, "y": 0, "z": 0}
    radius: Force radius in cm
    strength: Force strength
    """
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    params: Dict[str, Any] = {"actor_name": actor_name, "radius": radius, "strength": strength}
    if location is not None:
        params["location"] = location

    try:
        response = unreal.send_command("spawn_radial_force", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"spawn_radial_force error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def spawn_physics_constraint(
    actor_name: str = "PhysicsConstraintActor",
    location: Optional[Dict[str, float]] = None,
) -> Dict[str, Any]:
    """Spawn a PhysicsConstraintActor in the level.

    actor_name: Actor name
    location: {"x": 0, "y": 0, "z": 0}
    """
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    params: Dict[str, Any] = {"actor_name": actor_name}
    if location is not None:
        params["location"] = location

    try:
        response = unreal.send_command("spawn_physics_constraint", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"spawn_physics_constraint error: {e}")
        return make_error_response(str(e))
