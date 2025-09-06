"""
House Construction Helper Functions

Contains functions for building realistic houses with architectural details.
"""

from typing import Dict, Any, List
import logging

logger = logging.getLogger("UnrealMCP_Advanced")

# Import safe spawning functions
try:
    from .actor_name_manager import safe_spawn_actor
except ImportError:
    logger.warning("Could not import actor_name_manager, using fallback spawning")
    def safe_spawn_actor(unreal_connection, params, auto_unique_name=True):
        return unreal_connection.send_command("spawn_actor", params)

def build_house(
    unreal_connection,
    width: int,
    depth: int,
    height: int,
    location: List[float],
    name_prefix: str,
    mesh: str,
    house_style: str
) -> Dict[str, Any]:
    """Build a realistic house with architectural details and multiple rooms."""
    try:
        results = []
        wall_thickness = 20.0  # Thinner walls for realism
        floor_thickness = 30.0
        
        # Adjust dimensions based on style
        if house_style == "cottage":
            width = int(width * 0.8)
            depth = int(depth * 0.8)
            height = int(height * 0.9)
        
        # Create foundation as single large block
        foundation_params = {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - floor_thickness/2],
            "scale": [(width + 200)/100.0, (depth + 200)/100.0, floor_thickness/100.0],
            "static_mesh": mesh
        }
        foundation_resp = safe_spawn_actor(unreal_connection, foundation_params)
        if foundation_resp and foundation_resp.get("success"):
            results.append(foundation_resp)
        
        # Create floor as single piece
        floor_params = {
            "name": f"{name_prefix}_Floor",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [width/100.0, depth/100.0, floor_thickness/100.0],
            "static_mesh": mesh
        }
        floor_resp = safe_spawn_actor(unreal_connection, floor_params)
        if floor_resp and floor_resp.get("success"):
            results.append(floor_resp)
        
        base_z = location[2] + floor_thickness
        
        # Build walls
        _build_house_walls(unreal_connection, name_prefix, location, width, depth, height, base_z, wall_thickness, mesh, results)
        
        # Build roof
        _build_house_roof(unreal_connection, name_prefix, location, width, depth, height, base_z, mesh, house_style, results)
        
        # Add style-specific features
        _add_house_features(unreal_connection, name_prefix, location, width, depth, height, base_z, wall_thickness, mesh, house_style, results)
        
        return {
            "success": True,
            "actors": results,
            "house_style": house_style,
            "dimensions": {"width": width, "depth": depth, "height": height},
            "features": _get_house_features(house_style),
            "total_actors": len(results)
        }
        
    except Exception as e:
        logger.error(f"build_house error: {e}")
        return {"success": False, "message": str(e)}

def _build_house_walls(unreal_connection, name_prefix, location, width, depth, height, base_z, wall_thickness, mesh, results):
    """Build the main walls of the house with door and window openings."""
    door_width = 120.0
    door_height = 240.0
    
    # Front wall (with door opening)
    # Front wall - left side of door
    front_left_width = (width/2 - door_width/2)
    front_left_params = {
        "name": f"{name_prefix}_FrontWall_Left",
        "type": "StaticMeshActor",
        "location": [location[0] - width/4 - door_width/4, location[1] - depth/2, base_z + height/2],
        "scale": [front_left_width/100.0, wall_thickness/100.0, height/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, front_left_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Front wall - right side of door
    front_right_params = {
        "name": f"{name_prefix}_FrontWall_Right",
        "type": "StaticMeshActor",
        "location": [location[0] + width/4 + door_width/4, location[1] - depth/2, base_z + height/2],
        "scale": [front_left_width/100.0, wall_thickness/100.0, height/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, front_right_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Front wall - above door
    front_top_params = {
        "name": f"{name_prefix}_FrontWall_Top",
        "type": "StaticMeshActor",
        "location": [location[0], location[1] - depth/2, base_z + door_height + (height - door_height)/2],
        "scale": [door_width/100.0, wall_thickness/100.0, (height - door_height)/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, front_top_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Back wall with window openings
    window_width = 150.0
    window_height = 150.0
    window_y = base_z + height/2
    
    # Back wall - left section
    back_left_params = {
        "name": f"{name_prefix}_BackWall_Left",
        "type": "StaticMeshActor",
        "location": [location[0] - width/3, location[1] + depth/2, base_z + height/2],
        "scale": [width/3/100.0, wall_thickness/100.0, height/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, back_left_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Back wall - center section (with window cutouts)
    back_center_bottom_params = {
        "name": f"{name_prefix}_BackWall_Center_Bottom",
        "type": "StaticMeshActor",
        "location": [location[0], location[1] + depth/2, base_z + (window_y - window_height/2 - base_z)/2],
        "scale": [width/3/100.0, wall_thickness/100.0, (window_y - window_height/2 - base_z)/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, back_center_bottom_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    back_center_top_params = {
        "name": f"{name_prefix}_BackWall_Center_Top",
        "type": "StaticMeshActor",
        "location": [location[0], location[1] + depth/2, window_y + window_height/2 + (base_z + height - window_y - window_height/2)/2],
        "scale": [width/3/100.0, wall_thickness/100.0, (base_z + height - window_y - window_height/2)/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, back_center_top_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Back wall - right section
    back_right_params = {
        "name": f"{name_prefix}_BackWall_Right",
        "type": "StaticMeshActor",
        "location": [location[0] + width/3, location[1] + depth/2, base_z + height/2],
        "scale": [width/3/100.0, wall_thickness/100.0, height/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, back_right_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Left wall
    left_wall_params = {
        "name": f"{name_prefix}_LeftWall",
        "type": "StaticMeshActor",
        "location": [location[0] - width/2, location[1], base_z + height/2],
        "scale": [wall_thickness/100.0, depth/100.0, height/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, left_wall_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Right wall  
    right_wall_params = {
        "name": f"{name_prefix}_RightWall",
        "type": "StaticMeshActor",
        "location": [location[0] + width/2, location[1], base_z + height/2],
        "scale": [wall_thickness/100.0, depth/100.0, height/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, right_wall_params)
    if resp and resp.get("success"):
        results.append(resp)

def _build_house_roof(unreal_connection, name_prefix, location, width, depth, height, base_z, mesh, house_style, results):
    """Build the roof of the house."""
    roof_thickness = 30.0
    roof_overhang = 100.0
    
    # Single flat roof piece covering the entire house
    flat_roof_params = {
        "name": f"{name_prefix}_Roof",
        "type": "StaticMeshActor",
        "location": [
            location[0],
            location[1],
            base_z + height + roof_thickness/2
        ],
        "rotation": [0, 0, 0],  # No rotation - flat roof
        "scale": [(width + roof_overhang*2)/100.0, (depth + roof_overhang*2)/100.0, roof_thickness/100.0],
        "static_mesh": mesh
    }
    resp = safe_spawn_actor(unreal_connection, flat_roof_params)
    if resp and resp.get("success"):
        results.append(resp)
    
    # Add chimney for cottage style
    if house_style == "cottage":
        chimney_params = {
            "name": f"{name_prefix}_Chimney",
            "type": "StaticMeshActor",
            "location": [
                location[0] + width/3,
                location[1] + depth/3,
                base_z + height + roof_thickness + 150  # Position above flat roof
            ],
            "scale": [1.0, 1.0, 2.5],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        }
        resp = safe_spawn_actor(unreal_connection, chimney_params)
        if resp and resp.get("success"):
            results.append(resp)

def _add_house_features(unreal_connection, name_prefix, location, width, depth, height, base_z, wall_thickness, mesh, house_style, results):
    """Add style-specific features to the house."""

    
    # Add details based on style
    if house_style == "modern":
        # Add garage door
        garage_params = {
            "name": f"{name_prefix}_Garage_Door",
            "type": "StaticMeshActor",
            "location": [location[0] - width/3, location[1] - depth/2 + wall_thickness/2, base_z + 150],
            "scale": [2.5, 0.1, 2.5],
            "static_mesh": mesh
        }
        resp = safe_spawn_actor(unreal_connection, garage_params)
        if resp and resp.get("success"):
            results.append(resp)

def _get_house_features(house_style: str) -> List[str]:
    """Get the list of features for a house style."""
    base_features = ["foundation", "floor", "walls", "windows", "door", "flat_roof"]
    
    if house_style == "cottage":
        base_features.append("chimney")
    
    if house_style == "modern":
        base_features.append("garage")
    
    return base_features
