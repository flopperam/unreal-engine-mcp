"""
Unreal Engine Advanced MCP Server

A streamlined MCP server focused on advanced composition tools for Unreal Engine.
Contains only the advanced tools from the expanded MCP tool system to keep tool count manageable.
"""

import logging
import socket
import json
import math
import random
import time
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

from helpers.infrastructure_creation import (
    _create_street_grid, _create_street_lights, _create_town_vehicles, _create_town_decorations,
    _create_traffic_lights, _create_street_signage, _create_sidewalks_crosswalks, _create_urban_furniture,
    _create_street_utilities, _create_central_plaza
)
from helpers.building_creation import _create_town_building
from helpers.castle_creation import (
    get_castle_size_params, calculate_scaled_dimensions, build_outer_bailey_walls, 
    build_inner_bailey_walls, build_gate_complex, build_corner_towers, 
    build_inner_corner_towers, build_intermediate_towers, build_central_keep, 
    build_courtyard_complex, build_bailey_annexes, build_siege_weapons, 
    build_village_settlement, build_drawbridge_and_moat, add_decorative_flags
)
from helpers.house_construction import build_house

from helpers.mansion_creation import (
    get_mansion_size_params, calculate_mansion_layout, build_mansion_main_structure,
    build_mansion_exterior, add_mansion_interior
)
from helpers.actor_utilities import spawn_blueprint_actor, get_blueprint_material_info
from helpers.actor_name_manager import (
    safe_spawn_actor, safe_delete_actor
)
from helpers.bridge_aqueduct_creation import (
    build_suspension_bridge_structure, build_aqueduct_structure
)

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp_advanced.log'),
    ]
)
logger = logging.getLogger("UnrealMCP_Advanced")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

class UnrealConnection:
    """Connection to an Unreal Engine instance."""
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            # Close any existing socket
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(30)
            
            # Set socket options for better stability
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(30)
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                # Process the data received so far
                data = b''.join(chunks)
                decoded_data = data.decode('utf-8')
                
                # Try to parse as JSON to check if complete
                try:
                    json.loads(decoded_data)
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    # Not complete JSON yet, continue reading
                    logger.debug(f"Received partial response, waiting for more data...")
                    continue
                except Exception as e:
                    logger.warning(f"Error processing response chunk: {str(e)}")
                    continue
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                data = b''.join(chunks)
                try:
                    json.loads(data.decode('utf-8'))
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except:
                    pass
            raise Exception("Timeout receiving Unreal response")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise
    
    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response."""
        # Always reconnect for each command
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        
        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None
        
        try:
            command_obj = {
                "type": command,
                "params": params or {}
            }
            
            command_json = json.dumps(command_obj)
            logger.info(f"Sending command: {command_json}")
            self.socket.sendall(command_json.encode('utf-8'))
            
            response_data = self.receive_full_response(self.socket)
            response = json.loads(response_data.decode('utf-8'))
            
            logger.info(f"Complete response from Unreal: {response}")
            
            # Handle error responses
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                response = {
                    "status": "error",
                    "error": error_message
                }
            
            # Always close the connection after command
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {
                "status": "error",
                "error": str(e)
            }

# Global connection state
_unreal_connection: UnrealConnection = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        if _unreal_connection is None:
            _unreal_connection = UnrealConnection()
            if not _unreal_connection.connect():
                logger.warning("Could not connect to Unreal Engine")
                _unreal_connection = None
        return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection
    logger.info("UnrealMCP Advanced server starting up")
    try:
        _unreal_connection = get_unreal_connection()
        if _unreal_connection:
            logger.info("Connected to Unreal Engine on startup")
        else:
            logger.warning("Could not connect to Unreal Engine on startup")
    except Exception as e:
        logger.error(f"Error connecting to Unreal Engine on startup: {e}")
        _unreal_connection = None
    
    try:
        yield {}
    finally:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal MCP Advanced server shut down")

# Initialize server
mcp = FastMCP(
    "UnrealMCP_Advanced",
    description="Unreal Engine Advanced Tools - Streamlined MCP server for advanced composition and building tools",
    lifespan=server_lifespan
)

# Essential Actor Management Tools
@mcp.tool()
def get_actors_in_level(random_string: str = "") -> Dict[str, Any]:
    """Get a list of all actors in the current level."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        response = unreal.send_command("get_actors_in_level", {})
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"get_actors_in_level error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def find_actors_by_name(pattern: str) -> Dict[str, Any]:
    """Find actors by name pattern."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        response = unreal.send_command("find_actors_by_name", {"pattern": pattern})
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"find_actors_by_name error: {e}")
        return {"success": False, "message": str(e)}



@mcp.tool()
def delete_actor(name: str) -> Dict[str, Any]:
    """Delete an actor by name."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        # Use the safe delete function to update tracking
        response = safe_delete_actor(unreal, name)
        return response
    except Exception as e:
        logger.error(f"delete_actor error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def set_actor_transform(
    name: str,
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None
) -> Dict[str, Any]:
    """Set the transform of an actor."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    try:
        params = {"name": name}
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation
        if scale is not None:
            params["scale"] = scale

        response = unreal.send_command("set_actor_transform", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"set_actor_transform error: {e}")
        return {"success": False, "message": str(e)}

# Scene Management and Organization Tools

@mcp.tool()
def clear_scene_by_prefix(prefix: str, confirm: bool = False, dry_run: bool = True) -> Dict[str, Any]:
    """
    Clear all actors with names starting with the given prefix.

    This is useful for cleaning up generated content like buildings, walls, or other prefixed actors.

    Args:
        prefix: The prefix to match (e.g., "Building_", "Wall_", "Tower_")
        confirm: Whether to actually delete the actors (requires both confirm=True and dry_run=False)
        dry_run: If True, only return what would be deleted without actually deleting

    Returns:
        Dict with success, count_found, count_deleted, dry_run status, and actor_names

    Safety:
        - By default, dry_run=True to prevent accidental deletions
        - Requires both confirm=True and dry_run=False to actually delete
    """
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    try:
        # Find all actors matching the prefix pattern
        response = unreal.send_command("find_actors_by_name", {"pattern": prefix})

        if not response or response.get("status") != "success":
            logger.error(f"Failed to find actors with prefix '{prefix}': {response}")
            return {
                "success": False,
                "message": f"Failed to find actors: {response.get('error', 'Unknown error') if response else 'No response'}"
            }

        # Extract actors from response
        actors = response.get("actors", [])

        # Filter to only actors that start with the prefix
        matching_actors = []
        for actor in actors:
            if isinstance(actor, dict):
                actor_name = actor.get("name", "")
                if actor_name.startswith(prefix):
                    matching_actors.append(actor_name)
            elif isinstance(actor, str):
                if actor.startswith(prefix):
                    matching_actors.append(actor)

        count_found = len(matching_actors)

        # If dry run, just return the count and list
        if dry_run:
            logger.info(f"Dry run: Found {count_found} actors with prefix '{prefix}'")
            return {
                "success": True,
                "message": f"Dry run: Found {count_found} actors that would be deleted",
                "count_found": count_found,
                "count_deleted": 0,
                "dry_run": True,
                "actor_names": matching_actors
            }

        # If not confirmed, require confirmation
        if not confirm:
            logger.warning(f"Deletion not confirmed for {count_found} actors with prefix '{prefix}'")
            return {
                "success": False,
                "message": f"Found {count_found} actors but deletion not confirmed. Set confirm=True and dry_run=False to delete.",
                "count_found": count_found,
                "count_deleted": 0,
                "dry_run": False,
                "requires_confirmation": True,
                "actor_names": matching_actors
            }

        # Delete all matching actors
        deleted_count = 0
        failed_deletions = []

        for actor_name in matching_actors:
            delete_response = safe_delete_actor(unreal, actor_name)
            if delete_response and delete_response.get("status") == "success":
                deleted_count += 1
                logger.info(f"Deleted actor: {actor_name}")
            else:
                failed_deletions.append(actor_name)
                logger.error(f"Failed to delete actor {actor_name}: {delete_response}")

        logger.info(f"Deleted {deleted_count}/{count_found} actors with prefix '{prefix}'")

        result = {
            "success": True,
            "message": f"Deleted {deleted_count}/{count_found} actors",
            "count_found": count_found,
            "count_deleted": deleted_count,
            "dry_run": False,
            "actor_names": matching_actors
        }

        if failed_deletions:
            result["failed_deletions"] = failed_deletions
            result["message"] += f" ({len(failed_deletions)} failed)"

        return result

    except Exception as e:
        logger.error(f"clear_scene_by_prefix error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def get_scene_statistics() -> Dict[str, Any]:
    """
    Get comprehensive statistics about all actors in the current level.

    Provides:
    - Total actor count
    - Actors grouped by prefix (prefix before first underscore)
    - Scene bounding box (min/max X, Y, Z coordinates)
    - Actor type distribution

    Returns:
        Dict with total_actors, actors_by_prefix, bounds, and actor_types
    """
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    try:
        # Get all actors in the level
        response = unreal.send_command("get_actors_in_level", {})

        if not response or response.get("status") != "success":
            logger.error(f"Failed to get actors in level: {response}")
            return {
                "success": False,
                "message": f"Failed to get actors: {response.get('error', 'Unknown error') if response else 'No response'}"
            }

        # Extract actors from response
        actors = response.get("actors", [])

        if not actors:
            return {
                "success": True,
                "message": "No actors found in level",
                "total_actors": 0,
                "actors_by_prefix": {},
                "bounds": {"min": [0, 0, 0], "max": [0, 0, 0]},
                "actor_types": {}
            }

        # Initialize statistics
        total_actors = len(actors)
        actors_by_prefix = {}
        actor_types = {}

        # Initialize bounds
        min_x = min_y = min_z = float('inf')
        max_x = max_y = max_z = float('-inf')

        # Process each actor
        for actor in actors:
            if isinstance(actor, dict):
                actor_name = actor.get("name", "Unknown")
                actor_type = actor.get("type", "Unknown")
                location = actor.get("location", [])

                # Extract prefix (everything before first underscore)
                if "_" in actor_name:
                    prefix = actor_name.split("_")[0]
                else:
                    prefix = "NoPrefix"

                # Count by prefix
                if prefix not in actors_by_prefix:
                    actors_by_prefix[prefix] = []
                actors_by_prefix[prefix].append(actor_name)

                # Count by type
                if actor_type not in actor_types:
                    actor_types[actor_type] = 0
                actor_types[actor_type] += 1

                # Update bounds if location is available
                if location and len(location) >= 3:
                    x, y, z = location[0], location[1], location[2]
                    min_x = min(min_x, x)
                    max_x = max(max_x, x)
                    min_y = min(min_y, y)
                    max_y = max(max_y, y)
                    min_z = min(min_z, z)
                    max_z = max(max_z, z)

        # Convert actors_by_prefix to counts
        prefix_counts = {prefix: len(actor_list) for prefix, actor_list in actors_by_prefix.items()}

        # Handle case where no valid locations were found
        if min_x == float('inf'):
            bounds = {"min": [0, 0, 0], "max": [0, 0, 0]}
        else:
            bounds = {
                "min": [min_x, min_y, min_z],
                "max": [max_x, max_y, max_z]
            }

        logger.info(f"Scene statistics: {total_actors} actors, {len(prefix_counts)} prefixes, {len(actor_types)} types")

        return {
            "success": True,
            "message": f"Found {total_actors} actors in level",
            "total_actors": total_actors,
            "actors_by_prefix": prefix_counts,
            "bounds": bounds,
            "actor_types": actor_types
        }

    except Exception as e:
        logger.error(f"get_scene_statistics error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def organize_actors_by_height(prefix: str = "", layers: int = 5) -> Dict[str, Any]:
    """
    Organize actors into height-based layers for analysis and management.

    Divides actors into equal-height layers based on their Z coordinate.
    Useful for understanding vertical distribution of scene elements.

    Args:
        prefix: Optional prefix to filter actors (empty string = all actors)
        layers: Number of height layers to create (default: 5)

    Returns:
        Dict with layer definitions (z_min, z_max, actor_count, actor_names), total_actors, and layers
    """
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    try:
        # Get all actors in the level
        response = unreal.send_command("get_actors_in_level", {})

        if not response or response.get("status") != "success":
            logger.error(f"Failed to get actors in level: {response}")
            return {
                "success": False,
                "message": f"Failed to get actors: {response.get('error', 'Unknown error') if response else 'No response'}"
            }

        # Extract actors from response
        all_actors = response.get("actors", [])

        # Filter by prefix if specified
        actors_with_location = []
        for actor in all_actors:
            if isinstance(actor, dict):
                actor_name = actor.get("name", "")
                location = actor.get("location", [])

                # Skip if prefix is specified and doesn't match
                if prefix and not actor_name.startswith(prefix):
                    continue

                # Only include actors with valid location
                if location and len(location) >= 3:
                    actors_with_location.append({
                        "name": actor_name,
                        "z": location[2]
                    })

        if not actors_with_location:
            return {
                "success": True,
                "message": f"No actors found{' with prefix ' + prefix if prefix else ''}",
                "total_actors": 0,
                "layers": [],
                "filter_prefix": prefix
            }

        # Find min and max Z coordinates
        z_coords = [actor["z"] for actor in actors_with_location]
        min_z = min(z_coords)
        max_z = max(z_coords)

        # Handle case where all actors are at the same height
        if min_z == max_z:
            return {
                "success": True,
                "message": f"All {len(actors_with_location)} actors at same height (Z={min_z})",
                "total_actors": len(actors_with_location),
                "layers": [{
                    "layer": 0,
                    "z_min": min_z,
                    "z_max": max_z,
                    "actor_count": len(actors_with_location),
                    "actor_names": [a["name"] for a in actors_with_location]
                }],
                "filter_prefix": prefix
            }

        # Calculate layer height
        layer_height = (max_z - min_z) / layers

        # Initialize layers
        layer_data = []
        for i in range(layers):
            layer_min = min_z + (i * layer_height)
            layer_max = min_z + ((i + 1) * layer_height)

            # For the last layer, extend to include max_z
            if i == layers - 1:
                layer_max = max_z + 0.01  # Small epsilon to include max_z

            layer_data.append({
                "layer": i,
                "z_min": round(layer_min, 2),
                "z_max": round(layer_max, 2),
                "actors": []
            })

        # Assign actors to layers
        for actor in actors_with_location:
            z = actor["z"]
            # Find which layer this actor belongs to
            layer_index = min(int((z - min_z) / layer_height), layers - 1)
            layer_data[layer_index]["actors"].append(actor["name"])

        # Format output
        result_layers = []
        for layer in layer_data:
            result_layers.append({
                "layer": layer["layer"],
                "z_min": layer["z_min"],
                "z_max": layer["z_max"],
                "actor_count": len(layer["actors"]),
                "actor_names": layer["actors"]
            })

        logger.info(f"Organized {len(actors_with_location)} actors into {layers} height layers{' with prefix ' + prefix if prefix else ''}")

        return {
            "success": True,
            "message": f"Organized {len(actors_with_location)} actors into {layers} layers",
            "total_actors": len(actors_with_location),
            "layers": result_layers,
            "filter_prefix": prefix,
            "height_range": {
                "min": round(min_z, 2),
                "max": round(max_z, 2),
                "span": round(max_z - min_z, 2)
            }
        }

    except Exception as e:
        logger.error(f"organize_actors_by_height error: {e}")
        return {"success": False, "message": str(e)}

# Essential Blueprint Tools for Physics Actors
@mcp.tool()
def create_blueprint(name: str, parent_class: str) -> Dict[str, Any]:
    """Create a new Blueprint class."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "name": name,
            "parent_class": parent_class
        }
        response = unreal.send_command("create_blueprint", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"create_blueprint error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def add_component_to_blueprint(
    blueprint_name: str,
    component_type: str,
    component_name: str,
    location: List[float] = [],
    rotation: List[float] = [],
    scale: List[float] = [],
    component_properties: Dict[str, Any] = {}
) -> Dict[str, Any]:
    """Add a component to a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_type": component_type,
            "component_name": component_name,
            "location": location,
            "rotation": rotation,
            "scale": scale,
            "component_properties": component_properties
        }
        response = unreal.send_command("add_component_to_blueprint", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"add_component_to_blueprint error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def set_static_mesh_properties(
    blueprint_name: str,
    component_name: str,
    static_mesh: str = "/Engine/BasicShapes/Cube.Cube"
) -> Dict[str, Any]:
    """Set static mesh properties on a StaticMeshComponent."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "static_mesh": static_mesh
        }
        response = unreal.send_command("set_static_mesh_properties", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"set_static_mesh_properties error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def set_physics_properties(
    blueprint_name: str,
    component_name: str,
    simulate_physics: bool = True,
    gravity_enabled: bool = True,
    mass: float = 1,
    linear_damping: float = 0.01,
    angular_damping: float = 0
) -> Dict[str, Any]:
    """Set physics properties on a component."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "simulate_physics": simulate_physics,
            "gravity_enabled": gravity_enabled,
            "mass": mass,
            "linear_damping": linear_damping,
            "angular_damping": angular_damping
        }
        response = unreal.send_command("set_physics_properties", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"set_physics_properties error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def compile_blueprint(blueprint_name: str) -> Dict[str, Any]:
    """Compile a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {"blueprint_name": blueprint_name}
        response = unreal.send_command("compile_blueprint", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"compile_blueprint error: {e}")
        return {"success": False, "message": str(e)}



# Advanced Composition Tools
@mcp.tool()
def create_pyramid(
    base_size: int = 3,
    block_size: float = 100.0,
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "PyramidBlock",
    mesh: str = "/Engine/BasicShapes/Cube.Cube"
) -> Dict[str, Any]:
    """Spawn a pyramid made of cube actors."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        spawned = []
        scale = block_size / 100.0
        for level in range(base_size):
            count = base_size - level
            for x in range(count):
                for y in range(count):
                    actor_name = f"{name_prefix}_{level}_{x}_{y}"
                    loc = [
                        location[0] + (x - (count - 1)/2) * block_size,
                        location[1] + (y - (count - 1)/2) * block_size,
                        location[2] + level * block_size
                    ]
                    params = {
                        "name": actor_name,
                        "type": "StaticMeshActor",
                        "location": loc,
                        "scale": [scale, scale, scale],
                        "static_mesh": mesh
                    }
                    resp = safe_spawn_actor(unreal, params)
                    if resp and resp.get("status") == "success":
                        spawned.append(resp)
        return {"success": True, "actors": spawned}
    except Exception as e:
        logger.error(f"create_pyramid error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_wall(
    length: int = 5,
    height: int = 2,
    block_size: float = 100.0,
    location: List[float] = [0.0, 0.0, 0.0],
    orientation: str = "x",
    name_prefix: str = "WallBlock",
    mesh: str = "/Engine/BasicShapes/Cube.Cube"
) -> Dict[str, Any]:
    """Create a simple wall from cubes."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        spawned = []
        scale = block_size / 100.0
        for h in range(height):
            for i in range(length):
                actor_name = f"{name_prefix}_{h}_{i}"
                if orientation == "x":
                    loc = [location[0] + i * block_size, location[1], location[2] + h * block_size]
                else:
                    loc = [location[0], location[1] + i * block_size, location[2] + h * block_size]
                params = {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": loc,
                    "scale": [scale, scale, scale],
                    "static_mesh": mesh
                }
                resp = safe_spawn_actor(unreal, params)
                if resp and resp.get("status") == "success":
                    spawned.append(resp)
        return {"success": True, "actors": spawned}
    except Exception as e:
        logger.error(f"create_wall error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_tower(
    height: int = 10,
    base_size: int = 4,
    block_size: float = 100.0,
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "TowerBlock",
    mesh: str = "/Engine/BasicShapes/Cube.Cube",
    tower_style: str = "cylindrical"  # "cylindrical", "square", "tapered"
) -> Dict[str, Any]:
    """Create a realistic tower with various architectural styles."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        spawned = []
        scale = block_size / 100.0

        for level in range(height):
            level_height = location[2] + level * block_size
            
            if tower_style == "cylindrical":
                # Create circular tower
                radius = (base_size / 2) * block_size  # Convert to world units (centimeters)
                circumference = 2 * math.pi * radius
                num_blocks = max(8, int(circumference / block_size))
                
                for i in range(num_blocks):
                    angle = (2 * math.pi * i) / num_blocks
                    x = location[0] + radius * math.cos(angle)
                    y = location[1] + radius * math.sin(angle)
                    
                    actor_name = f"{name_prefix}_{level}_{i}"
                    params = {
                        "name": actor_name,
                        "type": "StaticMeshActor",
                        "location": [x, y, level_height],
                        "scale": [scale, scale, scale],
                        "static_mesh": mesh
                    }
                    resp = safe_spawn_actor(unreal, params)
                    if resp and resp.get("status") == "success":
                        spawned.append(resp)
                        
            elif tower_style == "tapered":
                # Create tapering square tower
                current_size = max(1, base_size - (level // 2))
                half_size = current_size / 2
                
                # Create walls for current level
                for side in range(4):
                    for i in range(current_size):
                        if side == 0:  # Front wall
                            x = location[0] + (i - half_size + 0.5) * block_size
                            y = location[1] - half_size * block_size
                            actor_name = f"{name_prefix}_{level}_front_{i}"
                        elif side == 1:  # Right wall
                            x = location[0] + half_size * block_size
                            y = location[1] + (i - half_size + 0.5) * block_size
                            actor_name = f"{name_prefix}_{level}_right_{i}"
                        elif side == 2:  # Back wall
                            x = location[0] + (half_size - i - 0.5) * block_size
                            y = location[1] + half_size * block_size
                            actor_name = f"{name_prefix}_{level}_back_{i}"
                        else:  # Left wall
                            x = location[0] - half_size * block_size
                            y = location[1] + (half_size - i - 0.5) * block_size
                            actor_name = f"{name_prefix}_{level}_left_{i}"
                            
                        params = {
                            "name": actor_name,
                            "type": "StaticMeshActor",
                            "location": [x, y, level_height],
                            "scale": [scale, scale, scale],
                            "static_mesh": mesh
                        }
                        resp = unreal.send_command("spawn_actor", params)
                        if resp:
                            spawned.append(resp)
                            
            else:  # square tower
                # Create square tower walls
                half_size = base_size / 2
                
                # Four walls
                for side in range(4):
                    for i in range(base_size):
                        if side == 0:  # Front wall
                            x = location[0] + (i - half_size + 0.5) * block_size
                            y = location[1] - half_size * block_size
                            actor_name = f"{name_prefix}_{level}_front_{i}"
                        elif side == 1:  # Right wall
                            x = location[0] + half_size * block_size
                            y = location[1] + (i - half_size + 0.5) * block_size
                            actor_name = f"{name_prefix}_{level}_right_{i}"
                        elif side == 2:  # Back wall
                            x = location[0] + (half_size - i - 0.5) * block_size
                            y = location[1] + half_size * block_size
                            actor_name = f"{name_prefix}_{level}_back_{i}"
                        else:  # Left wall
                            x = location[0] - half_size * block_size
                            y = location[1] + (half_size - i - 0.5) * block_size
                            actor_name = f"{name_prefix}_{level}_left_{i}"
                            
                        params = {
                            "name": actor_name,
                            "type": "StaticMeshActor",
                            "location": [x, y, level_height],
                            "scale": [scale, scale, scale],
                            "static_mesh": mesh
                        }
                        resp = unreal.send_command("spawn_actor", params)
                        if resp:
                            spawned.append(resp)
                            
            # Add decorative elements every few levels
            if level % 3 == 2 and level < height - 1:
                # Add corner details
                for corner in range(4):
                    angle = corner * math.pi / 2
                    detail_x = location[0] + (base_size/2 + 0.5) * block_size * math.cos(angle)
                    detail_y = location[1] + (base_size/2 + 0.5) * block_size * math.sin(angle)
                    
                    actor_name = f"{name_prefix}_{level}_detail_{corner}"
                    params = {
                        "name": actor_name,
                        "type": "StaticMeshActor",
                        "location": [detail_x, detail_y, level_height],
                        "scale": [scale * 0.7, scale * 0.7, scale * 0.7],
                        "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                    }
                    resp = safe_spawn_actor(unreal, params)
                    if resp and resp.get("status") == "success":
                        spawned.append(resp)
                        
        return {"success": True, "actors": spawned, "tower_style": tower_style}
    except Exception as e:
        logger.error(f"create_tower error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_staircase(
    steps: int = 5,
    step_size: List[float] = [100.0, 100.0, 50.0],
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "Stair",
    mesh: str = "/Engine/BasicShapes/Cube.Cube"
) -> Dict[str, Any]:
    """Create a staircase from cubes."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        spawned = []
        sx, sy, sz = step_size
        for i in range(steps):
            actor_name = f"{name_prefix}_{i}"
            loc = [location[0] + i * sx, location[1], location[2] + i * sz]
            scale = [sx/100.0, sy/100.0, sz/100.0]
            params = {
                "name": actor_name,
                "type": "StaticMeshActor",
                "location": loc,
                "scale": scale,
                "static_mesh": mesh
            }
            resp = safe_spawn_actor(unreal, params)
            if resp and resp.get("status") == "success":
                spawned.append(resp)
        return {"success": True, "actors": spawned}
    except Exception as e:
        logger.error(f"create_staircase error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def construct_house(
    width: int = 1200,
    depth: int = 1000,
    height: int = 600,
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "House",
    mesh: str = "/Engine/BasicShapes/Cube.Cube",
    house_style: str = "modern"  # "modern", "cottage"
) -> Dict[str, Any]:
    """Construct a realistic house with architectural details and multiple rooms."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        # Use the helper function to build the house
        return build_house(unreal, width, depth, height, location, name_prefix, mesh, house_style)

    except Exception as e:
        logger.error(f"construct_house error: {e}")
        return {"success": False, "message": str(e)}



@mcp.tool()
def construct_mansion(
    mansion_scale: str = "large",  # "small", "large", "epic", "legendary"
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "Mansion"
) -> Dict[str, Any]:
    """
    Construct a magnificent mansion with multiple wings, grand rooms, gardens,
    fountains, and luxury features perfect for dramatic TikTok reveals.
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Creating {mansion_scale} mansion")
        all_actors = []

        # Get size parameters and calculate scaled dimensions
        params = get_mansion_size_params(mansion_scale)
        layout = calculate_mansion_layout(params)

        # Build mansion main structure
        build_mansion_main_structure(unreal, name_prefix, location, layout, all_actors)

        # Build mansion exterior
        build_mansion_exterior(unreal, name_prefix, location, layout, all_actors)

        # Add luxurious interior
        add_mansion_interior(unreal, name_prefix, location, layout, all_actors)

        logger.info(f"Mansion construction complete! Created {len(all_actors)} elements")

        return {
            "success": True,
            "message": f"Magnificent {mansion_scale} mansion created with {len(all_actors)} elements!",
            "actors": all_actors,
            "stats": {
                "scale": mansion_scale,
                "wings": layout["wings"],
                "floors": layout["floors"],
                "main_rooms": layout["main_rooms"],
                "bedrooms": layout["bedrooms"],
                "garden_size": layout["garden_size"],
                "fountain_count": layout["fountain_count"],
                "car_count": layout["car_count"],
                "total_actors": len(all_actors)
            }
        }

    except Exception as e:
        logger.error(f"construct_mansion error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_arch(
    radius: float = 300.0,
    segments: int = 6,
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "ArchBlock",
    mesh: str = "/Engine/BasicShapes/Cube.Cube"
) -> Dict[str, Any]:
    """Create a simple arch using cubes in a semicircle."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        spawned = []
        angle_step = math.pi / segments
        scale = radius / 300.0 / 2
        for i in range(segments + 1):
            theta = angle_step * i
            x = radius * math.cos(theta)
            z = radius * math.sin(theta)
            actor_name = f"{name_prefix}_{i}"
            params = {
                "name": actor_name,
                "type": "StaticMeshActor",
                "location": [location[0] + x, location[1], location[2] + z],
                "scale": [scale, scale, scale],
                "static_mesh": mesh
            }
            resp = safe_spawn_actor(unreal, params)
            if resp and resp.get("status") == "success":
                spawned.append(resp)
        return {"success": True, "actors": spawned}
    except Exception as e:
        logger.error(f"create_arch error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def spawn_physics_blueprint_actor (
    name: str,
    mesh_path: str = "/Engine/BasicShapes/Cube.Cube",
    location: List[float] = [0.0, 0.0, 0.0],
    mass: float = 1.0,
    simulate_physics: bool = True,
    gravity_enabled: bool = True,
    color: List[float] = None,  # Optional color parameter [R, G, B] or [R, G, B, A]
    scale: List[float] = [1.0, 1.0, 1.0]  # Default scale
) -> Dict[str, Any]:
    """
    Quickly spawn a single actor with physics, color, and a specific mesh.

    This is the primary function for creating simple objects with physics properties.
    It handles creating a temporary Blueprint, setting up the mesh, color, and physics,
    and then spawns the actor in the world. It's ideal for quickly adding
    dynamic objects to the scene without needing to manually create Blueprints.
    
    Args:
        color: Optional color as [R, G, B] or [R, G, B, A] where values are 0.0-1.0.
               If [R, G, B] is provided, alpha will be set to 1.0 automatically.
    """
    try:
        bp_name = f"{name}_BP"
        create_blueprint(bp_name, "Actor")
        add_component_to_blueprint(bp_name, "StaticMeshComponent", "Mesh", scale=scale)
        set_static_mesh_properties(bp_name, "Mesh", mesh_path)
        set_physics_properties(bp_name, "Mesh", simulate_physics, gravity_enabled, mass)

        # Set color if provided
        if color is not None:
            # Convert 3-value color [R,G,B] to 4-value [R,G,B,A] if needed
            if len(color) == 3:
                color = color + [1.0]  # Add alpha=1.0
            elif len(color) != 4:
                logger.warning(f"Invalid color format: {color}. Expected [R,G,B] or [R,G,B,A]. Skipping color.")
                color = None

            if color is not None:
                color_result = set_mesh_material_color(bp_name, "Mesh", color)
                if not color_result.get("success", False):
                    logger.warning(f"Failed to set color {color} for {bp_name}: {color_result.get('message', 'Unknown error')}")

        compile_blueprint(bp_name)
        result = spawn_blueprint_actor(bp_name, name, location)
        
        # Spawn the blueprint actor using helper function
        unreal = get_unreal_connection()
        result = spawn_blueprint_actor(unreal, bp_name, name, location)

        # Ensure proper scale is set on the spawned actor
        if result.get("success", False):
            spawned_name = result.get("result", {}).get("name", name)
            set_actor_transform(spawned_name, scale=scale)

        return result
    except Exception as e:
        logger.error(f"spawn_physics_blueprint_actor  error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_maze(
    rows: int = 8,
    cols: int = 8,
    cell_size: float = 300.0,
    wall_height: int = 3,
    location: List[float] = [0.0, 0.0, 0.0]
) -> Dict[str, Any]:
    """Create a proper solvable maze with entrance, exit, and guaranteed path using recursive backtracking algorithm."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
        spawned = []
        
        # Initialize maze grid - True means wall, False means open
        maze = [[True for _ in range(cols * 2 + 1)] for _ in range(rows * 2 + 1)]
        
        # Recursive backtracking maze generation
        def carve_path(row, col):
            # Mark current cell as path
            maze[row * 2 + 1][col * 2 + 1] = False
            
            # Random directions
            directions = [(0, 1), (1, 0), (0, -1), (-1, 0)]
            random.shuffle(directions)
            
            for dr, dc in directions:
                new_row, new_col = row + dr, col + dc
                
                # Check bounds
                if (0 <= new_row < rows and 0 <= new_col < cols and 
                    maze[new_row * 2 + 1][new_col * 2 + 1]):
                    
                    # Carve wall between current and new cell
                    maze[row * 2 + 1 + dr][col * 2 + 1 + dc] = False
                    carve_path(new_row, new_col)
        
        # Start carving from top-left corner
        carve_path(0, 0)
        
        # Create entrance and exit
        maze[1][0] = False  # Entrance on left side
        maze[rows * 2 - 1][cols * 2] = False  # Exit on right side
        
        # Build the actual maze in Unreal
        maze_height = rows * 2 + 1
        maze_width = cols * 2 + 1
        
        for r in range(maze_height):
            for c in range(maze_width):
                if maze[r][c]:  # If this is a wall
                    # Stack blocks to create wall height
                    for h in range(wall_height):
                        x_pos = location[0] + (c - maze_width/2) * cell_size
                        y_pos = location[1] + (r - maze_height/2) * cell_size
                        z_pos = location[2] + h * cell_size
                        
                        actor_name = f"Maze_Wall_{r}_{c}_{h}"
                        params = {
                            "name": actor_name,
                            "type": "StaticMeshActor",
                            "location": [x_pos, y_pos, z_pos],
                            "scale": [cell_size/100.0, cell_size/100.0, cell_size/100.0],
                            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                        }
                        resp = safe_spawn_actor(unreal, params)
                        if resp and resp.get("status") == "success":
                            spawned.append(resp)
        
        # Add entrance and exit markers
        entrance_marker = safe_spawn_actor(unreal, {
            "name": "Maze_Entrance",
            "type": "StaticMeshActor",
            "location": [location[0] - maze_width/2 * cell_size - cell_size, 
                       location[1] + (-maze_height/2 + 1) * cell_size, 
                       location[2] + cell_size],
            "scale": [0.5, 0.5, 0.5],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if entrance_marker and entrance_marker.get("status") == "success":
            spawned.append(entrance_marker)
            
        exit_marker = safe_spawn_actor(unreal, {
            "name": "Maze_Exit",
            "type": "StaticMeshActor", 
            "location": [location[0] + maze_width/2 * cell_size + cell_size,
                       location[1] + (-maze_height/2 + rows * 2 - 1) * cell_size,
                       location[2] + cell_size],
            "scale": [0.5, 0.5, 0.5],
            "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
        })
        if exit_marker and exit_marker.get("status") == "success":
            spawned.append(exit_marker)
        
        return {
            "success": True, 
            "actors": spawned, 
            "maze_size": f"{rows}x{cols}",
            "wall_count": len([block for block in spawned if "Wall" in block.get("name", "")]),
            "entrance": "Left side (cylinder marker)",
            "exit": "Right side (sphere marker)"
        }
    except Exception as e:
        logger.error(f"create_maze error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def get_available_materials(
    search_path: str = "/Game/",
    include_engine_materials: bool = True
) -> Dict[str, Any]:
    """Get a list of available materials in the project that can be applied to objects."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "search_path": search_path,
            "include_engine_materials": include_engine_materials
        }
        response = unreal.send_command("get_available_materials", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"get_available_materials error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def apply_material_to_actor(
    actor_name: str,
    material_path: str,
    material_slot: int = 0
) -> Dict[str, Any]:
    """Apply a specific material to an actor in the level."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "actor_name": actor_name,
            "material_path": material_path,
            "material_slot": material_slot
        }
        response = unreal.send_command("apply_material_to_actor", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"apply_material_to_actor error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def apply_material_to_blueprint(
    blueprint_name: str,
    component_name: str,
    material_path: str,
    material_slot: int = 0
) -> Dict[str, Any]:
    """Apply a specific material to a component in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "material_path": material_path,
            "material_slot": material_slot
        }
        response = unreal.send_command("apply_material_to_blueprint", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"apply_material_to_blueprint error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def get_actor_material_info(
    actor_name: str
) -> Dict[str, Any]:
    """Get information about the materials currently applied to an actor."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {"actor_name": actor_name}
        response = unreal.send_command("get_actor_material_info", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"get_actor_material_info error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def set_mesh_material_color(
    blueprint_name: str,
    component_name: str,
    color: List[float],
    material_path: str = "/Engine/BasicShapes/BasicShapeMaterial",
    parameter_name: str = "BaseColor",
    material_slot: int = 0
) -> Dict[str, Any]:
    """Set material color on a mesh component using the proven color system."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        # Validate color format
        if not isinstance(color, list) or len(color) != 4:
            return {"success": False, "message": "Invalid color format. Must be a list of 4 float values [R, G, B, A]."}
        
        # Ensure all color values are floats between 0 and 1
        color = [float(min(1.0, max(0.0, val))) for val in color]
        
        # Set BaseColor parameter first
        params_base = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "color": color,
            "material_path": material_path,
            "parameter_name": "BaseColor",
            "material_slot": material_slot
        }
        response_base = unreal.send_command("set_mesh_material_color", params_base)
        
        # Set Color parameter second (for maximum compatibility)
        params_color = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "color": color,
            "material_path": material_path,
            "parameter_name": "Color",
            "material_slot": material_slot
        }
        response_color = unreal.send_command("set_mesh_material_color", params_color)
        
        # Return success if either parameter setting worked
        if (response_base and response_base.get("status") == "success") or (response_color and response_color.get("status") == "success"):
            return {
                "success": True, 
                "message": f"Color applied successfully to slot {material_slot}: {color}",
                "base_color_result": response_base,
                "color_result": response_color,
                "material_slot": material_slot
            }
        else:
            return {
                "success": False, 
                "message": f"Failed to set color parameters on slot {material_slot}. BaseColor: {response_base}, Color: {response_color}"
            }
            
    except Exception as e:
        logger.error(f"set_mesh_material_color error: {e}")
        return {"success": False, "message": str(e)}

# Advanced Town Generation System
@mcp.tool()
def create_town(
    town_size: str = "medium",  # "small", "medium", "large", "metropolis"
    building_density: float = 0.7,  # 0.0 to 1.0
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "Town",
    include_infrastructure: bool = True,
    architectural_style: str = "mixed"  # "modern", "cottage", "mansion", "mixed", "downtown", "futuristic"
) -> Dict[str, Any]:
    """Create a full dynamic town with buildings, streets, infrastructure, and vehicles."""
    try:
        random.seed()  # Use different seed each time for variety
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Creating {town_size} town with {building_density} density at {location}")
        
        # Define town parameters based on size
        town_params = {
            "small": {"blocks": 3, "block_size": 1500, "max_building_height": 5, "population": 20, "skyscraper_chance": 0.1},
            "medium": {"blocks": 5, "block_size": 2000, "max_building_height": 10, "population": 50, "skyscraper_chance": 0.3},
            "large": {"blocks": 7, "block_size": 2500, "max_building_height": 20, "population": 100, "skyscraper_chance": 0.5},
            "metropolis": {"blocks": 10, "block_size": 3000, "max_building_height": 40, "population": 200, "skyscraper_chance": 0.7}
        }
        
        params = town_params.get(town_size, town_params["medium"])
        blocks = params["blocks"]
        block_size = params["block_size"]
        max_height = params["max_building_height"]
        target_population = int(params["population"] * building_density)
        skyscraper_chance = params["skyscraper_chance"]
        
        all_spawned = []
        street_width = block_size * 0.3
        building_area = block_size * 0.7
        
        # Create street grid first
        logger.info("Creating street grid...")
        street_results = _create_street_grid(blocks, block_size, street_width, location, name_prefix)
        all_spawned.extend(street_results.get("actors", []))
        
        # Create buildings in each block
        logger.info("Placing buildings...")
        building_count = 0
        for block_x in range(blocks):
            for block_y in range(blocks):
                if building_count >= target_population:
                    break
                    
                # Skip some blocks randomly for variety
                if random.random() > building_density:
                    continue
                
                block_center_x = location[0] + (block_x - blocks/2) * block_size
                block_center_y = location[1] + (block_y - blocks/2) * block_size
                
                # Randomly choose building type based on style and location
                if architectural_style == "downtown" or architectural_style == "futuristic":
                    building_types = ["skyscraper", "office_tower", "apartment_complex", "shopping_mall", "parking_garage", "hotel"]
                elif architectural_style == "mixed":
                    # Central blocks get taller buildings
                    is_central = abs(block_x - blocks//2) <= 1 and abs(block_y - blocks//2) <= 1
                    if is_central and random.random() < skyscraper_chance:
                        building_types = ["skyscraper", "office_tower", "apartment_complex", "hotel", "shopping_mall"]
                    else:
                        building_types = ["house", "tower", "mansion", "commercial", "apartment_building", "restaurant", "store"]
                else:
                    building_types = [architectural_style] * 3 + ["commercial", "restaurant", "store"]
                
                building_type = random.choice(building_types)
                
                # Create building with variety
                building_result = _create_town_building(
                    building_type, 
                    [block_center_x, block_center_y, location[2]],
                    building_area,
                    max_height,
                    f"{name_prefix}_Building_{block_x}_{block_y}",
                    building_count
                )
                
                if building_result.get("status") == "success":
                    all_spawned.extend(building_result.get("actors", []))
                    building_count += 1
        
        # Add infrastructure if requested
        infrastructure_count = 0
        if include_infrastructure:
            logger.info("Adding infrastructure...")
            
            # Street lights
            light_results = _create_street_lights(blocks, block_size, location, name_prefix)
            all_spawned.extend(light_results.get("actors", []))
            infrastructure_count += len(light_results.get("actors", []))
            
            # Vehicles
            vehicle_results = _create_town_vehicles(blocks, block_size, street_width, location, name_prefix, target_population // 3)
            all_spawned.extend(vehicle_results.get("actors", []))
            infrastructure_count += len(vehicle_results.get("actors", []))
            
            # Parks and decorations
            decoration_results = _create_town_decorations(blocks, block_size, location, name_prefix)
            all_spawned.extend(decoration_results.get("actors", []))
            infrastructure_count += len(decoration_results.get("actors", []))
            
            
            # Add advanced infrastructure
            logger.info("Adding advanced infrastructure...")
            
            # Traffic lights at intersections
            traffic_results = _create_traffic_lights(blocks, block_size, location, name_prefix)
            all_spawned.extend(traffic_results.get("actors", []))
            infrastructure_count += len(traffic_results.get("actors", []))
            
            # Street signs and billboards
            signage_results = _create_street_signage(blocks, block_size, location, name_prefix, town_size)
            all_spawned.extend(signage_results.get("actors", []))
            infrastructure_count += len(signage_results.get("actors", []))
            
            # Sidewalks and crosswalks
            sidewalk_results = _create_sidewalks_crosswalks(blocks, block_size, street_width, location, name_prefix)
            all_spawned.extend(sidewalk_results.get("actors", []))
            infrastructure_count += len(sidewalk_results.get("actors", []))
            
            # Urban furniture (benches, trash cans, bus stops)
            furniture_results = _create_urban_furniture(blocks, block_size, location, name_prefix)
            all_spawned.extend(furniture_results.get("actors", []))
            infrastructure_count += len(furniture_results.get("actors", []))
            
            # Parking meters and hydrants
            utility_results = _create_street_utilities(blocks, block_size, location, name_prefix)
            all_spawned.extend(utility_results.get("actors", []))
            infrastructure_count += len(utility_results.get("actors", []))
            
            # Add plaza/square in center for large towns
            if town_size in ["large", "metropolis"]:
                plaza_results = _create_central_plaza(blocks, block_size, location, name_prefix)
                all_spawned.extend(plaza_results.get("actors", []))
                infrastructure_count += len(plaza_results.get("actors", []))
        
        return {
            "success": True,
            "town_stats": {
                "size": town_size,
                "density": building_density,
                "blocks": blocks,
                "buildings": building_count,
                "infrastructure_items": infrastructure_count,
                "total_actors": len(all_spawned),
                "architectural_style": architectural_style
            },
            "actors": all_spawned,
            "message": f"Created {town_size} town with {building_count} buildings and {infrastructure_count} infrastructure items"
        }
        
    except Exception as e:
        logger.error(f"create_town error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def create_castle_fortress(
    castle_size: str = "large",  # "small", "medium", "large", "epic"
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "Castle",
    include_siege_weapons: bool = True,
    include_village: bool = True,
    architectural_style: str = "medieval"  # "medieval", "fantasy", "gothic"
) -> Dict[str, Any]:
    """
    Create a massive castle fortress with walls, towers, courtyards, throne room,
    and surrounding village. Perfect for dramatic TikTok reveals showing
    the scale and detail of a complete medieval fortress.
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Creating {castle_size} {architectural_style} castle fortress")
        all_actors = []
        
        # Get size parameters and calculate scaled dimensions
        params = get_castle_size_params(castle_size)
        dimensions = calculate_scaled_dimensions(params, scale_factor=2.0)
        
        # Build castle components using helper functions
        build_outer_bailey_walls(unreal, name_prefix, location, dimensions, all_actors)
        build_inner_bailey_walls(unreal, name_prefix, location, dimensions, all_actors)
        build_gate_complex(unreal, name_prefix, location, dimensions, all_actors)
        build_corner_towers(unreal, name_prefix, location, dimensions, architectural_style, all_actors)
        build_inner_corner_towers(unreal, name_prefix, location, dimensions, all_actors)
        build_intermediate_towers(unreal, name_prefix, location, dimensions, all_actors)
        build_central_keep(unreal, name_prefix, location, dimensions, all_actors)
        build_courtyard_complex(unreal, name_prefix, location, dimensions, all_actors)
        build_bailey_annexes(unreal, name_prefix, location, dimensions, all_actors)
        
        # Add optional components
        if include_siege_weapons:
            build_siege_weapons(unreal, name_prefix, location, dimensions, all_actors)
        
        if include_village:
            build_village_settlement(unreal, name_prefix, location, dimensions, castle_size, all_actors)
        
        # Add final touches
        build_drawbridge_and_moat(unreal, name_prefix, location, dimensions, all_actors)
        add_decorative_flags(unreal, name_prefix, location, dimensions, all_actors)
        
        logger.info(f"Castle fortress creation complete! Created {len(all_actors)} actors")

        
        return {
            "success": True,
            "message": f"Epic {castle_size} {architectural_style} castle fortress created with {len(all_actors)} elements!",
            "actors": all_actors,
            "stats": {
                "size": castle_size,
                "style": architectural_style,
                "wall_sections": int(dimensions["outer_width"]/200) * 2 + int(dimensions["outer_depth"]/200) * 2,
                "towers": dimensions["tower_count"],
                "has_village": include_village,
                "has_siege_weapons": include_siege_weapons,
                "total_actors": len(all_actors)
            }
        }
        
    except Exception as e:
        logger.error(f"create_castle_fortress error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_suspension_bridge(
    span_length: float = 6000.0,
    deck_width: float = 800.0,
    tower_height: float = 4000.0,
    cable_sag_ratio: float = 0.12,
    module_size: float = 200.0,
    location: List[float] = [0.0, 0.0, 0.0],
    orientation: str = "x",
    name_prefix: str = "Bridge",
    deck_mesh: str = "/Engine/BasicShapes/Cube.Cube",
    tower_mesh: str = "/Engine/BasicShapes/Cube.Cube",
    cable_mesh: str = "/Engine/BasicShapes/Cylinder.Cylinder",
    suspender_mesh: str = "/Engine/BasicShapes/Cylinder.Cylinder",
    dry_run: bool = False
) -> Dict[str, Any]:
    """
    Build a suspension bridge with towers, deck, cables, and suspenders.
    
    Creates a realistic suspension bridge with parabolic main cables, vertical
    suspenders, twin towers, and a multi-lane deck. Perfect for dramatic reveals
    showing engineering marvels.
    
    Args:
        span_length: Total span between towers
        deck_width: Width of the bridge deck
        tower_height: Height of support towers
        cable_sag_ratio: Sag as fraction of span (0.1-0.15 typical)
        module_size: Resolution for segments (affects actor count)
        location: Center point of the bridge
        orientation: "x" or "y" for bridge direction
        name_prefix: Prefix for all spawned actors
        deck_mesh: Mesh for deck segments
        tower_mesh: Mesh for tower components
        cable_mesh: Mesh for cable segments
        suspender_mesh: Mesh for vertical suspenders
        dry_run: If True, calculate metrics without spawning
    
    Returns:
        Dictionary with success status, spawned actors, and performance metrics
    """
    try:
        start_time = time.perf_counter()
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Creating suspension bridge: span={span_length}, width={deck_width}, height={tower_height}")
        
        all_actors = []
        
        # Calculate expected actor counts for dry run
        if dry_run:
            expected_towers = 10  # 2 towers with main, base, top, and 2 attachment points each
            expected_deck = max(1, int(span_length / module_size)) * max(1, int(deck_width / module_size))
            expected_cables = 2 * max(1, int(span_length / module_size))  # 2 main cables
            expected_suspenders = 2 * max(1, int(span_length / (module_size * 3)))  # Every 3 modules
            
            elapsed_ms = int((time.perf_counter() - start_time) * 1000)
            
            return {
                "success": True,
                "dry_run": True,
                "metrics": {
                    "total_actors": expected_towers + expected_deck + expected_cables + expected_suspenders,
                    "deck_segments": expected_deck,
                    "cable_segments": expected_cables,
                    "suspender_count": expected_suspenders,
                    "towers": expected_towers,
                    "span_length": span_length,
                    "deck_width": deck_width,
                    "est_area": span_length * deck_width,
                    "elapsed_ms": elapsed_ms
                }
            }
        
        # Build the bridge structure
        counts = build_suspension_bridge_structure(
            unreal,
            span_length,
            deck_width,
            tower_height,
            cable_sag_ratio,
            module_size,
            location,
            orientation,
            name_prefix,
            deck_mesh,
            tower_mesh,
            cable_mesh,
            suspender_mesh,
            all_actors
        )
        
        # Calculate metrics
        elapsed_ms = int((time.perf_counter() - start_time) * 1000)
        total_actors = sum(counts.values())
        
        logger.info(f"Bridge construction complete: {total_actors} actors in {elapsed_ms}ms")
        
        return {
            "success": True,
            "message": f"Created suspension bridge with {total_actors} components",
            "actors": all_actors,
            "metrics": {
                "total_actors": total_actors,
                "deck_segments": counts["deck_segments"],
                "cable_segments": counts["cable_segments"],
                "suspender_count": counts["suspenders"],
                "towers": counts["towers"],
                "span_length": span_length,
                "deck_width": deck_width,
                "est_area": span_length * deck_width,
                "elapsed_ms": elapsed_ms
            }
        }
        
    except Exception as e:
        logger.error(f"create_suspension_bridge error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_aqueduct(
    arches: int = 18,
    arch_radius: float = 600.0,
    pier_width: float = 200.0,
    tiers: int = 2,
    deck_width: float = 600.0,
    module_size: float = 200.0,
    location: List[float] = [0.0, 0.0, 0.0],
    orientation: str = "x",
    name_prefix: str = "Aqueduct",
    arch_mesh: str = "/Engine/BasicShapes/Cylinder.Cylinder",
    pier_mesh: str = "/Engine/BasicShapes/Cube.Cube",
    deck_mesh: str = "/Engine/BasicShapes/Cube.Cube",
    dry_run: bool = False
) -> Dict[str, Any]:
    """
    Build a multi-tier Roman-style aqueduct with arches and water channel.

    Creates a majestic aqueduct with repeating arches, support piers, and
    a water channel deck. Each tier has progressively smaller piers for
    realistic tapering. Perfect for showing ancient engineering.

    Args:
        arches: Number of arches per tier
        arch_radius: Radius of each arch
        pier_width: Width of support piers
        tiers: Number of vertical tiers (1-3 recommended)
        deck_width: Width of the water channel
        module_size: Resolution for segments (affects actor count)
        location: Starting point of the aqueduct
        orientation: "x" or "y" for aqueduct direction
        name_prefix: Prefix for all spawned actors
        arch_mesh: Mesh for arch segments (cylinder)
        pier_mesh: Mesh for support piers
        deck_mesh: Mesh for deck and walls
        dry_run: If True, calculate metrics without spawning

    Returns:
        Dictionary with success status, spawned actors, and performance metrics
    """
    try:
        start_time = time.perf_counter()

        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Creating aqueduct: {arches} arches, {tiers} tiers, radius={arch_radius}")

        all_actors = []

        # Calculate dimensions
        total_length = arches * (2 * arch_radius + pier_width) + pier_width

        # Calculate expected actor counts for dry run
        if dry_run:
            # Arch segments per arch based on semicircle circumference
            arch_circumference = math.pi * arch_radius
            segments_per_arch = max(4, int(arch_circumference / module_size))
            expected_arch_segments = tiers * arches * segments_per_arch

            # Piers: (arches + 1) per tier
            expected_piers = tiers * (arches + 1)

            # Deck segments including side walls
            deck_length_segments = max(1, int(total_length / module_size))
            deck_width_segments = max(1, int(deck_width / module_size))
            expected_deck = deck_length_segments * deck_width_segments
            expected_deck += 2 * deck_length_segments  # Side walls

            elapsed_ms = int((time.perf_counter() - start_time) * 1000)

            return {
                "success": True,
                "dry_run": True,
                "metrics": {
                    "total_actors": expected_arch_segments + expected_piers + expected_deck,
                    "arch_segments": expected_arch_segments,
                    "pier_count": expected_piers,
                    "tiers": tiers,
                    "deck_segments": expected_deck,
                    "total_length": total_length,
                    "est_area": total_length * deck_width,
                    "elapsed_ms": elapsed_ms
                }
            }

        # Build the aqueduct structure
        counts = build_aqueduct_structure(
            unreal,
            arches,
            arch_radius,
            pier_width,
            tiers,
            deck_width,
            module_size,
            location,
            orientation,
            name_prefix,
            arch_mesh,
            pier_mesh,
            deck_mesh,
            all_actors
        )

        # Calculate metrics
        elapsed_ms = int((time.perf_counter() - start_time) * 1000)
        total_actors = sum(counts.values())

        logger.info(f"Aqueduct construction complete: {total_actors} actors in {elapsed_ms}ms")

        return {
            "success": True,
            "message": f"Created {tiers}-tier aqueduct with {arches} arches ({total_actors} components)",
            "actors": all_actors,
            "metrics": {
                "total_actors": total_actors,
                "arch_segments": counts["arch_segments"],
                "pier_count": counts["piers"],
                "tiers": tiers,
                "deck_segments": counts["deck_segments"],
                "total_length": total_length,
                "est_area": total_length * deck_width,
                "elapsed_ms": elapsed_ms
            }
        }

    except Exception as e:
        logger.error(f"create_aqueduct error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_church(
    church_size: str = "medium",  # "small", "medium", "large", "cathedral"
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "Church",
    architectural_style: str = "gothic",  # "gothic", "romanesque", "baroque", "modern"
    include_spire: bool = True,
    include_flying_buttresses: bool = True
) -> Dict[str, Any]:
    """
    Create a detailed church with various architectural styles and features.

    Creates an immersive church building with nave, transept, apse, and style-specific
    elements like gothic pointed arches, tall spires, flying buttresses, and rose windows.
    Perfect for medieval towns, fantasy settings, and architectural showcases.

    Args:
        church_size: Size of the church ("small", "medium", "large", "cathedral")
        location: Base point of the church
        name_prefix: Prefix for all spawned actors
        architectural_style: Visual style ("gothic", "romanesque", "baroque", "modern")
        include_spire: Add a tall spire/steeple
        include_flying_buttresses: Add flying buttresses (gothic style)

    Returns:
        Dictionary with success status, spawned actors, and church statistics
    """
    try:

        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Creating {church_size} {architectural_style} church at {location}")

        # Define church parameters based on size
        church_params = {
            "small": {"nave_length": 1200, "nave_width": 600, "nave_height": 800, "tower_height": 1200},
            "medium": {"nave_length": 2000, "nave_width": 800, "nave_height": 1200, "tower_height": 1800},
            "large": {"nave_length": 3000, "nave_width": 1200, "nave_height": 1600, "tower_height": 2400},
            "cathedral": {"nave_length": 4500, "nave_width": 1800, "nave_height": 2400, "tower_height": 3600}
        }

        params = church_params.get(church_size, church_params["medium"])
        nave_length = params["nave_length"]
        nave_width = params["nave_width"]
        nave_height = params["nave_height"]
        tower_height = params["tower_height"]

        all_actors = []
        wall_thickness = 100
        block_size = 100

        # Gothic style specific parameters
        if architectural_style == "gothic":
            arch_height_ratio = 1.8  # Tall pointed arches
            window_style = "pointed"
            buttress_projection = 300
        elif architectural_style == "romanesque":
            arch_height_ratio = 1.2  # Round arches
            window_style = "round"
            buttress_projection = 200
        elif architectural_style == "baroque":
            arch_height_ratio = 1.5
            window_style = "ornate"
            buttress_projection = 250
        else:  # modern
            arch_height_ratio = 1.0
            window_style = "simple"
            buttress_projection = 150

        logger.info(f"Building nave structure...")

        # Build nave (main body of church)
        # Floor
        floor_segments_x = int(nave_length / block_size)
        floor_segments_y = int(nave_width / block_size)
        for i in range(floor_segments_x):
            for j in range(floor_segments_y):
                floor_name = f"{name_prefix}_Floor_{i}_{j}"
                params_floor = {
                    "name": floor_name,
                    "type": "StaticMeshActor",
                    "location": [
                        location[0] + i * block_size - nave_length/2,
                        location[1] + j * block_size - nave_width/2,
                        location[2]
                    ],
                    "scale": [1, 1, 0.2],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                }
                floor_resp = safe_spawn_actor(unreal, params_floor)
                if floor_resp and floor_resp.get("status") == "success":
                    all_actors.append(floor_resp)

        # Build nave walls with gothic features
        wall_height_blocks = int(nave_height / block_size)

        # Long walls (left and right)
        for side in [-1, 1]:  # Left and right
            y_pos = location[1] + side * (nave_width/2 + wall_thickness/2)
            for i in range(floor_segments_x):
                x_pos = location[0] + i * block_size - nave_length/2

                # Create wall segments with window openings
                for h in range(wall_height_blocks):
                    z_pos = location[2] + h * block_size + block_size/2

                    # Create windows in upper sections (gothic style)
                    is_window_section = h >= wall_height_blocks // 3 and i % 3 == 1

                    if not is_window_section:
                        wall_name = f"{name_prefix}_NaveWall_{side}_{i}_{h}"
                        params_wall = {
                            "name": wall_name,
                            "type": "StaticMeshActor",
                            "location": [x_pos, y_pos, z_pos],
                            "scale": [1, wall_thickness/100, 1],
                            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                        }
                        wall_resp = safe_spawn_actor(unreal, params_wall)
                        if wall_resp and wall_resp.get("status") == "success":
                            all_actors.append(wall_resp)
                    else:
                        # Create pointed arch window (gothic style)
                        if architectural_style == "gothic":
                            # Window frame sides
                            for window_side in [-0.3, 0.3]:
                                frame_name = f"{name_prefix}_WindowFrame_{side}_{i}_{h}_{window_side}"
                                params_frame = {
                                    "name": frame_name,
                                    "type": "StaticMeshActor",
                                    "location": [x_pos + window_side * block_size, y_pos, z_pos],
                                    "scale": [0.2, wall_thickness/100, 1],
                                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                                }
                                frame_resp = safe_spawn_actor(unreal, params_frame)
                                if frame_resp and frame_resp.get("status") == "success":
                                    all_actors.append(frame_resp)

                            # Pointed arch top
                            arch_top_name = f"{name_prefix}_WindowArch_{side}_{i}_{h}"
                            params_arch = {
                                "name": arch_top_name,
                                "type": "StaticMeshActor",
                                "location": [x_pos, y_pos, z_pos + block_size/2],
                                "scale": [0.5, wall_thickness/100, 0.3],
                                "rotation": [0, 0, 45],
                                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                            }
                            arch_resp = safe_spawn_actor(unreal, params_arch)
                            if arch_resp and arch_resp.get("status") == "success":
                                all_actors.append(arch_resp)

        # Front and back walls
        for end in [-1, 1]:  # Front and back
            x_pos = location[0] + end * (nave_length/2 + wall_thickness/2)
            for j in range(floor_segments_y):
                y_pos = location[1] + j * block_size - nave_width/2

                for h in range(wall_height_blocks):
                    z_pos = location[2] + h * block_size + block_size/2

                    # Create rose window at front (gothic feature)
                    is_rose_window = (end == -1 and architectural_style == "gothic" and
                                     abs(j - floor_segments_y//2) <= 2 and
                                     h >= wall_height_blocks * 0.6)

                    if not is_rose_window:
                        wall_name = f"{name_prefix}_EndWall_{end}_{j}_{h}"
                        params_wall = {
                            "name": wall_name,
                            "type": "StaticMeshActor",
                            "location": [x_pos, y_pos, z_pos],
                            "scale": [wall_thickness/100, 1, 1],
                            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                        }
                        wall_resp = safe_spawn_actor(unreal, params_wall)
                        if wall_resp and wall_resp.get("status") == "success":
                            all_actors.append(wall_resp)

        # Create rose window (circular feature at front)
        if architectural_style == "gothic":
            logger.info(f"Creating rose window...")
            rose_window_segments = 12
            rose_radius = nave_width * 0.25
            rose_center_z = location[2] + nave_height * 0.7

            for i in range(rose_window_segments):
                angle = (2 * math.pi * i) / rose_window_segments
                x = location[0] - nave_length/2 - wall_thickness/2
                y = location[1] + rose_radius * math.sin(angle)
                z = rose_center_z + rose_radius * math.cos(angle)

                rose_name = f"{name_prefix}_RoseWindow_{i}"
                params_rose = {
                    "name": rose_name,
                    "type": "StaticMeshActor",
                    "location": [x, y, z],
                    "scale": [0.3, 0.3, 0.3],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                }
                rose_resp = safe_spawn_actor(unreal, params_rose)
                if rose_resp and rose_resp.get("status") == "success":
                    all_actors.append(rose_resp)

        # Build roof (pitched roof)
        logger.info(f"Creating roof...")
        roof_pitch = 30  # degrees
        roof_height = (nave_width / 2) * math.tan(math.radians(roof_pitch))

        for i in range(floor_segments_x):
            x_pos = location[0] + i * block_size - nave_length/2
            # Peaked roof using angled blocks
            for side in [-1, 1]:
                roof_y = location[1] + side * nave_width/4
                roof_z = location[2] + nave_height + roof_height/2

                roof_name = f"{name_prefix}_Roof_{side}_{i}"
                params_roof = {
                    "name": roof_name,
                    "type": "StaticMeshActor",
                    "location": [x_pos, roof_y, roof_z],
                    "scale": [1, nave_width/200, roof_height/100],
                    "rotation": [0, 0, side * roof_pitch],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                }
                roof_resp = safe_spawn_actor(unreal, params_roof)
                if roof_resp and roof_resp.get("status") == "success":
                    all_actors.append(roof_resp)

        # Build spire/tower at front
        if include_spire:
            logger.info(f"Creating spire...")
            tower_width = nave_width * 0.4
            tower_base_x = location[0] - nave_length/2
            tower_base_z = location[2] + nave_height

            # Tower base (square)
            tower_levels = int(tower_height / block_size)
            for level in range(tower_levels):
                z_pos = tower_base_z + level * block_size

                # Taper the tower as it goes up (gothic style)
                if architectural_style == "gothic":
                    level_ratio = 1.0 - (level / tower_levels) * 0.3
                    current_width = tower_width * level_ratio
                else:
                    current_width = tower_width

                # Create tower walls (hollow)
                for side_x in [-1, 1]:
                    for side_y in [-1, 1]:
                        tower_name = f"{name_prefix}_Tower_{level}_{side_x}_{side_y}"
                        params_tower = {
                            "name": tower_name,
                            "type": "StaticMeshActor",
                            "location": [
                                tower_base_x + side_x * current_width/2,
                                location[1] + side_y * current_width/2,
                                z_pos
                            ],
                            "scale": [wall_thickness/100, wall_thickness/100, 1],
                            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                        }
                        tower_resp = safe_spawn_actor(unreal, params_tower)
                        if tower_resp and tower_resp.get("status") == "success":
                            all_actors.append(tower_resp)

            # Spire top (cone)
            spire_height = tower_height * 0.5
            spire_name = f"{name_prefix}_Spire"
            params_spire = {
                "name": spire_name,
                "type": "StaticMeshActor",
                "location": [tower_base_x, location[1], tower_base_z + tower_height + spire_height/2],
                "scale": [tower_width/100, tower_width/100, spire_height/100],
                "static_mesh": "/Engine/BasicShapes/Cone.Cone"
            }
            spire_resp = safe_spawn_actor(unreal, params_spire)
            if spire_resp and spire_resp.get("status") == "success":
                all_actors.append(spire_resp)

            # Add cross at top
            cross_name = f"{name_prefix}_Cross"
            cross_height = 200
            params_cross = {
                "name": cross_name,
                "type": "StaticMeshActor",
                "location": [tower_base_x, location[1], tower_base_z + tower_height + spire_height + cross_height/2],
                "scale": [0.3, 0.3, cross_height/100],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            }
            cross_resp = safe_spawn_actor(unreal, params_cross)
            if cross_resp and cross_resp.get("status") == "success":
                all_actors.append(cross_resp)

            # Cross horizontal beam
            cross_beam_name = f"{name_prefix}_CrossBeam"
            params_beam = {
                "name": cross_beam_name,
                "type": "StaticMeshActor",
                "location": [tower_base_x, location[1], tower_base_z + tower_height + spire_height + cross_height * 0.7],
                "scale": [0.3, 1.5, 0.3],
                "rotation": [0, 90, 0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            }
            beam_resp = safe_spawn_actor(unreal, params_beam)
            if beam_resp and beam_resp.get("status") == "success":
                all_actors.append(beam_resp)

        # Build flying buttresses (gothic feature)
        if include_flying_buttresses and architectural_style == "gothic":
            logger.info(f"Creating flying buttresses...")
            buttress_count = max(2, floor_segments_x // 3)

            for i in range(buttress_count):
                x_pos = location[0] + (i * nave_length / buttress_count) - nave_length/2 + nave_length/(buttress_count * 2)

                for side in [-1, 1]:  # Both sides
                    # Buttress pier (vertical support outside wall)
                    pier_y = location[1] + side * (nave_width/2 + buttress_projection)
                    pier_height = nave_height * 0.8
                    pier_levels = int(pier_height / block_size)

                    for level in range(pier_levels):
                        z_pos = location[2] + level * block_size
                        pier_name = f"{name_prefix}_ButtressPier_{side}_{i}_{level}"
                        params_pier = {
                            "name": pier_name,
                            "type": "StaticMeshActor",
                            "location": [x_pos, pier_y, z_pos],
                            "scale": [1.5, 1.5, 1],
                            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                        }
                        pier_resp = safe_spawn_actor(unreal, params_pier)
                        if pier_resp and pier_resp.get("status") == "success":
                            all_actors.append(pier_resp)

                    # Flying arch connecting to wall
                    arch_segments = 5
                    wall_y = location[1] + side * nave_width/2
                    arch_start_z = location[2] + pier_height
                    arch_end_z = location[2] + nave_height * 0.7

                    for seg in range(arch_segments):
                        t = seg / (arch_segments - 1)
                        # Parabolic curve
                        y_interp = pier_y + t * (wall_y - pier_y)
                        z_interp = arch_start_z + t * (arch_end_z - arch_start_z) + (1 - (2*t - 1)**2) * buttress_projection * 0.3

                        arch_name = f"{name_prefix}_FlyingArch_{side}_{i}_{seg}"
                        params_arch = {
                            "name": arch_name,
                            "type": "StaticMeshActor",
                            "location": [x_pos, y_interp, z_interp],
                            "scale": [1, buttress_projection/(block_size * arch_segments), 0.8],
                            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                        }
                        arch_resp = safe_spawn_actor(unreal, params_arch)
                        if arch_resp and arch_resp.get("status") == "success":
                            all_actors.append(arch_resp)

                    # Pinnacle on top of pier
                    pinnacle_name = f"{name_prefix}_Pinnacle_{side}_{i}"
                    params_pinnacle = {
                        "name": pinnacle_name,
                        "type": "StaticMeshActor",
                        "location": [x_pos, pier_y, location[2] + pier_height + 150],
                        "scale": [0.8, 0.8, 3],
                        "static_mesh": "/Engine/BasicShapes/Cone.Cone"
                    }
                    pinnacle_resp = safe_spawn_actor(unreal, params_pinnacle)
                    if pinnacle_resp and pinnacle_resp.get("status") == "success":
                        all_actors.append(pinnacle_resp)

        # Build apse (rounded end at back)
        logger.info(f"Creating apse...")
        apse_radius = nave_width / 2
        apse_segments = 8
        apse_x = location[0] + nave_length/2 + apse_radius

        for i in range(apse_segments + 1):
            angle = (math.pi * i) / apse_segments  # Semicircle
            y = location[1] + apse_radius * math.sin(angle)
            x = apse_x - apse_radius * (1 - math.cos(angle))

            for h in range(wall_height_blocks):
                z_pos = location[2] + h * block_size + block_size/2

                apse_name = f"{name_prefix}_Apse_{i}_{h}"
                params_apse = {
                    "name": apse_name,
                    "type": "StaticMeshActor",
                    "location": [x, y, z_pos],
                    "scale": [wall_thickness/100, wall_thickness/100, 1],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                }
                apse_resp = safe_spawn_actor(unreal, params_apse)
                if apse_resp and apse_resp.get("status") == "success":
                    all_actors.append(apse_resp)

        logger.info(f"Church creation complete! Created {len(all_actors)} elements")

        return {
            "success": True,
            "message": f"Created {church_size} {architectural_style} church with {len(all_actors)} elements",
            "actors": all_actors,
            "stats": {
                "church_size": church_size,
                "architectural_style": architectural_style,
                "nave_length": nave_length,
                "nave_width": nave_width,
                "nave_height": nave_height,
                "tower_height": tower_height if include_spire else 0,
                "has_spire": include_spire,
                "has_flying_buttresses": include_flying_buttresses and architectural_style == "gothic",
                "total_actors": len(all_actors)
            }
        }

    except Exception as e:
        logger.error(f"create_church error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def batch_spawn_actors(
    actors: List[Dict[str, Any]],
    chunk_size: int = 100
) -> Dict[str, Any]:
    """
    Spawn multiple actors in batches for high performance.

    Efficiently spawns a large number of actors by processing them in chunks.
    This reduces memory overhead and improves performance for bulk operations.
    Each actor is spawned using the safe_spawn_actor helper to ensure unique names.

    Args:
        actors: List of actor parameter dictionaries. Each dict should contain:
            - name: Base name for the actor
            - type: Actor type (e.g., "cube", "sphere", "cylinder")
            - location: [x, y, z] position
            - scale: [x, y, z] scale (optional, defaults to [1.0, 1.0, 1.0])
            - static_mesh: Path to static mesh (optional)
            - rotation: [pitch, yaw, roll] rotation (optional)
        chunk_size: Number of actors to spawn per batch (default 100)

    Returns:
        Dictionary with:
            - success: Whether the operation completed
            - message: Summary message
            - actors: List of successfully spawned actor names
            - stats: Dictionary with performance statistics
                - total_requested: Number of actors requested
                - total_spawned: Number successfully spawned
                - failed_count: Number that failed to spawn
                - elapsed_time: Total time in seconds
                - chunks_processed: Number of chunks processed
    """

    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        start_time = time.time()
        total_requested = len(actors)

        logger.info(f"Batch spawning {total_requested} actors in chunks of {chunk_size}")

        spawned_actors = []
        failed_count = 0
        chunks_processed = 0

        # Process actors in chunks
        for chunk_start in range(0, total_requested, chunk_size):
            chunk_end = min(chunk_start + chunk_size, total_requested)
            chunk = actors[chunk_start:chunk_end]
            chunks_processed += 1

            logger.info(f"Processing chunk {chunks_processed}: actors {chunk_start} to {chunk_end}")

            for actor_params in chunk:
                try:
                    # Build spawn parameters
                    spawn_params = {
                        "name": actor_params.get("name", "Actor"),
                        "type": actor_params.get("type", "cube"),
                        "location": actor_params.get("location", [0.0, 0.0, 0.0]),
                        "scale": actor_params.get("scale", [1.0, 1.0, 1.0])
                    }

                    # Add optional parameters if provided
                    if "rotation" in actor_params:
                        spawn_params["rotation"] = actor_params["rotation"]
                    if "static_mesh" in actor_params:
                        spawn_params["static_mesh"] = actor_params["static_mesh"]
                    if "color" in actor_params:
                        spawn_params["color"] = actor_params["color"]

                    # Spawn the actor using safe_spawn_actor
                    response = safe_spawn_actor(unreal, spawn_params)

                    if response and response.get("status") == "success":
                        actor_name = spawn_params["name"]
                        if "result" in response and isinstance(response["result"], dict):
                            actor_name = response["result"].get("final_name", actor_name)
                        spawned_actors.append(actor_name)
                    else:
                        failed_count += 1
                        logger.warning(f"Failed to spawn actor: {spawn_params['name']}")

                except Exception as e:
                    failed_count += 1
                    logger.error(f"Error spawning actor in batch: {e}")

        elapsed_time = time.time() - start_time
        total_spawned = len(spawned_actors)

        logger.info(f"Batch spawn complete: {total_spawned}/{total_requested} actors in {elapsed_time:.2f}s")

        return {
            "success": True,
            "message": f"Spawned {total_spawned} of {total_requested} actors in {elapsed_time:.2f}s",
            "actors": spawned_actors,
            "stats": {
                "total_requested": total_requested,
                "total_spawned": total_spawned,
                "failed_count": failed_count,
                "elapsed_time": elapsed_time,
                "chunks_processed": chunks_processed,
                "actors_per_second": total_spawned / elapsed_time if elapsed_time > 0 else 0
            }
        }

    except Exception as e:
        logger.error(f"batch_spawn_actors error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def clone_actor_array(
    source_actor_name: str,
    count: int,
    pattern: str = "grid",
    spacing: float = 200.0,
    location: List[float] = [0.0, 0.0, 0.0]
) -> Dict[str, Any]:
    """
    Clone an existing actor multiple times in various spatial patterns.

    Creates multiple copies of an existing actor arranged in geometric patterns.
    Useful for quickly creating arrays of objects like street lights, trees,
    fence posts, or any repeating elements.

    Args:
        source_actor_name: Name of the actor to clone
        count: Number of clones to create
        pattern: Arrangement pattern. Options:
            - "grid": Square grid layout (calculates rows/cols automatically)
            - "circle": Circular arrangement around center point
            - "line": Straight line along X axis
            - "scatter": Random positions within radius
        spacing: Distance between clones (grid/line) or radius (circle/scatter)
        location: Center point for the array [x, y, z]

    Returns:
        Dictionary with:
            - success: Whether the operation completed
            - message: Summary message
            - actors: List of spawned actor names
            - pattern_used: The pattern that was used
            - count: Number of actors created
    """

    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Cloning actor '{source_actor_name}' {count} times in '{pattern}' pattern")

        # First, verify the source actor exists
        find_response = unreal.send_command("find_actors_by_name", {"pattern": source_actor_name})
        if not find_response or find_response.get("status") != "success":
            return {"success": False, "message": f"Source actor '{source_actor_name}' not found"}

        # Get source actor details (we'll use basic parameters for now)
        # In a real implementation, we'd query the actor's transform, mesh, etc.
        # For now, we'll create simple clones with position variations

        # Generate positions based on pattern
        positions = []

        if pattern == "grid":
            # Calculate grid dimensions (try to make it roughly square)
            cols = math.ceil(math.sqrt(count))
            rows = math.ceil(count / cols)

            for i in range(count):
                row = i // cols
                col = i % cols
                x = location[0] + (col - cols/2) * spacing
                y = location[1] + (row - rows/2) * spacing
                z = location[2]
                positions.append([x, y, z])

        elif pattern == "circle":
            # Evenly space around circumference
            for i in range(count):
                angle = (2 * math.pi * i) / count
                x = location[0] + spacing * math.cos(angle)
                y = location[1] + spacing * math.sin(angle)
                z = location[2]
                positions.append([x, y, z])

        elif pattern == "line":
            # Space along X axis
            for i in range(count):
                x = location[0] + (i - count/2) * spacing
                y = location[1]
                z = location[2]
                positions.append([x, y, z])

        elif pattern == "scatter":
            # Random positions within radius
            for i in range(count):
                # Random angle and distance
                angle = random.uniform(0, 2 * math.pi)
                distance = random.uniform(0, spacing)
                x = location[0] + distance * math.cos(angle)
                y = location[1] + distance * math.sin(angle)
                z = location[2] + random.uniform(-spacing * 0.1, spacing * 0.1)  # Small Z variation
                positions.append([x, y, z])
        else:
            return {"success": False, "message": f"Unknown pattern: {pattern}. Use 'grid', 'circle', 'line', or 'scatter'"}

        # Build actor parameter list for batch spawning
        # We'll use a simple cube for clones (in real usage, we'd copy the source actor's properties)
        actor_list = []
        for i, pos in enumerate(positions):
            actor_params = {
                "name": f"{source_actor_name}_clone_{i}",
                "type": "cube",  # Default type, ideally would match source
                "location": pos,
                "scale": [1.0, 1.0, 1.0]
            }
            actor_list.append(actor_params)

        # Use batch_spawn_actors to create all clones
        batch_result = batch_spawn_actors(actor_list, chunk_size=100)

        if not batch_result.get("success"):
            return batch_result

        return {
            "success": True,
            "message": f"Cloned '{source_actor_name}' {count} times in '{pattern}' pattern",
            "actors": batch_result.get("actors", []),
            "pattern_used": pattern,
            "count": len(batch_result.get("actors", [])),
            "stats": batch_result.get("stats", {})
        }

    except Exception as e:
        logger.error(f"clone_actor_array error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def apply_random_variation(
    actor_pattern: str,
    rotation_range: float = 45.0,
    scale_range: float = 0.2,
    location_jitter: float = 0.0
) -> Dict[str, Any]:
    """
    Apply random variations to rotation, scale, and location for matching actors.

    Adds natural randomness to a group of actors to make them look less uniform.
    Perfect for breaking up repeating patterns in forests, rocks, debris, etc.

    Args:
        actor_pattern: Name pattern to match actors (e.g., "Tree*" or "Rock_")
        rotation_range: Maximum rotation variation in degrees (applied to Z axis/yaw).
            For example, 45.0 means random rotation between -45 and +45 degrees.
        scale_range: Scale variation as a percentage (0.2 = ±20%).
            For example, 0.2 with base scale 1.0 produces scales between 0.8 and 1.2.
        location_jitter: Random position offset in X and Y axes (in Unreal units).
            For example, 50.0 means ±50 units in X and Y.

    Returns:
        Dictionary with:
            - success: Whether the operation completed
            - message: Summary message
            - actors_modified: List of actor names that were modified
            - count: Number of actors modified
            - ranges_applied: Dictionary showing the actual ranges used
    """

    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Applying random variation to actors matching '{actor_pattern}'")

        # Find all actors matching the pattern
        find_response = find_actors_by_name(actor_pattern)
        if not find_response or not find_response.get("success"):
            return {"success": False, "message": f"No actors found matching pattern '{actor_pattern}'"}

        actors = find_response.get("actors", [])
        if not actors:
            return {"success": False, "message": f"No actors found matching pattern '{actor_pattern}'"}

        logger.info(f"Found {len(actors)} actors to modify")

        modified_actors = []

        for actor in actors:
            try:
                # Get actor name
                actor_name = actor.get("name") if isinstance(actor, dict) else str(actor)

                # Get current transform (or use defaults if not available)
                current_location = actor.get("location", [0.0, 0.0, 0.0]) if isinstance(actor, dict) else [0.0, 0.0, 0.0]
                current_rotation = actor.get("rotation", [0.0, 0.0, 0.0]) if isinstance(actor, dict) else [0.0, 0.0, 0.0]
                current_scale = actor.get("scale", [1.0, 1.0, 1.0]) if isinstance(actor, dict) else [1.0, 1.0, 1.0]

                # Apply random variations
                new_location = None
                new_rotation = None
                new_scale = None

                if location_jitter > 0:
                    new_location = [
                        current_location[0] + random.uniform(-location_jitter, location_jitter),
                        current_location[1] + random.uniform(-location_jitter, location_jitter),
                        current_location[2]  # Don't jitter Z to keep actors on ground
                    ]

                if rotation_range > 0:
                    # Apply rotation variation to yaw (Z axis)
                    rotation_delta = random.uniform(-rotation_range, rotation_range)
                    new_rotation = [
                        current_rotation[0],  # Keep pitch
                        current_rotation[1] + rotation_delta,  # Vary yaw
                        current_rotation[2]   # Keep roll
                    ]

                if scale_range > 0:
                    # Apply uniform scale variation
                    scale_multiplier = 1.0 + random.uniform(-scale_range, scale_range)
                    new_scale = [
                        current_scale[0] * scale_multiplier,
                        current_scale[1] * scale_multiplier,
                        current_scale[2] * scale_multiplier
                    ]

                # Apply the transform if any variations were requested
                if new_location or new_rotation or new_scale:
                    transform_response = set_actor_transform(
                        name=actor_name,
                        location=new_location,
                        rotation=new_rotation,
                        scale=new_scale
                    )

                    if transform_response and transform_response.get("success"):
                        modified_actors.append(actor_name)
                    else:
                        logger.warning(f"Failed to modify actor: {actor_name}")

            except Exception as e:
                logger.error(f"Error modifying actor: {e}")
                continue

        logger.info(f"Modified {len(modified_actors)} actors")

        return {
            "success": True,
            "message": f"Applied random variation to {len(modified_actors)} actors",
            "actors_modified": modified_actors,
            "count": len(modified_actors),
            "ranges_applied": {
                "rotation_range": f"±{rotation_range} degrees",
                "scale_range": f"±{scale_range * 100}%",
                "location_jitter": f"±{location_jitter} units"
            }
        }

    except Exception as e:
        logger.error(f"apply_random_variation error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_forest(
    forest_size: str = "medium",  # "small", "medium", "large", "massive"
    tree_density: float = 0.7,  # 0.0 to 1.0
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "Forest",
    forest_style: str = "fantasy",  # "fantasy", "realistic", "enchanted", "dark"
    include_magical_elements: bool = True
) -> Dict[str, Any]:
    """
    Create a procedural forest with trees, undergrowth, and optional magical elements.

    Creates an immersive forest environment with varying tree types, sizes, and
    optional fantasy elements like glowing mushrooms, magical crystals, and fairy rings.
    Perfect for fantasy game environments and mystical scenes.

    Args:
        forest_size: Size of the forest area ("small", "medium", "large", "massive")
        tree_density: Density of trees (0.0-1.0)
        location: Center point of the forest
        name_prefix: Prefix for all spawned actors
        forest_style: Visual style ("fantasy", "realistic", "enchanted", "dark")
        include_magical_elements: Add magical features for fantasy style

    Returns:
        Dictionary with success status, spawned actors, and forest statistics
    """
    try:
        random.seed()  # Use different seed for variety

        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Creating {forest_size} {forest_style} forest at {location}")

        # Define forest parameters based on size
        forest_params = {
            "small": {"area": 3000, "tree_count": 30, "undergrowth_factor": 1.5},
            "medium": {"area": 6000, "tree_count": 80, "undergrowth_factor": 2.0},
            "large": {"area": 10000, "tree_count": 150, "undergrowth_factor": 2.5},
            "massive": {"area": 20000, "tree_count": 300, "undergrowth_factor": 3.0}
        }

        params = forest_params.get(forest_size, forest_params["medium"])
        forest_area = params["area"]
        target_trees = int(params["tree_count"] * tree_density)
        undergrowth_factor = params["undergrowth_factor"]

        all_actors = []
        tree_count = 0
        undergrowth_count = 0
        magical_element_count = 0

        # Generate trees
        logger.info(f"Planting {target_trees} trees...")
        for i in range(target_trees):
            # Random position within forest area
            x_offset = random.uniform(-forest_area/2, forest_area/2)
            y_offset = random.uniform(-forest_area/2, forest_area/2)
            tree_location = [
                location[0] + x_offset,
                location[1] + y_offset,
                location[2]
            ]

            # Vary tree characteristics based on style
            if forest_style == "fantasy" or forest_style == "enchanted":
                # Fantasy trees are taller and more varied
                tree_height = random.randint(8, 20)
                trunk_radius = random.uniform(1.5, 3.5)
                canopy_radius = random.uniform(3.0, 6.0)
                tree_colors = [[0.4, 0.25, 0.15, 1.0], [0.3, 0.2, 0.1, 1.0], [0.5, 0.3, 0.2, 1.0]]
                foliage_colors = [[0.2, 0.8, 0.3, 1.0], [0.3, 0.7, 0.4, 1.0], [0.4, 0.9, 0.5, 1.0]]
            elif forest_style == "dark":
                # Dark forest has twisted, ominous trees
                tree_height = random.randint(6, 15)
                trunk_radius = random.uniform(1.0, 2.5)
                canopy_radius = random.uniform(2.5, 5.0)
                tree_colors = [[0.2, 0.15, 0.1, 1.0], [0.15, 0.1, 0.08, 1.0]]
                foliage_colors = [[0.15, 0.3, 0.2, 1.0], [0.1, 0.25, 0.15, 1.0]]
            else:  # realistic
                tree_height = random.randint(7, 16)
                trunk_radius = random.uniform(1.2, 3.0)
                canopy_radius = random.uniform(3.0, 5.5)
                tree_colors = [[0.35, 0.2, 0.12, 1.0], [0.4, 0.25, 0.15, 1.0]]
                foliage_colors = [[0.25, 0.6, 0.25, 1.0], [0.3, 0.65, 0.3, 1.0]]

            # Create tree trunk (cylinder)
            trunk_name = f"{name_prefix}_Tree_{i}_Trunk"
            trunk_scale = [trunk_radius, trunk_radius, tree_height * 2]  # *2 because cylinder default
            trunk_color = random.choice(tree_colors)

            params = {
                "name": trunk_name,
                "type": "StaticMeshActor",
                "location": [tree_location[0], tree_location[1], tree_location[2] + tree_height * 50],
                "scale": trunk_scale,
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            }
            trunk_resp = safe_spawn_actor(unreal, params)
            if trunk_resp and trunk_resp.get("status") == "success":
                all_actors.append(trunk_resp)
                tree_count += 1

            # Create tree canopy (sphere or cone for fantasy)
            canopy_name = f"{name_prefix}_Tree_{i}_Canopy"
            canopy_height = tree_location[2] + tree_height * 100
            canopy_scale = [canopy_radius, canopy_radius, canopy_radius * 1.2]
            canopy_color = random.choice(foliage_colors)

            # Use cone for some fantasy trees, sphere for others
            canopy_mesh = "/Engine/BasicShapes/Cone.Cone" if (forest_style == "fantasy" and random.random() > 0.5) else "/Engine/BasicShapes/Sphere.Sphere"

            params = {
                "name": canopy_name,
                "type": "StaticMeshActor",
                "location": [tree_location[0], tree_location[1], canopy_height],
                "scale": canopy_scale,
                "static_mesh": canopy_mesh
            }
            canopy_resp = safe_spawn_actor(unreal, params)
            if canopy_resp and canopy_resp.get("status") == "success":
                all_actors.append(canopy_resp)

        # Add undergrowth (bushes, shrubs)
        logger.info(f"Adding undergrowth...")
        target_undergrowth = int(target_trees * undergrowth_factor)
        for i in range(target_undergrowth):
            x_offset = random.uniform(-forest_area/2, forest_area/2)
            y_offset = random.uniform(-forest_area/2, forest_area/2)
            undergrowth_location = [
                location[0] + x_offset,
                location[1] + y_offset,
                location[2] + 50
            ]

            bush_scale = random.uniform(0.8, 2.0)
            bush_color = random.choice([[0.2, 0.5, 0.2, 1.0], [0.25, 0.55, 0.25, 1.0], [0.3, 0.6, 0.3, 1.0]])

            bush_name = f"{name_prefix}_Bush_{i}"
            params = {
                "name": bush_name,
                "type": "StaticMeshActor",
                "location": undergrowth_location,
                "scale": [bush_scale, bush_scale, bush_scale * 0.8],
                "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
            }
            bush_resp = safe_spawn_actor(unreal, params)
            if bush_resp and bush_resp.get("status") == "success":
                all_actors.append(bush_resp)
                undergrowth_count += 1

        # Add magical elements for fantasy/enchanted styles
        if include_magical_elements and (forest_style == "fantasy" or forest_style == "enchanted"):
            logger.info(f"Adding magical elements...")

            # Glowing mushrooms
            mushroom_count = int(target_trees * 0.4)
            for i in range(mushroom_count):
                x_offset = random.uniform(-forest_area/2, forest_area/2)
                y_offset = random.uniform(-forest_area/2, forest_area/2)
                mushroom_location = [
                    location[0] + x_offset,
                    location[1] + y_offset,
                    location[2] + 25
                ]

                # Mushroom colors - vibrant and glowing
                mushroom_colors = [
                    [0.8, 0.2, 0.8, 1.0],  # Purple
                    [0.2, 0.8, 0.9, 1.0],  # Cyan
                    [0.9, 0.5, 0.2, 1.0],  # Orange
                    [0.3, 0.9, 0.4, 1.0]   # Bright green
                ]
                mushroom_color = random.choice(mushroom_colors)
                mushroom_scale = random.uniform(0.3, 0.8)

                # Mushroom stem (cylinder)
                stem_name = f"{name_prefix}_Mushroom_{i}_Stem"
                params = {
                    "name": stem_name,
                    "type": "StaticMeshActor",
                    "location": [mushroom_location[0], mushroom_location[1], mushroom_location[2] + 30],
                    "scale": [mushroom_scale * 0.3, mushroom_scale * 0.3, mushroom_scale * 0.8],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                }
                stem_resp = safe_spawn_actor(unreal, params)
                if stem_resp and stem_resp.get("status") == "success":
                    all_actors.append(stem_resp)
                    magical_element_count += 1

                # Mushroom cap (sphere)
                cap_name = f"{name_prefix}_Mushroom_{i}_Cap"
                params = {
                    "name": cap_name,
                    "type": "StaticMeshActor",
                    "location": [mushroom_location[0], mushroom_location[1], mushroom_location[2] + 80],
                    "scale": [mushroom_scale, mushroom_scale, mushroom_scale * 0.6],
                    "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
                }
                cap_resp = safe_spawn_actor(unreal, params)
                if cap_resp and cap_resp.get("status") == "success":
                    all_actors.append(cap_resp)

            # Magical crystals
            crystal_count = int(target_trees * 0.2)
            for i in range(crystal_count):
                x_offset = random.uniform(-forest_area/2, forest_area/2)
                y_offset = random.uniform(-forest_area/2, forest_area/2)
                crystal_location = [
                    location[0] + x_offset,
                    location[1] + y_offset,
                    location[2]
                ]

                crystal_colors = [
                    [0.5, 0.7, 1.0, 0.8],   # Light blue
                    [0.9, 0.4, 0.9, 0.8],   # Purple
                    [0.4, 0.9, 0.7, 0.8],   # Cyan-green
                    [1.0, 0.9, 0.4, 0.8]    # Golden
                ]
                crystal_color = random.choice(crystal_colors)
                crystal_height = random.uniform(1.5, 3.5)

                crystal_name = f"{name_prefix}_Crystal_{i}"
                params = {
                    "name": crystal_name,
                    "type": "StaticMeshActor",
                    "location": [crystal_location[0], crystal_location[1], crystal_location[2] + crystal_height * 50],
                    "scale": [0.5, 0.5, crystal_height],
                    "rotation": [0, 0, random.uniform(0, 360)],
                    "static_mesh": "/Engine/BasicShapes/Cone.Cone"
                }
                crystal_resp = safe_spawn_actor(unreal, params)
                if crystal_resp and crystal_resp.get("status") == "success":
                    all_actors.append(crystal_resp)
                    magical_element_count += 1

            # Fairy rings (circles of small glowing orbs)
            fairy_ring_count = random.randint(2, 5)
            for ring_idx in range(fairy_ring_count):
                ring_x = location[0] + random.uniform(-forest_area/3, forest_area/3)
                ring_y = location[1] + random.uniform(-forest_area/3, forest_area/3)
                ring_radius = random.uniform(200, 400)
                orbs_in_ring = random.randint(8, 16)

                for orb_idx in range(orbs_in_ring):
                    angle = (2 * math.pi * orb_idx) / orbs_in_ring
                    orb_x = ring_x + ring_radius * math.cos(angle)
                    orb_y = ring_y + ring_radius * math.sin(angle)

                    orb_name = f"{name_prefix}_FairyOrb_{ring_idx}_{orb_idx}"
                    orb_color = [0.9, 0.9, 0.3, 0.9]  # Glowing yellow

                    params = {
                        "name": orb_name,
                        "type": "StaticMeshActor",
                        "location": [orb_x, orb_y, location[2] + 80],
                        "scale": [0.3, 0.3, 0.3],
                        "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
                    }
                    orb_resp = safe_spawn_actor(unreal, params)
                    if orb_resp and orb_resp.get("status") == "success":
                        all_actors.append(orb_resp)
                        magical_element_count += 1

        logger.info(f"Forest creation complete! Created {len(all_actors)} elements")

        return {
            "success": True,
            "message": f"Created {forest_size} {forest_style} forest with {tree_count} trees",
            "actors": all_actors,
            "stats": {
                "forest_size": forest_size,
                "style": forest_style,
                "tree_count": tree_count,
                "undergrowth_count": undergrowth_count,
                "magical_elements": magical_element_count,
                "total_actors": len(all_actors),
                "forest_area": forest_area,
                "tree_density": tree_density
            }
        }

    except Exception as e:
        logger.error(f"create_forest error: {e}")
        return {"success": False, "message": str(e)}


# Smart Selection and Manipulation Tools

@mcp.tool()
def select_actors_by_region(
    center: List[float] = [0.0, 0.0, 0.0],
    radius: float = 1000.0,
    shape: str = "sphere",
    actor_type_filter: str = None
) -> Dict[str, Any]:
    """
    Select all actors within a specified region (sphere or box).

    Queries all actors in the level and filters them based on their distance
    from a center point. Useful for selecting groups of actors for batch operations.
    Supports both spherical and box-shaped selection regions.

    Args:
        center: Center point of the selection region [x, y, z]
        radius: Radius for sphere shape, or half-extent for box shape
        shape: Selection shape - "sphere" or "box"
        actor_type_filter: Optional filter by actor type (contains check)

    Returns:
        Dictionary with success status, list of actor names in region, and statistics
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Selecting actors in {shape} region at {center} with radius {radius}")

        # Get all actors in the level
        actors_response = unreal.send_command("get_actors_in_level", {})
        if not actors_response or not actors_response.get("success"):
            return {"success": False, "message": "Failed to get actors in level"}

        actors_data = actors_response.get("actors", [])
        if not actors_data:
            return {
                "success": True,
                "message": "No actors found in level",
                "actors_in_region": [],
                "center": center,
                "radius": radius,
                "shape": shape,
                "count": 0
            }

        selected_actors = []

        # Filter actors based on region shape
        for actor in actors_data:
            actor_name = actor.get("name", "")
            actor_type = actor.get("type", "")
            actor_location = actor.get("location", [0.0, 0.0, 0.0])

            # Apply type filter if specified
            if actor_type_filter and actor_type_filter not in actor_type:
                continue

            # Calculate distance based on shape
            if shape.lower() == "sphere":
                # Spherical distance check (3D Euclidean distance)
                dx = actor_location[0] - center[0]
                dy = actor_location[1] - center[1]
                dz = actor_location[2] - center[2]
                distance = math.sqrt(dx*dx + dy*dy + dz*dz)

                if distance <= radius:
                    selected_actors.append(actor_name)

            elif shape.lower() == "box":
                # Box check (within radius on each axis)
                within_x = abs(actor_location[0] - center[0]) <= radius
                within_y = abs(actor_location[1] - center[1]) <= radius
                within_z = abs(actor_location[2] - center[2]) <= radius

                if within_x and within_y and within_z:
                    selected_actors.append(actor_name)
            else:
                return {"success": False, "message": f"Invalid shape: {shape}. Use 'sphere' or 'box'"}

        logger.info(f"Selected {len(selected_actors)} actors in {shape} region")

        return {
            "success": True,
            "message": f"Selected {len(selected_actors)} actors in {shape} region",
            "actors_in_region": selected_actors,
            "center": center,
            "radius": radius,
            "shape": shape,
            "count": len(selected_actors),
            "filter_applied": actor_type_filter if actor_type_filter else "none"
        }

    except Exception as e:
        logger.error(f"select_actors_by_region error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def move_actors_relative(
    actor_names: List[str],
    offset: List[float] = [0.0, 0.0, 0.0],
    preserve_rotation: bool = True
) -> Dict[str, Any]:
    """
    Move multiple actors by the same offset vector.

    Translates a group of actors by adding the offset to their current positions.
    Useful for batch moving selected actors while maintaining their relative positions.
    Can optionally preserve or modify rotation during the move.

    Args:
        actor_names: List of actor names to move
        offset: Translation offset to apply [dx, dy, dz]
        preserve_rotation: If True, only modify location (keep rotation unchanged)

    Returns:
        Dictionary with success status, count of moved actors, and operation details
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        if not actor_names:
            return {"success": False, "message": "No actors specified"}

        logger.info(f"Moving {len(actor_names)} actors by offset {offset}")

        # Get current actor transforms
        actors_response = unreal.send_command("get_actors_in_level", {})
        if not actors_response or not actors_response.get("success"):
            return {"success": False, "message": "Failed to get actors in level"}

        actors_data = actors_response.get("actors", [])
        actor_dict = {actor.get("name"): actor for actor in actors_data}

        moved_count = 0
        failed_actors = []

        # Move each actor
        for actor_name in actor_names:
            if actor_name not in actor_dict:
                logger.warning(f"Actor not found: {actor_name}")
                failed_actors.append(actor_name)
                continue

            actor_data = actor_dict[actor_name]
            current_location = actor_data.get("location", [0.0, 0.0, 0.0])

            # Calculate new location
            new_location = [
                current_location[0] + offset[0],
                current_location[1] + offset[1],
                current_location[2] + offset[2]
            ]

            # Set transform (preserve rotation if requested)
            transform_params = {
                "name": actor_name,
                "location": new_location
            }

            if not preserve_rotation:
                current_rotation = actor_data.get("rotation", [0.0, 0.0, 0.0])
                transform_params["rotation"] = current_rotation

            response = unreal.send_command("set_actor_transform", transform_params)

            if response and response.get("success"):
                moved_count += 1
            else:
                failed_actors.append(actor_name)

        logger.info(f"Moved {moved_count} actors successfully")

        return {
            "success": True,
            "message": f"Moved {moved_count} of {len(actor_names)} actors",
            "count_moved": moved_count,
            "offset_applied": offset,
            "preserve_rotation": preserve_rotation,
            "moved_actors": [name for name in actor_names if name not in failed_actors],
            "failed_actors": failed_actors
        }

    except Exception as e:
        logger.error(f"move_actors_relative error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def rotate_actors_around_point(
    actor_names: List[str],
    center: List[float] = [0.0, 0.0, 0.0],
    angle_degrees: float = 90.0,
    axis: str = "z"
) -> Dict[str, Any]:
    """
    Rotate multiple actors around a central point.

    Rotates a group of actors around a specified pivot point by applying
    rotation transformations. The actors maintain their distance from the
    center point while moving to new positions. Supports rotation around
    X, Y, or Z axis.

    Args:
        actor_names: List of actor names to rotate
        center: Pivot point for rotation [x, y, z]
        angle_degrees: Rotation angle in degrees
        axis: Rotation axis - "x", "y", or "z"

    Returns:
        Dictionary with success status, count of rotated actors, and operation details
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        if not actor_names:
            return {"success": False, "message": "No actors specified"}

        # Validate axis
        axis_lower = axis.lower()
        if axis_lower not in ["x", "y", "z"]:
            return {"success": False, "message": f"Invalid axis: {axis}. Use 'x', 'y', or 'z'"}

        logger.info(f"Rotating {len(actor_names)} actors around {center} by {angle_degrees} degrees on {axis_lower} axis")

        # Convert angle to radians
        angle_radians = math.radians(angle_degrees)
        cos_angle = math.cos(angle_radians)
        sin_angle = math.sin(angle_radians)

        # Get current actor transforms
        actors_response = unreal.send_command("get_actors_in_level", {})
        if not actors_response or not actors_response.get("success"):
            return {"success": False, "message": "Failed to get actors in level"}

        actors_data = actors_response.get("actors", [])
        actor_dict = {actor.get("name"): actor for actor in actors_data}

        rotated_count = 0
        failed_actors = []

        # Rotate each actor
        for actor_name in actor_names:
            if actor_name not in actor_dict:
                logger.warning(f"Actor not found: {actor_name}")
                failed_actors.append(actor_name)
                continue

            actor_data = actor_dict[actor_name]
            current_location = actor_data.get("location", [0.0, 0.0, 0.0])

            # Translate to origin (relative to center point)
            rel_x = current_location[0] - center[0]
            rel_y = current_location[1] - center[1]
            rel_z = current_location[2] - center[2]

            # Apply rotation based on axis
            if axis_lower == "z":
                # Rotate around Z axis (XY plane rotation)
                new_x = rel_x * cos_angle - rel_y * sin_angle
                new_y = rel_x * sin_angle + rel_y * cos_angle
                new_z = rel_z
            elif axis_lower == "y":
                # Rotate around Y axis (XZ plane rotation)
                new_x = rel_x * cos_angle + rel_z * sin_angle
                new_y = rel_y
                new_z = -rel_x * sin_angle + rel_z * cos_angle
            else:  # axis_lower == "x"
                # Rotate around X axis (YZ plane rotation)
                new_x = rel_x
                new_y = rel_y * cos_angle - rel_z * sin_angle
                new_z = rel_y * sin_angle + rel_z * cos_angle

            # Translate back to world coordinates
            new_location = [
                new_x + center[0],
                new_y + center[1],
                new_z + center[2]
            ]

            # Set new transform
            transform_params = {
                "name": actor_name,
                "location": new_location
            }

            response = unreal.send_command("set_actor_transform", transform_params)

            if response and response.get("success"):
                rotated_count += 1
            else:
                failed_actors.append(actor_name)

        logger.info(f"Rotated {rotated_count} actors successfully")

        return {
            "success": True,
            "message": f"Rotated {rotated_count} of {len(actor_names)} actors around {axis_lower} axis",
            "count_rotated": rotated_count,
            "center": center,
            "angle_degrees": angle_degrees,
            "axis": axis_lower,
            "rotated_actors": [name for name in actor_names if name not in failed_actors],
            "failed_actors": failed_actors
        }

    except Exception as e:
        logger.error(f"rotate_actors_around_point error: {e}")
        return {"success": False, "message": str(e)}


# Validation and Debugging Tools

@mcp.tool()
def validate_scene_performance(
    max_actors: int = 10000,
    warn_threshold: int = 5000
) -> Dict[str, Any]:
    """
    Validate scene performance by analyzing actor count and distribution.

    Checks the total number of actors in the level against warning and critical thresholds,
    identifies the most common actor prefixes, and provides recommendations for optimization.

    Args:
        max_actors: Critical threshold - scenes should not exceed this count
        warn_threshold: Warning threshold - scenes approaching this need optimization

    Returns:
        Dict containing:
            - success: Whether the validation completed successfully
            - total_actors: Total number of actors in the level
            - status: "ok", "warning", or "critical"
            - top_prefixes: Top 5 most common actor name prefixes with counts
            - recommendations: List of optimization suggestions
            - message: Summary message
    """
    try:
        logger.info(f"Validating scene performance (warn: {warn_threshold}, max: {max_actors})")
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        # Get all actors in the level
        response = unreal.send_command("get_actors_in_level", {})
        if not response or not response.get("success"):
            return {"success": False, "message": "Failed to get actors from level"}

        actors = response.get("actors", [])
        total_actors = len(actors)

        logger.info(f"Found {total_actors} actors in level")

        # Determine status
        if total_actors >= max_actors:
            status = "critical"
        elif total_actors >= warn_threshold:
            status = "warning"
        else:
            status = "ok"

        # Calculate actors by prefix (before first underscore or number)
        prefix_counts: Dict[str, int] = {}
        for actor in actors:
            actor_name = actor.get("name", "")
            # Extract prefix - everything before first underscore or digit
            prefix = ""
            for char in actor_name:
                if char == "_" or char.isdigit():
                    break
                prefix += char

            if prefix:
                prefix_counts[prefix] = prefix_counts.get(prefix, 0) + 1
            else:
                prefix_counts["Unknown"] = prefix_counts.get("Unknown", 0) + 1

        # Get top 5 prefixes
        sorted_prefixes = sorted(prefix_counts.items(), key=lambda x: x[1], reverse=True)
        top_prefixes = [{"prefix": prefix, "count": count} for prefix, count in sorted_prefixes[:5]]

        # Generate recommendations
        recommendations = []
        if status == "critical":
            recommendations.append("CRITICAL: Actor count exceeds maximum threshold. Scene may have severe performance issues.")
            recommendations.append("Consider using instanced static meshes for repeated objects.")
            recommendations.append("Remove unnecessary actors or split the level into sublevels.")
        elif status == "warning":
            recommendations.append("WARNING: Actor count is approaching maximum threshold.")
            recommendations.append("Review actor count and consider optimization before adding more content.")
        else:
            recommendations.append("Scene actor count is within acceptable limits.")

        # Add specific recommendations based on top prefixes
        for prefix_info in top_prefixes[:3]:
            if prefix_info["count"] > 500:
                recommendations.append(
                    f"Consider converting '{prefix_info['prefix']}' actors ({prefix_info['count']} instances) "
                    f"to instanced static meshes for better performance."
                )

        percentage_used = (total_actors / max_actors) * 100 if max_actors > 0 else 0

        return {
            "success": True,
            "total_actors": total_actors,
            "status": status,
            "percentage_of_max": round(percentage_used, 2),
            "top_prefixes": top_prefixes,
            "recommendations": recommendations,
            "message": f"Scene has {total_actors} actors ({status.upper()}). {percentage_used:.1f}% of maximum."
        }

    except Exception as e:
        logger.error(f"validate_scene_performance error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def find_overlapping_actors(
    tolerance: float = 10.0,
    actor_pattern: str = "*"
) -> Dict[str, Any]:
    """
    Find actors that are overlapping or very close to each other.

    Compares the 3D locations of all matching actors and identifies pairs that are
    closer than the specified tolerance distance. Useful for detecting placement errors,
    duplicate actors, or collision issues.

    Args:
        tolerance: Maximum distance (in Unreal units) to consider actors as overlapping
        actor_pattern: Name pattern to filter actors (use "*" for all actors)

    Returns:
        Dict containing:
            - success: Whether the operation completed successfully
            - overlapping_pairs: List of overlapping actor pairs with distances
            - total_checked: Total number of actors checked
            - tolerance: The tolerance value used
            - message: Summary message
    """
    try:
        logger.info(f"Finding overlapping actors (tolerance: {tolerance}, pattern: {actor_pattern})")
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        # Get actors based on pattern
        if actor_pattern == "*":
            response = unreal.send_command("get_actors_in_level", {})
        else:
            response = unreal.send_command("find_actors_by_name", {"pattern": actor_pattern})

        if not response or not response.get("success"):
            return {"success": False, "message": "Failed to get actors from level"}

        actors = response.get("actors", [])
        total_checked = len(actors)

        logger.info(f"Checking {total_checked} actors for overlaps")

        # Find overlapping pairs
        overlapping_pairs: List[Dict[str, Any]] = []

        for i in range(len(actors)):
            actor1 = actors[i]
            name1 = actor1.get("name", "")
            loc1 = actor1.get("location", [0, 0, 0])

            # Ensure location is a list with 3 elements
            if not isinstance(loc1, (list, tuple)) or len(loc1) < 3:
                logger.warning(f"Actor {name1} has invalid location: {loc1}")
                continue

            for j in range(i + 1, len(actors)):
                actor2 = actors[j]
                name2 = actor2.get("name", "")
                loc2 = actor2.get("location", [0, 0, 0])

                # Ensure location is a list with 3 elements
                if not isinstance(loc2, (list, tuple)) or len(loc2) < 3:
                    logger.warning(f"Actor {name2} has invalid location: {loc2}")
                    continue

                # Calculate 3D distance
                dx = loc2[0] - loc1[0]
                dy = loc2[1] - loc1[1]
                dz = loc2[2] - loc1[2]
                distance = math.sqrt(dx * dx + dy * dy + dz * dz)

                # Check if overlapping
                if distance < tolerance:
                    overlapping_pairs.append({
                        "actor1": name1,
                        "actor2": name2,
                        "distance": round(distance, 2),
                        "location1": [round(loc1[0], 2), round(loc1[1], 2), round(loc1[2], 2)],
                        "location2": [round(loc2[0], 2), round(loc2[1], 2), round(loc2[2], 2)]
                    })

        logger.info(f"Found {len(overlapping_pairs)} overlapping pairs")

        return {
            "success": True,
            "overlapping_pairs": overlapping_pairs,
            "total_checked": total_checked,
            "tolerance": tolerance,
            "pattern": actor_pattern,
            "message": f"Found {len(overlapping_pairs)} overlapping pairs out of {total_checked} actors checked"
        }

    except Exception as e:
        logger.error(f"find_overlapping_actors error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def analyze_actor_distribution(grid_size: float = 1000.0) -> Dict[str, Any]:
    """
    Analyze the spatial distribution of actors in the level using a grid-based approach.

    Divides the level space into a grid and counts how many actors fall into each cell.
    Identifies dense areas (hot spots) where many actors are concentrated and sparse
    areas with few or no actors. Useful for level design analysis and optimization.

    Args:
        grid_size: Size of each grid cell in Unreal units (default 1000 = 10 meters)

    Returns:
        Dict containing:
            - success: Whether the operation completed successfully
            - grid_size: The grid cell size used
            - total_actors: Total number of actors analyzed
            - total_cells: Total number of grid cells with at least one actor
            - cells_with_actors: Detailed information about occupied cells
            - densest_cell: Information about the most crowded grid cell
            - sparsest_areas: List of nearly empty regions
            - bounds: Min/max coordinates of all actors
            - message: Summary message
    """
    try:
        logger.info(f"Analyzing actor distribution (grid size: {grid_size})")
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        # Get all actors in the level
        response = unreal.send_command("get_actors_in_level", {})
        if not response or not response.get("success"):
            return {"success": False, "message": "Failed to get actors from level"}

        actors = response.get("actors", [])
        total_actors = len(actors)

        if total_actors == 0:
            return {
                "success": True,
                "message": "No actors found in level",
                "total_actors": 0,
                "grid_size": grid_size
            }

        logger.info(f"Analyzing distribution of {total_actors} actors")

        # Build grid: map (grid_x, grid_y, grid_z) -> list of actor names
        grid: Dict[tuple, List[str]] = {}
        min_x = min_y = min_z = float('inf')
        max_x = max_y = max_z = float('-inf')

        for actor in actors:
            name = actor.get("name", "")
            loc = actor.get("location", [0, 0, 0])

            # Ensure location is valid
            if not isinstance(loc, (list, tuple)) or len(loc) < 3:
                logger.warning(f"Actor {name} has invalid location: {loc}")
                continue

            x, y, z = loc[0], loc[1], loc[2]

            # Update bounds
            min_x, max_x = min(min_x, x), max(max_x, x)
            min_y, max_y = min(min_y, y), max(max_y, y)
            min_z, max_z = min(min_z, z), max(max_z, z)

            # Calculate grid cell
            grid_x = int(x // grid_size)
            grid_y = int(y // grid_size)
            grid_z = int(z // grid_size)

            cell_key = (grid_x, grid_y, grid_z)
            if cell_key not in grid:
                grid[cell_key] = []
            grid[cell_key].append(name)

        total_cells = len(grid)

        # Find densest cell
        densest_cell = None
        max_count = 0
        for cell_coords, actor_list in grid.items():
            if len(actor_list) > max_count:
                max_count = len(actor_list)
                densest_cell = {
                    "grid_coordinates": cell_coords,
                    "world_position": [
                        cell_coords[0] * grid_size + grid_size / 2,
                        cell_coords[1] * grid_size + grid_size / 2,
                        cell_coords[2] * grid_size + grid_size / 2
                    ],
                    "actor_count": max_count,
                    "sample_actors": actor_list[:5]  # Show first 5 actors
                }

        # Find cells with low actor counts (1-2 actors)
        sparsest_cells = []
        for cell_coords, actor_list in grid.items():
            if len(actor_list) <= 2 and len(actor_list) > 0:
                sparsest_cells.append({
                    "grid_coordinates": cell_coords,
                    "world_position": [
                        cell_coords[0] * grid_size + grid_size / 2,
                        cell_coords[1] * grid_size + grid_size / 2,
                        cell_coords[2] * grid_size + grid_size / 2
                    ],
                    "actor_count": len(actor_list),
                    "actors": actor_list
                })

        # Sort sparsest cells and limit to top 10
        sparsest_cells.sort(key=lambda x: x["actor_count"])
        sparsest_areas = sparsest_cells[:10]

        # Build detailed cell information (limit to top 10 most dense cells)
        cells_info = []
        sorted_cells = sorted(grid.items(), key=lambda x: len(x[1]), reverse=True)
        for cell_coords, actor_list in sorted_cells[:10]:
            cells_info.append({
                "grid_coordinates": cell_coords,
                "world_position": [
                    cell_coords[0] * grid_size + grid_size / 2,
                    cell_coords[1] * grid_size + grid_size / 2,
                    cell_coords[2] * grid_size + grid_size / 2
                ],
                "actor_count": len(actor_list),
                "sample_actors": actor_list[:5]
            })

        # Calculate average actors per cell
        avg_actors_per_cell = total_actors / total_cells if total_cells > 0 else 0

        logger.info(f"Distribution analysis complete: {total_cells} cells, densest has {max_count} actors")

        return {
            "success": True,
            "grid_size": grid_size,
            "total_actors": total_actors,
            "total_cells": total_cells,
            "average_actors_per_cell": round(avg_actors_per_cell, 2),
            "densest_cell": densest_cell,
            "top_dense_cells": cells_info,
            "sparsest_areas": sparsest_areas,
            "bounds": {
                "min": [round(min_x, 2), round(min_y, 2), round(min_z, 2)],
                "max": [round(max_x, 2), round(max_y, 2), round(max_z, 2)],
                "size": [
                    round(max_x - min_x, 2),
                    round(max_y - min_y, 2),
                    round(max_z - min_z, 2)
                ]
            },
            "message": f"Analyzed {total_actors} actors across {total_cells} grid cells. Densest cell has {max_count} actors."
        }

    except Exception as e:
        logger.error(f"analyze_actor_distribution error: {e}")
        return {"success": False, "message": str(e)}

# ============================================================================
# Terrain, Path Creation, and Lighting Tools
# ============================================================================

@mcp.tool()
def create_terrain_grid(
    width: int = 10,
    depth: int = 10,
    block_size: float = 100.0,
    height_map: List[float] = None,
    location: List[float] = [0.0, 0.0, 0.0],
    name_prefix: str = "Terrain"
) -> Dict[str, Any]:
    """
    Create a grid of cubes to form terrain with optional height mapping.

    Args:
        width: Number of blocks along X axis
        depth: Number of blocks along Y axis
        block_size: Size of each terrain block in Unreal units (cm)
        height_map: Optional list of height values (width*depth values). If None, creates flat terrain
        location: Starting location [x, y, z] for the terrain grid
        name_prefix: Prefix for naming spawned actors

    Returns:
        Dictionary with success status, spawned actors, and terrain statistics
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Creating terrain grid: {width}x{depth}, block_size={block_size}")
        spawned = []
        scale_xy = block_size / 100.0

        # Validate height_map if provided
        if height_map is not None:
            expected_count = width * depth
            if len(height_map) != expected_count:
                return {
                    "success": False,
                    "message": f"Height map must have exactly {expected_count} values (width*depth), got {len(height_map)}"
                }

        min_height = float('inf')
        max_height = float('-inf')

        # Create the terrain grid
        for z_idx in range(depth):
            for x_idx in range(width):
                # Calculate position
                x = location[0] + x_idx * block_size
                y = location[1] + z_idx * block_size

                # Determine height
                if height_map is not None:
                    height_index = z_idx * width + x_idx
                    height_value = height_map[height_index]
                    z = location[2] + height_value
                    # Calculate scale for height variation
                    scale_z = max(0.1, abs(height_value) / 100.0) if height_value != 0 else scale_xy
                else:
                    z = location[2]
                    scale_z = scale_xy

                min_height = min(min_height, z)
                max_height = max(max_height, z)

                # Spawn terrain block
                actor_name = f"{name_prefix}_{x_idx}_{z_idx}"
                params = {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": [x, y, z],
                    "scale": [scale_xy, scale_xy, scale_z],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                }

                resp = safe_spawn_actor(unreal, params)
                if resp and resp.get("status") == "success":
                    spawned.append(resp)

        logger.info(f"Terrain grid complete! Created {len(spawned)} blocks")

        return {
            "success": True,
            "message": f"Created {width}x{depth} terrain grid with {len(spawned)} blocks",
            "actors": spawned,
            "stats": {
                "width": width,
                "depth": depth,
                "total_blocks": len(spawned),
                "block_size": block_size,
                "height_range": {
                    "min": min_height,
                    "max": max_height,
                    "variation": max_height - min_height
                },
                "has_height_map": height_map is not None
            }
        }

    except Exception as e:
        logger.error(f"create_terrain_grid error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_path(
    waypoints: List[List[float]],
    width: float = 200.0,
    style: str = "road",
    name_prefix: str = "Path"
) -> Dict[str, Any]:
    """
    Create a path connecting waypoints using blocks.

    Args:
        waypoints: List of [x, y, z] coordinates defining the path
        width: Width of the path in Unreal units (cm)
        style: Path style - "road" (standard), "river" (flat), "wall" (tall)
        name_prefix: Prefix for naming spawned actors

    Returns:
        Dictionary with success status, spawned actors, and path statistics
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        if not waypoints or len(waypoints) < 2:
            return {"success": False, "message": "Path requires at least 2 waypoints"}

        logger.info(f"Creating {style} path with {len(waypoints)} waypoints")
        spawned = []
        total_length = 0.0
        block_spacing = 100.0  # Distance between blocks along path

        # Determine scale based on style
        if style == "river":
            scale = [width/100.0, width/100.0, 0.3]  # Flat
            mesh = "/Engine/BasicShapes/Cube.Cube"
        elif style == "wall":
            scale = [width/100.0, width/100.0, 3.0]  # Tall
            mesh = "/Engine/BasicShapes/Cube.Cube"
        else:  # "road" or default
            scale = [width/100.0, width/100.0, 0.5]  # Standard
            mesh = "/Engine/BasicShapes/Cube.Cube"

        segment_count = 0
        block_count = 0

        # Create path segments between waypoints
        for i in range(len(waypoints) - 1):
            start = waypoints[i]
            end = waypoints[i + 1]

            # Calculate segment vector and length
            dx = end[0] - start[0]
            dy = end[1] - start[1]
            dz = end[2] - start[2]
            segment_length = math.sqrt(dx*dx + dy*dy + dz*dz)
            total_length += segment_length

            if segment_length == 0:
                continue

            # Normalize direction
            dir_x = dx / segment_length
            dir_y = dy / segment_length
            dir_z = dz / segment_length

            # Calculate number of blocks needed for this segment
            num_blocks = max(1, int(segment_length / block_spacing))

            # Place blocks along the segment
            for j in range(num_blocks):
                t = j / max(1, num_blocks - 1) if num_blocks > 1 else 0.5
                x = start[0] + dx * t
                y = start[1] + dy * t
                z = start[2] + dz * t

                actor_name = f"{name_prefix}_Seg{segment_count}_Block{j}"
                params = {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": [x, y, z],
                    "scale": scale,
                    "static_mesh": mesh
                }

                resp = safe_spawn_actor(unreal, params)
                if resp and resp.get("status") == "success":
                    spawned.append(resp)
                    block_count += 1

            segment_count += 1

        logger.info(f"Path creation complete! Created {len(spawned)} blocks across {segment_count} segments")

        return {
            "success": True,
            "message": f"Created {style} path with {len(spawned)} blocks",
            "actors": spawned,
            "stats": {
                "style": style,
                "waypoint_count": len(waypoints),
                "total_segments": segment_count,
                "total_blocks": block_count,
                "path_length": total_length,
                "width": width
            }
        }

    except Exception as e:
        logger.error(f"create_path error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def snap_actors_to_height(
    actor_pattern: str,
    target_z: float = 0.0,
    relative: bool = False
) -> Dict[str, Any]:
    """
    Snap actors matching a pattern to a specific height.

    Args:
        actor_pattern: Name pattern to match actors (e.g., "Terrain_*", "Path_*")
        target_z: Target Z coordinate (height)
        relative: If False, set absolute Z. If True, add target_z to current Z

    Returns:
        Dictionary with success status and number of actors modified
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Snapping actors matching '{actor_pattern}' to height {target_z} (relative={relative})")

        # Find all actors matching the pattern
        find_response = unreal.send_command("find_actors_by_name", {"pattern": actor_pattern})

        if not find_response or not find_response.get("success"):
            return {"success": False, "message": f"Failed to find actors matching pattern: {actor_pattern}"}

        actors = find_response.get("actors", [])
        if not actors:
            return {
                "success": True,
                "message": f"No actors found matching pattern: {actor_pattern}",
                "actors_modified": 0
            }

        modified_count = 0
        failed_count = 0

        # Update each actor's Z position
        for actor in actors:
            actor_name = actor.get("name")
            if not actor_name:
                continue

            try:
                if relative:
                    # Get current location first
                    current_location = actor.get("location")
                    if current_location and len(current_location) >= 3:
                        new_z = current_location[2] + target_z
                        new_location = [current_location[0], current_location[1], new_z]
                    else:
                        # If we can't get current location, skip
                        failed_count += 1
                        continue
                else:
                    # For absolute positioning, we need current X and Y
                    current_location = actor.get("location")
                    if current_location and len(current_location) >= 2:
                        new_location = [current_location[0], current_location[1], target_z]
                    else:
                        failed_count += 1
                        continue

                # Set the new transform
                params = {
                    "name": actor_name,
                    "location": new_location
                }

                response = unreal.send_command("set_actor_transform", params)
                if response and response.get("success"):
                    modified_count += 1
                else:
                    failed_count += 1

            except Exception as actor_error:
                logger.error(f"Error updating actor {actor_name}: {actor_error}")
                failed_count += 1

        logger.info(f"Height snap complete! Modified {modified_count} actors, {failed_count} failed")

        return {
            "success": True,
            "message": f"Snapped {modified_count} actors to height {target_z}",
            "actors_modified": modified_count,
            "actors_failed": failed_count,
            "stats": {
                "pattern": actor_pattern,
                "target_z": target_z,
                "relative_mode": relative,
                "total_found": len(actors),
                "modified": modified_count,
                "failed": failed_count
            }
        }

    except Exception as e:
        logger.error(f"snap_actors_to_height error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_lighting_rig(
    style: str = "day",
    intensity: float = 1.0,
    location: List[float] = [0.0, 0.0, 5000.0],
    name_prefix: str = "Light"
) -> Dict[str, Any]:
    """
    Create a lighting rig with multiple lights based on style.

    Args:
        style: Lighting style - "day", "sunset", "night", "dramatic"
        intensity: Base light intensity multiplier
        location: Center location [x, y, z] for the lighting rig
        name_prefix: Prefix for naming light actors

    Returns:
        Dictionary with success status and spawned light actors
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        logger.info(f"Creating {style} lighting rig at {location}")
        spawned = []
        light_positions = []

        # Define lighting configurations for each style
        if style == "day":
            # Bright overhead sun
            lights_config = [
                {"name": "Sun_Main", "offset": [0, 0, 0], "scale": [intensity * 5.0] * 3},
                {"name": "Fill_Sky", "offset": [2000, 2000, 500], "scale": [intensity * 2.0] * 3},
            ]
        elif style == "sunset":
            # Warm angled light
            lights_config = [
                {"name": "Sun_Sunset", "offset": [-3000, 0, -1000], "scale": [intensity * 4.0] * 3},
                {"name": "Fill_Warm", "offset": [1000, 1000, 0], "scale": [intensity * 1.5] * 3},
                {"name": "Ambient_Orange", "offset": [0, 2000, 500], "scale": [intensity * 1.0] * 3},
            ]
        elif style == "night":
            # Dim moonlight
            lights_config = [
                {"name": "Moon_Main", "offset": [0, -2000, 1000], "scale": [intensity * 0.8] * 3},
                {"name": "Stars_Ambient", "offset": [1500, 1500, 500], "scale": [intensity * 0.3] * 3},
            ]
        elif style == "dramatic":
            # Strong side light with rim
            lights_config = [
                {"name": "Key_Light", "offset": [-4000, 0, 1000], "scale": [intensity * 6.0] * 3},
                {"name": "Rim_Light", "offset": [4000, 0, 2000], "scale": [intensity * 3.0] * 3},
                {"name": "Fill_Soft", "offset": [0, 3000, 500], "scale": [intensity * 1.0] * 3},
            ]
        else:
            return {"success": False, "message": f"Unknown lighting style: {style}. Use 'day', 'sunset', 'night', or 'dramatic'"}

        # Spawn each light
        for light_config in lights_config:
            light_name = f"{name_prefix}_{light_config['name']}"
            offset = light_config['offset']
            light_location = [
                location[0] + offset[0],
                location[1] + offset[1],
                location[2] + offset[2]
            ]

            params = {
                "name": light_name,
                "type": "PointLight",  # Using PointLight as it's commonly available
                "location": light_location,
                "scale": light_config['scale']
            }

            resp = safe_spawn_actor(unreal, params)
            if resp and resp.get("status") == "success":
                spawned.append(resp)
                light_positions.append({
                    "name": light_name,
                    "location": light_location
                })

        logger.info(f"Lighting rig complete! Created {len(spawned)} lights")

        return {
            "success": True,
            "message": f"Created {style} lighting rig with {len(spawned)} lights",
            "actors": spawned,
            "stats": {
                "style": style,
                "lights_created": len(spawned),
                "base_intensity": intensity,
                "center_location": location,
                "light_positions": light_positions
            }
        }

    except Exception as e:
        logger.error(f"create_lighting_rig error: {e}")
        return {"success": False, "message": str(e)}


# Run the server
if __name__ == "__main__":
    logger.info("Starting Advanced MCP server with stdio transport")
    mcp.run(transport='stdio') 