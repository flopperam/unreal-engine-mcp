"""
Infrastructure creation helper functions for town generation.
Includes streets, lights, vehicles, and decorations.
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

def _safe_spawn_infrastructure_actor(unreal, params):
    """Helper function to safely spawn infrastructure actors and track results."""
    resp = safe_spawn_actor(unreal, params, auto_unique_name=True)
    return resp


def _create_street_grid(blocks: int, block_size: float, street_width: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a grid of streets for the town."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        set_actor_transform = server.set_actor_transform
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        streets = []
        
        # Create horizontal streets
        for i in range(blocks + 1):
            street_y = location[1] + (i - blocks/2) * block_size
            for j in range(blocks):
                street_x = location[0] + (j - blocks/2 + 0.5) * block_size
                
                actor_name = f"{name_prefix}_Street_H_{i}_{j}"
                
                # Simple street spawn
                result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": [street_x, street_y, location[2] - 5]
                })
                
                # Scale the street segment
                if result and result.get("success"):
                    set_actor_transform(
                        actor_name,
                        scale=[block_size/100.0 * 0.7, street_width/100.0, 0.1]
                    )
                    streets.append(result.get("result"))
        
        # Create vertical streets
        for i in range(blocks + 1):
            street_x = location[0] + (i - blocks/2) * block_size
            for j in range(blocks):
                street_y = location[1] + (j - blocks/2 + 0.5) * block_size
                
                actor_name = f"{name_prefix}_Street_V_{i}_{j}"
                
                # Simple street spawn
                result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": [street_x, street_y, location[2] - 5]
                })
                
                # Scale the street segment
                if result and result.get("success"):
                    set_actor_transform(
                        actor_name,
                        scale=[street_width/100.0, block_size/100.0 * 0.7, 0.1]
                    )
                    streets.append(result.get("result"))
        
        return {"success": True, "actors": streets}
        
    except Exception as e:
        logger.error(f"_create_street_grid error: {e}")
        return {"success": False, "actors": []}


def _create_street_lights(blocks: int, block_size: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create street lights throughout the town."""
    try:
        import random
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        set_actor_transform = server.set_actor_transform
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        lights = []
        
        # Place lights at street intersections and along streets
        for i in range(blocks + 1):
            for j in range(blocks + 1):
                # Skip some randomly for variety
                if random.random() > 0.7:
                    continue
                    
                light_x = location[0] + (i - blocks/2) * block_size
                light_y = location[1] + (j - blocks/2) * block_size
                
                # Create pole (simple cylinder)
                pole_name = f"{name_prefix}_LightPole_{i}_{j}"
                pole_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": pole_name,
                    "type": "StaticMeshActor", 
                    "location": [light_x, light_y, location[2] + 200]
                })
                if pole_result and pole_result.get("success"):
                    set_actor_transform(pole_name, scale=[0.2, 0.2, 4.0])
                    lights.append(pole_result.get("result"))
                
                # Create light (simple sphere)
                light_name = f"{name_prefix}_Light_{i}_{j}"
                light_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": light_name,
                    "type": "StaticMeshActor",
                    "location": [light_x, light_y, location[2] + 380]
                })
                if light_result and light_result.get("success"):
                    set_actor_transform(light_name, scale=[0.3, 0.3, 0.3])
                    lights.append(light_result.get("result"))
        
        return {"success": True, "actors": lights}
        
    except Exception as e:
        logger.error(f"_create_street_lights error: {e}")
        return {"success": False, "actors": []}


def _create_town_vehicles(blocks: int, block_size: float, street_width: float, location: List[float], name_prefix: str, vehicle_count: int) -> Dict[str, Any]:
    """Create vehicles throughout the town."""
    try:
        import random
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        set_actor_transform = server.set_actor_transform
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        vehicles = []
        
        for i in range(vehicle_count):
            # Random position on streets
            street_x = location[0] + random.uniform(-blocks*block_size/2, blocks*block_size/2)
            street_y = location[1] + random.uniform(-blocks*block_size/2, blocks*block_size/2)
            
            # Create simple car (basic cube)
            car_name = f"{name_prefix}_Car_{i}"
            car_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": car_name,
                "type": "StaticMeshActor",
                "location": [street_x, street_y, location[2] + 50]
            })
            
            if car_result and car_result.get("success"):
                # Scale to car proportions
                set_actor_transform(car_name, scale=[4.0, 2.0, 1.5])
                vehicles.append(car_result.get("result"))
        
        return {"success": True, "actors": vehicles}
        
    except Exception as e:
        logger.error(f"_create_town_vehicles error: {e}")
        return {"success": False, "actors": []}


def _create_town_decorations(blocks: int, block_size: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create parks, trees, and other decorative elements."""
    try:
        import random
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        set_actor_transform = server.set_actor_transform
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        decorations = []
        
        # Create a few parks with trees
        num_parks = max(1, blocks // 3)
        for park_id in range(num_parks):
            park_x = location[0] + random.uniform(-blocks*block_size/3, blocks*block_size/3)
            park_y = location[1] + random.uniform(-blocks*block_size/3, blocks*block_size/3)
            
            # Create several trees in each park
            trees_per_park = random.randint(3, 8)
            for tree_id in range(trees_per_park):
                tree_x = park_x + random.uniform(-200, 200)
                tree_y = park_y + random.uniform(-200, 200)
                
                # Tree trunk (simple cylinder)
                trunk_name = f"{name_prefix}_TreeTrunk_{park_id}_{tree_id}"
                trunk_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": trunk_name,
                    "type": "StaticMeshActor",
                    "location": [tree_x, tree_y, location[2] + 150]
                })
                if trunk_result and trunk_result.get("success"):
                    set_actor_transform(trunk_name, scale=[0.5, 0.5, 3.0])
                    decorations.append(trunk_result.get("result"))
                
                # Tree leaves (simple sphere)
                leaves_name = f"{name_prefix}_TreeLeaves_{park_id}_{tree_id}"
                leaves_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": leaves_name,
                    "type": "StaticMeshActor",
                    "location": [tree_x, tree_y, location[2] + 350]
                })
                if leaves_result and leaves_result.get("success"):
                    set_actor_transform(leaves_name, scale=[2.0, 2.0, 2.0])
                    decorations.append(leaves_result.get("result"))
        
        return {"success": True, "actors": decorations}
        
    except Exception as e:
        logger.error(f"_create_town_decorations error: {e}")
        return {"success": False, "actors": []}


def _create_traffic_lights(blocks: int, block_size: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create traffic lights at major intersections."""
    try:
        import random
        import math
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        traffic_lights = []
        
        # Place traffic lights at major intersections
        for i in range(1, blocks, 2):  # Every other intersection
            for j in range(1, blocks, 2):
                intersection_x = location[0] + (i - blocks/2) * block_size
                intersection_y = location[1] + (j - blocks/2) * block_size
                
                # Create traffic light poles at four corners
                for corner in range(4):
                    angle = corner * math.pi / 2
                    offset = 150  # Distance from intersection center
                    
                    pole_x = intersection_x + offset * math.cos(angle)
                    pole_y = intersection_y + offset * math.sin(angle)
                    
                    # Pole
                    pole_name = f"{name_prefix}_TrafficPole_{i}_{j}_{corner}"
                    pole_result = _safe_spawn_infrastructure_actor(unreal, {
                        "name": pole_name,
                        "type": "StaticMeshActor",
                        "location": [pole_x, pole_y, location[2] + 150],
                        "scale": [0.15, 0.15, 3.0],
                        "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                    })
                    if pole_result and pole_result.get("success"):
                        traffic_lights.append(pole_result.get("result"))
                    
                    # Traffic light box
                    light_name = f"{name_prefix}_TrafficLight_{i}_{j}_{corner}"
                    light_result = _safe_spawn_infrastructure_actor(unreal, {
                        "name": light_name,
                        "type": "StaticMeshActor",
                        "location": [pole_x, pole_y, location[2] + 280],
                        "scale": [0.3, 0.2, 0.8],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if light_result and light_result.get("success"):
                        traffic_lights.append(light_result.get("result"))
        
        return {"success": True, "actors": traffic_lights}
        
    except Exception as e:
        logger.error(f"_create_traffic_lights error: {e}")
        return {"success": False, "actors": []}


def _create_street_signage(blocks: int, block_size: float, location: List[float], name_prefix: str, town_size: str) -> Dict[str, Any]:
    """Create street signs and billboards."""
    try:
        import random
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        signage = []
        
        # Street name signs at corners
        street_names = ["Main St", "1st Ave", "2nd Ave", "Park Blvd", "Commerce Dr", "Tech Way"]
        
        for i in range(0, blocks + 1, 2):
            for j in range(0, blocks + 1, 2):
                if random.random() > 0.5:
                    continue
                    
                sign_x = location[0] + (i - blocks/2) * block_size + 100
                sign_y = location[1] + (j - blocks/2) * block_size + 100
                
                # Sign pole
                pole_name = f"{name_prefix}_SignPole_{i}_{j}"
                pole_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": pole_name,
                    "type": "StaticMeshActor",
                    "location": [sign_x, sign_y, location[2] + 100],
                    "scale": [0.1, 0.1, 2.0],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                })
                if pole_result and pole_result.get("success"):
                    signage.append(pole_result.get("result"))
                
                # Sign
                sign_name = f"{name_prefix}_StreetSign_{i}_{j}"
                sign_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": sign_name,
                    "type": "StaticMeshActor",
                    "location": [sign_x, sign_y, location[2] + 180],
                    "scale": [1.5, 0.05, 0.3],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if sign_result and sign_result.get("success"):
                    signage.append(sign_result.get("result"))
        
        # Billboards for larger towns
        if town_size in ["large", "metropolis"]:
            num_billboards = random.randint(3, 8)
            for b in range(num_billboards):
                billboard_x = location[0] + random.uniform(-blocks*block_size/3, blocks*block_size/3)
                billboard_y = location[1] + random.uniform(-blocks*block_size/3, blocks*block_size/3)
                
                # Billboard structure
                billboard_name = f"{name_prefix}_Billboard_{b}"
                billboard_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": billboard_name,
                    "type": "StaticMeshActor",
                    "location": [billboard_x, billboard_y, location[2] + 400],
                    "scale": [3.0, 0.1, 2.0],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if billboard_result and billboard_result.get("success"):
                    signage.append(billboard_result.get("result"))
                
                # Billboard supports
                for support_offset in [-100, 100]:
                    support_name = f"{name_prefix}_BillboardSupport_{b}_{support_offset}"
                    support_result = _safe_spawn_infrastructure_actor(unreal, {
                        "name": support_name,
                        "type": "StaticMeshActor",
                        "location": [billboard_x + support_offset, billboard_y, location[2] + 200],
                        "scale": [0.2, 0.2, 4.0],
                        "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                    })
                    if support_result and support_result.get("success"):
                        signage.append(support_result.get("result"))
        
        return {"success": True, "actors": signage}
        
    except Exception as e:
        logger.error(f"_create_street_signage error: {e}")
        return {"success": False, "actors": []}


def _create_sidewalks_crosswalks(blocks: int, block_size: float, street_width: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create sidewalks and crosswalks."""
    try:
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        sidewalks = []
        sidewalk_width = 150.0
        
        # Create sidewalks along streets
        for i in range(blocks):
            for j in range(blocks + 1):
                # Horizontal sidewalks
                sidewalk_y = location[1] + (j - blocks/2) * block_size
                sidewalk_x = location[0] + (i - blocks/2 + 0.5) * block_size
                
                # North sidewalk
                north_sidewalk_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": f"{name_prefix}_SidewalkH_North_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x, sidewalk_y - street_width/2 + sidewalk_width/2, location[2]],
                    "scale": [block_size/100.0 * 0.7, sidewalk_width/100.0, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if north_sidewalk_result and north_sidewalk_result.get("success"):
                    sidewalks.append(north_sidewalk_result.get("result"))
                
                # South sidewalk
                south_sidewalk_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": f"{name_prefix}_SidewalkH_South_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x, sidewalk_y + street_width/2 - sidewalk_width/2, location[2]],
                    "scale": [block_size/100.0 * 0.7, sidewalk_width/100.0, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if south_sidewalk_result and south_sidewalk_result.get("success"):
                    sidewalks.append(south_sidewalk_result.get("result"))
        
        # Vertical sidewalks
        for i in range(blocks + 1):
            for j in range(blocks):
                sidewalk_x = location[0] + (i - blocks/2) * block_size
                sidewalk_y = location[1] + (j - blocks/2 + 0.5) * block_size
                
                # East sidewalk
                east_sidewalk_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": f"{name_prefix}_SidewalkV_East_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x - street_width/2 + sidewalk_width/2, sidewalk_y, location[2]],
                    "scale": [sidewalk_width/100.0, block_size/100.0 * 0.7, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if east_sidewalk_result and east_sidewalk_result.get("success"):
                    sidewalks.append(east_sidewalk_result.get("result"))
                
                # West sidewalk
                west_sidewalk_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": f"{name_prefix}_SidewalkV_West_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x + street_width/2 - sidewalk_width/2, sidewalk_y, location[2]],
                    "scale": [sidewalk_width/100.0, block_size/100.0 * 0.7, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if west_sidewalk_result and west_sidewalk_result.get("success"):
                    sidewalks.append(west_sidewalk_result.get("result"))
        
        # Create crosswalks at intersections
        crosswalk_width = 200.0
        for i in range(blocks + 1):
            for j in range(blocks + 1):
                intersection_x = location[0] + (i - blocks/2) * block_size
                intersection_y = location[1] + (j - blocks/2) * block_size
                
                # Create crosswalk stripes
                for stripe in range(5):
                    stripe_offset = (stripe - 2) * 40
                    
                    # North-South crosswalk
                    ns_crosswalk_result = _safe_spawn_infrastructure_actor(unreal, {
                        "name": f"{name_prefix}_CrosswalkNS_{i}_{j}_{stripe}",
                        "type": "StaticMeshActor",
                        "location": [intersection_x + stripe_offset, intersection_y, location[2] + 1],
                        "scale": [0.3, crosswalk_width/100.0, 0.02],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if ns_crosswalk_result and ns_crosswalk_result.get("success"):
                        sidewalks.append(ns_crosswalk_result.get("result"))
                    
                    # East-West crosswalk
                    ew_crosswalk_result = _safe_spawn_infrastructure_actor(unreal, {
                        "name": f"{name_prefix}_CrosswalkEW_{i}_{j}_{stripe}",
                        "type": "StaticMeshActor",
                        "location": [intersection_x, intersection_y + stripe_offset, location[2] + 1],
                        "scale": [crosswalk_width/100.0, 0.3, 0.02],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if ew_crosswalk_result and ew_crosswalk_result.get("success"):
                        sidewalks.append(ew_crosswalk_result.get("result"))
        
        return {"success": True, "actors": sidewalks}
        
    except Exception as e:
        logger.error(f"_create_sidewalks_crosswalks error: {e}")
        return {"success": False, "actors": []}


def _create_urban_furniture(blocks: int, block_size: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create benches, trash cans, and bus stops."""
    try:
        import random
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        furniture = []
        
        # Place furniture along sidewalks
        num_furniture_items = blocks * blocks // 2
        
        for f in range(num_furniture_items):
            # Random position along a street
            street_x = location[0] + random.uniform(-blocks*block_size/2, blocks*block_size/2)
            street_y = location[1] + random.uniform(-blocks*block_size/2, blocks*block_size/2)
            
            # Offset to sidewalk
            sidewalk_offset = random.choice([-200, 200])
            if random.random() > 0.5:
                furniture_x = street_x + sidewalk_offset
                furniture_y = street_y
            else:
                furniture_x = street_x
                furniture_y = street_y + sidewalk_offset
            
            furniture_type = random.choice(["bench", "trash", "bus_stop"])
            
            if furniture_type == "bench":
                # Create bench
                bench_name = f"{name_prefix}_Bench_{f}"
                bench_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": bench_name,
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y, location[2] + 30],
                    "scale": [1.5, 0.5, 0.6],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if bench_result and bench_result.get("success"):
                    furniture.append(bench_result.get("result"))
                
                # Bench supports
                for support_offset in [-50, 50]:
                    support_name = f"{name_prefix}_BenchSupport_{f}_{support_offset}"
                    support_result = _safe_spawn_infrastructure_actor(unreal, {
                        "name": support_name,
                        "type": "StaticMeshActor",
                        "location": [furniture_x + support_offset, furniture_y, location[2] + 15],
                        "scale": [0.1, 0.5, 0.3],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if support_result and support_result.get("success"):
                        furniture.append(support_result.get("result"))
            
            elif furniture_type == "trash":
                # Create trash can
                trash_name = f"{name_prefix}_TrashCan_{f}"
                trash_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": trash_name,
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y, location[2] + 40],
                    "scale": [0.4, 0.4, 0.8],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                })
                if trash_result and trash_result.get("success"):
                    furniture.append(trash_result.get("result"))
            
            else:  # bus_stop
                # Create bus stop shelter
                shelter_name = f"{name_prefix}_BusStop_{f}"
                shelter_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": shelter_name,
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y, location[2] + 120],
                    "scale": [2.0, 1.0, 0.1],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if shelter_result and shelter_result.get("success"):
                    furniture.append(shelter_result.get("result"))
                
                # Bus stop posts
                for post_x in [-80, 80]:
                    post_name = f"{name_prefix}_BusStopPost_{f}_{post_x}"
                    post_result = _safe_spawn_infrastructure_actor(unreal, {
                        "name": post_name,
                        "type": "StaticMeshActor",
                        "location": [furniture_x + post_x, furniture_y, location[2] + 60],
                        "scale": [0.1, 0.1, 1.2],
                        "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                    })
                    if post_result and post_result.get("success"):
                        furniture.append(post_result.get("result"))
                
                # Bus stop bench
                bench_name = f"{name_prefix}_BusStopBench_{f}"
                bench_result = _safe_spawn_infrastructure_actor(unreal, {
                    "name": bench_name,
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y + 30, location[2] + 25],
                    "scale": [1.8, 0.4, 0.5],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if bench_result and bench_result.get("success"):
                    furniture.append(bench_result.get("result"))
        
        return {"success": True, "actors": furniture}
        
    except Exception as e:
        logger.error(f"_create_urban_furniture error: {e}")
        return {"success": False, "actors": []}


def _create_street_utilities(blocks: int, block_size: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create parking meters and fire hydrants."""
    try:
        import random
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        utilities = []
        
        # Parking meters along commercial streets
        num_meters = blocks * 4
        for m in range(num_meters):
            meter_x = location[0] + random.uniform(-blocks*block_size/3, blocks*block_size/3)
            meter_y = location[1] + random.uniform(-blocks*block_size/3, blocks*block_size/3)
            
            # Place on sidewalk edge
            sidewalk_offset = random.choice([-180, 180])
            if random.random() > 0.5:
                meter_x += sidewalk_offset
            else:
                meter_y += sidewalk_offset
            
            # Parking meter
            meter_name = f"{name_prefix}_ParkingMeter_{m}"
            meter_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": meter_name,
                "type": "StaticMeshActor",
                "location": [meter_x, meter_y, location[2] + 50],
                "scale": [0.15, 0.15, 1.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if meter_result and meter_result.get("success"):
                utilities.append(meter_result.get("result"))
            
            # Meter head
            head_name = f"{name_prefix}_MeterHead_{m}"
            head_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": head_name,
                "type": "StaticMeshActor",
                "location": [meter_x, meter_y, location[2] + 100],
                "scale": [0.25, 0.15, 0.3],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if head_result and head_result.get("success"):
                utilities.append(head_result.get("result"))
        
        # Fire hydrants at corners
        num_hydrants = blocks + 2
        for h in range(num_hydrants):
            hydrant_x = location[0] + random.uniform(-blocks*block_size/2, blocks*block_size/2)
            hydrant_y = location[1] + random.uniform(-blocks*block_size/2, blocks*block_size/2)
            
            # Fire hydrant
            hydrant_name = f"{name_prefix}_Hydrant_{h}"
            hydrant_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": hydrant_name,
                "type": "StaticMeshActor",
                "location": [hydrant_x, hydrant_y, location[2] + 40],
                "scale": [0.3, 0.3, 0.8],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if hydrant_result and hydrant_result.get("success"):
                utilities.append(hydrant_result.get("result"))
            
            # Hydrant cap
            cap_name = f"{name_prefix}_HydrantCap_{h}"
            cap_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": cap_name,
                "type": "StaticMeshActor",
                "location": [hydrant_x, hydrant_y, location[2] + 75],
                "scale": [0.35, 0.35, 0.1],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if cap_result and cap_result.get("success"):
                utilities.append(cap_result.get("result"))
        
        return {"success": True, "actors": utilities}
        
    except Exception as e:
        logger.error(f"_create_street_utilities error: {e}")
        return {"success": False, "actors": []}


def _create_central_plaza(blocks: int, block_size: float, location: List[float], name_prefix: str) -> Dict[str, Any]:
    """Create a central plaza with fountain and monuments."""
    try:
        import math
        # Import here to avoid circular imports
        import unreal_mcp_server_advanced as server
        get_unreal_connection = server.get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "actors": []}
            
        plaza = []
        plaza_size = block_size * 0.8
        
        # Plaza floor
        plaza_floor_result = _safe_spawn_infrastructure_actor(unreal, {
            "name": f"{name_prefix}_PlazaFloor",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 2],
            "scale": [plaza_size/100.0, plaza_size/100.0, 0.05],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if plaza_floor_result and plaza_floor_result.get("success"):
            plaza.append(plaza_floor_result.get("result"))
        
        # Central fountain base
        fountain_base_result = _safe_spawn_infrastructure_actor(unreal, {
            "name": f"{name_prefix}_FountainBase",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 10],
            "scale": [3.0, 3.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if fountain_base_result and fountain_base_result.get("success"):
            plaza.append(fountain_base_result.get("result"))
        
        # Fountain center
        fountain_center_result = _safe_spawn_infrastructure_actor(unreal, {
            "name": f"{name_prefix}_FountainCenter",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 50],
            "scale": [0.5, 0.5, 0.8],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if fountain_center_result and fountain_center_result.get("success"):
            plaza.append(fountain_center_result.get("result"))
        
        # Fountain top
        fountain_top_result = _safe_spawn_infrastructure_actor(unreal, {
            "name": f"{name_prefix}_FountainTop",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 80],
            "scale": [1.5, 1.5, 0.1],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if fountain_top_result and fountain_top_result.get("success"):
            plaza.append(fountain_top_result.get("result"))
        
        # Monument/statue
        monument_result = _safe_spawn_infrastructure_actor(unreal, {
            "name": f"{name_prefix}_Monument",
            "type": "StaticMeshActor",
            "location": [location[0] + plaza_size/3, location[1], location[2] + 100],
            "scale": [1.0, 1.0, 2.0],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if monument_result and monument_result.get("success"):
            plaza.append(monument_result.get("result"))
        
        # Monument base
        monument_base_result = _safe_spawn_infrastructure_actor(unreal, {
            "name": f"{name_prefix}_MonumentBase",
            "type": "StaticMeshActor",
            "location": [location[0] + plaza_size/3, location[1], location[2] + 30],
            "scale": [2.0, 2.0, 0.6],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if monument_base_result and monument_base_result.get("success"):
            plaza.append(monument_base_result.get("result"))
        
        # Plaza benches in circle
        num_benches = 8
        for i in range(num_benches):
            angle = (2 * math.pi * i) / num_benches
            bench_x = location[0] + plaza_size/3 * math.cos(angle)
            bench_y = location[1] + plaza_size/3 * math.sin(angle)
            bench_rotation = [0, 0, angle * 180/math.pi]
            
            bench_name = f"{name_prefix}_PlazaBench_{i}"
            bench_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": bench_name,
                "type": "StaticMeshActor",
                "location": [bench_x, bench_y, location[2] + 30],
                "rotation": bench_rotation,
                "scale": [1.5, 0.5, 0.6],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if bench_result and bench_result.get("success"):
                plaza.append(bench_result.get("result"))
        
        # Decorative light posts around plaza
        num_lights = 12
        for i in range(num_lights):
            angle = (2 * math.pi * i) / num_lights
            light_x = location[0] + plaza_size/2 * math.cos(angle)
            light_y = location[1] + plaza_size/2 * math.sin(angle)
            
            # Decorative light post
            post_name = f"{name_prefix}_PlazaLightPost_{i}"
            post_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": post_name,
                "type": "StaticMeshActor",
                "location": [light_x, light_y, location[2] + 100],
                "scale": [0.15, 0.15, 2.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if post_result and post_result.get("success"):
                plaza.append(post_result.get("result"))
            
            # Light fixture
            light_name = f"{name_prefix}_PlazaLight_{i}"
            light_result = _safe_spawn_infrastructure_actor(unreal, {
                "name": light_name,
                "type": "StaticMeshActor",
                "location": [light_x, light_y, location[2] + 180],
                "scale": [0.4, 0.4, 0.3],
                "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
            })
            if light_result and light_result.get("success"):
                plaza.append(light_result.get("result"))
        
        return {"success": True, "actors": plaza}
        
    except Exception as e:
        logger.error(f"_create_central_plaza error: {e}")
        return {"success": False, "actors": []}
