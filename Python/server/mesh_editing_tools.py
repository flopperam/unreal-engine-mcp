"""Mesh Editing tools for the Unreal MCP server.

Grouped tools exposing Static Mesh modification C++ commands through a single Python MCP tool.
Each tool uses an `action` parameter to dispatch to the correct C++ command.
"""

import logging
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_MeshEditing")

@mcp.tool()
def asset_mesh_editing_tool(
    action: str,
    asset_path: str,
    # Common parameters
    enabled: Optional[bool] = None,
    fallback_percent: Optional[float] = None,
    resolution: Optional[int] = None,
    bounds: Optional[Dict[str, Dict[str, float]]] = None,
    shape_type: Optional[str] = None,
    complexity: Optional[str] = None,
    lod_group: Optional[str] = None,
    socket_name: Optional[str] = None,
    location: Optional[Dict[str, float]] = None,
    rotation: Optional[Dict[str, float]] = None,
    tool_mesh_path: Optional[str] = None,
    operation: Optional[str] = None,
    target_triangle_count: Optional[int] = None
) -> Dict[str, Any]:
    """
    Manage and edit Static Mesh properties, collisions, sockets, LODs, and geometry.

    Args:
        action: The operation to perform. Supported actions:
            - get_details
            - set_nanite_settings
            - set_lightmap_settings
            - edit_bounds
            - generate_collision
            - set_collision_complexity
            - add_simple_collision
            - remove_collisions
            - set_lod_group
            - add_socket
            - remove_socket
            - update_socket
            - mesh_boolean
            - mesh_remesh
            - mesh_simplify
            - mesh_uv_unwrap
        asset_path: The path to the static mesh asset (e.g. "/Game/Meshes/SM_Box").
        enabled: Boolean flag for nanite enablement.
        fallback_percent: Nanite fallback triangle percentage (e.g. 100.0).
        resolution: Lightmap resolution.
        bounds: Positive and negative bounds extension: {"positive": {"x": 10, "y": 10, "z": 10}, "negative": ...}
        shape_type: Collision shape type ("Box", "Sphere", "Capsule", "10DOPX", etc.)
        complexity: Collision complexity ("Default", "UseSimpleAsComplex", "UseComplexAsSimple")
        lod_group: LOD group name.
        socket_name: Name of the socket.
        location: Socket location dict: {"x": 0.0, "y": 0.0, "z": 0.0}
        rotation: Socket rotation dict: {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}
        tool_mesh_path: For mesh_boolean, the path of the tool mesh.
        operation: Boolean operation ("Subtract", "Union", "Intersect").
        target_triangle_count: Target triangle count for remesh and simplify.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Not connected to Unreal Engine")

    try:
        if action == "get_details":
            return unreal.send_command("get_static_mesh_details", {"asset_path": asset_path})

        elif action == "set_nanite_settings":
            params = {"asset_path": asset_path}
            if enabled is not None: params["enabled"] = enabled
            if fallback_percent is not None: params["fallback_percent"] = fallback_percent
            return unreal.send_command("set_nanite_settings", params)

        elif action == "set_lightmap_settings":
            params = {"asset_path": asset_path}
            if resolution is not None: params["resolution"] = resolution
            return unreal.send_command("set_lightmap_settings", params)

        elif action == "edit_bounds":
            params = {"asset_path": asset_path}
            if bounds is not None: params["bounds"] = bounds
            return unreal.send_command("edit_mesh_bounds", params)

        elif action == "generate_collision":
            params = {"asset_path": asset_path}
            if shape_type is not None: params["shape_type"] = shape_type
            return unreal.send_command("generate_collision", params)

        elif action == "set_collision_complexity":
            params = {"asset_path": asset_path}
            if complexity is not None: params["complexity"] = complexity
            return unreal.send_command("set_collision_complexity", params)

        elif action == "add_simple_collision":
            params = {"asset_path": asset_path}
            if shape_type is not None: params["shape_type"] = shape_type
            return unreal.send_command("add_simple_collision", params)

        elif action == "remove_collisions":
            return unreal.send_command("remove_collisions", {"asset_path": asset_path})

        elif action == "set_lod_group":
            params = {"asset_path": asset_path}
            if lod_group is not None: params["lod_group"] = lod_group
            return unreal.send_command("set_lod_group", params)

        elif action == "add_socket":
            params = {"asset_path": asset_path, "socket_name": socket_name}
            if location is not None: params["location"] = location
            if rotation is not None: params["rotation"] = rotation
            return unreal.send_command("add_socket", params)

        elif action == "remove_socket":
            return unreal.send_command("remove_socket", {"asset_path": asset_path, "socket_name": socket_name})

        elif action == "update_socket":
            params = {"asset_path": asset_path, "socket_name": socket_name}
            if location is not None: params["location"] = location
            if rotation is not None: params["rotation"] = rotation
            return unreal.send_command("update_socket", params)

        elif action == "mesh_boolean":
            params = {"asset_path": asset_path, "tool_mesh_path": tool_mesh_path}
            if operation is not None: params["operation"] = operation
            return unreal.send_command("mesh_boolean", params)

        elif action == "mesh_remesh":
            params = {"asset_path": asset_path}
            if target_triangle_count is not None: params["target_triangle_count"] = target_triangle_count
            return unreal.send_command("mesh_remesh", params)

        elif action == "mesh_simplify":
            params = {"asset_path": asset_path}
            if target_triangle_count is not None: params["target_triangle_count"] = target_triangle_count
            return unreal.send_command("mesh_simplify", params)

        elif action == "mesh_uv_unwrap":
            return unreal.send_command("mesh_uv_unwrap", {"asset_path": asset_path})

        else:
            return make_error_response(f"Unknown asset_mesh_editing_tool action: {action}")

    except Exception as e:
        logger.error(f"asset_mesh_editing_tool error: {e}")
        return make_error_response(str(e))
