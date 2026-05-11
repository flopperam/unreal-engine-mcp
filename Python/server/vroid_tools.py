"""VRoid / VRM avatar tools for the Unreal MCP server.

These tools interface with the VRM4U plugin (or compatible VRM importer)
through the Unreal C++ bridge. They provide:

- Plugin capability detection
- Local .vrm file import
- Avatar actor spawning with skeletal mesh
- Post-import asset validation
"""

import logging
from typing import Any, Dict, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, validate_unreal_path, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


def _vector_dict_to_list(value: Optional[Dict[str, float]], keys: list, defaults: list) -> list:
    """Convert a dict to a list using the specified keys and default values."""
    v = value or {}
    return [float(v.get(k, d)) for k, d in zip(keys, defaults)]


@mcp.tool()
def vroid_check_plugin() -> Dict[str, Any]:
    """Check whether a VRM importer plugin (e.g., VRM4U) is installed and enabled in Unreal."""
    try:
        conn = get_unreal_connection()
        result = conn.send_command("vroid_check_plugin", {})
    except Exception as e:
        return make_error_response(f"Failed to query VRM plugin status: {e}")

    if not result.get("success", False):
        return make_error_response(f"Unreal command failed: {result.get('error', 'unknown error')}")

    return result


@mcp.tool()
def vroid_import_vrm(
    source_path: str,
    destination_path: str,
    asset_name: Optional[str] = None,
) -> Dict[str, Any]:
    """Import a local .vrm file into the Unreal project.

    Requires the VRM4U plugin (or compatible VRM importer) to be installed
    and enabled. The destination_path must start with /Game/.

    Args:
        source_path: Absolute path to the .vrm file on disk.
        destination_path: Content path starting with /Game/ (e.g., /Game/Avatars).
        asset_name: Optional name for the imported asset. Defaults to the filename.
    """
    try:
        validate_string(source_path, "source_path")
        validate_unreal_path(destination_path, "destination_path")
        if asset_name is not None:
            validate_string(asset_name, "asset_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    params: Dict[str, Any] = {
        "source_path": source_path,
        "destination_path": destination_path,
    }
    if asset_name is not None:
        params["asset_name"] = asset_name

    try:
        conn = get_unreal_connection()
        result = conn.send_command("vroid_import_vrm", params)
    except Exception as e:
        return make_error_response(f"Failed to import VRM file: {e}")

    if not result.get("success", False):
        return make_error_response(f"Unreal command failed: {result.get('error', 'unknown error')}")

    return result


@mcp.tool()
def vroid_spawn_avatar(
    skeletal_mesh_path: str,
    actor_name: Optional[str] = None,
    location: Optional[Dict[str, float]] = None,
    rotation: Optional[Dict[str, float]] = None,
    scale: Optional[Dict[str, float]] = None,
    focus_viewport: bool = True,
) -> Dict[str, Any]:
    """Spawn a VRM avatar actor in the Unreal editor world.

    Args:
        skeletal_mesh_path: Path to the imported SkeletalMesh asset.
        actor_name: Optional name for the spawned actor.
        location: Optional {"x", "y", "z"} dict. Defaults to origin.
        rotation: Optional {"pitch", "yaw", "roll"} dict. Defaults to zero.
        scale: Optional {"x", "y", "z"} dict. Defaults to 1,1,1.
        focus_viewport: Whether to focus the editor viewport on the spawned actor.
    """
    try:
        validate_unreal_path(skeletal_mesh_path, "skeletal_mesh_path")
        if actor_name is not None:
            validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    params: Dict[str, Any] = {
        "skeletal_mesh_path": skeletal_mesh_path,
        "focus_viewport": focus_viewport,
    }
    if actor_name is not None:
        params["actor_name"] = actor_name

    params["location"] = _vector_dict_to_list(location, ["x", "y", "z"], [0.0, 0.0, 0.0])
    params["rotation"] = _vector_dict_to_list(rotation, ["pitch", "yaw", "roll"], [0.0, 0.0, 0.0])
    params["scale"] = _vector_dict_to_list(scale, ["x", "y", "z"], [1.0, 1.0, 1.0])

    try:
        conn = get_unreal_connection()
        result = conn.send_command("vroid_spawn_avatar", params)
    except Exception as e:
        return make_error_response(f"Failed to spawn avatar actor: {e}")

    if not result.get("success", False):
        return make_error_response(f"Unreal command failed: {result.get('error', 'unknown error')}")

    return result


@mcp.tool()
def vroid_validate_avatar_asset(asset_path: str) -> Dict[str, Any]:
    """Validate that an imported avatar asset has expected sub-assets.

    Checks for SkeletalMesh, Materials, Skeleton, and PhysicsAsset.
    Returns a health report with warnings for missing optional assets.

    Args:
        asset_path: Path to the asset to validate (e.g., /Game/Avatars/MyAvatar.MyAvatar).
    """
    try:
        validate_unreal_path(asset_path, "asset_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        conn = get_unreal_connection()
        result = conn.send_command("vroid_validate_avatar_asset", {"asset_path": asset_path})
    except Exception as e:
        return make_error_response(f"Failed to validate avatar asset: {e}")

    if not result.get("success", False):
        return make_error_response(f"Unreal command failed: {result.get('error', 'unknown error')}")

    return result
