"""Cesium for Unreal MCP tools (PoC).

Wraps the C++ Cesium command handler for capability detection. Heavier
operations (georeference, tileset spawn, geolocation placement) are exposed
as documented stubs that return actionable hints when the plugin is missing
or the C++-side implementation is not yet wired.
"""

from typing import Any, Dict, Optional

from server.core import mcp, get_unreal_connection
from server.validation import (
    validate_string,
    ValidationError,
    make_validation_error_response_from_exception,
)
from utils.responses import make_error_response


def _envelope(name: str, result: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(result, dict):
        return make_error_response(f"Unexpected Unreal response for '{name}'")
    if not result.get("success", False):
        err = result.get("error", "unknown error")
        hint = result.get("hint")
        if hint:
            return make_error_response(f"{name}: {err} (hint: {hint})")
        return make_error_response(f"{name}: {err}")
    return result


@mcp.tool()
def cesium_check_plugin() -> Dict[str, Any]:
    """Check whether the CesiumForUnreal plugin is installed and enabled.

    Returns the plugin descriptor info (version, enabled state) and per-module
    load status for `CesiumRuntime` / `CesiumEditor`. When unavailable the
    response includes a `hint` describing how to install the plugin (UE 5.7
    typically requires a source build).
    """
    try:
        conn = get_unreal_connection()
        result = conn.send_command("cesium_check_plugin", {})
    except Exception as e:
        return make_error_response(f"Failed to call Unreal command 'cesium_check_plugin': {e}")
    return _envelope("cesium_check_plugin", result)


@mcp.tool()
def cesium_setup_georeference(
    actor_name: str = "CesiumGeoreference",
    origin_latitude: float = 0.0,
    origin_longitude: float = 0.0,
    origin_height: float = 0.0,
) -> Dict[str, Any]:
    """[PoC stub] Spawn an `ACesiumGeoreference` actor anchored at a Lat/Lon/Height.

    This call always reaches the C++ side, but the underlying georeference spawn
    is intentionally a stub until the Cesium plugin is added as a public Build.cs
    dependency. The C++ handler returns an actionable error envelope describing
    the next implementation step.
    """
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        conn = get_unreal_connection()
        result = conn.send_command("cesium_setup_georeference", {
            "actor_name": actor_name,
            "origin_latitude": float(origin_latitude),
            "origin_longitude": float(origin_longitude),
            "origin_height": float(origin_height),
        })
    except Exception as e:
        return make_error_response(f"Failed to call Unreal command 'cesium_setup_georeference': {e}")
    return _envelope("cesium_setup_georeference", result)


@mcp.tool()
def cesium_add_tileset(
    actor_name: str = "Cesium3DTileset",
    ion_asset_id: Optional[int] = None,
    ion_access_token: Optional[str] = None,
    url: Optional[str] = None,
) -> Dict[str, Any]:
    """[PoC stub] Spawn an `ACesium3DTileset` actor sourced from an Ion asset or URL.

    Token values are NEVER logged. Pass `ion_access_token` only when needed.
    The C++ side currently returns an actionable error until the Cesium runtime
    dependency is wired (see plan 2026-05-13-cesium-wfc-async-semantic.md).
    """
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    params: Dict[str, Any] = {"actor_name": actor_name}
    if ion_asset_id is not None:
        params["ion_asset_id"] = int(ion_asset_id)
    if ion_access_token is not None:
        params["ion_access_token"] = ion_access_token
    if url is not None:
        params["url"] = url
    try:
        conn = get_unreal_connection()
        result = conn.send_command("cesium_add_tileset", params)
    except Exception as e:
        return make_error_response(f"Failed to call Unreal command 'cesium_add_tileset': {e}")
    return _envelope("cesium_add_tileset", result)


@mcp.tool()
def cesium_place_actor_at_geolocation(
    actor_mcp_id: str,
    latitude: float,
    longitude: float,
    height: float = 0.0,
) -> Dict[str, Any]:
    """[PoC stub] Convert Lat/Lon/Height to Unreal coordinates and move an actor.

    Requires the `ACesiumGeoreference` to exist in the level (use
    `cesium_setup_georeference` first). The C++ handler currently returns an
    actionable error envelope until the Cesium runtime dependency is wired.
    """
    try:
        validate_string(actor_mcp_id, "actor_mcp_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        conn = get_unreal_connection()
        result = conn.send_command("cesium_place_actor_at_geolocation", {
            "actor_mcp_id": actor_mcp_id,
            "latitude": float(latitude),
            "longitude": float(longitude),
            "height": float(height),
        })
    except Exception as e:
        return make_error_response(f"Failed to call Unreal command 'cesium_place_actor_at_geolocation': {e}")
    return _envelope("cesium_place_actor_at_geolocation", result)
