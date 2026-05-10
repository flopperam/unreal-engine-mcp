"""AI and Navigation tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def create_behavior_tree(
    asset_name: str,
    package_path: str = "/Game/AI/",
) -> Dict[str, Any]:
    """Create a new BehaviorTree asset.

    asset_name: Name for the new BehaviorTree asset
    package_path: Package path (default /Game/AI/)
    """
    try:
        validate_string(asset_name, "asset_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command(
            "create_behavior_tree",
            {"asset_name": asset_name, "package_path": package_path},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_behavior_tree error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_blackboard(
    asset_name: str,
    package_path: str = "/Game/AI/",
) -> Dict[str, Any]:
    """Create a new BlackboardData asset.

    asset_name: Name for the new Blackboard asset
    package_path: Package path (default /Game/AI/)
    """
    try:
        validate_string(asset_name, "asset_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command(
            "create_blackboard",
            {"asset_name": asset_name, "package_path": package_path},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_blackboard error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_nav_modifier_volume(
    name: str = "NavModifierVolume",
    location: Optional[List[float]] = None,
    extent: Optional[List[float]] = None,
) -> Dict[str, Any]:
    """Create a NavModifierVolume in the level.

    name: Actor name
    location: [x, y, z] location
    extent: [x, y, z] box extent in cm
    """
    try:
        validate_string(name, "name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    params: Dict[str, Any] = {"name": name}
    if location is not None:
        params["location"] = {"value": location}
    if extent is not None:
        params["extent"] = {"value": extent}

    try:
        response = unreal.send_command("create_nav_modifier_volume", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_nav_modifier_volume error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_nav_link_proxy(
    name: str = "NavLinkProxy",
    location: Optional[List[float]] = None,
    left: Optional[List[float]] = None,
    right: Optional[List[float]] = None,
) -> Dict[str, Any]:
    """Create a NavLinkProxy in the level.

    name: Actor name
    location: [x, y, z] location
    left: [x, y, z] left connection point relative to location
    right: [x, y, z] right connection point relative to location
    """
    try:
        validate_string(name, "name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    params: Dict[str, Any] = {"name": name}
    if location is not None:
        params["location"] = {"value": location}
    if left is not None:
        params["left"] = {"value": left}
    if right is not None:
        params["right"] = {"value": right}

    try:
        response = unreal.send_command("create_nav_link_proxy", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_nav_link_proxy error: {e}")
        return make_error_response(str(e))
