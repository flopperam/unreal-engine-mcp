"""
Advanced building creation helper functions for complex structures.
Includes skyscrapers, office towers, apartment complexes, shopping malls, and parking garages.
"""
from typing import Dict, Any, List
import logging
import sys
import os

sys.path.append(os.path.dirname(os.path.dirname(__file__)))

logger = logging.getLogger(__name__)

# Import safe spawning functions
try:
    from .actor_name_manager import safe_spawn_actor
except ImportError:
    logger.warning("Could not import actor_name_manager, using fallback spawning")
    def safe_spawn_actor(unreal_connection, params, auto_unique_name=True):
        return unreal_connection.send_command("spawn_actor", params)

def _safe_spawn_building_actor(unreal, params):
    """Helper function to safely spawn building actors and track results."""
    resp = safe_spawn_actor(unreal, params, auto_unique_name=True)
    return resp


def _create_skyscraper(height: int, base_width: float, base_depth: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create an impressive skyscraper with multiple sections and details."""
    try:
        import random
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        floor_height = 150.0  # Standard floor height
        
        # Create foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 30],
            "scale": [(base_width + 200)/100.0, (base_depth + 200)/100.0, 0.6],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Create main tower in sections for tapering effect
        sections = min(5, height // 5)
        current_width = base_width
        current_depth = base_depth
        current_height = location[2]
        
        for section in range(sections):
            section_floors = height // sections
            if section == sections - 1:
                section_floors += height % sections  # Add remainder to last section
            
            # Taper the building
            taper_factor = 1 - (section * 0.1)
            current_width = base_width * max(0.6, taper_factor)
            current_depth = base_depth * max(0.6, taper_factor)
            
            # Create main structure for this section
            section_height = section_floors * floor_height
            section_result = _safe_spawn_building_actor(unreal, {
                "name": f"{name_prefix}_Section_{section}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1], current_height + section_height/2],
                "scale": [current_width/100.0, current_depth/100.0, section_height/100.0],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if section_result and section_result.get("success"):
                actors.append(section_result.get("result"))
            
            # Add setback/balcony every few floors
            if section < sections - 1:
                balcony_result = _safe_spawn_building_actor(unreal, {
                    "name": f"{name_prefix}_Balcony_{section}",
                    "type": "StaticMeshActor",
                    "location": [location[0], location[1], current_height + section_height - 25],
                    "scale": [(current_width + 100)/100.0, (current_depth + 100)/100.0, 0.5],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if balcony_result and balcony_result.get("success"):
                    actors.append(balcony_result.get("result"))
            
            current_height += section_height
        
        # Add rooftop features
        # Antenna/spire
        spire_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Spire",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], current_height + 300],
            "scale": [0.2, 0.2, 6.0],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if spire_result and spire_result.get("success"):
            actors.append(spire_result.get("result"))
        
        # Rooftop equipment
        for i in range(3):
            equipment_x = location[0] + random.uniform(-current_width/4, current_width/4)
            equipment_y = location[1] + random.uniform(-current_depth/4, current_depth/4)
            equipment_result = _safe_spawn_building_actor(unreal, {
                "name": f"{name_prefix}_RoofEquipment_{i}",
                "type": "StaticMeshActor",
                "location": [equipment_x, equipment_y, current_height + 50],
                "scale": [1.0, 1.0, 1.0],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if equipment_result and equipment_result.get("success"):
                actors.append(equipment_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_skyscraper error: {e}")
        return {"success": False, "actors": []}


def _create_office_tower(floors: int, width: float, depth: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a modern office tower with glass facade appearance."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        floor_height = 140.0
        
        # Foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 15],
            "scale": [(width + 100)/100.0, (depth + 100)/100.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Lobby (taller first floor)
        lobby_height = floor_height * 1.5
        lobby_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Lobby",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + lobby_height/2],
            "scale": [width/100.0, depth/100.0, lobby_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if lobby_result and lobby_result.get("success"):
            actors.append(lobby_result.get("result"))
        
        # Main tower
        tower_height = (floors - 1) * floor_height
        tower_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Tower",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + lobby_height + tower_height/2],
            "scale": [width/100.0, depth/100.0, tower_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if tower_result and tower_result.get("success"):
            actors.append(tower_result.get("result"))
        
        # Add window bands every few floors for glass facade effect
        for floor in range(2, floors, 3):
            band_height = location[2] + lobby_height + (floor - 1) * floor_height
            band_result = _safe_spawn_building_actor(unreal, {
                "name": f"{name_prefix}_WindowBand_{floor}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1], band_height],
                "scale": [(width + 20)/100.0, (depth + 20)/100.0, 0.2],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if band_result and band_result.get("success"):
                actors.append(band_result.get("result"))
        
        # Rooftop
        rooftop_height = location[2] + lobby_height + tower_height
        rooftop_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Rooftop",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], rooftop_height + 30],
            "scale": [(width - 100)/100.0, (depth - 100)/100.0, 0.6],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if rooftop_result and rooftop_result.get("success"):
            actors.append(rooftop_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_office_tower error: {e}")
        return {"success": False, "actors": []}


def _create_apartment_complex(floors: int, units_per_floor: int, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a multi-unit residential complex with balconies."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        floor_height = 120.0
        width = 200 * units_per_floor // 2
        depth = 800
        
        # Foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 15],
            "scale": [(width + 100)/100.0, (depth + 100)/100.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Main building
        building_height = floors * floor_height
        building_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Building",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + building_height/2],
            "scale": [width/100.0, depth/100.0, building_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if building_result and building_result.get("success"):
            actors.append(building_result.get("result"))
        
        # Add balconies on front and back
        for floor in range(1, floors):
            balcony_height = location[2] + floor * floor_height - 20
            
            # Front balconies
            front_balcony_result = _safe_spawn_building_actor(unreal, {
                "name": f"{name_prefix}_FrontBalcony_{floor}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1] - depth/2 - 50, balcony_height],
                "scale": [width/100.0, 1.0, 0.2],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if front_balcony_result and front_balcony_result.get("success"):
                actors.append(front_balcony_result.get("result"))
            
            # Back balconies
            back_balcony_result = _safe_spawn_building_actor(unreal, {
                "name": f"{name_prefix}_BackBalcony_{floor}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1] + depth/2 + 50, balcony_height],
                "scale": [width/100.0, 1.0, 0.2],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if back_balcony_result and back_balcony_result.get("success"):
                actors.append(back_balcony_result.get("result"))
        
        # Rooftop
        rooftop_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Rooftop",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + building_height + 15],
            "scale": [(width + 50)/100.0, (depth + 50)/100.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if rooftop_result and rooftop_result.get("success"):
            actors.append(rooftop_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_apartment_complex error: {e}")
        return {"success": False, "actors": []}


def _create_shopping_mall(width: float, depth: float, floors: int, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a large shopping mall with entrance canopy."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        floor_height = 200.0  # Tall ceilings for retail
        
        # Foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 20],
            "scale": [(width + 200)/100.0, (depth + 200)/100.0, 0.4],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Main structure
        mall_height = floors * floor_height
        main_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Main",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + mall_height/2],
            "scale": [width/100.0, depth/100.0, mall_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if main_result and main_result.get("success"):
            actors.append(main_result.get("result"))
        
        # Entrance canopy
        canopy_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Canopy",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth/2 - 150, location[2] + floor_height],
            "scale": [width/100.0 * 0.8, 3.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if canopy_result and canopy_result.get("success"):
            actors.append(canopy_result.get("result"))
        
        # Entrance pillars
        for i, x_offset in enumerate([-width/3, 0, width/3]):
            pillar_result = _safe_spawn_building_actor(unreal, {
                "name": f"{name_prefix}_Pillar_{i}",
                "type": "StaticMeshActor",
                "location": [location[0] + x_offset, location[1] - depth/2 - 100, location[2] + floor_height/2],
                "scale": [0.5, 0.5, floor_height/100.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if pillar_result and pillar_result.get("success"):
                actors.append(pillar_result.get("result"))
        
        # Rooftop parking deck indicator
        parking_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_RoofParking",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + mall_height + 15],
            "scale": [width/100.0 * 0.9, depth/100.0 * 0.9, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if parking_result and parking_result.get("success"):
            actors.append(parking_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_shopping_mall error: {e}")
        return {"success": False, "actors": []}


def _create_parking_garage(levels: int, width: float, depth: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a multi-level parking structure."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        level_height = 120.0  # Low ceiling height for parking
        
        # Foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 15],
            "scale": [(width + 50)/100.0, (depth + 50)/100.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Create each level with open sides
        for level in range(levels):
            level_z = location[2] + level * level_height
            
            # Floor slab
            floor_result = _safe_spawn_building_actor(unreal, {
                "name": f"{name_prefix}_Floor_{level}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1], level_z],
                "scale": [width/100.0, depth/100.0, 0.2],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if floor_result and floor_result.get("success"):
                actors.append(floor_result.get("result"))
            
            # Support pillars
            for x in [-width/3, 0, width/3]:
                for y in [-depth/3, 0, depth/3]:
                    pillar_result = _safe_spawn_building_actor(unreal, {
                        "name": f"{name_prefix}_Pillar_{level}_{x}_{y}",
                        "type": "StaticMeshActor",
                        "location": [location[0] + x, location[1] + y, level_z + level_height/2],
                        "scale": [0.4, 0.4, level_height/100.0],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if pillar_result and pillar_result.get("success"):
                        actors.append(pillar_result.get("result"))
            
            # Side barriers (partial walls)
            if level > 0:  # Not on ground level
                for side in ["left", "right", "front", "back"]:
                    if side == "left":
                        barrier_loc = [location[0] - width/2, location[1], level_z + 40]
                        barrier_scale = [0.1, depth/100.0, 0.8]
                    elif side == "right":
                        barrier_loc = [location[0] + width/2, location[1], level_z + 40]
                        barrier_scale = [0.1, depth/100.0, 0.8]
                    elif side == "front":
                        barrier_loc = [location[0], location[1] - depth/2, level_z + 40]
                        barrier_scale = [width/100.0, 0.1, 0.8]
                    else:  # back
                        barrier_loc = [location[0], location[1] + depth/2, level_z + 40]
                        barrier_scale = [width/100.0, 0.1, 0.8]
                    
                    barrier_result = _safe_spawn_building_actor(unreal, {
                        "name": f"{name_prefix}_Barrier_{level}_{side}",
                        "type": "StaticMeshActor",
                        "location": barrier_loc,
                        "scale": barrier_scale,
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if barrier_result and barrier_result.get("success"):
                        actors.append(barrier_result.get("result"))
        
        # Ramp structure (simplified)
        ramp_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Ramp",
            "type": "StaticMeshActor",
            "location": [location[0] + width/2 + 100, location[1], location[2] + (levels * level_height)/2],
            "scale": [1.5, 2.0, levels * level_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if ramp_result and ramp_result.get("success"):
            actors.append(ramp_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_parking_garage error: {e}")
        return {"success": False, "actors": []}


def _create_hotel(floors: int, width: float, depth: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a luxury hotel with distinctive features."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        floor_height = 130.0
        
        # Grand foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 20],
            "scale": [(width + 150)/100.0, (depth + 150)/100.0, 0.4],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Lobby (extra tall)
        lobby_height = floor_height * 2
        lobby_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Lobby",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + lobby_height/2],
            "scale": [width/100.0, depth/100.0, lobby_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if lobby_result and lobby_result.get("success"):
            actors.append(lobby_result.get("result"))
        
        # Main tower
        tower_height = (floors - 2) * floor_height
        tower_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Tower",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + lobby_height + tower_height/2],
            "scale": [width/100.0 * 0.9, depth/100.0 * 0.9, tower_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if tower_result and tower_result.get("success"):
            actors.append(tower_result.get("result"))
        
        # Penthouse (top floor wider)
        penthouse_height = location[2] + lobby_height + tower_height
        penthouse_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Penthouse",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], penthouse_height + floor_height/2],
            "scale": [width/100.0, depth/100.0, floor_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if penthouse_result and penthouse_result.get("success"):
            actors.append(penthouse_result.get("result"))
        
        # Rooftop pool area
        pool_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Pool",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], penthouse_height + floor_height + 20],
            "scale": [width/100.0 * 0.5, depth/100.0 * 0.3, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if pool_result and pool_result.get("success"):
            actors.append(pool_result.get("result"))
        
        # Entrance canopy
        canopy_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Canopy",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth/2 - 100, location[2] + 150],
            "scale": [width/100.0 * 0.6, 2.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if canopy_result and canopy_result.get("success"):
            actors.append(canopy_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_hotel error: {e}")
        return {"success": False, "actors": []}


def _create_restaurant(width: float, depth: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a small restaurant/cafe building."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        height = 150.0
        
        # Foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 10],
            "scale": [(width + 50)/100.0, (depth + 50)/100.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Main building
        main_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Main",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + height/2],
            "scale": [width/100.0, depth/100.0, height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if main_result and main_result.get("success"):
            actors.append(main_result.get("result"))
        
        # Outdoor seating area (patio)
        patio_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Patio",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth/2 - 75, location[2]],
            "scale": [width/100.0, 1.5, 0.1],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if patio_result and patio_result.get("success"):
            actors.append(patio_result.get("result"))
        
        # Awning
        awning_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Awning",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth/2 - 50, location[2] + height - 20],
            "scale": [width/100.0 * 1.2, 1.0, 0.1],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if awning_result and awning_result.get("success"):
            actors.append(awning_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_restaurant error: {e}")
        return {"success": False, "actors": []}


def _create_store(width: float, depth: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a small retail store."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        height = 140.0
        
        # Foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 10],
            "scale": [(width + 30)/100.0, (depth + 30)/100.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Main building
        main_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Main",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + height/2],
            "scale": [width/100.0, depth/100.0, height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if main_result and main_result.get("success"):
            actors.append(main_result.get("result"))
        
        # Storefront sign
        sign_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Sign",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth/2 - 10, location[2] + height + 20],
            "scale": [width/100.0 * 0.8, 0.1, 0.4],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if sign_result and sign_result.get("success"):
            actors.append(sign_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_store error: {e}")
        return {"success": False, "actors": []}


def _create_apartment_building(floors: int, width: float, depth: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a smaller residential apartment building."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        actors = []
        floor_height = 110.0
        
        # Foundation
        foundation_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] - 15],
            "scale": [(width + 50)/100.0, (depth + 50)/100.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if foundation_result and foundation_result.get("success"):
            actors.append(foundation_result.get("result"))
        
        # Main building
        building_height = floors * floor_height
        building_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Building",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + building_height/2],
            "scale": [width/100.0, depth/100.0, building_height/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if building_result and building_result.get("success"):
            actors.append(building_result.get("result"))
        
        # Entry steps
        steps_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Steps",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth/2 - 30, location[2] + 10],
            "scale": [width/100.0 * 0.3, 0.6, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if steps_result and steps_result.get("success"):
            actors.append(steps_result.get("result"))
        
        # Simple roof
        roof_result = _safe_spawn_building_actor(unreal, {
            "name": f"{name_prefix}_Roof",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + building_height + 15],
            "scale": [(width + 20)/100.0, (depth + 20)/100.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if roof_result and roof_result.get("success"):
            actors.append(roof_result.get("result"))
        
        return {"success": True, "actors": actors}
        
    except Exception as e:
        logger.error(f"_create_apartment_building error: {e}")
        return {"success": False, "actors": []}
