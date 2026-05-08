"""Mesh Editing tools for the Unreal MCP server.

Grouped tools exposing Static Mesh modification C++ commands through a single Python MCP tool.
Each tool uses an `action` parameter to dispatch to the correct C++ command.
"""

import logging
from typing import Dict, Any, Optional, List

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
    target_triangle_count: Optional[int] = None,
    # New parameters for additional actions
    num_lods: Optional[int] = None,
    lod_index: Optional[int] = None,
    voxel_count: Optional[int] = None,
    uv_channel: Optional[int] = None,
    pivot_location: Optional[Dict[str, float]] = None,
    source_mesh_paths: Optional[List[str]] = None,
    color: Optional[Dict[str, float]] = None,
    paint_mode: Optional[str] = None,
    bake_type: Optional[str] = None,
    output_size: Optional[int] = None,
    collision_mesh_path: Optional[str] = None,
    replace_existing: Optional[bool] = None,
    plane_origin: Optional[Dict[str, float]] = None,
    plane_normal: Optional[Dict[str, float]] = None,
    extrude_distance: Optional[float] = None,
    unwrap_mode: Optional[str] = None,
    tool_name: Optional[str] = None,
    tool_parameters: Optional[Dict[str, Any]] = None,
    # UV Operation parameters
    generate: Optional[bool] = None,
    lightmap_coordinate_index: Optional[int] = None,
    # Actor merge parameters
    actor_paths: Optional[List[str]] = None
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
            - mesh_voxel_remesh
            - mesh_uv_layout
            - set_pivot
            - mesh_merge
            - set_vertex_colors
            - mesh_bake
            - poly_edit
            - modeling_tool_execute
            - generate_lods
            - generate_lightmap_uvs
            - import_ucx_collision
            - generate_box_uv_channel
            - generate_planar_uv_channel
            - generate_cylindrical_uv_channel
            - add_uv_channel
            - remove_uv_channel
            - set_lods
            - remove_lods
            - join_static_mesh_actors
            - merge_static_mesh_actors
            - create_proxy_mesh_actor
            - set_generate_lightmap_uvs
            - has_vertex_colors
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
        operation: Boolean operation ("Subtract", "Union", "Intersect") or poly edit operation.
        target_triangle_count: Target triangle count for remesh and simplify.
        num_lods: Number of LODs to generate (1-8).
        lod_index: LOD index for lightmap UV generation.
        voxel_count: Voxel count for voxel remesh.
        uv_channel: UV channel index for UV operations.
        pivot_location: Pivot location dict: {"x": 0.0, "y": 0.0, "z": 0.0}
        source_mesh_paths: List of source mesh paths for merge operation.
        color: Color dict: {"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0}
        paint_mode: Vertex color paint mode ("fill").
        bake_type: Bake type ("ambient_occlusion").
        output_size: Output texture size for baking.
        collision_mesh_path: Path to collision mesh for UCX import.
        replace_existing: Whether to replace existing collision.
        plane_origin: Plane origin for poly edit: {"x": 0.0, "y": 0.0, "z": 0.0}
        plane_normal: Plane normal for poly edit: {"x": 0.0, "y": 0.0, "z": 1.0}
        extrude_distance: Extrude distance for poly edit.
        unwrap_mode: UV unwrap mode ("auto_plot" or "repack").
        tool_name: Modeling Mode tool name for modeling_tool_execute diagnostics.
        tool_parameters: Tool-specific parameters for modeling_tool_execute.
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
            if target_triangle_count is not None: params["target_percentage"] = target_triangle_count / 100.0 * 100.0  # Convert to percentage
            return unreal.send_command("mesh_simplify", params)

        elif action == "mesh_uv_unwrap":
            params = {"asset_path": asset_path}
            if unwrap_mode is not None: params["unwrap_mode"] = unwrap_mode
            if uv_channel is not None: params["uv_channel"] = uv_channel
            return unreal.send_command("mesh_uv_unwrap", params)

        elif action == "mesh_voxel_remesh":
            params = {"asset_path": asset_path}
            if voxel_count is not None: params["voxel_count"] = voxel_count
            return unreal.send_command("mesh_voxel_remesh", params)

        elif action == "mesh_uv_layout":
            params = {"asset_path": asset_path}
            if uv_channel is not None: params["uv_channel"] = uv_channel
            return unreal.send_command("mesh_uv_layout", params)

        elif action == "set_pivot":
            params = {"asset_path": asset_path}
            if pivot_location is not None: params["pivot_location"] = pivot_location
            return unreal.send_command("set_pivot", params)

        elif action == "mesh_merge":
            params = {"asset_path": asset_path}
            if source_mesh_paths is not None: params["source_mesh_paths"] = source_mesh_paths
            return unreal.send_command("mesh_merge", params)

        elif action == "set_vertex_colors":
            params = {"asset_path": asset_path}
            if color is not None: params["color"] = color
            if paint_mode is not None: params["paint_mode"] = paint_mode
            return unreal.send_command("set_vertex_colors", params)

        elif action == "mesh_bake":
            params = {"asset_path": asset_path}
            if bake_type is not None: params["bake_type"] = bake_type
            if output_size is not None: params["output_size"] = output_size
            return unreal.send_command("mesh_bake", params)

        elif action == "poly_edit":
            params = {"asset_path": asset_path}
            if operation is not None: params["operation"] = operation
            if plane_origin is not None: params["plane_origin"] = plane_origin
            if plane_normal is not None: params["plane_normal"] = plane_normal
            if extrude_distance is not None: params["extrude_distance"] = extrude_distance
            return unreal.send_command("poly_edit", params)

        elif action == "modeling_tool_execute":
            params = {"asset_path": asset_path}
            if tool_name is not None: params["tool_name"] = tool_name
            if tool_parameters is not None: params["tool_parameters"] = tool_parameters
            return unreal.send_command("modeling_tool_execute", params)

        elif action == "generate_lods":
            params = {"asset_path": asset_path}
            if num_lods is not None: params["num_lods"] = num_lods
            return unreal.send_command("generate_lods", params)

        elif action == "generate_lightmap_uvs":
            params = {"asset_path": asset_path}
            if lod_index is not None: params["lod_index"] = lod_index
            return unreal.send_command("generate_lightmap_uvs", params)

        elif action == "import_ucx_collision":
            params = {"asset_path": asset_path, "collision_mesh_path": collision_mesh_path}
            if replace_existing is not None: params["replace_existing"] = replace_existing
            return unreal.send_command("import_ucx_collision", params)

        # UV Operations (UStaticMeshEditorSubsystem alternatives)
        elif action == "generate_box_uv_channel":
            params = {"asset_path": asset_path}
            if uv_channel is not None: params["uv_channel"] = uv_channel
            if lod_index is not None: params["lod_index"] = lod_index
            return unreal.send_command("generate_box_uv_channel", params)

        elif action == "generate_planar_uv_channel":
            params = {"asset_path": asset_path}
            if uv_channel is not None: params["uv_channel"] = uv_channel
            if lod_index is not None: params["lod_index"] = lod_index
            return unreal.send_command("generate_planar_uv_channel", params)

        elif action == "generate_cylindrical_uv_channel":
            params = {"asset_path": asset_path}
            if uv_channel is not None: params["uv_channel"] = uv_channel
            if lod_index is not None: params["lod_index"] = lod_index
            return unreal.send_command("generate_cylindrical_uv_channel", params)

        elif action == "add_uv_channel":
            params = {"asset_path": asset_path}
            if lod_index is not None: params["lod_index"] = lod_index
            return unreal.send_command("add_uv_channel", params)

        elif action == "remove_uv_channel":
            params = {"asset_path": asset_path}
            if uv_channel is not None: params["uv_channel"] = uv_channel
            return unreal.send_command("remove_uv_channel", params)

        # LOD Operations (UStaticMeshEditorSubsystem alternatives)
        elif action == "set_lods":
            params = {"asset_path": asset_path}
            return unreal.send_command("set_lods", params)

        elif action == "remove_lods":
            return unreal.send_command("remove_lods", {"asset_path": asset_path})

        # Mesh Merge Operations (UStaticMeshEditorSubsystem alternatives)
        elif action == "join_static_mesh_actors":
            params = {"asset_path": asset_path}
            if actor_paths is not None: params["actor_paths"] = actor_paths
            return unreal.send_command("join_static_mesh_actors", params)

        elif action == "merge_static_mesh_actors":
            params = {"asset_path": asset_path}
            if actor_paths is not None: params["actor_paths"] = actor_paths
            return unreal.send_command("merge_static_mesh_actors", params)

        elif action == "create_proxy_mesh_actor":
            params = {"asset_path": asset_path}
            if actor_paths is not None: params["actor_paths"] = actor_paths
            return unreal.send_command("create_proxy_mesh_actor", params)

        # Other utilities
        elif action == "set_generate_lightmap_uvs":
            params = {"asset_path": asset_path}
            if generate is not None: params["generate"] = generate
            if lightmap_coordinate_index is not None: params["lightmap_coordinate_index"] = lightmap_coordinate_index
            return unreal.send_command("set_generate_lightmap_uvs", params)

        elif action == "has_vertex_colors":
            return unreal.send_command("has_vertex_colors", {"asset_path": asset_path})

        else:
            return make_error_response(f"Unknown asset_mesh_editing_tool action: {action}")

    except Exception as e:
        logger.error(f"asset_mesh_editing_tool error: {e}")
        return make_error_response(str(e))
