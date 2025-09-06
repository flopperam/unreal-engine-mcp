"""
Advanced Tower Creation Helpers

This module provides helper functions for creating massive, cool-shaped, and colored towers
with various architectural styles and color schemes.
"""

import math
import random
from typing import List, Dict, Any, Optional, Tuple
import logging

logger = logging.getLogger("TowerCreation")

def get_tower_color_palette(palette_name: str = "rainbow") -> List[List[float]]:
    """
    Get a predefined color palette for tower creation.
    
    Args:
        palette_name: Name of the color palette to use
        
    Returns:
        List of RGBA color values [R, G, B, A] where values are 0.0-1.0
    """
    palettes = {
        "rainbow": [
            [1.0, 0.0, 0.0, 1.0],  # Red
            [1.0, 0.5, 0.0, 1.0],  # Orange
            [1.0, 1.0, 0.0, 1.0],  # Yellow
            [0.0, 1.0, 0.0, 1.0],  # Green
            [0.0, 0.0, 1.0, 1.0],  # Blue
            [0.5, 0.0, 1.0, 1.0],  # Purple
        ],
        "fire": [
            [1.0, 0.0, 0.0, 1.0],  # Red
            [1.0, 0.3, 0.0, 1.0],  # Red-Orange
            [1.0, 0.6, 0.0, 1.0],  # Orange
            [1.0, 0.8, 0.0, 1.0],  # Yellow-Orange
            [1.0, 1.0, 0.2, 1.0],  # Yellow
        ],
        "ocean": [
            [0.0, 0.2, 0.4, 1.0],  # Dark Blue
            [0.0, 0.4, 0.6, 1.0],  # Blue
            [0.0, 0.6, 0.8, 1.0],  # Light Blue
            [0.0, 0.8, 1.0, 1.0],  # Cyan
            [0.2, 1.0, 1.0, 1.0],  # Light Cyan
        ],
        "sunset": [
            [1.0, 0.4, 0.6, 1.0],  # Pink
            [1.0, 0.6, 0.4, 1.0],  # Coral
            [1.0, 0.8, 0.2, 1.0],  # Gold
            [1.0, 0.9, 0.6, 1.0],  # Light Gold
            [0.9, 0.7, 0.9, 1.0],  # Lavender
        ],
        "forest": [
            [0.2, 0.5, 0.2, 1.0],  # Dark Green
            [0.3, 0.6, 0.3, 1.0],  # Green
            [0.4, 0.7, 0.2, 1.0],  # Light Green
            [0.5, 0.4, 0.1, 1.0],  # Brown
            [0.6, 0.3, 0.1, 1.0],  # Dark Brown
        ],
        "cosmic": [
            [0.2, 0.0, 0.4, 1.0],  # Deep Purple
            [0.4, 0.0, 0.6, 1.0],  # Purple
            [0.6, 0.2, 0.8, 1.0],  # Magenta
            [0.8, 0.4, 1.0, 1.0],  # Light Purple
            [1.0, 0.6, 1.0, 1.0],  # Pink
        ],
        "metallic": [
            [0.7, 0.7, 0.7, 1.0],  # Silver
            [0.9, 0.8, 0.6, 1.0],  # Gold
            [0.5, 0.3, 0.1, 1.0],  # Bronze
            [0.4, 0.4, 0.5, 1.0],  # Steel
            [0.6, 0.4, 0.2, 1.0],  # Copper
        ],
    }
    
    return palettes.get(palette_name, palettes["rainbow"])

def assign_tower_piece_color(level: int, piece_index: int, total_levels: int, color_palette: List[List[float]], color_pattern: str = "gradient") -> List[float]:
    """
    Assign a color to a tower piece based on level, position, and pattern.
    
    Args:
        level: Current level of the tower (0 = bottom)
        piece_index: Index of the piece within the level
        total_levels: Total number of levels in the tower
        color_palette: List of available colors
        color_pattern: Pattern for color assignment ("gradient", "random", "alternating", "spiral")
        
    Returns:
        RGBA color values [R, G, B, A]
    """
    if not color_palette:
        return [0.7, 0.7, 0.7, 1.0]  # Default gray
    
    if color_pattern == "gradient":
        # Gradient from bottom to top
        ratio = level / max(1, total_levels - 1)
        color_index = int(ratio * (len(color_palette) - 1))
        return color_palette[color_index]
        
    elif color_pattern == "alternating":
        # Alternate colors by level
        color_index = level % len(color_palette)
        return color_palette[color_index]
        
    elif color_pattern == "spiral":
        # Spiral pattern based on piece index
        color_index = (level + piece_index) % len(color_palette)
        return color_palette[color_index]
        
    elif color_pattern == "random":
        # Random color from palette (consistent per level+piece)
        random.seed(level * 1000 + piece_index)
        return random.choice(color_palette)
        
    else:  # Default to gradient
        ratio = level / max(1, total_levels - 1)
        color_index = int(ratio * (len(color_palette) - 1))
        return color_palette[color_index]

def create_spiral_tower_pieces(level: int, height: int, base_size: int, block_size: float, 
                             location: List[float], name_prefix: str, 
                             color_palette: List[List[float]], color_pattern: str) -> List[Dict[str, Any]]:
    """
    Generate piece data for a single level of a spiral tower.
    Returns data structure instead of spawning directly for batch processing.
    """
    pieces = []
    
    # Calculate spiral parameters
    level_height = location[2] + level * block_size
    twist_angle = (level / height) * math.pi * 4  # 4 full rotations over tower height
    radius_reduction = level * 0.05  # Gradually reduce radius
    current_radius = (base_size / 2) * block_size - radius_reduction * block_size
    
    if current_radius <= 0:
        return pieces
        
    # Number of blocks per level decreases with height
    num_blocks = max(6, int((base_size * 8) * (1 - level / height * 0.5)))
    
    for i in range(num_blocks):
        angle = (2 * math.pi * i) / num_blocks + twist_angle
        x = location[0] + current_radius * math.cos(angle)
        y = location[1] + current_radius * math.sin(angle)
        
        # Add some randomness to create organic look
        x += random.uniform(-block_size * 0.1, block_size * 0.1)
        y += random.uniform(-block_size * 0.1, block_size * 0.1)
        
        # Get color for this piece
        color = assign_tower_piece_color(level, i, height, color_palette, color_pattern)
        
        pieces.append({
            "name": f"{name_prefix}_spiral_{level}_{i}",
            "location": [x, y, level_height],
            "color": color,
            "scale": [block_size / 100.0, block_size / 100.0, block_size / 100.0]
        })
    
    return pieces

def create_spiral_tower_level(unreal, level: int, height: int, base_size: int, block_size: float, 
                            location: List[float], name_prefix: str, mesh: str, 
                            color_palette: List[List[float]], color_pattern: str) -> List[Dict[str, Any]]:
    """
    DEPRECATED: Use create_spiral_tower_pieces for batch processing.
    Create a single level of a spiral tower with twisted geometry.
    """
    logger.warning("create_spiral_tower_level is deprecated. Use batch processing instead.")
    pieces = create_spiral_tower_pieces(level, height, base_size, block_size, location, name_prefix, color_palette, color_pattern)
    
    if not pieces:
        return []
        
    result = create_tower_blueprints_and_batch_spawn(unreal, pieces, mesh, name_prefix)
    return result.get("actors", [])

def create_twisted_tower_level(unreal, level: int, height: int, base_size: int, block_size: float,
                             location: List[float], name_prefix: str, mesh: str,
                             color_palette: List[List[float]], color_pattern: str) -> List[Dict[str, Any]]:
    """
    Create a single level of a twisted square tower.
    """
    spawned = []
    
    level_height = location[2] + level * block_size
    twist_angle = (level / height) * math.pi * 2  # 2 full rotations
    size_reduction = level * 0.02  # Gradual tapering
    current_size = max(2, base_size - size_reduction)
    
    # Create twisted square perimeter
    half_size = current_size / 2
    perimeter_blocks = int(current_size * 4)
    
    for i in range(perimeter_blocks):
        # Calculate position on square perimeter
        side = i // int(current_size)
        pos_on_side = i % int(current_size)
        
        if side == 0:  # Bottom edge
            local_x = pos_on_side - half_size + 0.5
            local_y = -half_size
        elif side == 1:  # Right edge
            local_x = half_size
            local_y = pos_on_side - half_size + 0.5
        elif side == 2:  # Top edge
            local_x = half_size - pos_on_side - 0.5
            local_y = half_size
        else:  # Left edge
            local_x = -half_size
            local_y = half_size - pos_on_side - 0.5
        
        # Apply twist transformation
        cos_twist = math.cos(twist_angle)
        sin_twist = math.sin(twist_angle)
        twisted_x = local_x * cos_twist - local_y * sin_twist
        twisted_y = local_x * sin_twist + local_y * cos_twist
        
        # Convert to world coordinates
        x = location[0] + twisted_x * block_size
        y = location[1] + twisted_y * block_size
        
        # Get color for this piece
        color = assign_tower_piece_color(level, i, height, color_palette, color_pattern)
        
        # Create colored physics block
        actor_name = f"{name_prefix}_twisted_{level}_{i}"
        result = spawn_colored_tower_piece(unreal, actor_name, mesh, [x, y, level_height], color, block_size)
        
        if result.get("success"):
            spawned.append(result)
    
    return spawned

def create_multi_tiered_level(unreal, level: int, height: int, base_size: int, block_size: float,
                            location: List[float], name_prefix: str, mesh: str,
                            color_palette: List[List[float]], color_pattern: str) -> List[Dict[str, Any]]:
    """
    Create a multi-tiered level with multiple concentric rings.
    """
    spawned = []
    
    level_height = location[2] + level * block_size
    
    # Create multiple tiers per level
    tier_count = 1 + (level % 3)  # 1-3 tiers per level
    
    for tier in range(tier_count):
        tier_radius = (base_size / 2 - tier * 1.5) * block_size
        if tier_radius <= 0:
            continue
            
        circumference = 2 * math.pi * tier_radius
        num_blocks = max(4, int(circumference / block_size))
        
        for i in range(num_blocks):
            angle = (2 * math.pi * i) / num_blocks
            x = location[0] + tier_radius * math.cos(angle)
            y = location[1] + tier_radius * math.sin(angle)
            z = level_height + tier * block_size * 0.3  # Slight height offset per tier
            
            # Get color for this piece (different for each tier)
            color = assign_tower_piece_color(level * 10 + tier, i, height * 10, color_palette, color_pattern)
            
            # Create colored physics block
            actor_name = f"{name_prefix}_tiered_{level}_{tier}_{i}"
            result = spawn_colored_tower_piece(unreal, actor_name, mesh, [x, y, z], color, block_size)
            
            if result.get("success"):
                spawned.append(result)
    
    return spawned

# Global cache for reusable colored blueprints
_tower_blueprint_cache = {}

def clear_tower_blueprint_cache():
    """Clear the blueprint cache. Useful when starting fresh or after errors."""
    global _tower_blueprint_cache
    _tower_blueprint_cache.clear()
    logger.info("Cleared tower blueprint cache")

def get_or_create_colored_blueprint(unreal, mesh: str, color: List[float], base_name: str = "TowerPiece") -> str:
    """
    Get or create a reusable colored blueprint for tower pieces.
    Uses a global cache to avoid creating duplicate blueprints for the same color.
    
    Args:
        unreal: Unreal connection object
        mesh: Mesh path to use
        color: RGBA color [r, g, b, a] 
        base_name: Base name for the blueprint
        
    Returns:
        Blueprint name that can be reused for spawning
    """
    # Create a cache key based on mesh and color (rounded to avoid float precision issues)
    color_key = tuple(round(c, 2) for c in color)  # Round to 2 decimals for better matching
    cache_key = (mesh, color_key)
    
    # Return cached blueprint if it exists
    if cache_key in _tower_blueprint_cache:
        bp_name = _tower_blueprint_cache[cache_key]
        logger.info(f"CACHE HIT: Using cached blueprint {bp_name} for color {color} -> {color_key}")
        return bp_name
    
    logger.info(f"CACHE MISS: Creating new blueprint for color {color} -> {color_key}. Cache has {len(_tower_blueprint_cache)} entries.")
    
    # Generate a stable, unique blueprint name for this color/mesh combination
    color_hash = abs(hash(cache_key)) % 10000
    bp_name = f"{base_name}_Color_{color_hash}_BP"
    
    # Check if this blueprint already exists from a previous session
    # If so, just use it and cache it - no need to create a new one
    create_result = unreal.send_command("create_blueprint", {"name": bp_name, "parent_class": "Actor"})
    
    blueprint_exists = False
    if create_result and create_result.get("success"):
        # Blueprint was created successfully
        logger.info(f"Created new blueprint {bp_name}")
    elif create_result and "already exists" in create_result.get("error", "").lower():
        # Blueprint already exists from previous session - we can reuse it!
        logger.info(f"Blueprint {bp_name} already exists, reusing it")
        blueprint_exists = True
    else:
        # Some other error occurred
        logger.error(f"Failed to create blueprint {bp_name}: {create_result.get('error', 'Unknown error')}")
        return None
    
    # If blueprint already existed, just cache and return it
    if blueprint_exists:
        _tower_blueprint_cache[cache_key] = bp_name
        logger.info(f"CACHED EXISTING: Reusing existing blueprint {bp_name} for color {color} -> {color_key}")
        return bp_name
    
    # Set up the newly created blueprint
    try:
        # Add mesh component
        add_result = unreal.send_command("add_component_to_blueprint", {
            "blueprint_name": bp_name,
            "component_type": "StaticMeshComponent", 
            "component_name": "Mesh"
        })
        if not add_result or not add_result.get("success"):
            logger.warning(f"Failed to add component to {bp_name}")
            return None
        
        # Set mesh properties
        mesh_result = unreal.send_command("set_static_mesh_properties", {
            "blueprint_name": bp_name,
            "component_name": "Mesh",
            "static_mesh": mesh
        })
        if not mesh_result or not mesh_result.get("success"):
            logger.warning(f"Failed to set mesh properties for {bp_name}")
        
        # Set physics properties
        physics_result = unreal.send_command("set_physics_properties", {
            "blueprint_name": bp_name,
            "component_name": "Mesh",
            "simulate_physics": True,
            "gravity_enabled": True,
            "mass": 1.0
        })
        if not physics_result or not physics_result.get("success"):
            logger.warning(f"Failed to set physics properties for {bp_name}")
        
        # Apply color
        color_result = unreal.send_command("set_mesh_material_color", {
            "blueprint_name": bp_name,
            "component_name": "Mesh",
            "color": color,
            "material_slot": 0
        })
        if not color_result or not color_result.get("success"):
            logger.warning(f"Failed to set color for {bp_name}")
        
        # Compile blueprint
        compile_result = unreal.send_command("compile_blueprint", {"blueprint_name": bp_name})
        if not compile_result or not compile_result.get("success"):
            logger.warning(f"Failed to compile blueprint {bp_name}")
        
        # Cache the blueprint for reuse
        _tower_blueprint_cache[cache_key] = bp_name
        logger.info(f"CACHED NEW: Created and cached new blueprint {bp_name} for color {color} -> {color_key}. Cache now has {len(_tower_blueprint_cache)} entries.")
        
        return bp_name
        
    except Exception as e:
        logger.error(f"Error setting up colored blueprint: {e}")
        return None

def create_tower_blueprints_and_batch_spawn(
    unreal, 
    tower_pieces: List[Dict[str, Any]], 
    mesh: str = "/Engine/BasicShapes/Cube.Cube",
    name_prefix: str = "Tower"
) -> Dict[str, Any]:
    """
    Efficiently create towers by:
    1. Creating one blueprint per unique color
    2. Batch spawning all pieces of each color using the same blueprint
    
    Args:
        unreal: Unreal connection object
        tower_pieces: List of pieces with format:
            [{"location": [x,y,z], "color": [r,g,b,a], "scale": [x,y,z], "name": "piece_name"}, ...]
        mesh: Mesh path to use for all pieces
        name_prefix: Prefix for naming
    
    Returns:
        Dict with success status and spawned actors list
    """
    try:
        from helpers.actor_utilities import spawn_blueprint_actor
        
        logger.info(f"Creating tower with {len(tower_pieces)} pieces")
        spawned_actors = []
        
        # Group pieces by color to minimize blueprint creation
        color_groups = {}
        for piece in tower_pieces:
            color_key = tuple(round(c, 2) for c in piece["color"])
            if color_key not in color_groups:
                color_groups[color_key] = []
            color_groups[color_key].append(piece)
        
        logger.info(f"Tower uses {len(color_groups)} unique colors")
        
        # Create one blueprint per unique color and spawn all pieces of that color
        for color_key, pieces in color_groups.items():
            color = list(color_key)  # Convert back to list
            logger.info(f"Processing {len(pieces)} pieces with color {color}")
            
            # Create blueprint for this color
            color_hash = abs(hash(color_key)) % 10000
            bp_name = f"{name_prefix}_Color_{color_hash}_BP"
            
            # Try to create blueprint (or reuse if exists)
            create_result = unreal.send_command("create_blueprint", {"name": bp_name, "parent_class": "Actor"})
            
            blueprint_exists = False
            if create_result and create_result.get("success"):
                logger.info(f"Created new blueprint {bp_name} for color {color}")
                
                # Set up the blueprint
                # Add mesh component
                unreal.send_command("add_component_to_blueprint", {
                    "blueprint_name": bp_name,
                    "component_type": "StaticMeshComponent", 
                    "component_name": "Mesh"
                })
                
                # Set mesh properties
                unreal.send_command("set_static_mesh_properties", {
                    "blueprint_name": bp_name,
                    "component_name": "Mesh",
                    "static_mesh": mesh
                })
                
                # Set physics properties
                unreal.send_command("set_physics_properties", {
                    "blueprint_name": bp_name,
                    "component_name": "Mesh",
                    "simulate_physics": True,
                    "gravity_enabled": True,
                    "mass": 1.0
                })
                
                # Apply color
                unreal.send_command("set_mesh_material_color", {
                    "blueprint_name": bp_name,
                    "component_name": "Mesh",
                    "color": color,
                    "material_slot": 0
                })
                
                # Compile blueprint
                unreal.send_command("compile_blueprint", {"blueprint_name": bp_name})
                
            elif create_result and "already exists" in create_result.get("error", "").lower():
                logger.info(f"Blueprint {bp_name} already exists, reusing it")
                blueprint_exists = True
            else:
                logger.error(f"Failed to create blueprint {bp_name}: {create_result.get('error', 'Unknown error')}")
                continue
            
            # Now spawn all pieces of this color using the same blueprint
            pieces_spawned = 0
            for piece in pieces:
                spawn_result = spawn_blueprint_actor(unreal, bp_name, piece["name"], piece["location"])
                if spawn_result.get("success"):
                    # Set correct scale
                    spawned_name = spawn_result.get("result", {}).get("name", piece["name"])
                    unreal.send_command("set_actor_transform", {
                        "name": spawned_name,
                        "scale": piece["scale"]
                    })
                    spawned_actors.append(spawn_result)
                    pieces_spawned += 1
                else:
                    logger.warning(f"Failed to spawn piece {piece['name']}")
            
            logger.info(f"Spawned {pieces_spawned}/{len(pieces)} pieces for color {color}")
        
        logger.info(f"Tower creation complete! Created {len(color_groups)} blueprints and spawned {len(spawned_actors)} actors")
        
        return {
            "success": True,
            "actors": spawned_actors,
            "blueprints_created": len(color_groups),
            "total_pieces": len(tower_pieces)
        }
        
    except Exception as e:
        logger.error(f"Error in batch tower creation: {e}")
        return {"success": False, "message": str(e)}

def spawn_colored_tower_piece(unreal, actor_name: str, mesh: str, location: List[float], 
                            color: List[float], block_size: float) -> Dict[str, Any]:
    """
    DEPRECATED: Use create_tower_blueprints_and_batch_spawn instead for efficiency.
    This function is kept for backward compatibility but is not recommended.
    """
    logger.warning("spawn_colored_tower_piece is deprecated. Use create_tower_blueprints_and_batch_spawn for better performance.")
    
    # Convert single piece to batch format and call the new function
    tower_pieces = [{
        "location": location,
        "color": color,
        "scale": [block_size / 100.0, block_size / 100.0, block_size / 100.0],
        "name": actor_name
    }]
    
    result = create_tower_blueprints_and_batch_spawn(unreal, tower_pieces, mesh, "TowerPiece")
    
    if result.get("success") and result.get("actors"):
        return result["actors"][0]  # Return the single spawned actor
    else:
        return {"success": False, "message": result.get("message", "Failed to spawn piece")}

def create_decorative_tower_elements(unreal, location: List[float], base_size: int, height: int,
                                   name_prefix: str, color_palette: List[List[float]]) -> List[Dict[str, Any]]:
    """
    Create decorative elements for the tower like spires, flags, and ornaments.
    """
    spawned = []
    
    try:
        # Create a spire at the top
        spire_height = height // 5
        for i in range(spire_height):
            spire_location = [
                location[0],
                location[1], 
                location[2] + height * 100 + i * 50
            ]
            
            # Gradually shrinking spire
            scale_factor = (spire_height - i) / spire_height * 0.3
            color = color_palette[-1] if color_palette else [0.9, 0.9, 0.1, 1.0]  # Gold top
            
            actor_name = f"{name_prefix}_spire_{i}"
            result = spawn_colored_tower_piece(unreal, actor_name, "/Engine/BasicShapes/Cone.Cone", 
                                             spire_location, color, scale_factor * 100)
            
            if result.get("success"):
                spawned.append(result)
        
        # Add corner banners/flags every few levels
        for level in range(0, height, max(1, height // 8)):
            level_height = location[2] + level * 100
            
            for corner in range(4):
                angle = corner * math.pi / 2
                flag_x = location[0] + (base_size * 0.6) * 100 * math.cos(angle)
                flag_y = location[1] + (base_size * 0.6) * 100 * math.sin(angle)
                
                # Create flag pole
                actor_name = f"{name_prefix}_flag_{level}_{corner}"
                flag_color = color_palette[corner % len(color_palette)] if color_palette else [0.8, 0.2, 0.2, 1.0]
                
                result = spawn_colored_tower_piece(unreal, actor_name, "/Engine/BasicShapes/Cylinder.Cylinder",
                                                 [flag_x, flag_y, level_height + 150], flag_color, 20)
                
                if result.get("success"):
                    spawned.append(result)
    
    except Exception as e:
        logger.error(f"Error creating decorative elements: {e}")
    
    return spawned