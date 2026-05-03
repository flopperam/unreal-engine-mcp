"""Actor management tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from server.validation import (
    validate_vector3, validate_string, validate_unreal_path,
    ValidationError, make_validation_error_response_from_exception,
    MAX_ACTORS_PER_BATCH,
)
from utils.responses import make_error_response, is_success_response
from helpers.actor_name_manager import safe_spawn_actor, safe_delete_actor

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def get_actors_in_level() -> Dict[str, Any]:
    """Get a list of all actors in the current level."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("get_actors_in_level", {})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"get_actors_in_level error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def find_actors_by_name(pattern: str) -> Dict[str, Any]:
    """Find actors whose name contains the given pattern (case-insensitive substring match)."""
    try:
        validate_string(pattern, "pattern")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("find_actors_by_name", {"pattern": pattern})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"find_actors_by_name error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def delete_actor(name: str) -> Dict[str, Any]:
    """Delete an actor by name."""
    try:
        validate_string(name, "name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = safe_delete_actor(unreal, name)
        return response
    except Exception as e:
        logger.error(f"delete_actor error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def spawn_actor(
    type: str,
    name: str,
    location: Optional[List[float]] = None,
    rotation: Optional[List[float]] = None,
    scale: Optional[List[float]] = None,
    static_mesh: Optional[str] = None,
    mcp_id: Optional[str] = None,
    tags: Optional[List[str]] = None
) -> Dict[str, Any]:
    """Spawn an actor by type and name. Optionally assign an mcp_id tag and additional tags for scene sync identity."""
    try:
        validate_string(type, "type")
        validate_string(name, "name")
        validate_vector3(location, "location", allow_none=True)
        validate_vector3(rotation, "rotation", allow_none=True)
        validate_vector3(scale, "scale", allow_none=True)
        if static_mesh is not None:
            validate_unreal_path(static_mesh, "static_mesh")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "type": type,
            "name": name
        }
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation
        if scale is not None:
            params["scale"] = scale
        if static_mesh is not None:
            params["static_mesh"] = static_mesh
        if mcp_id is not None:
            params["mcp_id"] = mcp_id
        if tags is not None:
            params["tags"] = tags

        return safe_spawn_actor(unreal, params, auto_unique_name=False)
    except Exception as e:
        logger.error(f"spawn_actor error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_actor_transform(
    name: str,
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None
) -> Dict[str, Any]:
    """Set the transform of an actor."""
    try:
        validate_string(name, "name")
        validate_vector3(location, "location", allow_none=True)
        validate_vector3(rotation, "rotation", allow_none=True)
        validate_vector3(scale, "scale", allow_none=True)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation
        if scale is not None:
            params["scale"] = scale

        response = unreal.send_command("set_actor_transform", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_actor_transform error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def batch_spawn_actors(
    actors: List[Dict[str, Any]],
    dry_run: bool = False
) -> Dict[str, Any]:
    """Spawn multiple actors in a single call. Each actor dict should have 'name', 'type', and optionally 'location', 'rotation', 'scale', 'static_mesh', 'mcp_id', 'tags'.

    Example:
        actors = [
            {"name": "Wall_001", "type": "StaticMeshActor", "location": [0,0,0], "static_mesh": "/Engine/BasicShapes/Cube.Cube", "mcp_id": "wall_001", "tags": ["managed_by_mcp"]},
            {"name": "Wall_002", "type": "StaticMeshActor", "location": [100,0,0]}
        ]
    """
    from server.validation import validate_positive_int
    if not isinstance(actors, list):
        return make_error_response("actors must be a list of actor dictionaries")
    if len(actors) == 0:
        return make_error_response("actors list must not be empty")
    if len(actors) > MAX_ACTORS_PER_BATCH:
        return make_error_response(
            f"Requested {len(actors)} actors exceeds batch limit of {MAX_ACTORS_PER_BATCH}",
            requested=len(actors),
            max_actors=MAX_ACTORS_PER_BATCH,
        )

    validated = []
    for i, actor_def in enumerate(actors):
        if not isinstance(actor_def, dict):
            return make_error_response(f"actors[{i}] must be a dictionary, got {type(actor_def).__name__}")
        try:
            name = validate_string(actor_def.get("name"), f"actors[{i}].name")
            actor_type = validate_string(actor_def.get("type"), f"actors[{i}].type")
            loc = validate_vector3(actor_def.get("location"), f"actors[{i}].location", allow_none=True)
            rot = validate_vector3(actor_def.get("rotation"), f"actors[{i}].rotation", allow_none=True)
            scl = validate_vector3(actor_def.get("scale"), f"actors[{i}].scale", allow_none=True)
            mesh = actor_def.get("static_mesh")
            if mesh is not None:
                mesh = validate_unreal_path(mesh, f"actors[{i}].static_mesh")
            mcp_id_val = actor_def.get("mcp_id")
            if mcp_id_val is not None:
                mcp_id_val = validate_string(mcp_id_val, f"actors[{i}].mcp_id")
            tags_val = actor_def.get("tags")
            if tags_val is not None and not isinstance(tags_val, list):
                return make_error_response(f"actors[{i}].tags must be a list of strings")
        except ValidationError as e:
            return make_validation_error_response_from_exception(e)
        validated.append({
            "name": name,
            "type": actor_type,
            "location": loc,
            "rotation": rot,
            "scale": scl,
            "static_mesh": mesh,
            "mcp_id": mcp_id_val,
            "tags": tags_val,
        })

    if dry_run:
        return {
            "success": True,
            "dry_run": True,
            "actor_count": len(validated),
            "actors": validated,
            "message": f"Would spawn {len(validated)} actors. Set dry_run=False to execute.",
        }

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    spawned = []
    failed = []
    for actor_def in validated:
        params = {"name": actor_def["name"], "type": actor_def["type"]}
        if actor_def["location"] is not None:
            params["location"] = actor_def["location"]
        if actor_def["rotation"] is not None:
            params["rotation"] = actor_def["rotation"]
        if actor_def["scale"] is not None:
            params["scale"] = actor_def["scale"]
        if actor_def["static_mesh"] is not None:
            params["static_mesh"] = actor_def["static_mesh"]
        if actor_def.get("mcp_id") is not None:
            params["mcp_id"] = actor_def["mcp_id"]
        if actor_def.get("tags") is not None:
            params["tags"] = actor_def["tags"]
        try:
            resp = safe_spawn_actor(unreal, params)
            if resp and is_success_response(resp):
                spawned.append(resp)
            else:
                failed.append({"name": actor_def["name"], "reason": str(resp)})
        except Exception as e:
            failed.append({"name": actor_def["name"], "reason": str(e)})

    result = {
        "success": len(failed) == 0,
        "spawned_count": len(spawned),
        "failed_count": len(failed),
        "actors": spawned,
    }
    if failed:
        result["failed"] = failed
        result["error"] = f"Spawned {len(spawned)}/{len(validated)} actors. {len(failed)} failed."
    else:
        result["message"] = f"Successfully spawned all {len(spawned)} actors."
    return result


@mcp.tool()
def find_actor_by_mcp_id(mcp_id: str) -> Dict[str, Any]:
    """Find an actor by its mcp_id tag. Returns the actor details or an error if not found or multiple matches exist."""
    try:
        validate_string(mcp_id, "mcp_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("find_actor_by_mcp_id", {"mcp_id": mcp_id})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"find_actor_by_mcp_id error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_actor_transform_by_mcp_id(
    mcp_id: str,
    location: Optional[List[float]] = None,
    rotation: Optional[List[float]] = None,
    scale: Optional[List[float]] = None
) -> Dict[str, Any]:
    """Set the transform of an actor identified by its mcp_id tag. More stable than name-based transform."""
    try:
        validate_string(mcp_id, "mcp_id")
        validate_vector3(location, "location", allow_none=True)
        validate_vector3(rotation, "rotation", allow_none=True)
        validate_vector3(scale, "scale", allow_none=True)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"mcp_id": mcp_id}
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation
        if scale is not None:
            params["scale"] = scale

        response = unreal.send_command("set_actor_transform_by_mcp_id", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_actor_transform_by_mcp_id error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def request_cognitive_processing(
    actor_name: str,
    context: str = ""
) -> Dict[str, Any]:
    """
    Request cognitive processing (simulated LLM inference) for an AI actor.
    Target actor should be a CognitiveAIController or an actor that has 
    the RequestCognitiveProcessing function.
    
    :param actor_name: The name or MCP ID of the AI agent/controller.
    :param context: Environmental context for the AI to process.
    """
    try:
        unreal = get_unreal_connection()
        response = unreal.send_command("request_cognitive_processing", {
            "actor_name": actor_name,
            "context": context
        })
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"request_cognitive_processing error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def delete_actor_by_mcp_id(mcp_id: str) -> Dict[str, Any]:
    """Delete an actor by its mcp_id tag. Idempotent: returns success if actor is already missing."""
    try:
        validate_string(mcp_id, "mcp_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("delete_actor_by_mcp_id", {"mcp_id": mcp_id})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"delete_actor_by_mcp_id error: {e}")
        return make_error_response(str(e))
