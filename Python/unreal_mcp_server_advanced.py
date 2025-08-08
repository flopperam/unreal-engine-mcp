"""
Unreal Engine Advanced MCP Server

A streamlined MCP server focused on advanced composition tools for Unreal Engine.
Contains only the advanced tools from the expanded MCP tool system to keep tool count manageable.
"""

import logging
import socket
import sys
import json
import math
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
import os, sys
# Ensure local helpers are importable regardless of CWD
sys.path.insert(0, os.path.dirname(__file__))

from helpers.town_helpers import (
    create_street_grid as helper_create_street_grid,
    create_street_lights as helper_create_street_lights,
    create_traffic_lights as helper_create_traffic_lights,
    create_street_signage as helper_create_street_signage,
    create_town_vehicles as helper_create_town_vehicles,
    create_town_decorations as helper_create_town_decorations,
    create_urban_furniture as helper_create_urban_furniture,
    create_sidewalks_crosswalks as helper_create_sidewalks_crosswalks,
    create_street_utilities as helper_create_street_utilities,
    create_central_plaza as helper_create_central_plaza,

)
from helpers.town_buildings import (
    create_town_building as helper_create_town_building,
)
from mcp.server.fastmcp import FastMCP, Context

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

# Spatial Awareness System
class SpatialManager:
    """Manages spatial awareness to prevent overlapping structures."""
    
    def __init__(self):
        self.occupied_regions = []  # List of (min_x, min_y, min_z, max_x, max_y, max_z, name)
    
    def add_region(self, location: List[float], size: List[float], name: str):
        """Add an occupied region to track."""
        min_x = location[0] - size[0]/2
        max_x = location[0] + size[0]/2
        min_y = location[1] - size[1]/2
        max_y = location[1] + size[1]/2
        min_z = location[2] - size[2]/2
        max_z = location[2] + size[2]/2
        self.occupied_regions.append((min_x, min_y, min_z, max_x, max_y, max_z, name))
    
    def check_collision(self, location: List[float], size: List[float], buffer: float = 100.0) -> bool:
        """Check if a proposed structure would collide with existing ones."""
        min_x = location[0] - size[0]/2 - buffer
        max_x = location[0] + size[0]/2 + buffer
        min_y = location[1] - size[1]/2 - buffer
        max_y = location[1] + size[1]/2 + buffer
        min_z = location[2] - size[2]/2 - buffer
        max_z = location[2] + size[2]/2 + buffer
        
        for region in self.occupied_regions:
            r_min_x, r_min_y, r_min_z, r_max_x, r_max_y, r_max_z, r_name = region
            
            # Check for overlap in all three dimensions
            if (min_x < r_max_x and max_x > r_min_x and
                min_y < r_max_y and max_y > r_min_y and
                min_z < r_max_z and max_z > r_min_z):
                return True
        return False
    
    def find_clear_location(self, preferred_location: List[float], size: List[float], 
                          search_radius: float = 3000.0, buffer: float = 200.0) -> List[float]:
        """Find a clear location near the preferred location - ONLY searches horizontally."""
        # Try the preferred location first
        if not self.check_collision(preferred_location, size, buffer):
            return preferred_location
        
        # Search in expanding circles - ONLY X,Y movement, keep same Z
        for radius in range(int(buffer * 2), int(search_radius), 300):
            for angle in range(0, 360, 20):  # Check every 20 degrees for more options
                rad = math.radians(angle)
                test_location = [
                    preferred_location[0] + radius * math.cos(rad),
                    preferred_location[1] + radius * math.sin(rad),
                    preferred_location[2]  # KEEP SAME Z - DON'T ELEVATE
                ]
                
                if not self.check_collision(test_location, size, buffer):
                    return test_location
        
        # If still no clear location, try a few more distant spots
        for i in range(5):
            distant_location = [
                preferred_location[0] + (i + 1) * search_radius * (1 if i % 2 == 0 else -1),
                preferred_location[1] + (i + 1) * search_radius * (1 if i % 3 == 0 else -1),
                preferred_location[2]  # KEEP SAME Z
            ]
            if not self.check_collision(distant_location, size, buffer):
                return distant_location
        
        # Last resort: return original location (let it overlap rather than float)
        logger.warning(f"Could not find clear location, using original: {preferred_location}")
        return preferred_location

# Global spatial manager instance
spatial_manager = SpatialManager()

def refresh_spatial_awareness():
    """Refresh spatial awareness by querying existing actors."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return
        
        # Clear existing regions
        spatial_manager.occupied_regions.clear()
        
        # Get all actors in level
        response = unreal.send_command("get_actors_in_level", {})
        if not response or not response.get("success"):
            return
        
        actors = response.get("actors", [])
        for actor in actors:
            if "location" in actor and "name" in actor:
                # Estimate size based on actor type and scale
                scale = actor.get("scale", [1, 1, 1])
                base_size = [200, 200, 200]  # Default size
                
                # Adjust size based on actor type and name patterns
                actor_name = actor["name"].lower()
                if "tower" in actor_name:
                    base_size = [800, 800, 1000]
                elif "wall" in actor_name or "terrace" in actor_name:
                    base_size = [300, 300, 300]
                elif "maze" in actor_name:
                    base_size = [1500, 1500, 200]
                elif "arch" in actor_name:
                    base_size = [500, 200, 500]
                elif "stair" in actor_name:
                    base_size = [200, 200, 100]
                elif "house" in actor_name:
                    base_size = [1200, 1000, 600]
                elif "pyramid" in actor_name:
                    base_size = [600, 600, 400]
                
                # Apply scale - be more conservative with scaling
                actual_size = [base_size[i] * max(scale[i], 1.0) for i in range(3)]
                spatial_manager.add_region(actor["location"], actual_size, actor["name"])
                
        logger.info(f"Refreshed spatial awareness with {len(spatial_manager.occupied_regions)} regions")
        
    except Exception as e:
        logger.error(f"Failed to refresh spatial awareness: {e}")

def get_safe_location(preferred_location: List[float], structure_size: List[float], 
                     structure_name: str = "structure") -> List[float]:
    """Get a safe location for building, avoiding collisions."""
    # Refresh spatial awareness
    refresh_spatial_awareness()
    
    # Find clear location
    safe_location = spatial_manager.find_clear_location(preferred_location, structure_size)
    
    # Add this structure to spatial manager
    spatial_manager.add_region(safe_location, structure_size, structure_name)
    
    logger.info(f"Safe location for {structure_name}: {safe_location} (preferred: {preferred_location})")
    return safe_location

@mcp.tool()
def clear_spatial_awareness() -> Dict[str, Any]:
    """Clear the spatial awareness cache. Useful when manually deleting actors."""
    try:
        spatial_manager.occupied_regions.clear()
        logger.info("Spatial awareness cache cleared")
        return {"success": True, "message": "Spatial awareness cache cleared"}
    except Exception as e:
        logger.error(f"clear_spatial_awareness error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool() 
def test_spatial_system(base_location: List[float] = [0, 0, 0]) -> Dict[str, Any]:
    """Test the spatial awareness system by building several structures that should not overlap."""
    try:
        results = []
        
        # Test 1: Create a tower
        tower_result = create_tower(
            height=5, 
            base_size=3, 
            location=base_location, 
            name_prefix="TestTower"
        )
        results.append({"structure": "tower", "result": tower_result})
        
        # Test 2: Try to create another tower nearby (should be repositioned)
        tower2_result = create_tower(
            height=4, 
            base_size=2, 
            location=[base_location[0] + 100, base_location[1], base_location[2]], 
            name_prefix="TestTower2"
        )
        results.append({"structure": "tower2", "result": tower2_result})
        
        # Test 3: Create a maze nearby (should be repositioned)
        maze_result = create_maze(
            rows=4, 
            cols=4, 
            location=[base_location[0], base_location[1] + 200, base_location[2]], 
            cell_size=200
        )
        results.append({"structure": "maze", "result": maze_result})
        
        # Test 4: Create a staircase (should be repositioned)
        stair_result = create_staircase(
            steps=6, 
            location=[base_location[0] - 200, base_location[1], base_location[2]], 
            name_prefix="TestStair"
        )
        results.append({"structure": "staircase", "result": stair_result})
        
        return {
            "success": True, 
            "message": "Spatial awareness test completed",
            "results": results,
            "occupied_regions": len(spatial_manager.occupied_regions)
        }
        
    except Exception as e:
        logger.error(f"test_spatial_system error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def spawn_actor(
    name: str,
    type: str,
    location: List[float] = [0, 0, 0],
    rotation: List[float] = [0, 0, 0]
) -> Dict[str, Any]:
    """Create a new actor in the current level."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "name": name,
            "type": type,
            "location": location,
            "rotation": rotation
        }
        response = unreal.send_command("spawn_actor", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"spawn_actor error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def delete_actor(name: str) -> Dict[str, Any]:
    """Delete an actor by name."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        response = unreal.send_command("delete_actor", {"name": name})
        return response or {"success": False, "message": "No response from Unreal"}
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

@mcp.tool()
def spawn_blueprint_actor(
    blueprint_name: str,
    actor_name: str,
    location: List[float] = [0, 0, 0],
    rotation: List[float] = [0, 0, 0]
) -> Dict[str, Any]:
    """Spawn an actor from a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    
    try:
        params = {
            "blueprint_name": blueprint_name,
            "actor_name": actor_name,
            "location": location,
            "rotation": rotation
        }
        response = unreal.send_command("spawn_blueprint_actor", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"spawn_blueprint_actor error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def configure_blueprint(
    blueprint_name: str,
    components: List[Dict[str, Any]],
    compile_at_end: bool = True
) -> Dict[str, Any]:
    """Configure a Blueprint in one call and optionally compile at the end.

    This function attaches components, applies transforms, sets meshes and colors,
    then compiles the Blueprint when finished. It eliminates the need to call
    multiple functions for common setup flows.

    Each item in the `components` list can include:
    - component_type: e.g. "StaticMeshComponent" (required)
    - component_name: unique name for the component (required)
    - location: [x, y, z]
    - rotation: [pitch, yaw, roll]
    - scale: [sx, sy, sz]
    - component_properties: dict of additional properties for `add_component_to_blueprint`
    - static_mesh: asset path to assign to the component's mesh
    - color: [r, g, b, a] where values are 0..1, applied to BaseColor/Color
    - material_path: optional material to apply when setting color
    - parameter_name: optional material parameter name to prioritize (default BaseColor)
    """
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    overall_success = True
    component_results: List[Dict[str, Any]] = []

    for component in components or []:
        result: Dict[str, Any] = {
            "component_name": component.get("component_name"),
            "component_type": component.get("component_type"),
            "steps": {}
        }
        try:
            # 1) Attach/add the component
            add_params = {
                "blueprint_name": blueprint_name,
                "component_type": component.get("component_type"),
                "component_name": component.get("component_name"),
                "location": component.get("location", []),
                "rotation": component.get("rotation", []),
                "scale": component.get("scale", []),
                "component_properties": component.get("component_properties", {})
            }
            add_resp = unreal.send_command("add_component_to_blueprint", add_params)
            result["steps"]["add_component_to_blueprint"] = add_resp
            if not (add_resp and add_resp.get("success")):
                overall_success = False

            # 2) Assign static mesh if provided
            if component.get("static_mesh"):
                mesh_resp = unreal.send_command(
                    "set_static_mesh_properties",
                    {
                        "blueprint_name": blueprint_name,
                        "component_name": component.get("component_name"),
                        "static_mesh": component.get("static_mesh")
                    }
                )
                result["steps"]["set_static_mesh_properties"] = mesh_resp
                if not (mesh_resp and mesh_resp.get("success")):
                    overall_success = False

            # 3) Apply color if provided
            if component.get("color") is not None:
                color_list = component.get("color")
                try:
                    # Clamp color values to [0,1]
                    color_list = [float(min(1.0, max(0.0, v))) for v in color_list]
                except Exception:
                    pass

                material_path = component.get(
                    "material_path",
                    "/Engine/BasicShapes/BasicShapeMaterial"
                )
                preferred_param = component.get("parameter_name", "BaseColor")

                # Try preferred parameter first, then a fallback to "Color"
                base_resp = unreal.send_command(
                    "set_mesh_material_color",
                    {
                        "blueprint_name": blueprint_name,
                        "component_name": component.get("component_name"),
                        "color": color_list,
                        "material_path": material_path,
                        "parameter_name": preferred_param
                    }
                )
                color_resp = unreal.send_command(
                    "set_mesh_material_color",
                    {
                        "blueprint_name": blueprint_name,
                        "component_name": component.get("component_name"),
                        "color": color_list,
                        "material_path": material_path,
                        "parameter_name": "Color"
                    }
                )
                result["steps"]["set_mesh_material_color"] = {
                    "preferred": base_resp,
                    "fallback": color_resp
                }
                if not ((base_resp and base_resp.get("success")) or (color_resp and color_resp.get("success"))):
                    overall_success = False

        except Exception as inner_e:
            result["error"] = str(inner_e)
            overall_success = False

        component_results.append(result)

    compile_result: Dict[str, Any] = {}
    if compile_at_end:
        try:
            compile_result = unreal.send_command("compile_blueprint", {"blueprint_name": blueprint_name}) or {}
            if not compile_result.get("success"):
                overall_success = False
        except Exception as e:
            compile_result = {"success": False, "message": str(e)}
            overall_success = False

    return {
        "success": overall_success,
        "blueprint_name": blueprint_name,
        "components": component_results,
        "compile_result": compile_result
    }

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
        
        # Calculate pyramid dimensions for spatial awareness
        pyramid_width = base_size * block_size
        pyramid_height = base_size * block_size  # Pyramid height
        pyramid_size = [pyramid_width, pyramid_width, pyramid_height]
        
        # Get safe location
        safe_location = get_safe_location(location, pyramid_size, f"{name_prefix}_Pyramid")
        spawned = []
        scale = block_size / 100.0
        for level in range(base_size):
            count = base_size - level
            for x in range(count):
                for y in range(count):
                    actor_name = f"{name_prefix}_{level}_{x}_{y}"
                    loc = [
                        safe_location[0] + (x - (count - 1)/2) * block_size,
                        safe_location[1] + (y - (count - 1)/2) * block_size,
                        safe_location[2] + level * block_size
                    ]
                    params = {
                        "name": actor_name,
                        "type": "StaticMeshActor",
                        "location": loc,
                        "scale": [scale, scale, scale],
                        "static_mesh": mesh
                    }
                    resp = unreal.send_command("spawn_actor", params)
                    if resp:
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
        
        # Calculate wall dimensions for spatial awareness
        if orientation == "x":
            wall_size = [length * block_size, block_size, height * block_size]
        else:
            wall_size = [block_size, length * block_size, height * block_size]
        
        # Get safe location
        safe_location = get_safe_location(location, wall_size, f"{name_prefix}_Wall")
        
        spawned = []
        scale = block_size / 100.0
        for h in range(height):
            for i in range(length):
                actor_name = f"{name_prefix}_{h}_{i}"
                if orientation == "x":
                    loc = [safe_location[0] + i * block_size, safe_location[1], safe_location[2] + h * block_size]
                else:
                    loc = [safe_location[0], safe_location[1] + i * block_size, safe_location[2] + h * block_size]
                params = {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": loc,
                    "scale": [scale, scale, scale],
                    "static_mesh": mesh
                }
                resp = unreal.send_command("spawn_actor", params)
                if resp:
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
        
        # Calculate tower dimensions for spatial awareness
        tower_width = base_size * block_size * 1.2  # Add some buffer
        tower_height = height * block_size
        tower_size = [tower_width, tower_width, tower_height]
        
        # Get safe location
        safe_location = get_safe_location(location, tower_size, f"{name_prefix}_Tower")
        spawned = []
        scale = block_size / 100.0

        for level in range(height):
            level_height = safe_location[2] + level * block_size
            
            if tower_style == "cylindrical":
                # Create circular tower
                radius = (base_size / 2) * block_size  # Convert to world units (centimeters)
                circumference = 2 * math.pi * radius
                num_blocks = max(8, int(circumference / block_size))
                
                for i in range(num_blocks):
                    angle = (2 * math.pi * i) / num_blocks
                    x = safe_location[0] + radius * math.cos(angle)
                    y = safe_location[1] + radius * math.sin(angle)
                    
                    actor_name = f"{name_prefix}_{level}_{i}"
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
                        
            elif tower_style == "tapered":
                # Create tapering square tower
                current_size = max(1, base_size - (level // 2))
                half_size = current_size / 2
                
                # Create walls for current level
                for side in range(4):
                    for i in range(current_size):
                        if side == 0:  # Front wall
                            x = safe_location[0] + (i - half_size + 0.5) * block_size
                            y = safe_location[1] - half_size * block_size
                            actor_name = f"{name_prefix}_{level}_front_{i}"
                        elif side == 1:  # Right wall
                            x = safe_location[0] + half_size * block_size
                            y = safe_location[1] + (i - half_size + 0.5) * block_size
                            actor_name = f"{name_prefix}_{level}_right_{i}"
                        elif side == 2:  # Back wall
                            x = safe_location[0] + (half_size - i - 0.5) * block_size
                            y = safe_location[1] + half_size * block_size
                            actor_name = f"{name_prefix}_{level}_back_{i}"
                        else:  # Left wall
                            x = safe_location[0] - half_size * block_size
                            y = safe_location[1] + (half_size - i - 0.5) * block_size
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
                            x = safe_location[0] + (i - half_size + 0.5) * block_size
                            y = safe_location[1] - half_size * block_size
                            actor_name = f"{name_prefix}_{level}_front_{i}"
                        elif side == 1:  # Right wall
                            x = safe_location[0] + half_size * block_size
                            y = safe_location[1] + (i - half_size + 0.5) * block_size
                            actor_name = f"{name_prefix}_{level}_right_{i}"
                        elif side == 2:  # Back wall
                            x = safe_location[0] + (half_size - i - 0.5) * block_size
                            y = safe_location[1] + half_size * block_size
                            actor_name = f"{name_prefix}_{level}_back_{i}"
                        else:  # Left wall
                            x = safe_location[0] - half_size * block_size
                            y = safe_location[1] + (half_size - i - 0.5) * block_size
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
                    detail_x = safe_location[0] + (base_size/2 + 0.5) * block_size * math.cos(angle)
                    detail_y = safe_location[1] + (base_size/2 + 0.5) * block_size * math.sin(angle)
                    
                    actor_name = f"{name_prefix}_{level}_detail_{corner}"
                    params = {
                        "name": actor_name,
                        "type": "StaticMeshActor",
                        "location": [detail_x, detail_y, level_height],
                        "scale": [scale * 0.7, scale * 0.7, scale * 0.7],
                        "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                    }
                    resp = unreal.send_command("spawn_actor", params)
                    if resp:
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
        
        # Calculate staircase dimensions for spatial awareness
        sx, sy, sz = step_size
        staircase_width = steps * sx
        staircase_depth = sy
        staircase_height = steps * sz
        staircase_size = [staircase_width, staircase_depth, staircase_height]
        
        # Get safe location
        safe_location = get_safe_location(location, staircase_size, f"{name_prefix}_Staircase")
        
        spawned = []
        for i in range(steps):
            actor_name = f"{name_prefix}_{i}"
            loc = [safe_location[0] + i * sx, safe_location[1], safe_location[2] + i * sz]
            scale = [sx/100.0, sy/100.0, sz/100.0]
            params = {
                "name": actor_name,
                "type": "StaticMeshActor",
                "location": loc,
                "scale": scale,
                "static_mesh": mesh
            }
            resp = unreal.send_command("spawn_actor", params)
            if resp:
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
    house_style: str = "modern"  # "modern", "cottage", "mansion"
) -> Dict[str, Any]:
    """Construct a realistic house with architectural details and multiple rooms."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        # Adjust dimensions based on style
        if house_style == "mansion":
            width = int(width * 1.5)
            depth = int(depth * 1.5)
            height = int(height * 1.3)
        elif house_style == "cottage":
            width = int(width * 0.8)
            depth = int(depth * 0.8)
            height = int(height * 0.9)
        
        # Calculate house dimensions for spatial awareness
        house_size = [width + 200, depth + 200, height]  # Add buffer for foundation
        
        # Get safe location
        safe_location = get_safe_location(location, house_size, f"{name_prefix}_House")
            
        results = []
        wall_thickness = 20.0  # Thinner walls for realism
        floor_thickness = 30.0
        
        # Create foundation as single large block
        foundation_params = {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [safe_location[0], safe_location[1], safe_location[2] - floor_thickness/2],
            "scale": [(width + 200)/100.0, (depth + 200)/100.0, floor_thickness/100.0],
            "static_mesh": mesh
        }
        foundation_resp = unreal.send_command("spawn_actor", foundation_params)
        if foundation_resp:
            results.append(foundation_resp)
        
        # Create floor as single piece
        floor_params = {
            "name": f"{name_prefix}_Floor",
            "type": "StaticMeshActor",
            "location": [safe_location[0], safe_location[1], safe_location[2] + floor_thickness/2],
            "scale": [width/100.0, depth/100.0, floor_thickness/100.0],
            "static_mesh": mesh
        }
        floor_resp = unreal.send_command("spawn_actor", floor_params)
        if floor_resp:
            results.append(floor_resp)
        
        base_z = safe_location[2] + floor_thickness
        
        # Create walls as large segments
        # Front wall (with door opening)
        door_width = 120.0
        door_height = 240.0
        
        # Front wall - left side of door
        front_left_width = (width/2 - door_width/2)
        front_left_params = {
            "name": f"{name_prefix}_FrontWall_Left",
            "type": "StaticMeshActor",
            "location": [safe_location[0] - width/4 - door_width/4, safe_location[1] - depth/2, base_z + height/2],
            "scale": [front_left_width/100.0, wall_thickness/100.0, height/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", front_left_params)
        if resp:
            results.append(resp)
        
        # Front wall - right side of door
        front_right_params = {
            "name": f"{name_prefix}_FrontWall_Right",
            "type": "StaticMeshActor",
            "location": [safe_location[0] + width/4 + door_width/4, safe_location[1] - depth/2, base_z + height/2],
            "scale": [front_left_width/100.0, wall_thickness/100.0, height/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", front_right_params)
        if resp:
            results.append(resp)
        
        # Front wall - above door
        front_top_params = {
            "name": f"{name_prefix}_FrontWall_Top",
            "type": "StaticMeshActor",
            "location": [safe_location[0], safe_location[1] - depth/2, base_z + door_height + (height - door_height)/2],
            "scale": [door_width/100.0, wall_thickness/100.0, (height - door_height)/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", front_top_params)
        if resp:
            results.append(resp)
        
        # Back wall with window openings
        window_width = 150.0
        window_height = 150.0
        window_y = base_z + height/2
        
        # Back wall - left section
        back_left_params = {
            "name": f"{name_prefix}_BackWall_Left",
            "type": "StaticMeshActor",
            "location": [safe_location[0] - width/3, safe_location[1] + depth/2, base_z + height/2],
            "scale": [width/3/100.0, wall_thickness/100.0, height/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", back_left_params)
        if resp:
            results.append(resp)
        
        # Back wall - center section (with window cutouts)
        back_center_bottom_params = {
            "name": f"{name_prefix}_BackWall_Center_Bottom",
            "type": "StaticMeshActor",
            "location": [safe_location[0], safe_location[1] + depth/2, base_z + (window_y - window_height/2 - base_z)/2],
            "scale": [width/3/100.0, wall_thickness/100.0, (window_y - window_height/2 - base_z)/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", back_center_bottom_params)
        if resp:
            results.append(resp)
        
        back_center_top_params = {
            "name": f"{name_prefix}_BackWall_Center_Top",
            "type": "StaticMeshActor",
            "location": [safe_location[0], safe_location[1] + depth/2, window_y + window_height/2 + (base_z + height - window_y - window_height/2)/2],
            "scale": [width/3/100.0, wall_thickness/100.0, (base_z + height - window_y - window_height/2)/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", back_center_top_params)
        if resp:
            results.append(resp)
        
        # Back wall - right section
        back_right_params = {
            "name": f"{name_prefix}_BackWall_Right",
            "type": "StaticMeshActor",
            "location": [safe_location[0] + width/3, safe_location[1] + depth/2, base_z + height/2],
            "scale": [width/3/100.0, wall_thickness/100.0, height/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", back_right_params)
        if resp:
            results.append(resp)
        
        # Left wall
        left_wall_params = {
            "name": f"{name_prefix}_LeftWall",
            "type": "StaticMeshActor",
            "location": [safe_location[0] - width/2, safe_location[1], base_z + height/2],
            "scale": [wall_thickness/100.0, depth/100.0, height/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", left_wall_params)
        if resp:
            results.append(resp)
        
        # Right wall  
        right_wall_params = {
            "name": f"{name_prefix}_RightWall",
            "type": "StaticMeshActor",
            "location": [safe_location[0] + width/2, safe_location[1], base_z + height/2],
            "scale": [wall_thickness/100.0, depth/100.0, height/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", right_wall_params)
        if resp:
            results.append(resp)
        
                    # Create flat roof
            roof_thickness = 30.0
            roof_overhang = 100.0
            
            # Single flat roof piece covering the entire house
            flat_roof_params = {
                "name": f"{name_prefix}_Roof",
                "type": "StaticMeshActor",
                "location": [
                    safe_location[0],
                    safe_location[1],
                    base_z + height + roof_thickness/2
                ],
                "rotation": [0, 0, 0],  # No rotation - flat roof
                "scale": [(width + roof_overhang*2)/100.0, (depth + roof_overhang*2)/100.0, roof_thickness/100.0],
                "static_mesh": mesh
            }
            resp = unreal.send_command("spawn_actor", flat_roof_params)
            if resp:
                results.append(resp)
        
                    # Add chimney for cottage/mansion styles
            if house_style in ["cottage", "mansion"]:
                chimney_params = {
                    "name": f"{name_prefix}_Chimney",
                    "type": "StaticMeshActor",
                    "location": [
                        safe_location[0] + width/3,
                        safe_location[1] + depth/3,
                        base_z + height + roof_thickness + 150  # Position above flat roof
                    ],
                    "scale": [1.0, 1.0, 2.5],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                }
                resp = unreal.send_command("spawn_actor", chimney_params)
                if resp:
                    results.append(resp)
        
        # Add porch for mansion style
        if house_style == "mansion":
            # Porch floor
            porch_floor_params = {
                "name": f"{name_prefix}_Porch_Floor",
                "type": "StaticMeshActor",
                "location": [safe_location[0], safe_location[1] - depth/2 - 150, location[2]],
                "scale": [width/100.0, 3.0, 0.3],
                "static_mesh": mesh
            }
            resp = unreal.send_command("spawn_actor", porch_floor_params)
            if resp:
                results.append(resp)
            
            # Porch columns
            for i, x_offset in enumerate([-width/3, 0, width/3]):
                column_params = {
                    "name": f"{name_prefix}_Porch_Column_{i}",
                    "type": "StaticMeshActor",
                    "location": [
                        safe_location[0] + x_offset,
                        safe_location[1] - depth/2 - 250,
                        base_z + height/2
                    ],
                    "scale": [0.5, 0.5, height/100.0],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                }
                resp = unreal.send_command("spawn_actor", column_params)
                if resp:
                    results.append(resp)
            
                            # Porch roof
                porch_roof_params = {
                    "name": f"{name_prefix}_Porch_Roof",
                    "type": "StaticMeshActor",
                    "location": [safe_location[0], safe_location[1] - depth/2 - 150, base_z + height - 50],
                    "scale": [(width + 100)/100.0, 4.0, 0.3],  # Consistent thickness with main roof
                    "static_mesh": mesh
                }
                resp = unreal.send_command("spawn_actor", porch_roof_params)
                if resp:
                    results.append(resp)
        
        # Add details based on style
        if house_style == "modern":
            # Add garage door
            garage_params = {
                "name": f"{name_prefix}_Garage_Door",
                "type": "StaticMeshActor",
                "location": [safe_location[0] - width/3, safe_location[1] - depth/2 + wall_thickness/2, base_z + 150],
                "scale": [2.5, 0.1, 2.5],
                "static_mesh": mesh
            }
            resp = unreal.send_command("spawn_actor", garage_params)
            if resp:
                results.append(resp)
        
        return {
            "success": True,
            "actors": results,
            "house_style": house_style,
            "dimensions": {"width": width, "depth": depth, "height": height},
            "features": [
                "foundation", "floor", "walls", "windows", "door", "flat_roof"
            ] + (["chimney"] if house_style in ["cottage", "mansion"] else []) + 
                (["porch", "columns"] if house_style == "mansion" else []) +
                (["garage"] if house_style == "modern" else []),
            "total_actors": len(results)
        }
    except Exception as e:
        logger.error(f"construct_house error: {e}")
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
        
        # Calculate arch dimensions for spatial awareness
        arch_width = radius * 2
        arch_depth = 200  # Arch depth
        arch_height = radius
        arch_size = [arch_width, arch_depth, arch_height]
        
        # Get safe location
        safe_location = get_safe_location(location, arch_size, f"{name_prefix}_Arch")
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
                "location": [safe_location[0] + x, safe_location[1], safe_location[2] + z],
                "scale": [scale, scale, scale],
                "static_mesh": mesh
            }
            resp = unreal.send_command("spawn_actor", params)
            if resp:
                spawned.append(resp)
        return {"success": True, "actors": spawned}
    except Exception as e:
        logger.error(f"create_arch error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def spawn_physics_actor(
    name: str,
    mesh_path: str = "/Engine/BasicShapes/Cube.Cube",
    location: List[float] = [0.0, 0.0, 0.0],
    mass: float = 1.0,
    simulate_physics: bool = True,
    gravity_enabled: bool = True,
    color: List[float] = None,  # Optional color parameter [R, G, B, A]
    scale: List[float] = [1.0, 1.0, 1.0]  # Default scale
) -> Dict[str, Any]:
    """Spawn an actor with physics properties using a temporary Blueprint."""
    try:
        bp_name = f"{name}_BP"
        create_blueprint(bp_name, "Actor")
        add_component_to_blueprint(bp_name, "StaticMeshComponent", "Mesh", scale=scale)
        set_static_mesh_properties(bp_name, "Mesh", mesh_path)
        set_physics_properties(bp_name, "Mesh", simulate_physics, gravity_enabled, mass)
        
        # Set color if provided
        if color is not None:
            set_mesh_material_color(bp_name, "Mesh", color)
        
        compile_blueprint(bp_name)
        result = spawn_blueprint_actor(bp_name, name, location)
        
        # Ensure proper scale is set on the spawned actor
        if result.get("success", False):
            spawned_name = result.get("result", {}).get("name", name)
            set_actor_transform(spawned_name, scale=scale)
        
        return result
    except Exception as e:
        logger.error(f"spawn_physics_actor error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def create_bouncy_ball(
    name: str,
    location: List[float] = [0.0, 0.0, 0.0],
    color: List[float] = [1.0, 0.0, 0.0, 1.0],  # Default red color
    size: float = 2.0  # Default size multiplier
) -> Dict[str, Any]:
    """Convenience function to spawn a bouncing sphere with color and size control."""
    try:
        # Create the physics blueprint
        bp_name = f"{name}_BP"
        
        # Create blueprint
        create_result = create_blueprint(bp_name, "Actor")
        if not create_result.get("success", False):
            return {"success": False, "message": f"Failed to create blueprint: {create_result.get('message', 'Unknown error')}"}
        
        # Add mesh component
        add_comp_result = add_component_to_blueprint(bp_name, "StaticMeshComponent", "Mesh")
        if not add_comp_result.get("success", False):
            return {"success": False, "message": f"Failed to add component: {add_comp_result.get('message', 'Unknown error')}"}
        
        # Set sphere mesh
        mesh_result = set_static_mesh_properties(bp_name, "Mesh", "/Engine/BasicShapes/Sphere.Sphere")
        if not mesh_result.get("success", False):
            return {"success": False, "message": f"Failed to set mesh: {mesh_result.get('message', 'Unknown error')}"}
        
        # Set physics properties for bouncing
        physics_result = set_physics_properties(
            bp_name, "Mesh", 
            simulate_physics=True, 
            gravity_enabled=True, 
            mass=1.0, 
            linear_damping=0.1,  # Some damping for realistic bouncing
            angular_damping=0.1
        )
        if not physics_result.get("success", False):
            return {"success": False, "message": f"Failed to set physics: {physics_result.get('message', 'Unknown error')}"}
        
        # Set color using the proven color system
        color_result = set_mesh_material_color(
            bp_name, 
            "Mesh", 
            color, 
            material_path="/Engine/BasicShapes/BasicShapeMaterial"
        )
        if not color_result.get("success", False):
            # Don't fail if color setting fails, just warn
            logger.warning(f"Failed to set color: {color_result.get('message', 'Unknown error')}")
        
        # Compile blueprint
        compile_result = compile_blueprint(bp_name)
        if not compile_result.get("success", False):
            return {"success": False, "message": f"Failed to compile blueprint: {compile_result.get('message', 'Unknown error')}"}
        
        # Spawn the actor with proper scale
        spawn_result = spawn_blueprint_actor(bp_name, name, location)
        if not spawn_result.get("success", False):
            return {"success": False, "message": f"Failed to spawn actor: {spawn_result.get('message', 'Unknown error')}"}
        
        # Get the actual spawned actor name from the response
        spawned_actor_name = spawn_result.get("result", {}).get("name", name)
        
        # Set the scale to make it visible and sized appropriately
        scale_result = set_actor_transform(spawned_actor_name, scale=[size, size, size])
        if not scale_result.get("success", False):
            logger.warning(f"Failed to set scale: {scale_result.get('message', 'Unknown error')}")
        
        return {
            "success": True,
            "actor": spawn_result.get("actor", {}),
            "color": color,
            "size": size,
            "message": f"Created bouncy ball '{name}' at {location} with color {color} and size {size}"
        }
        
    except Exception as e:
        logger.error(f"create_bouncy_ball error: {e}")
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
        
        # Calculate maze dimensions for spatial awareness
        maze_width = cols * cell_size
        maze_height = rows * cell_size
        maze_z_height = wall_height * 100  # Assuming 100 units per wall height
        maze_size = [maze_width, maze_height, maze_z_height]
        
        # Get safe location
        safe_location = get_safe_location(location, maze_size, "Maze")
            
        import random
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
                        x_pos = safe_location[0] + (c - maze_width/2) * cell_size
                        y_pos = safe_location[1] + (r - maze_height/2) * cell_size
                        z_pos = safe_location[2] + h * cell_size
                        
                        actor_name = f"Maze_Wall_{r}_{c}_{h}"
                        params = {
                            "name": actor_name,
                            "type": "StaticMeshActor",
                            "location": [x_pos, y_pos, z_pos],
                            "scale": [cell_size/100.0, cell_size/100.0, cell_size/100.0],
                            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                        }
                        resp = unreal.send_command("spawn_actor", params)
                        if resp:
                            spawned.append(resp)
        
        # Add entrance and exit markers
        entrance_marker = unreal.send_command("spawn_actor", {
            "name": "Maze_Entrance",
            "type": "StaticMeshActor",
            "location": [safe_location[0] - maze_width/2 * cell_size - cell_size, 
                       safe_location[1] + (-maze_height/2 + 1) * cell_size, 
                       safe_location[2] + cell_size],
            "scale": [0.5, 0.5, 0.5],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if entrance_marker:
            spawned.append(entrance_marker)
            
        exit_marker = unreal.send_command("spawn_actor", {
            "name": "Maze_Exit",
            "type": "StaticMeshActor", 
            "location": [safe_location[0] + maze_width/2 * cell_size + cell_size,
                       safe_location[1] + (-maze_height/2 + rows * 2 - 1) * cell_size,
                       safe_location[2] + cell_size],
            "scale": [0.5, 0.5, 0.5],
            "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
        })
        if exit_marker:
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
def create_obstacle_course(
    checkpoints: int = 5,
    spacing: float = 500.0,
    location: List[float] = [0.0, 0.0, 0.0]
) -> Dict[str, Any]:
    """Create a simple obstacle course of pillars."""
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        # Calculate obstacle course dimensions for spatial awareness
        course_length = checkpoints * spacing
        course_size = [course_length, 200, 400]  # Length, width, height
        
        # Get safe location
        safe_location = get_safe_location(location, course_size, "ObstacleCourse")
        
        spawned = []
        for i in range(checkpoints):
            actor_name = f"Obstacle_{i}"
            loc = [safe_location[0] + i * spacing, safe_location[1], safe_location[2]]
            params = {
                "name": actor_name,
                "type": "StaticMeshActor",
                "location": loc,
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            }
            resp = unreal.send_command("spawn_actor", params)
            if resp:
                spawned.append(resp)
        return {"success": True, "actors": spawned}
    except Exception as e:
        logger.error(f"create_obstacle_course error: {e}")
        return {"success": False, "message": str(e)}


# Material Color Function
@mcp.tool()
def set_mesh_material_color(
    blueprint_name: str,
    component_name: str,
    color: List[float],
    material_path: str = "/Engine/BasicShapes/BasicShapeMaterial",
    parameter_name: str = "BaseColor"
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
            "parameter_name": "BaseColor"
        }
        response_base = unreal.send_command("set_mesh_material_color", params_base)
        
        # Set Color parameter second (for maximum compatibility)
        params_color = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "color": color,
            "material_path": material_path,
            "parameter_name": "Color"
        }
        response_color = unreal.send_command("set_mesh_material_color", params_color)
        
        # Return success if either parameter setting worked
        if (response_base and response_base.get("success")) or (response_color and response_color.get("success")):
            return {
                "success": True, 
                "message": f"Color applied successfully: {color}",
                "base_color_result": response_base,
                "color_result": response_color
            }
        else:
            return {
                "success": False, 
                "message": f"Failed to set color parameters. BaseColor: {response_base}, Color: {response_color}"
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
        import random
        random.seed()  # Use different seed each time for variety
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        # Calculate town dimensions based on size
        town_sizes = {
            "small": 8, "medium": 12, "large": 16, "metropolis": 24
        }
        blocks = town_sizes.get(town_size, 12)
        block_size = 1000
        town_width = blocks * block_size
        town_size_array = [town_width, town_width, 800]  # Width, depth, height
        
        # Get safe location
        safe_location = get_safe_location(location, town_size_array, f"{name_prefix}_Town")
        
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
        street_results = helper_create_street_grid(unreal, set_actor_transform, blocks, block_size, street_width, safe_location, name_prefix)
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
                
                # Place buildings at block positions (aligned with grid)
                block_center_x = safe_location[0] + (block_x - blocks/2) * block_size
                block_center_y = safe_location[1] + (block_y - blocks/2) * block_size
                
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
                building_result = helper_create_town_building(
                    unreal,
                    set_actor_transform,
                    construct_house,
                    create_tower,
                    [block_center_x, block_center_y, location[2]],
                    building_area,
                    max_height,
                    f"{name_prefix}_Building_{block_x}_{block_y}",
                    building_count,
                    building_type,
                )
                
                if building_result.get("success"):
                    all_spawned.extend(building_result.get("actors", []))
                    building_count += 1
        
        # Add infrastructure if requested
        infrastructure_count = 0
        if include_infrastructure:
            logger.info("Adding infrastructure...")
            
            # Street lights
            light_results = helper_create_street_lights(unreal, set_actor_transform, blocks, block_size, location, name_prefix)
            all_spawned.extend(light_results.get("actors", []))
            infrastructure_count += len(light_results.get("actors", []))
            
            # Vehicles
            vehicle_results = helper_create_town_vehicles(unreal, set_actor_transform, blocks, block_size, location, name_prefix, target_population // 3)
            all_spawned.extend(vehicle_results.get("actors", []))
            infrastructure_count += len(vehicle_results.get("actors", []))
            
            # Parks and decorations
            decoration_results = helper_create_town_decorations(unreal, set_actor_transform, blocks, block_size, location, name_prefix)
            all_spawned.extend(decoration_results.get("actors", []))
            infrastructure_count += len(decoration_results.get("actors", []))
            

            
            # Add advanced infrastructure
            logger.info("Adding advanced infrastructure...")
            
            # Traffic lights at intersections
            traffic_results = helper_create_traffic_lights(unreal, blocks, block_size, location, name_prefix)
            all_spawned.extend(traffic_results.get("actors", []))
            infrastructure_count += len(traffic_results.get("actors", []))
            
            # Street signs and billboards
            signage_results = helper_create_street_signage(unreal, blocks, block_size, location, name_prefix, town_size)
            all_spawned.extend(signage_results.get("actors", []))
            infrastructure_count += len(signage_results.get("actors", []))
            
            # Sidewalks and crosswalks
            sidewalk_results = helper_create_sidewalks_crosswalks(unreal, blocks, block_size, street_width, location, name_prefix)
            all_spawned.extend(sidewalk_results.get("actors", []))
            infrastructure_count += len(sidewalk_results.get("actors", []))
            
            # Urban furniture (benches, trash cans, bus stops)
            furniture_results = helper_create_urban_furniture(unreal, blocks, block_size, location, name_prefix)
            all_spawned.extend(furniture_results.get("actors", []))
            infrastructure_count += len(furniture_results.get("actors", []))
            
            # Parking meters and hydrants
            utility_results = helper_create_street_utilities(unreal, blocks, block_size, location, name_prefix)
            all_spawned.extend(utility_results.get("actors", []))
            infrastructure_count += len(utility_results.get("actors", []))
            
            # Add plaza/square in center for large towns
            if town_size in ["large", "metropolis"]:
                plaza_results = helper_create_central_plaza(unreal, location, block_size, name_prefix)
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


# Run the server
if __name__ == "__main__":
    logger.info("Starting Advanced MCP server with stdio transport")
    mcp.run(transport='stdio') 