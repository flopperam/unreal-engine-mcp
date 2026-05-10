"""Audio tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def create_sound_cue(cue_path: str, sound_wave_path: str = "") -> Dict[str, Any]:
    """Create a SoundCue asset.

    cue_path: Asset path (e.g., /Game/Audio/MyCue)
    sound_wave_path: Optional path to a SoundWave to reference
    """
    try:
        validate_string(cue_path, "cue_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"cue_path": cue_path}
        if sound_wave_path:
            params["sound_wave_path"] = sound_wave_path
        response = unreal.send_command("create_sound_cue", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_sound_cue error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_audio_component(
    actor_name: str,
    sound_path: str,
    volume: float = 1.0,
    pitch: float = 1.0,
    auto_activate: bool = False,
    loop: bool = False,
) -> Dict[str, Any]:
    """Add an AudioComponent to an actor.

    actor_name: Name of the target actor
    sound_path: Path to the SoundCue or SoundWave asset
    volume: Volume multiplier
    pitch: Pitch multiplier
    auto_activate: Whether to auto-play on begin play
    loop: Whether to loop (cue-dependent)
    """
    try:
        validate_string(actor_name, "actor_name")
        validate_string(sound_path, "sound_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "add_audio_component",
            {
                "actor_name": actor_name,
                "sound_path": sound_path,
                "volume": volume,
                "pitch": pitch,
                "auto_activate": auto_activate,
                "loop": loop,
            },
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_audio_component error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_sound_attenuation(
    attenuation_path: str,
    radius: Optional[float] = None,
    spatialization: Optional[bool] = None,
    cone_attenuation: Optional[bool] = None,
    cone_inner_angle: Optional[float] = None,
    cone_outer_angle: Optional[float] = None,
    reverb_send: Optional[float] = None,
) -> Dict[str, Any]:
    """Create or update a SoundAttenuation asset.

    attenuation_path: Asset path (e.g., /Game/Audio/MyAttenuation)
    radius: Falloff distance
    spatialization: Enable 3D spatialization
    cone_attenuation: Enable cone-based attenuation
    cone_inner_angle: Inner cone angle in degrees
    cone_outer_angle: Outer cone angle in degrees
    reverb_send: Reverb send distance
    """
    try:
        validate_string(attenuation_path, "attenuation_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {"attenuation_path": attenuation_path}
        if radius is not None:
            params["radius"] = radius
        if spatialization is not None:
            params["spatialization"] = spatialization
        if cone_attenuation is not None:
            params["cone_attenuation"] = cone_attenuation
        if cone_inner_angle is not None:
            params["cone_inner_angle"] = cone_inner_angle
        if cone_outer_angle is not None:
            params["cone_outer_angle"] = cone_outer_angle
        if reverb_send is not None:
            params["reverb_send"] = reverb_send
        response = unreal.send_command("set_sound_attenuation", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_sound_attenuation error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_sound_class(
    asset_path: str,
    volume: float = 1.0,
    pitch: float = 1.0,
) -> Dict[str, Any]:
    """Create a SoundClass asset.

    asset_path: Asset path (e.g., /Game/Audio/MySoundClass)
    volume: Default volume multiplier
    pitch: Default pitch multiplier
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
            "create_sound_class",
            {"asset_path": asset_path, "volume": volume, "pitch": pitch},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_sound_class error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_sound_mix(
    asset_path: str,
) -> Dict[str, Any]:
    """Create a SoundMix asset.

    asset_path: Asset path (e.g., /Game/Audio/MySoundMix)
    """
    try:
        validate_string(asset_path, "asset_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("create_sound_mix", {"asset_path": asset_path})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_sound_mix error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def spawn_ambient_sound(
    sound_path: str,
    actor_name: str = "AmbientSound",
    location: Optional[Dict[str, float]] = None,
    volume: float = 1.0,
    pitch: float = 1.0,
) -> Dict[str, Any]:
    """Spawn an AmbientSound actor in the level.

    sound_path: Path to the SoundCue or SoundWave asset
    actor_name: Actor name
    location: {"x": 0, "y": 0, "z": 0}
    volume: Volume multiplier
    pitch: Pitch multiplier
    """
    try:
        validate_string(sound_path, "sound_path")
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params: Dict[str, Any] = {
            "sound_path": sound_path,
            "actor_name": actor_name,
            "volume": volume,
            "pitch": pitch,
        }
        if location is not None:
            params["location"] = location
        response = unreal.send_command("spawn_ambient_sound", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"spawn_ambient_sound error: {e}")
        return make_error_response(str(e))
