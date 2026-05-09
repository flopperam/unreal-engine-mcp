"""Rendering and lighting settings tools for the Unreal MCP server."""

import logging
from typing import Dict, Any

from server.core import mcp, get_unreal_connection
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def set_global_illumination(method: str) -> Dict[str, Any]:
    """Set the global illumination method.

    Supported methods: Off, Lumen, ScreenSpace, RayTraced
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"method": method}
        response = unreal.send_command("set_global_illumination", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_global_illumination error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_lumen_enabled(enabled: bool) -> Dict[str, Any]:
    """Enable or disable Lumen global illumination and reflections."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"enabled": enabled}
        response = unreal.send_command("set_lumen_enabled", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_lumen_enabled error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_lumen_scene_detail(
    card_refresh_fraction: float = -1.0,
    radiosity_iterations: int = -1
) -> Dict[str, Any]:
    """Set Lumen scene detail parameters.

    card_refresh_fraction: 0.0-1.0 (fraction of cards updated per frame)
    radiosity_iterations: number of propagation iterations
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {}
        if card_refresh_fraction >= 0.0:
            params["card_refresh_fraction"] = card_refresh_fraction
        if radiosity_iterations >= 0:
            params["radiosity_iterations"] = radiosity_iterations
        response = unreal.send_command("set_lumen_scene_detail", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_lumen_scene_detail error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_lumen_reflection_quality(
    max_bounces: int = -1,
    screen_trace_iterations: int = -1
) -> Dict[str, Any]:
    """Set Lumen reflection quality parameters."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {}
        if max_bounces >= 0:
            params["max_bounces"] = max_bounces
        if screen_trace_iterations >= 0:
            params["screen_trace_iterations"] = screen_trace_iterations
        response = unreal.send_command("set_lumen_reflection_quality", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_lumen_reflection_quality error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_hardware_ray_tracing(enabled: bool) -> Dict[str, Any]:
    """Enable or disable hardware ray tracing (requires compatible GPU)."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"enabled": enabled}
        response = unreal.send_command("set_hardware_ray_tracing", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_hardware_ray_tracing error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_path_tracing(
    enabled: bool = True,
    max_bounces: int = -1
) -> Dict[str, Any]:
    """Enable or disable path tracing and set max bounces."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {"enabled": enabled}
        if max_bounces >= 0:
            params["max_bounces"] = max_bounces
        response = unreal.send_command("set_path_tracing", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_path_tracing error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_virtual_shadow_maps(
    enabled: bool = True,
    resolution_lod_bias: float = 0.0
) -> Dict[str, Any]:
    """Enable or disable virtual shadow maps (VSM)."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "enabled": enabled,
            "resolution_lod_bias": resolution_lod_bias,
        }
        response = unreal.send_command("set_virtual_shadow_maps", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_virtual_shadow_maps error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_shadow_quality(
    max_cascades: int = -1,
    distance_scale: float = -1.0
) -> Dict[str, Any]:
    """Set shadow quality parameters for CSM shadows."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {}
        if max_cascades >= 0:
            params["max_cascades"] = max_cascades
        if distance_scale > 0.0:
            params["distance_scale"] = distance_scale
        response = unreal.send_command("set_shadow_quality", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_shadow_quality error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_anti_aliasing(method: str) -> Dict[str, Any]:
    """Set the anti-aliasing method.

    Supported methods: None, FXAA, TAA, TSR, MSAA
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"method": method}
        response = unreal.send_command("set_anti_aliasing", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_anti_aliasing error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_tsr_settings(
    algorithm: str = "",
    history_screen_percentage: float = -1.0
) -> Dict[str, Any]:
    """Set Temporal Super Resolution (TSR) settings.

    algorithm: Gen4 or Gen5
    history_screen_percentage: percentage for history buffer resolution
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {}
        if algorithm:
            params["algorithm"] = algorithm
        if history_screen_percentage >= 0.0:
            params["history_screen_percentage"] = history_screen_percentage
        response = unreal.send_command("set_tsr_settings", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_tsr_settings error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_upscaler(upscaler: str, enabled: bool) -> Dict[str, Any]:
    """Enable or disable an upscaling technology.

    Supported upscalers: DLSS, FSR, XeSS, NIS
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "upscaler": upscaler,
            "enabled": enabled,
        }
        response = unreal.send_command("set_upscaler", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_upscaler error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_nanite_visualization(mode: str) -> Dict[str, Any]:
    """Set Nanite visualization mode.

    Supported modes: Off, Clusters, Triangles
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"mode": mode}
        response = unreal.send_command("set_nanite_visualization", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_nanite_visualization error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def get_shader_compile_status() -> Dict[str, Any]:
    """Get the current shader compilation status."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("get_shader_compile_status", {})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"get_shader_compile_status error: {e}")
        return make_error_response(str(e))
