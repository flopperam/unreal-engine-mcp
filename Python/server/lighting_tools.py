"""Lighting and atmosphere tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, validate_vector3, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


# ---------------------------------------------------------------------------
# Phase 1: Basic Light Property Control
# ---------------------------------------------------------------------------

@mcp.tool()
def set_light_intensity(actor_name: str, intensity: float) -> Dict[str, Any]:
    """Set the intensity of a light actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_intensity", {"actor_name": actor_name, "intensity": intensity})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_intensity error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_color(actor_name: str, color: List[float]) -> Dict[str, Any]:
    """Set the color of a light actor. Color should be [R, G, B] with values 0.0-1.0."""
    try:
        validate_string(actor_name, "actor_name")
        validate_vector3(color, "color")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_color", {"actor_name": actor_name, "color": color})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_color error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_temperature(actor_name: str, temperature: float = 6500.0, enabled: bool = True) -> Dict[str, Any]:
    """Set the temperature (in Kelvin) of a light actor. Enable/disable temperature usage."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_temperature", {"actor_name": actor_name, "temperature": temperature, "enabled": enabled})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_temperature error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_mobility(actor_name: str, mobility: str) -> Dict[str, Any]:
    """Set the mobility of a light actor. Options: Static, Stationary, Movable."""
    try:
        validate_string(actor_name, "actor_name")
        validate_string(mobility, "mobility")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    if mobility not in {"Static", "Stationary", "Movable"}:
        return make_error_response("mobility must be one of: Static, Stationary, Movable")
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_mobility", {"actor_name": actor_name, "mobility": mobility})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_mobility error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_shadow_enabled(actor_name: str, enabled: bool = True) -> Dict[str, Any]:
    """Enable or disable shadow casting for a light actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_shadow_enabled", {"actor_name": actor_name, "enabled": enabled})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_shadow_enabled error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_shadow_bias(actor_name: str, bias: float) -> Dict[str, Any]:
    """Set the shadow bias of a light actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_shadow_bias", {"actor_name": actor_name, "bias": bias})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_shadow_bias error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_contact_shadows(actor_name: str, enabled: bool = True, length: float = 0.0) -> Dict[str, Any]:
    """Enable/disable contact shadows for a light actor, with optional length."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_contact_shadows", {"actor_name": actor_name, "enabled": enabled, "length": length})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_contact_shadows error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_volumetric_scattering(actor_name: str, enabled: bool = True, intensity: float = 1.0) -> Dict[str, Any]:
    """Enable/disable volumetric scattering for a light actor, with optional intensity."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_volumetric_scattering", {"actor_name": actor_name, "enabled": enabled, "intensity": intensity})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_volumetric_scattering error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_attenuation_radius(actor_name: str, radius: float) -> Dict[str, Any]:
    """Set the attenuation radius for a point/spot light actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_attenuation_radius", {"actor_name": actor_name, "radius": radius})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_attenuation_radius error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_cone_angles(actor_name: str, inner: float = 0.0, outer: float = 44.0) -> Dict[str, Any]:
    """Set the inner and outer cone angles (in degrees) for a spot light actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_cone_angles", {"actor_name": actor_name, "inner": inner, "outer": outer})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_cone_angles error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_source_radius(actor_name: str, radius: float = 0.0, soft_radius: float = 0.0) -> Dict[str, Any]:
    """Set the source radius and soft source radius for a point/spot light actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_source_radius", {"actor_name": actor_name, "radius": radius, "soft_radius": soft_radius})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_source_radius error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Phase 2: Advanced Light Properties
# ---------------------------------------------------------------------------

@mcp.tool()
def set_light_ies_profile(actor_name: str, ies_path: str) -> Dict[str, Any]:
    """Apply an IES light profile texture to a light actor."""
    try:
        validate_string(actor_name, "actor_name")
        validate_string(ies_path, "ies_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_ies_profile", {"actor_name": actor_name, "ies_path": ies_path})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_ies_profile error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_light_channel(actor_name: str, channel: int) -> Dict[str, Any]:
    """Set the lighting channel for a light actor. Valid values: 0-3."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    if channel < 0 or channel > 3:
        return make_error_response("channel must be between 0 and 3")
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_light_channel", {"actor_name": actor_name, "channel": channel})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_light_channel error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_rect_light_properties(actor_name: str, source_width: Optional[float] = None, source_height: Optional[float] = None, barn_door_angle: Optional[float] = None, barn_door_length: Optional[float] = None) -> Dict[str, Any]:
    """Set properties specific to a rect light actor (source width/height, barn door angle/length)."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"actor_name": actor_name}
        if source_width is not None:
            params["source_width"] = source_width
        if source_height is not None:
            params["source_height"] = source_height
        if barn_door_angle is not None:
            params["barn_door_angle"] = barn_door_angle
        if barn_door_length is not None:
            params["barn_door_length"] = barn_door_length
        response = unreal.send_command("set_rect_light_properties", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_rect_light_properties error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Phase 3: Sky & Atmosphere
# ---------------------------------------------------------------------------

@mcp.tool()
def set_sky_light_properties(actor_name: str, cubemap_path: Optional[str] = None, intensity: Optional[float] = None, lower_hemisphere_color: Optional[List[float]] = None) -> Dict[str, Any]:
    """Set properties for a sky light actor (cubemap, intensity, lower hemisphere color)."""
    try:
        validate_string(actor_name, "actor_name")
        if cubemap_path is not None:
            validate_string(cubemap_path, "cubemap_path")
        if lower_hemisphere_color is not None:
            validate_vector3(lower_hemisphere_color, "lower_hemisphere_color")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"actor_name": actor_name}
        if cubemap_path is not None:
            params["cubemap_path"] = cubemap_path
        if intensity is not None:
            params["intensity"] = intensity
        if lower_hemisphere_color is not None:
            params["lower_hemisphere_color"] = lower_hemisphere_color
        response = unreal.send_command("set_sky_light_properties", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_sky_light_properties error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_sky_atmosphere_properties(actor_name: str, ground_radius: Optional[float] = None, atmosphere_height: Optional[float] = None, mie_scattering: Optional[float] = None, rayleigh_scattering: Optional[float] = None, sun_illuminance_scale: Optional[float] = None) -> Dict[str, Any]:
    """Set properties for a sky atmosphere actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"actor_name": actor_name}
        if ground_radius is not None:
            params["ground_radius"] = ground_radius
        if atmosphere_height is not None:
            params["atmosphere_height"] = atmosphere_height
        if mie_scattering is not None:
            params["mie_scattering"] = mie_scattering
        if rayleigh_scattering is not None:
            params["rayleigh_scattering"] = rayleigh_scattering
        if sun_illuminance_scale is not None:
            params["sun_illuminance_scale"] = sun_illuminance_scale
        response = unreal.send_command("set_sky_atmosphere_properties", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_sky_atmosphere_properties error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_height_fog_properties(actor_name: str, fog_density: Optional[float] = None, fog_height_falloff: Optional[float] = None, fog_max_opacity: Optional[float] = None, start_distance: Optional[float] = None, light_inscattering_color: Optional[List[float]] = None) -> Dict[str, Any]:
    """Set properties for an exponential height fog actor."""
    try:
        validate_string(actor_name, "actor_name")
        if light_inscattering_color is not None:
            validate_vector3(light_inscattering_color, "light_inscattering_color")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"actor_name": actor_name}
        if fog_density is not None:
            params["fog_density"] = fog_density
        if fog_height_falloff is not None:
            params["fog_height_falloff"] = fog_height_falloff
        if fog_max_opacity is not None:
            params["fog_max_opacity"] = fog_max_opacity
        if start_distance is not None:
            params["start_distance"] = start_distance
        if light_inscattering_color is not None:
            params["light_inscattering_color"] = light_inscattering_color
        response = unreal.send_command("set_height_fog_properties", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_height_fog_properties error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_volumetric_fog(actor_name: str, enabled: bool = True) -> Dict[str, Any]:
    """Enable or disable volumetric fog for an exponential height fog actor."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_volumetric_fog", {"actor_name": actor_name, "enabled": enabled})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_volumetric_fog error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Phase 4: Sun, Environment, Reflections
# ---------------------------------------------------------------------------

@mcp.tool()
def set_directional_light_as_sun(actor_name: str, is_sun: bool = True) -> Dict[str, Any]:
    """Tag a directional light as the sun for sky atmosphere integration."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_directional_light_as_sun", {"actor_name": actor_name, "is_sun": is_sun})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_directional_light_as_sun error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_sun_position(actor_name: str, azimuth: float = 0.0, zenith: float = 45.0) -> Dict[str, Any]:
    """Set the sun position by rotating a directional light actor. Azimuth = yaw, Zenith = angle from vertical."""
    try:
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_sun_position", {"actor_name": actor_name, "azimuth": azimuth, "zenith": zenith})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_sun_position error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_hdri_backdrop(actor_name: str, hdri_path: str, intensity: float = 1.0, rotation: Optional[List[float]] = None) -> Dict[str, Any]:
    """Create a sky light actor with an HDRI cubemap for environment lighting."""
    try:
        validate_string(actor_name, "actor_name")
        validate_string(hdri_path, "hdri_path")
        if rotation is not None:
            validate_vector3(rotation, "rotation")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"actor_name": actor_name, "hdri_path": hdri_path, "intensity": intensity}
        if rotation is not None:
            params["rotation"] = rotation
        response = unreal.send_command("create_hdri_backdrop", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_hdri_backdrop error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_reflection_capture(actor_name: str, type: str, location: Optional[List[float]] = None, radius: Optional[float] = None, extent: Optional[List[float]] = None, brightness: float = 1.0) -> Dict[str, Any]:
    """Create a sphere or box reflection capture actor."""
    try:
        validate_string(actor_name, "actor_name")
        validate_string(type, "type")
        if location is not None:
            validate_vector3(location, "location")
        if extent is not None:
            validate_vector3(extent, "extent")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    if type not in {"Sphere", "Box"}:
        return make_error_response("type must be one of: Sphere, Box")
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"actor_name": actor_name, "type": type, "brightness": brightness}
        if location is not None:
            params["location"] = location
        if radius is not None:
            params["radius"] = radius
        if extent is not None:
            params["extent"] = extent
        response = unreal.send_command("create_reflection_capture", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_reflection_capture error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_reflection_capture_settings(actor_name: str, brightness: Optional[float] = None, capture_offset: Optional[List[float]] = None) -> Dict[str, Any]:
    """Adjust brightness and capture offset of an existing reflection capture actor."""
    try:
        validate_string(actor_name, "actor_name")
        if capture_offset is not None:
            validate_vector3(capture_offset, "capture_offset")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"actor_name": actor_name}
        if brightness is not None:
            params["brightness"] = brightness
        if capture_offset is not None:
            params["capture_offset"] = capture_offset
        response = unreal.send_command("set_reflection_capture_settings", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_reflection_capture_settings error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def build_reflection_captures() -> Dict[str, Any]:
    """Trigger a build of all reflection captures in the level."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("build_reflection_captures", {})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"build_reflection_captures error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Phase 5: Production Pipeline
# ---------------------------------------------------------------------------

@mcp.tool()
def create_lightmass_importance_volume(location: Optional[List[float]] = None, extent: Optional[List[float]] = None) -> Dict[str, Any]:
    """Create a Lightmass Importance Volume at the given location and extent."""
    try:
        if location is not None:
            validate_vector3(location, "location")
        if extent is not None:
            validate_vector3(extent, "extent")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {}
        if location is not None:
            params["location"] = location
        if extent is not None:
            params["extent"] = extent
        response = unreal.send_command("create_lightmass_importance_volume", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_lightmass_importance_volume error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def build_lighting(quality: str = "Preview") -> Dict[str, Any]:
    """Build lighting for the current level. Quality options: Preview, Medium, High, Production."""
    if quality not in {"Preview", "Medium", "High", "Production"}:
        return make_error_response("quality must be one of: Preview, Medium, High, Production")
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("build_lighting", {"quality": quality})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"build_lighting error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_lighting_scenario(scenario_name: str, activate: bool = True) -> Dict[str, Any]:
    """Activate or deactivate a lighting scenario by streaming level name."""
    try:
        validate_string(scenario_name, "scenario_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_lighting_scenario", {"scenario_name": scenario_name, "activate": activate})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_lighting_scenario error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_megaliights(enabled: bool = True, quality: int = 1) -> Dict[str, Any]:
    """Enable/disable MegaLights and set quality level."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("set_megaliights", {"enabled": enabled, "quality": quality})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_megaliights error: {e}")
        return make_error_response(str(e))
