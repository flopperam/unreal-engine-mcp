"""Material-related tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from utils.responses import make_error_response, is_success_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def get_available_materials(
    search_path: str = "/Game/",
    include_engine_materials: bool = True
) -> Dict[str, Any]:
    """Get a list of available materials in the project that can be applied to objects."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "search_path": search_path,
            "include_engine_materials": include_engine_materials
        }
        response = unreal.send_command("get_available_materials", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"get_available_materials error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def apply_material_to_actor(
    actor_name: str,
    material_path: str,
    material_slot: int = 0
) -> Dict[str, Any]:
    """Apply a specific material to an actor in the level."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "actor_name": actor_name,
            "material_path": material_path,
            "material_slot": material_slot
        }
        response = unreal.send_command("apply_material_to_actor", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"apply_material_to_actor error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def apply_material_to_blueprint(
    blueprint_name: str,
    component_name: str,
    material_path: str,
    material_slot: int = 0
) -> Dict[str, Any]:
    """Apply a specific material to a component in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "material_path": material_path,
            "material_slot": material_slot
        }
        response = unreal.send_command("apply_material_to_blueprint", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"apply_material_to_blueprint error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def get_actor_material_info(
    actor_name: str
) -> Dict[str, Any]:
    """Get information about the materials currently applied to an actor."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"actor_name": actor_name}
        response = unreal.send_command("get_actor_material_info", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"get_actor_material_info error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def get_blueprint_material_info(
    blueprint_name: str,
    component_name: str
) -> Dict[str, Any]:
    """Get information about the materials currently applied to a Blueprint component."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name
        }
        response = unreal.send_command("get_blueprint_material_info", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"get_blueprint_material_info error: {e}")
        return make_error_response(str(e))


# Deprecated alias for backwards compatibility; prefer the decorated tool above.
# get_blueprint_material_info is the canonical MCP tool.
_get_blueprint_material_info_alias = get_blueprint_material_info


@mcp.tool()
def create_material_instance(
    parent_material: str,
    instance_name: str,
    package_path: str = "/Game/Materials/"
) -> Dict[str, Any]:
    """Create a Material Instance Constant (MIC) from a parent material asset."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "parent_material": parent_material,
            "instance_name": instance_name,
            "package_path": package_path,
        }
        response = unreal.send_command("create_material_instance", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_material_instance error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_dynamic_material_instance(
    actor_name: str,
    material_slot: int = 0,
    source_material: str = ""
) -> Dict[str, Any]:
    """Create a Material Instance Dynamic (MID) on an actor's mesh component."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "actor_name": actor_name,
            "material_slot": material_slot,
            "source_material": source_material,
        }
        response = unreal.send_command("create_dynamic_material_instance", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_dynamic_material_instance error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def batch_update_material_parameters(
    instance_path: str,
    parameters: List[Dict[str, Any]]
) -> Dict[str, Any]:
    """Batch update multiple parameters on a material instance in a single shader compile.

    Each parameter in the list should be a dict with:
    - name: str
    - type: "scalar" | "vector" | "texture" | "static_switch"
    - value: float for scalar, [R,G,B,A] list for vector, str path for texture, bool for static_switch
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "instance_path": instance_path,
            "parameters": parameters,
        }
        response = unreal.send_command("batch_update_material_parameters", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"batch_update_material_parameters error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_material_scalar_parameter(
    instance_path: str,
    parameter_name: str,
    value: float
) -> Dict[str, Any]:
    """Set a single scalar parameter on a material instance."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "instance_path": instance_path,
            "parameter_name": parameter_name,
            "value": value,
        }
        response = unreal.send_command("set_material_scalar_parameter", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_material_scalar_parameter error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_material_vector_parameter(
    instance_path: str,
    parameter_name: str,
    value: List[float]
) -> Dict[str, Any]:
    """Set a single vector/color parameter on a material instance. Value must be [R, G, B, A]."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    if not isinstance(value, list) or len(value) < 3:
        return make_error_response("Invalid color format. Must be a list of at least 3 float values [R, G, B, A].")

    try:
        params = {
            "instance_path": instance_path,
            "parameter_name": parameter_name,
            "value": value,
        }
        response = unreal.send_command("set_material_vector_parameter", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_material_vector_parameter error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_material_texture_parameter(
    instance_path: str,
    parameter_name: str,
    texture_path: str
) -> Dict[str, Any]:
    """Set a single texture parameter on a material instance."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "instance_path": instance_path,
            "parameter_name": parameter_name,
            "texture_path": texture_path,
        }
        response = unreal.send_command("set_material_texture_parameter", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_material_texture_parameter error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_material_static_switch_parameter(
    instance_path: str,
    parameter_name: str,
    value: bool
) -> Dict[str, Any]:
    """Set a single static switch parameter on a material instance."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "instance_path": instance_path,
            "parameter_name": parameter_name,
            "value": value,
        }
        response = unreal.send_command("set_material_static_switch_parameter", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_material_static_switch_parameter error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_material_parameter_collection(
    name: str,
    package_path: str = "/Game/Materials/"
) -> Dict[str, Any]:
    """Create a Material Parameter Collection asset."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "name": name,
            "package_path": package_path,
        }
        response = unreal.send_command("create_material_parameter_collection", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_material_parameter_collection error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def edit_material_parameter_collection(
    collection_path: str,
    add_scalars: Optional[List[str]] = None,
    add_vectors: Optional[List[str]] = None,
    remove_params: Optional[List[str]] = None
) -> Dict[str, Any]:
    """Edit a Material Parameter Collection by adding or removing scalar/vector parameters."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {"collection_path": collection_path}
        if add_scalars is not None:
            params["add_scalars"] = add_scalars
        if add_vectors is not None:
            params["add_vectors"] = add_vectors
        if remove_params is not None:
            params["remove_params"] = remove_params

        response = unreal.send_command("edit_material_parameter_collection", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"edit_material_parameter_collection error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_mesh_material_color(
    blueprint_name: str,
    component_name: str,
    color: List[float],
    material_path: str = "/Engine/BasicShapes/BasicShapeMaterial",
    parameter_name: str = "BaseColor",
    material_slot: int = 0
) -> Dict[str, Any]:
    """Set material color on a mesh component using the proven color system."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if not isinstance(color, list) or len(color) != 4:
            return make_error_response("Invalid color format. Must be a list of 4 float values [R, G, B, A].")

        validated_color = [float(min(1.0, max(0.0, val))) for val in color]

        params_primary = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "color": validated_color,
            "material_path": material_path,
            "parameter_name": parameter_name,
            "material_slot": material_slot
        }
        response_primary = unreal.send_command("set_mesh_material_color", params_primary)

        if response_primary and is_success_response(response_primary):
            return {
                "success": True,
                "message": f"Color applied via parameter '{parameter_name}' on slot {material_slot}: {validated_color}",
                "result": response_primary,
                "material_slot": material_slot
            }

        fallback_names = []
        if parameter_name != "BaseColor":
            fallback_names.append("BaseColor")
        if parameter_name != "Color":
            fallback_names.append("Color")

        fallback_results = []
        for fallback_name in fallback_names:
            params_fb = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "color": validated_color,
                "material_path": material_path,
                "parameter_name": fallback_name,
                "material_slot": material_slot
            }
            response_fb = unreal.send_command("set_mesh_material_color", params_fb)
            fallback_results.append((fallback_name, response_fb))
            if response_fb and is_success_response(response_fb):
                return {
                    "success": True,
                    "message": f"Color applied via fallback parameter '{fallback_name}' on slot {material_slot}: {validated_color}",
                    "result": response_fb,
                    "material_slot": material_slot,
                    "fallback_used": fallback_name
                }

        return make_error_response(
            f"Failed to set color on slot {material_slot}. "
            f"Primary parameter '{parameter_name}' and all fallbacks failed.",
            primary_result=response_primary,
            fallback_results=fallback_results,
        )

    except Exception as e:
        logger.error(f"set_mesh_material_color error: {e}")
        return make_error_response(str(e))