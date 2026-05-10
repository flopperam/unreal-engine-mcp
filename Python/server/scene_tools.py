"""Scene database tools for the Unreal MCP server.

These tools call the Rust scene-syncd service over HTTP and are additive
to the existing direct Unreal tools. They do NOT replace them.
"""

import logging
from typing import Any, Dict, List, Optional

from server.core import mcp
from server.scene_client import call_scene_syncd, call_scene_syncd_get
from server.actor_sink import ActorSpec, SceneDbActorSink
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception, sanitize_mcp_id, normalize_scene_id
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


def _scene_syncd_error_response(result: Dict[str, Any], operation: str) -> Dict[str, Any]:
    """Convert a scene-syncd error response to a consistent error format."""
    if result.get("success") is False and result.get("error"):
        err = result["error"]
        msg = err.get("message", str(err)) if isinstance(err, dict) else str(err)
        return make_error_response(f"scene-syncd {operation} failed: {msg}")
    return result


def _scene_syncd_data(result: Dict[str, Any]) -> Dict[str, Any]:
    """Return the scene-syncd data envelope when present."""
    data = result.get("data")
    return data if isinstance(data, dict) else result


def _extract_layout_kind(obj: Dict[str, Any]) -> str:
    visual = obj.get("visual") or {}
    draft = visual.get("draft") if isinstance(visual, dict) else None
    if isinstance(draft, dict) and draft.get("proxy_group"):
        return str(draft["proxy_group"])
    for tag in obj.get("tags") or []:
        if isinstance(tag, str) and tag.startswith("layout_kind:"):
            return tag.split(":", 1)[1]
    return "layout"


def _object_to_draft_instance(obj: Dict[str, Any]) -> Dict[str, Any]:
    transform = obj.get("transform") or {}
    instance = {
        "location": transform.get("location") or {"x": 0.0, "y": 0.0, "z": 0.0},
        "rotation": transform.get("rotation") or {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
        "scale": transform.get("scale") or {"x": 1.0, "y": 1.0, "z": 1.0},
    }
    visual = obj.get("visual") or {}
    draft = visual.get("draft") if isinstance(visual, dict) else None
    if isinstance(draft, dict) and draft.get("color") is not None:
        instance["color"] = draft["color"]
    return instance


def _send_draft_proxy_replace(
    conn: Any,
    proxy_name: str,
    mesh_path: str,
    material_path: Optional[str],
    instances: List[Dict[str, Any]],
    use_dither: bool,
) -> Dict[str, Any]:
    create_params: Dict[str, Any] = {
        "proxy_name": proxy_name,
        "mesh_path": mesh_path,
        "instances": instances,
        "use_dither": use_dither,
    }
    if material_path:
        create_params["material_path"] = material_path

    result = conn.send_command("create_draft_proxy", create_params)
    if result.get("success", False):
        return result

    error = str(result.get("error", ""))
    if "already exists" not in error:
        return result

    update_params: Dict[str, Any] = {
        "proxy_name": proxy_name,
        "instances": instances,
        "use_dither": use_dither,
    }
    if material_path:
        update_params["material_path"] = material_path
    return conn.send_command("update_draft_proxy", update_params)


@mcp.tool()
def scene_create(
    scene_id: str = "main",
    name: Optional[str] = None,
    description: Optional[str] = None,
) -> Dict[str, Any]:
    """Create or update a scene in the scene database."""
    payload = {"scene_id": scene_id}
    if name is not None:
        payload["name"] = name
    if description is not None:
        payload["description"] = description
    return _scene_syncd_error_response(
        call_scene_syncd("/scenes/create", payload), "scene_create"
    )


@mcp.tool()
def scene_upsert_actor(
    scene_id: str = "main",
    mcp_id: str = "",
    desired_name: Optional[str] = None,
    actor_type: str = "StaticMeshActor",
    asset_ref: Optional[Dict[str, Any]] = None,
    transform: Optional[Dict[str, Any]] = None,
    visual: Optional[Dict[str, Any]] = None,
    physics: Optional[Dict[str, Any]] = None,
    tags: Optional[List[str]] = None,
    group_id: Optional[str] = None,
) -> Dict[str, Any]:
    """Write desired actor state to the scene database. Does NOT touch Unreal."""
    try:
        mcp_id = sanitize_mcp_id(mcp_id)
        scene_id = normalize_scene_id(scene_id)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {
        "scene_id": scene_id,
        "mcp_id": mcp_id,
        "actor_type": actor_type,
    }
    if desired_name is not None:
        payload["desired_name"] = desired_name
    if asset_ref is not None:
        payload["asset_ref"] = asset_ref
    if transform is not None:
        payload["transform"] = transform
    if visual is not None:
        payload["visual"] = visual
    if physics is not None:
        payload["physics"] = physics
    if tags is not None:
        payload["tags"] = tags
    if group_id is not None:
        payload["group_id"] = group_id

    return _scene_syncd_error_response(
        call_scene_syncd("/objects/upsert", payload), "scene_upsert_actor"
    )


@mcp.tool()
def scene_upsert_actors(
    scene_id: str = "main",
    group_id: Optional[str] = None,
    objects: Optional[List[Dict[str, Any]]] = None,
) -> Dict[str, Any]:
    """Bulk upsert multiple actors to the scene database. Does NOT touch Unreal."""
    if not objects:
        return make_error_response("objects list must not be empty")

    try:
        scene_id = normalize_scene_id(scene_id)
        for i, obj in enumerate(objects):
            if "mcp_id" in obj:
                obj["mcp_id"] = sanitize_mcp_id(obj["mcp_id"])
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {
        "scene_id": scene_id,
        "objects": objects,
    }
    if group_id is not None:
        payload["group_id"] = group_id

    return _scene_syncd_error_response(
        call_scene_syncd("/objects/bulk-upsert", payload), "scene_upsert_actors"
    )


@mcp.tool()
def scene_delete_actor(
    scene_id: str = "main",
    mcp_id: str = "",
) -> Dict[str, Any]:
    """Tombstone an actor in the scene database. Does NOT delete from Unreal until scene_sync."""
    try:
        validate_string(mcp_id, "mcp_id")
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload = {"scene_id": scene_id, "mcp_id": mcp_id}
    return _scene_syncd_error_response(
        call_scene_syncd("/objects/delete", payload), "scene_delete_actor"
    )


@mcp.tool()
def scene_snapshot_create(
    scene_id: str = "main",
    name: str = "",
    description: Optional[str] = None,
) -> Dict[str, Any]:
    """Snapshot the current desired scene state in the database. Does NOT touch Unreal."""
    try:
        validate_string(scene_id, "scene_id")
        validate_string(name, "name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {"scene_id": scene_id, "name": name}
    if description is not None:
        payload["description"] = description

    return _scene_syncd_error_response(
        call_scene_syncd("/snapshots/create", payload), "scene_snapshot_create"
    )


@mcp.tool()
def scene_snapshot_restore(
    snapshot_id: str = "",
    restore_mode: str = "replace_desired",
) -> Dict[str, Any]:
    """Restore snapshot contents to desired state in the database. Run scene_sync separately."""
    try:
        validate_string(snapshot_id, "snapshot_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload = {"snapshot_id": snapshot_id, "restore_mode": restore_mode}
    return _scene_syncd_error_response(
        call_scene_syncd("/snapshots/restore", payload), "scene_snapshot_restore"
    )


@mcp.tool()
def scene_list_objects(
    scene_id: str = "main",
    include_deleted: bool = False,
) -> Dict[str, Any]:
    """List desired objects in the scene database."""
    payload = {"scene_id": scene_id, "include_deleted": include_deleted}
    return _scene_syncd_error_response(
        call_scene_syncd("/objects/list", payload), "scene_list_objects"
    )



@mcp.tool()
def scene_list_scenes() -> Dict[str, Any]:
    """List all managed scenes in the database."""
    return _scene_syncd_error_response(
        call_scene_syncd("/scenes/list", {}), "scene_list_scenes"
    )


@mcp.tool()
def scene_list_snapshots(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """List all snapshots for a given scene."""
    payload = {"scene_id": scene_id}
    return _scene_syncd_error_response(
        call_scene_syncd("/snapshots/list", payload), "scene_list_snapshots"
    )
@mcp.tool()
def scene_create_wall(
    scene_id: str = "main",
    group_id: str = "wall_001",
    start: Optional[Dict[str, float]] = None,
    length: float = 1000.0,
    height: float = 300.0,
    thickness: float = 50.0,
    segments: int = 10,
    axis: str = "x",
) -> Dict[str, Any]:
    """Write wall segment desired actors to the scene database. Does NOT touch Unreal."""
    if segments < 1:
        return make_error_response("segments must be at least 1")
    if axis not in ("x", "y"):
        return make_error_response("axis must be 'x' or 'y'")

    origin = start or {"x": 0.0, "y": 0.0, "z": 0.0}
    segment_length = length / segments
    sink = SceneDbActorSink(scene_id=scene_id, group_id=group_id)
    for index in range(segments):
        x = float(origin.get("x", 0.0))
        y = float(origin.get("y", 0.0))
        if axis == "x":
            x += index * segment_length
        else:
            y += index * segment_length
        sink.spawn(ActorSpec(
            mcp_id=f"{group_id}_segment_{index:03d}",
            desired_name=f"{group_id}_segment_{index:03d}",
            actor_type="StaticMeshActor",
            asset_ref={"path": "/Engine/BasicShapes/Cube.Cube"},
            transform={
                "location": {"x": x, "y": y, "z": float(origin.get("z", 0.0)) + height / 2.0},
                "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                "scale": {
                    "x": segment_length / 100.0 if axis == "x" else thickness / 100.0,
                    "y": thickness / 100.0 if axis == "x" else segment_length / 100.0,
                    "z": height / 100.0,
                },
            },
            tags=["scene_wall", group_id],
            group_id=group_id,
        ))

    return _scene_syncd_error_response(sink.flush(), "scene_create_wall")


@mcp.tool()
def scene_create_pyramid(
    scene_id: str = "main",
    group_id: str = "pyramid_001",
    base_location: Optional[Dict[str, float]] = None,
    levels: int = 5,
    block_size: float = 100.0,
) -> Dict[str, Any]:
    """Write pyramid block desired actors to the scene database. Does NOT touch Unreal."""
    if levels < 1:
        return make_error_response("levels must be at least 1")
    if block_size <= 0:
        return make_error_response("block_size must be greater than 0")

    origin = base_location or {"x": 0.0, "y": 0.0, "z": 0.0}
    sink = SceneDbActorSink(scene_id=scene_id, group_id=group_id)
    index = 0
    for level in range(levels):
        width = levels - level
        offset = (width - 1) * block_size / 2.0
        for row in range(width):
            for col in range(width):
                sink.spawn(ActorSpec(
                    mcp_id=f"{group_id}_block_{index:03d}",
                    desired_name=f"{group_id}_block_{index:03d}",
                    actor_type="StaticMeshActor",
                    asset_ref={"path": "/Engine/BasicShapes/Cube.Cube"},
                    transform={
                        "location": {
                            "x": float(origin.get("x", 0.0)) + col * block_size - offset,
                            "y": float(origin.get("y", 0.0)) + row * block_size - offset,
                            "z": float(origin.get("z", 0.0)) + level * block_size + block_size / 2.0,
                        },
                        "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                        "scale": {"x": block_size / 100.0, "y": block_size / 100.0, "z": block_size / 100.0},
                    },
                    tags=["scene_pyramid", group_id],
                    group_id=group_id,
                ))
                index += 1

    return _scene_syncd_error_response(sink.flush(), "scene_create_pyramid")


@mcp.tool()
def scene_health() -> Dict[str, Any]:
    """Check the health of the scene-syncd service."""
    return call_scene_syncd_get("/health")


@mcp.tool()
def scene_plan_sync(
    scene_id: str = "main",
    mode: str = "plan_only",
    orphan_policy: Optional[str] = None,
) -> Dict[str, Any]:
    """Compare desired state in the database with actual state in Unreal and return a plan of create/update/delete/noop/conflict operations. Does NOT modify Unreal."""
    try:
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {
        "scene_id": scene_id,
        "mode": mode,
    }
    if orphan_policy is not None:
        payload["orphan_policy"] = orphan_policy

    return _scene_syncd_error_response(
        call_scene_syncd("/sync/plan", payload), "scene_plan_sync"
    )


@mcp.tool()
def scene_sync(
    scene_id: str = "main",
    mode: str = "apply_safe",
    allow_delete: bool = False,
    max_operations: int = 500,
) -> Dict[str, Any]:
    """Apply a sync to create/update/delete actors in Unreal based on desired state in the database. Use scene_plan_sync first to preview changes."""
    try:
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {
        "scene_id": scene_id,
        "mode": mode,
        "allow_delete": allow_delete,
        "max_operations": max_operations,
    }

    return _scene_syncd_error_response(
        call_scene_syncd("/sync/apply", payload), "scene_sync"
    )


@mcp.tool()
def scene_get_instance_sets(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """Get instance set grouping preview for a scene. Returns which objects would be grouped into ISM/HISM instance sets by the density planner, along with their mesh, material, and instance count. Useful for debugging render planning and instance set sync."""
    try:
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    result = call_scene_syncd("/sync/plan", {"scene_id": scene_id, "mode": "plan_only"})
    data = _scene_syncd_data(result)
    if result.get("success") is False:
        return _scene_syncd_error_response(result, "scene_get_instance_sets")

    instance_sets = data.get("instance_sets", [])
    summary = data.get("summary", {})

    return {
        "success": True,
        "scene_id": scene_id,
        "instance_sets": instance_sets,
        "summary": {
            "total_instance_sets": summary.get("instance_sets", 0),
            "instance_set_creates": summary.get("instance_set_creates", 0),
            "individual_creates": summary.get("create", 0),
            "individual_updates": summary.get("update_transform", 0),
            "individual_deletes": summary.get("delete", 0),
        },
    }


@mcp.tool()
def scene_spawn_instance_set(
    set_id: str = "",
    mesh_path: str = "",
    material_path: Optional[str] = None,
    transforms: Optional[List[Dict[str, Any]]] = None,
) -> Dict[str, Any]:
    """Spawn an instance set (HISM/ISM) in Unreal. Creates an actor with a HierarchicalInstancedStaticMeshComponent containing all instances at once. Use for efficiently rendering many identical meshes (crenellations, bricks, tiles)."""
    try:
        validate_string(set_id, "set_id")
        validate_string(mesh_path, "mesh_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    params: Dict[str, Any] = {
        "set_id": set_id,
        "mesh_path": mesh_path,
        "transforms": transforms or [],
    }
    if material_path:
        params["material_path"] = material_path

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        result = conn.send_command("spawn_instance_set", params)
    except Exception as e:
        return make_error_response(f"Failed to spawn instance set in Unreal: {e}")

    if not result.get("success", False):
        return make_error_response(f"Unreal command failed: {result.get('error', 'unknown error')}")

    return {"success": True, "unreal_result": result}


@mcp.tool()
def scene_update_instance_set(
    set_id: str = "",
    transforms: Optional[List[Dict[str, Any]]] = None,
    material_path: Optional[str] = None,
) -> Dict[str, Any]:
    """Update an existing instance set in Unreal. Replaces all instances with the provided transforms. Optionally updates the material."""
    try:
        validate_string(set_id, "set_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    params: Dict[str, Any] = {
        "set_id": set_id,
        "transforms": transforms or [],
    }
    if material_path:
        params["material_path"] = material_path

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        result = conn.send_command("update_instance_set", params)
    except Exception as e:
        return make_error_response(f"Failed to update instance set in Unreal: {e}")

    if not result.get("success", False):
        return make_error_response(f"Unreal command failed: {result.get('error', 'unknown error')}")

    return {"success": True, "unreal_result": result}


@mcp.tool()
def scene_delete_instance_set(
    set_id: str = "",
) -> Dict[str, Any]:
    """Delete an instance set from Unreal by set_id. Removes the actor and its ISM/HISM component."""
    try:
        validate_string(set_id, "set_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        result = conn.send_command("delete_instance_set", {"set_id": set_id})
    except Exception as e:
        return make_error_response(f"Failed to delete instance set in Unreal: {e}")

    if not result.get("success", False):
        return make_error_response(f"Unreal command failed: {result.get('error', 'unknown error')}")

    return {"success": True, "unreal_result": result}


@mcp.tool()
def scene_get_instance_set_state(
    set_id: str = "",
) -> Dict[str, Any]:
    """Query the state of an instance set in Unreal. Returns instance count, mesh path, material path, and whether it uses HISM."""
    try:
        validate_string(set_id, "set_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        result = conn.send_command("get_instance_set_state", {"set_id": set_id})
    except Exception as e:
        return make_error_response(f"Failed to get instance set state from Unreal: {e}")

    return {"success": True, "unreal_result": result}


@mcp.tool()
def scene_list_instance_sets() -> Dict[str, Any]:
    """List all instance sets currently in Unreal. Returns set_id, mesh, material, instance_count, and use_hism for each set."""
    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        result = conn.send_command("list_instance_sets", {})
    except Exception as e:
        return make_error_response(f"Failed to list instance sets from Unreal: {e}")

    return {"success": True, "unreal_result": result}


@mcp.tool()
def scene_create_navmesh_volume(
    scene_id: str = "main",
    volume_name: str = "NavMeshVolume",
    location: Optional[Dict[str, float]] = None,
    extent: Optional[Dict[str, float]] = None,
) -> Dict[str, Any]:
    """Create a NavMeshBoundsVolume in Unreal and register it as a component in the scene database."""
    try:
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    loc = location or {"x": 0.0, "y": 0.0, "z": 0.0}
    ext = extent or {"x": 500.0, "y": 500.0, "z": 500.0}

    # Send command to Unreal
    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        unreal_result = conn.send_command("create_nav_mesh_volume", {
            "volume_name": volume_name,
            "location": [loc["x"], loc["y"], loc["z"]],
            "extent": [ext["x"], ext["y"], ext["z"]],
        })
    except Exception as e:
        return make_error_response(f"Failed to send NavMesh volume command to Unreal: {e}")

    # Check Unreal command result
    if not unreal_result.get("success", False):
        return make_error_response(
            f"Unreal command failed: {unreal_result.get('error', 'unknown error')}"
        )

    # Store as component in scene DB
    component_payload = {
        "scene_id": scene_id,
        "entity_id": volume_name,
        "component_type": "navmesh",
        "name": volume_name,
        "properties": {
            "location": loc,
            "extent": ext,
        },
    }
    db_result = call_scene_syncd("/components/upsert", component_payload)
    db_err = _scene_syncd_error_response(db_result, "scene_create_navmesh_volume/db")
    if not db_err.get("success", True):
        return db_err

    return {
        "success": True,
        "unreal_result": unreal_result,
        "db_result": db_result,
    }


@mcp.tool()
def scene_create_patrol_route(
    scene_id: str = "main",
    route_name: str = "PatrolRoute_001",
    points: Optional[List[Dict[str, float]]] = None,
    closed_loop: bool = False,
) -> Dict[str, Any]:
    """Create a patrol route (spline-based path) in Unreal and register it as an AI component in the scene database."""
    try:
        validate_string(scene_id, "scene_id")
        validate_string(route_name, "route_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    if not points or len(points) < 2:
        return make_error_response("patrol route requires at least 2 points")

    # Send command to Unreal
    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        unreal_result = conn.send_command("create_patrol_route", {
            "patrol_route_name": route_name,
            "points": points,
            "closed_loop": closed_loop,
        })
    except Exception as e:
        return make_error_response(f"Failed to send patrol route command to Unreal: {e}")

    # Check Unreal command result
    if not unreal_result.get("success", False):
        return make_error_response(
            f"Unreal command failed: {unreal_result.get('error', 'unknown error')}"
        )

    # Store as component in scene DB
    component_payload = {
        "scene_id": scene_id,
        "entity_id": route_name,
        "component_type": "ai_patrol",
        "name": route_name,
        "properties": {
            "points": points,
            "closed_loop": closed_loop,
        },
    }
    db_result = call_scene_syncd("/components/upsert", component_payload)
    db_err = _scene_syncd_error_response(db_result, "scene_create_patrol_route/db")
    if not db_err.get("success", True):
        return db_err

    return {
        "success": True,
        "unreal_result": unreal_result,
        "db_result": db_result,
    }


@mcp.tool()
def scene_set_ai_behavior(
    scene_id: str = "main",
    entity_id: str = "",
    actor_name: Optional[str] = None,
    behavior_tree: Optional[str] = None,
    perception_radius: float = 1000.0,
) -> Dict[str, Any]:
    """Configure AI behavior for an actor in Unreal and store it as a component in the scene database."""
    try:
        validate_string(scene_id, "scene_id")
        validate_string(entity_id, "entity_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    target_name = actor_name or entity_id

    # Send command to Unreal
    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        params = {
            "actor_name": target_name,
            "perception_radius": perception_radius,
        }
        if behavior_tree:
            params["behavior_tree_path"] = behavior_tree
        unreal_result = conn.send_command("set_ai_behavior", params)
    except Exception as e:
        return make_error_response(f"Failed to send AI behavior command to Unreal: {e}")

    # Check Unreal command result
    if not unreal_result.get("success", False):
        return make_error_response(
            f"Unreal command failed: {unreal_result.get('error', 'unknown error')}"
        )

    # Store as component in scene DB
    component_payload = {
        "scene_id": scene_id,
        "entity_id": entity_id,
        "component_type": "ai_behavior",
        "name": f"ai_{entity_id}",
        "properties": {
            "behavior_tree": behavior_tree,
            "perception_radius": perception_radius,
        },
    }
    db_result = call_scene_syncd("/components/upsert", component_payload)
    db_err = _scene_syncd_error_response(db_result, "scene_set_ai_behavior/db")
    if not db_err.get("success", True):
        return db_err

    return {
        "success": True,
        "unreal_result": unreal_result,
        "db_result": db_result,
    }


@mcp.tool()
def scene_spawn_blueprint(
    scene_id: str = "main",
    entity_id: str = "",
    blueprint_path: str = "",
    actor_name: Optional[str] = None,
    location: Optional[Dict[str, float]] = None,
    rotation: Optional[Dict[str, float]] = None,
    scale: Optional[Dict[str, float]] = None,
) -> Dict[str, Any]:
    """Spawn an actor from a Blueprint in Unreal and register a realization record in the scene database."""
    try:
        validate_string(scene_id, "scene_id")
        validate_string(entity_id, "entity_id")
        validate_string(blueprint_path, "blueprint_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    name = actor_name or entity_id
    loc = location or {"x": 0.0, "y": 0.0, "z": 0.0}
    rot = rotation or {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}
    scl = scale or {"x": 1.0, "y": 1.0, "z": 1.0}

    # Send command to Unreal
    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        unreal_result = conn.send_command("spawn_blueprint_actor", {
            "blueprint_name": blueprint_path,
            "actor_name": name,
            "location": [loc["x"], loc["y"], loc["z"]],
            "rotation": [rot["pitch"], rot["yaw"], rot["roll"]],
            "scale": [scl["x"], scl["y"], scl["z"]],
        })
    except Exception as e:
        return make_error_response(f"Failed to spawn blueprint actor in Unreal: {e}")

    # Check Unreal command result
    if not unreal_result.get("success", False):
        return make_error_response(
            f"Unreal command failed: {unreal_result.get('error', 'unknown error')}"
        )

    # Store as realization in scene DB
    realization_payload = {
        "scene_id": scene_id,
        "entity_id": entity_id,
        "policy": "blueprint",
        "status": "realized",
        "unreal_actor_name": name,
        "metadata": {
            "blueprint_path": blueprint_path,
            "location": loc,
            "rotation": rot,
            "scale": scl,
        },
    }
    db_result = call_scene_syncd("/realizations/upsert", realization_payload)
    db_err = _scene_syncd_error_response(db_result, "scene_spawn_blueprint/db")
    if not db_err.get("success", True):
        return db_err

    return {
        "success": True,
        "unreal_result": unreal_result,
        "db_result": db_result,
    }


@mcp.tool()
def scene_component_upsert(
    scene_id: str = "main",
    entity_id: str = "",
    component_type: str = "",
    name: str = "",
    properties: Optional[Dict[str, Any]] = None,
    metadata: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Create or update a component (collision, navmesh, AI, etc.) for an entity in the scene database. Does NOT touch Unreal directly."""
    try:
        validate_string(scene_id, "scene_id")
        validate_string(entity_id, "entity_id")
        validate_string(component_type, "component_type")
        validate_string(name, "name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {
        "scene_id": scene_id,
        "entity_id": entity_id,
        "component_type": component_type,
        "name": name,
    }
    if properties is not None:
        payload["properties"] = properties
    if metadata is not None:
        payload["metadata"] = metadata

    return _scene_syncd_error_response(
        call_scene_syncd("/components/upsert", payload), "scene_component_upsert"
    )


@mcp.tool()
def scene_generate_layout_objects(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """Convert Semantic Layout Graph entities into scene_objects and upsert them into the database.

    Reads all scene_entity and scene_relation records for the scene, denormalizes
    them into scene_object actors, and upserts the results. The generated objects
    can then be synced to Unreal with scene_sync.
    """
    try:
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/denormalize", {}), "scene_generate_layout_objects"
    )


@mcp.tool()
def scene_create_layout(
    scene_id: str = "main",
    theme: str = "medieval_european_castle",
    nodes: Optional[List[Dict[str, Any]]] = None,
    edges: Optional[List[Dict[str, Any]]] = None,
    name: Optional[str] = None,
) -> Dict[str, Any]:
    """Create or update a Semantic Layout Graph in the scene database.

    Nodes map to scene_entity records. Edges map to scene_relation records.
    This is the high-level planning entrypoint before preview, approval, and
    realization.
    """
    try:
        scene_id = normalize_scene_id(scene_id)
        validate_string(theme, "theme")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    scene_result = _scene_syncd_error_response(
        call_scene_syncd(
            "/scenes/create",
            {
                "scene_id": scene_id,
                "name": name or scene_id,
                "description": f"Semantic layout graph: {theme}",
            },
        ),
        "scene_create_layout/scene",
    )
    if scene_result.get("success") is False:
        return scene_result

    result: Dict[str, Any] = {
        "success": True,
        "scene": scene_result,
        "entities": None,
        "relations": None,
    }

    if nodes:
        entity_result = _scene_syncd_error_response(
            call_scene_syncd(
                "/entities/bulk-upsert",
                {
                    "scene_id": scene_id,
                    "entities": nodes,
                },
            ),
            "scene_create_layout/entities",
        )
        if entity_result.get("success") is False:
            return entity_result
        result["entities"] = entity_result

    if edges:
        relation_result = _scene_syncd_error_response(
            call_scene_syncd(
                "/relations/bulk-upsert",
                {
                    "scene_id": scene_id,
                    "relations": edges,
                },
            ),
            "scene_create_layout/relations",
        )
        if relation_result.get("success") is False:
            return relation_result
        result["relations"] = relation_result

    return result


@mcp.tool()
def scene_create_draft_proxy(
    proxy_name: str = "draft_layout",
    mesh_path: str = "/Engine/BasicShapes/Cube.Cube",
    material_path: Optional[str] = None,
    instances: Optional[List[Dict[str, Any]]] = None,
    use_dither: bool = False,
) -> Dict[str, Any]:
    """Create a Hierarchical Instanced Static Mesh (HISM) draft proxy in Unreal.

    Lightweight visualization of many instances with a single draw call.
    Instances are translucent cubes by default (no collision, no shadows).
    """
    try:
        validate_string(proxy_name, "proxy_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        params: Dict[str, Any] = {
            "proxy_name": proxy_name,
            "mesh_path": mesh_path,
            "instances": instances or [],
            "use_dither": use_dither,
        }
        if material_path:
            params["material_path"] = material_path
        unreal_result = conn.send_command("create_draft_proxy", params)
    except Exception as e:
        return make_error_response(f"Failed to create draft proxy in Unreal: {e}")

    if not unreal_result.get("success", False):
        return make_error_response(
            f"Unreal command failed: {unreal_result.get('error', 'unknown error')}"
        )

    return {
        "success": True,
        "unreal_result": unreal_result,
    }


@mcp.tool()
def scene_update_draft_proxy(
    proxy_name: str = "draft_layout",
    material_path: Optional[str] = None,
    instances: Optional[List[Dict[str, Any]]] = None,
    use_dither: bool = False,
) -> Dict[str, Any]:
    """Update an existing HISM draft proxy in Unreal (replace all instances)."""
    try:
        validate_string(proxy_name, "proxy_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        params: Dict[str, Any] = {
            "proxy_name": proxy_name,
            "instances": instances or [],
            "use_dither": use_dither,
        }
        if material_path:
            params["material_path"] = material_path
        unreal_result = conn.send_command("update_draft_proxy", params)
    except Exception as e:
        return make_error_response(f"Failed to update draft proxy in Unreal: {e}")

    if not unreal_result.get("success", False):
        return make_error_response(
            f"Unreal command failed: {unreal_result.get('error', 'unknown error')}"
        )

    return {
        "success": True,
        "unreal_result": unreal_result,
    }


@mcp.tool()
def scene_delete_draft_proxy(
    proxy_name: str = "draft_layout",
) -> Dict[str, Any]:
    """Delete a HISM draft proxy from Unreal."""
    try:
        validate_string(proxy_name, "proxy_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        unreal_result = conn.send_command("delete_draft_proxy", {"proxy_name": proxy_name})
    except Exception as e:
        return make_error_response(f"Failed to delete draft proxy in Unreal: {e}")

    if not unreal_result.get("success", False):
        return make_error_response(
            f"Unreal command failed: {unreal_result.get('error', 'unknown error')}"
        )

    return {
        "success": True,
        "unreal_result": unreal_result,
    }


@mcp.tool()
def scene_show_draft_proxy(
    scene_id: str = "main",
    proxy_name: str = "draft_layout",
    mesh_path: str = "/Engine/BasicShapes/Cube.Cube",
    material_path: Optional[str] = None,
    group_by_kind: bool = True,
    use_dither: bool = True,
) -> Dict[str, Any]:
    """Preview a Semantic Layout Graph in Unreal as HISM draft proxies.

    The layout is denormalized in memory, then batched into one HISM proxy per
    semantic kind by default. Splitting proxies by kind keeps large layouts
    cheap while preserving reviewable structure: walls, towers, keeps, bridges,
    and generated detail can be toggled or deleted independently.
    """
    try:
        scene_id = normalize_scene_id(scene_id)
        validate_string(proxy_name, "proxy_name")
        validate_string(mesh_path, "mesh_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    preview_result = _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/preview", {}),
        "scene_show_draft_proxy/preview",
    )
    if preview_result.get("success") is False:
        return preview_result

    preview_data = _scene_syncd_data(preview_result)
    objects = preview_data.get("objects") or []
    if not isinstance(objects, list):
        return make_error_response("scene-syncd preview response did not include an objects list")

    batches: Dict[str, List[Dict[str, Any]]] = {}
    for obj in objects:
        if not isinstance(obj, dict):
            continue
        group = _extract_layout_kind(obj) if group_by_kind else "layout"
        batches.setdefault(group, []).append(_object_to_draft_instance(obj))

    try:
        from server.core import get_unreal_connection
        conn = get_unreal_connection()
        proxy_results = []
        for group, instances in sorted(batches.items()):
            batch_proxy_name = f"{proxy_name}_{group}" if group_by_kind else proxy_name
            unreal_result = _send_draft_proxy_replace(
                conn,
                batch_proxy_name,
                mesh_path,
                material_path,
                instances,
                use_dither,
            )
            if not unreal_result.get("success", False):
                return make_error_response(
                    f"Unreal draft proxy '{batch_proxy_name}' failed: "
                    f"{unreal_result.get('error', 'unknown error')}"
                )
            proxy_results.append(
                {
                    "proxy_name": batch_proxy_name,
                    "group": group,
                    "instance_count": len(instances),
                    "unreal_result": unreal_result,
                }
            )
    except Exception as e:
        return make_error_response(f"Failed to show draft proxy in Unreal: {e}")

    return {
        "success": True,
        "scene_id": scene_id,
        "object_count": len(objects),
        "proxy_count": len(proxy_results),
        "proxies": proxy_results,
    }


@mcp.tool()
def scene_update_layout_node(
    scene_id: str = "main",
    entity_id: str = "",
    transform: Optional[Dict[str, Any]] = None,
    properties: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Update a layout node's transform or properties in the scene database.

    This modifies the scene_entity record and can be followed by
    scene_preview_layout or scene_generate_layout_objects to see the result.
    """
    try:
        validate_string(scene_id, "scene_id")
        validate_string(entity_id, "entity_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {}
    if transform is not None:
        payload["location"] = transform.get("location")
        payload["rotation"] = transform.get("rotation")
        payload["scale"] = transform.get("scale")
    if properties is not None:
        payload["properties"] = properties

    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/nodes/{entity_id}/transform", payload),
        "scene_update_layout_node",
    )


@mcp.tool()
def scene_preview_layout(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """Preview the Semantic Layout Graph as scene_objects without persisting them.

    Returns the denormalized objects that would be created by
    scene_generate_layout_objects, useful for reviewing before approval.
    """
    try:
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/preview", {}), "scene_preview_layout"
    )


@mcp.tool()
def scene_approve_layout(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """Approve the Semantic Layout Graph and prepare it for realization.

    Changes the scene status to 'approved_layout' and creates an auto-snapshot
    for rollback. After approval, use scene_generate_layout_objects followed
    by scene_sync to materialize the layout in Unreal.
    """
    try:
        validate_string(scene_id, "scene_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/approve", {}), "scene_approve_layout"
    )


@mcp.tool()
def scene_realize_layout(
    scene_id: str = "main",
    stage: str = "blockout",
    persist: bool = True,
) -> Dict[str, Any]:
    """Realize an approved layout at a given stage.

    Stages:
        blockout  - Use default Cube/Plane placeholders.
        assets    - Bind real assets from the scene_asset library.
        detail    - Apply components and decorative detail (placeholder).
        finalize  - Final production-ready objects.

    When persist=True, the resulting scene_objects are upserted into the DB
    and can then be synced to Unreal with scene_sync.
    """
    try:
        validate_string(scene_id, "scene_id")
        validate_string(stage, "stage")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    return _scene_syncd_error_response(
        call_scene_syncd(f"/realizations/{scene_id}/realize", {
            "stage": stage,
            "persist": persist,
        }),
        "scene_realize_layout",
    )


@mcp.tool()
def scene_compile_preview(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """Generate a preview compilation for a scene.

    Returns a lightweight compilation result suitable for preview
    without persisting changes. Faster than full compile_apply.
    """
    try:
        scene_id = normalize_scene_id(scene_id)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/compile/preview", {}), "scene_compile_preview"
    )


@mcp.tool()
def scene_upsert_procedural_mesh(
    mcp_id: str,
    actor_name: Optional[str] = None,
    vertex_count: int = 3,
    index_count: int = 3,
    positions: List[List[float]] = None,
    normals: List[List[float]] = None,
    indices: List[int] = None,
    uvs: List[List[float]] = None,
    material_path: str = "",
    location: Optional[Dict[str, float]] = None,
    rotation: Optional[Dict[str, float]] = None,
    scale: Optional[Dict[str, float]] = None,
    focus_viewport: bool = True,
) -> Dict[str, Any]:
    """Upsert a procedural mesh in Unreal Engine by sending vertex data through the Rust scene-syncd service.

    This tool bypasses the JSON-only UnrealConnection and uses the Rust scene-syncd
    TCP binary protocol for efficient large mesh transfer. mcp_id is required.
    """
    positions = positions or []
    normals = normals or []
    indices = indices or []
    uvs = uvs or []
    
    try:
        validate_string(mcp_id, "mcp_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    if len(positions) != vertex_count:
        return make_error_response(f"positions length ({len(positions)}) does not match vertex_count ({vertex_count})")
    if len(normals) != vertex_count:
        return make_error_response(f"normals length ({len(normals)}) does not match vertex_count ({vertex_count})")
    if len(indices) != index_count:
        return make_error_response(f"indices length ({len(indices)}) does not match index_count ({index_count})")

    payload = {
        "mcp_id": mcp_id,
        "actor_name": actor_name or mcp_id,
        "vertex_count": vertex_count,
        "index_count": index_count,
        "positions": positions,
        "normals": normals,
        "indices": indices,
        "material_path": material_path,
        "focus_viewport": focus_viewport,
    }
    
    if location:
        payload["location"] = [location.get("x", 0.0), location.get("y", 0.0), location.get("z", 0.0)]
    if rotation:
        payload["rotation"] = [rotation.get("pitch", 0.0), rotation.get("yaw", 0.0), rotation.get("roll", 0.0)]
    if scale:
        payload["scale"] = [scale.get("x", 1.0), scale.get("y", 1.0), scale.get("z", 1.0)]

    if uvs:
        payload["uvs"] = uvs
        payload["flags"] = 0x01

    result = call_scene_syncd("/procedural/create-mesh", payload)
    return _scene_syncd_error_response(result, "scene_upsert_procedural_mesh")


@mcp.tool()
def scene_create_sdf_mesh(
    mcp_id: str,
    sdf_tree: Optional[Dict[str, Any]] = None,
    sdf_type: str = "sphere",
    center: Optional[Dict[str, float]] = None,
    radius: float = 100.0,
    box_min: Optional[Dict[str, float]] = None,
    box_max: Optional[Dict[str, float]] = None,
    major_radius: float = 100.0,
    minor_radius: float = 30.0,
    frequency: float = 1.0,
    thickness: float = 10.0,
    resolution: int = 32,
    bounds: Optional[Dict[str, Dict[str, float]]] = None,
    bounds_padding: float = 10.0,
    actor_name: Optional[str] = None,
    material_path: str = "",
    location: Optional[Dict[str, float]] = None,
    rotation: Optional[Dict[str, float]] = None,
    scale: Optional[Dict[str, float]] = None,
    focus_viewport: bool = True,
) -> Dict[str, Any]:
    """Generate a procedural mesh from a Signed Distance Function (SDF) using Marching Cubes.

    Supports: sphere, box, torus, gyroid, scherk. SDF meshes do not have UVs —
    use World-Aligned materials on the Unreal side for texturing.
    """
    try:
        validate_string(mcp_id, "mcp_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    sdf_payload = sdf_tree or {
        "type": sdf_type,
        "center": [center.get("x", 0.0), center.get("y", 0.0), center.get("z", 0.0)] if center else [0.0, 0.0, 0.0],
        "radius": radius,
        "min": [box_min.get("x", -100.0), box_min.get("y", -100.0), box_min.get("z", -100.0)] if box_min else [-100.0, -100.0, -100.0],
        "max": [box_max.get("x", 100.0), box_max.get("y", 100.0), box_max.get("z", 100.0)] if box_max else [100.0, 100.0, 100.0],
        "major_radius": major_radius,
        "minor_radius": minor_radius,
        "frequency": frequency,
        "thickness": thickness,
    }

    payload = {
        "mcp_id": mcp_id,
        "sdf": sdf_payload,
        "resolution": resolution,
        "bounds_padding": bounds_padding,
        "actor_name": actor_name or mcp_id,
        "material_path": material_path,
        "focus_viewport": focus_viewport,
    }

    if bounds:
        payload["bounds"] = {
            "min": [bounds["min"].get("x", 0.0), bounds["min"].get("y", 0.0), bounds["min"].get("z", 0.0)],
            "max": [bounds["max"].get("x", 0.0), bounds["max"].get("y", 0.0), bounds["max"].get("z", 0.0)],
        }

    if location:
        payload["location"] = [location.get("x", 0.0), location.get("y", 0.0), location.get("z", 0.0)]
    if rotation:
        payload["rotation"] = [rotation.get("pitch", 0.0), rotation.get("yaw", 0.0), rotation.get("roll", 0.0)]
    if scale:
        payload["scale"] = [scale.get("x", 1.0), scale.get("y", 1.0), scale.get("z", 1.0)]

    result = call_scene_syncd("/procedural/sdf-mesh", payload)
    return _scene_syncd_error_response(result, "scene_create_sdf_mesh")


@mcp.tool()
def scene_create_superformula_mesh(
    mcp_id: str,
    m1: float = 6.0,
    n1_1: float = 1.0,
    n2_1: float = 1.0,
    n3_1: float = 1.0,
    a1: float = 1.0,
    b1: float = 1.0,
    m2: float = 6.0,
    n1_2: float = 1.0,
    n2_2: float = 1.0,
    n3_2: float = 1.0,
    a2: float = 1.0,
    b2: float = 1.0,
    resolution: int = 32,
    scale: float = 100.0,
    actor_name: Optional[str] = None,
    material_path: str = "",
    location: Optional[Dict[str, float]] = None,
    rotation: Optional[Dict[str, float]] = None,
    scale_override: Optional[Dict[str, float]] = None,
    focus_viewport: bool = True,
) -> Dict[str, Any]:
    """Generate a procedural mesh from the 3D Superformula (Gielis).

    The Superformula creates parametric shapes from two 2D superformulas applied
    in spherical coordinates. UV mapping is derived from (theta, phi).
    Try m=8, n1=0.5 for star-like shapes, or m=4, n1=10 for rounded squares.
    """
    try:
        validate_string(mcp_id, "mcp_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload = {
        "mcp_id": mcp_id,
        "m1": m1, "n1_1": n1_1, "n2_1": n2_1, "n3_1": n3_1, "a1": a1, "b1": b1,
        "m2": m2, "n1_2": n1_2, "n2_2": n2_2, "n3_2": n3_2, "a2": a2, "b2": b2,
        "resolution": resolution,
        "scale": scale,
        "actor_name": actor_name or mcp_id,
        "material_path": material_path,
        "focus_viewport": focus_viewport,
    }

    if location:
        payload["location"] = [location.get("x", 0.0), location.get("y", 0.0), location.get("z", 0.0)]
    if rotation:
        payload["rotation"] = [rotation.get("pitch", 0.0), rotation.get("yaw", 0.0), rotation.get("roll", 0.0)]
    if scale_override:
        payload["scale_override"] = [scale_override.get("x", 1.0), scale_override.get("y", 1.0), scale_override.get("z", 1.0)]

    result = call_scene_syncd("/procedural/superformula-mesh", payload)
    return _scene_syncd_error_response(result, "scene_create_superformula_mesh")


@mcp.tool()
def scene_create_lsystem_spline(
    mcp_id: str,
    axiom: str = "F",
    rules: Optional[List[List[str]]] = None,
    iterations: int = 3,
    step_length: float = 50.0,
    angle_degrees: float = 90.0,
    origin: Optional[Dict[str, float]] = None,
    heading: Optional[Dict[str, float]] = None,
    up: Optional[Dict[str, float]] = None,
    closed_loop: bool = False,
    tangent_mode: str = "curve",
    spline_name: Optional[str] = None,
    focus_viewport: bool = True,
) -> Dict[str, Any]:
    """Generate a spline from an L-System grammar and create it in Unreal.

    The L-System turtle produces segments that are sent to Unreal as a
    create_spline_from_points command. Supports 3D operations: +/- for yaw,
    &/^ for pitch, \\\\/ for roll, [/] for push/pop branching.
    Common grammars: Koch curve (F→F+F-F-F+F, angle=90), tree (F→F[+F]F[-F]F).
    """
    try:
        validate_string(mcp_id, "mcp_id")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    rules = rules or [["F", "F+F-F-F+F"]]

    # Step 1: Evaluate L-System via scene-syncd to get segments
    lsystem_payload = {
        "mcp_id": mcp_id,
        "axiom": axiom,
        "rules": rules,
        "iterations": iterations,
        "step_length": step_length,
        "angle_degrees": angle_degrees,
        "closed_loop": closed_loop,
        "tangent_mode": tangent_mode,
        "spline_name": spline_name or mcp_id,
        "create_in_unreal": True,
        "focus_viewport": focus_viewport,
    }

    if origin:
        lsystem_payload["origin"] = [origin.get("x", 0.0), origin.get("y", 0.0), origin.get("z", 0.0)]
    if heading:
        lsystem_payload["heading"] = [heading.get("x", 1.0), heading.get("y", 0.0), heading.get("z", 0.0)]
    if up:
        lsystem_payload["up"] = [up.get("x", 0.0), up.get("y", 0.0), up.get("z", 1.0)]

    result = call_scene_syncd("/procedural/lsystem-spline", lsystem_payload)
    return _scene_syncd_error_response(result, "scene_create_lsystem_spline")


@mcp.tool()
def scene_validate(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """Validate a scene for errors, warnings, and optimization opportunities.

    Runs the Rust compiler pipeline in validate-only mode against the scene.
    Returns diagnostics (errors, warnings, infos) and a summary.
    """
    try:
        scene_id = normalize_scene_id(scene_id)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/validate", {}), "scene_validate"
    )


@mcp.tool()
def scene_compile_plan(
    scene_id: str = "main",
) -> Dict[str, Any]:
    """Generate a compilation plan showing what changes would be applied to Unreal.

    Fetches the actual state from Unreal and diffs it against the desired scene state.
    Returns create/update/delete/noop counts and operation details without modifying Unreal.
    """
    try:
        scene_id = normalize_scene_id(scene_id)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/compile/plan", {}), "scene_compile_plan"
    )


@mcp.tool()
def scene_compile_apply(
    scene_id: str = "main",
    allow_delete: bool = False,
) -> Dict[str, Any]:
    """Apply the compilation plan to Unreal.

    Compiles the scene and pushes changes into Unreal. By default deletes are
    disabled for safety. Set allow_delete=True to permit actor removal.
    """
    try:
        scene_id = normalize_scene_id(scene_id)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {"scene_id": scene_id, "allow_delete": allow_delete}
    return _scene_syncd_error_response(
        call_scene_syncd(f"/layouts/{scene_id}/compile/apply", payload), "scene_compile_apply"
    )


@mcp.tool()
def scene_run_pie_test(
    scene_id: str = "main",
    mode: str = "smoke",
    timeout_secs: int = 60,
) -> Dict[str, Any]:
    """Run a Play-In-Editor (PIE) test on the scene.

    mode: "smoke" | "full" | "performance"
    timeout_secs: max seconds to wait before force-stopping PIE (capped at 120)
    """
    try:
        scene_id = normalize_scene_id(scene_id)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {
        "scene_id": scene_id,
        "mode": mode,
        "timeout_secs": min(max(timeout_secs, 1), 120),
    }
    return _scene_syncd_error_response(
        call_scene_syncd("/unreal/pie/run", payload), "scene_run_pie_test"
    )


@mcp.tool()
def scene_generate_fix_plan(
    scene_id: str = "main",
    diagnostics: Optional[List[Dict[str, Any]]] = None,
) -> Dict[str, Any]:
    """Generate an automated fix plan from scene diagnostics.

    Accepts a list of diagnostic objects (e.g. from scene_validate or PIE logs)
    and returns a confidence-scored plan of operations to resolve them.
    """
    try:
        scene_id = normalize_scene_id(scene_id)
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    payload: Dict[str, Any] = {
        "scene_id": scene_id,
        "diagnostics": diagnostics or [],
    }
    return _scene_syncd_error_response(
        call_scene_syncd("/unreal/fix-plan", payload), "scene_generate_fix_plan"
    )
