"""Sequencer tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def create_level_sequence(
    sequence_path: str,
    duration_frames: int = 150,
    frame_rate_numerator: int = 30,
    frame_rate_denominator: int = 1,
) -> Dict[str, Any]:
    """Create a new Level Sequence asset.

    sequence_path: Asset path (e.g., /Game/Cinematics/MySequence)
    duration_frames: Total duration in frames (default: 150)
    frame_rate_numerator: Frame rate numerator (default: 30)
    frame_rate_denominator: Frame rate denominator (default: 1)
    """
    try:
        validate_string(sequence_path, "sequence_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "create_level_sequence",
            {
                "sequence_path": sequence_path,
                "duration_frames": duration_frames,
                "frame_rate_numerator": frame_rate_numerator,
                "frame_rate_denominator": frame_rate_denominator,
            },
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_level_sequence error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_actor_binding(sequence_path: str, actor_name: str) -> Dict[str, Any]:
    """Bind an actor from the current level to a Level Sequence.

    sequence_path: Asset path to the Level Sequence
    actor_name: Name of the actor in the level to bind
    """
    try:
        validate_string(sequence_path, "sequence_path")
        validate_string(actor_name, "actor_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "add_actor_binding",
            {"sequence_path": sequence_path, "actor_name": actor_name},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_actor_binding error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_transform_track(sequence_path: str, binding_guid: str) -> Dict[str, Any]:
    """Add a 3D transform track to a bound actor in a Level Sequence.

    sequence_path: Asset path to the Level Sequence
    binding_guid: GUID of the actor binding (returned by add_actor_binding)
    """
    try:
        validate_string(sequence_path, "sequence_path")
        validate_string(binding_guid, "binding_guid")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "add_transform_track",
            {"sequence_path": sequence_path, "binding_guid": binding_guid},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_transform_track error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_camera_cut_track(sequence_path: str, camera_binding_guid: Optional[str] = None) -> Dict[str, Any]:
    """Add a camera cut track to a Level Sequence.

    sequence_path: Asset path to the Level Sequence
    camera_binding_guid: Optional GUID of the camera binding to cut to
    """
    try:
        validate_string(sequence_path, "sequence_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        payload: Dict[str, Any] = {"sequence_path": sequence_path}
        if camera_binding_guid:
            payload["camera_binding_guid"] = camera_binding_guid
        response = unreal.send_command("add_camera_cut_track", payload)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_camera_cut_track error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_event_track(sequence_path: str, binding_guid: str) -> Dict[str, Any]:
    """Add an event track to a bound actor in a Level Sequence.

    sequence_path: Asset path to the Level Sequence
    binding_guid: GUID of the actor binding
    """
    try:
        validate_string(sequence_path, "sequence_path")
        validate_string(binding_guid, "binding_guid")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "add_event_track",
            {"sequence_path": sequence_path, "binding_guid": binding_guid},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_event_track error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_keyframe(
    sequence_path: str,
    binding_guid: str,
    frame: int = 0,
    location: Optional[Dict[str, float]] = None,
    rotation: Optional[Dict[str, float]] = None,
    scale: Optional[Dict[str, float]] = None,
) -> Dict[str, Any]:
    """Add a keyframe to a transform track for a bound actor.

    sequence_path: Asset path to the Level Sequence
    binding_guid: GUID of the actor binding
    frame: Frame number for the keyframe
    location: Optional {"x", "y", "z"} dict
    rotation: Optional {"x", "y", "z"} dict (Euler degrees)
    scale: Optional {"x", "y", "z"} dict
    """
    try:
        validate_string(sequence_path, "sequence_path")
        validate_string(binding_guid, "binding_guid")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        payload: Dict[str, Any] = {
            "sequence_path": sequence_path,
            "binding_guid": binding_guid,
            "frame": frame,
        }
        if location:
            payload["location"] = location
        if rotation:
            payload["rotation"] = rotation
        if scale:
            payload["scale"] = scale
        response = unreal.send_command("add_keyframe", payload)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_keyframe error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_playback_range(
    sequence_path: str, start_frame: int = 0, end_frame: int = 150
) -> Dict[str, Any]:
    """Set the playback range of a Level Sequence.

    sequence_path: Asset path to the Level Sequence
    start_frame: Start frame
    end_frame: End frame
    """
    try:
        validate_string(sequence_path, "sequence_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "set_playback_range",
            {
                "sequence_path": sequence_path,
                "start_frame": start_frame,
                "end_frame": end_frame,
            },
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_playback_range error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_frame_rate(
    sequence_path: str, numerator: int = 30, denominator: int = 1
) -> Dict[str, Any]:
    """Set the display frame rate of a Level Sequence.

    sequence_path: Asset path to the Level Sequence
    numerator: Frame rate numerator (e.g., 30, 24, 60)
    denominator: Frame rate denominator (e.g., 1, 1001)
    """
    try:
        validate_string(sequence_path, "sequence_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    if denominator <= 0:
        return make_error_response("denominator must be > 0")
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "set_frame_rate",
            {
                "sequence_path": sequence_path,
                "numerator": numerator,
                "denominator": denominator,
            },
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_frame_rate error: {e}")
        return make_error_response(str(e))
