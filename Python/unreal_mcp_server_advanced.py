"""
Unreal Engine Advanced MCP Server

A streamlined MCP server focused on advanced composition tools for Unreal Engine.
Contains only the advanced tools from the expanded MCP tool system to keep tool count manageable.
"""

import logging
import socket
import json
import math
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP

from helpers.infrastructure_creation import (
    _create_street_grid, _create_street_lights, _create_town_vehicles, _create_town_decorations,
    _create_traffic_lights, _create_street_signage, _create_sidewalks_crosswalks, _create_urban_furniture,
    _create_street_utilities, _create_central_plaza
)
from helpers.building_creation import _create_town_building

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
            
        results = []
        wall_thickness = 20.0  # Thinner walls for realism
        floor_thickness = 30.0
        
        # Adjust dimensions based on style
        if house_style == "mansion":
            width = int(width * 1.5)
            depth = int(depth * 1.5)
            height = int(height * 1.3)
        elif house_style == "cottage":
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
        foundation_resp = unreal.send_command("spawn_actor", foundation_params)
        if foundation_resp:
            results.append(foundation_resp)
        
        # Create floor as single piece
        floor_params = {
            "name": f"{name_prefix}_Floor",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [width/100.0, depth/100.0, floor_thickness/100.0],
            "static_mesh": mesh
        }
        floor_resp = unreal.send_command("spawn_actor", floor_params)
        if floor_resp:
            results.append(floor_resp)
        
        base_z = location[2] + floor_thickness
        
        # Create walls as large segments
        # Front wall (with door opening)
        door_width = 120.0
        door_height = 240.0
        
        # Front wall - left side of door
        front_left_width = (width/2 - door_width/2)
        front_left_params = {
            "name": f"{name_prefix}_FrontWall_Left",
            "type": "StaticMeshActor",
            "location": [location[0] - width/4 - door_width/4, location[1] - depth/2, base_z + height/2],
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
            "location": [location[0] + width/4 + door_width/4, location[1] - depth/2, base_z + height/2],
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
            "location": [location[0], location[1] - depth/2, base_z + door_height + (height - door_height)/2],
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
            "location": [location[0] - width/3, location[1] + depth/2, base_z + height/2],
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
            "location": [location[0], location[1] + depth/2, base_z + (window_y - window_height/2 - base_z)/2],
            "scale": [width/3/100.0, wall_thickness/100.0, (window_y - window_height/2 - base_z)/100.0],
            "static_mesh": mesh
        }
        resp = unreal.send_command("spawn_actor", back_center_bottom_params)
        if resp:
            results.append(resp)
        
        back_center_top_params = {
            "name": f"{name_prefix}_BackWall_Center_Top",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] + depth/2, window_y + window_height/2 + (base_z + height - window_y - window_height/2)/2],
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
            "location": [location[0] + width/3, location[1] + depth/2, base_z + height/2],
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
            "location": [location[0] - width/2, location[1], base_z + height/2],
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
            "location": [location[0] + width/2, location[1], base_z + height/2],
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
                    location[0],
                    location[1],
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
                        location[0] + width/3,
                        location[1] + depth/3,
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
                "location": [location[0], location[1] - depth/2 - 150, location[2]],
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
                        location[0] + x_offset,
                        location[1] - depth/2 - 250,
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
                    "location": [location[0], location[1] - depth/2 - 150, base_z + height - 50],
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
                "location": [location[0] - width/3, location[1] - depth/2 + wall_thickness/2, base_z + 150],
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
            resp = unreal.send_command("spawn_actor", params)
            if resp:
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
                        resp = unreal.send_command("spawn_actor", params)
                        if resp:
                            spawned.append(resp)
        
        # Add entrance and exit markers
        entrance_marker = unreal.send_command("spawn_actor", {
            "name": "Maze_Entrance",
            "type": "StaticMeshActor",
            "location": [location[0] - maze_width/2 * cell_size - cell_size, 
                       location[1] + (-maze_height/2 + 1) * cell_size, 
                       location[2] + cell_size],
            "scale": [0.5, 0.5, 0.5],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if entrance_marker:
            spawned.append(entrance_marker)
            
        exit_marker = unreal.send_command("spawn_actor", {
            "name": "Maze_Exit",
            "type": "StaticMeshActor", 
            "location": [location[0] + maze_width/2 * cell_size + cell_size,
                       location[1] + (-maze_height/2 + rows * 2 - 1) * cell_size,
                       location[2] + cell_size],
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
        spawned = []
        for i in range(checkpoints):
            actor_name = f"Obstacle_{i}"
            loc = [location[0] + i * spacing, location[1], location[2]]
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

@mcp.tool()
def set_actor_material_color(
    name: str,
    color: List[float],
    material_path: str = "/Engine/BasicShapes/BasicShapeMaterial",
    parameter_name: str = "BaseColor",
    material_slot: int = 0
) -> Dict[str, Any]:
    """Set a material color directly on an existing actor's StaticMeshComponent."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    try:
        if not isinstance(color, list) or len(color) != 4:
            return {"success": False, "message": "Invalid color format. Must be [R,G,B,A] floats."}
        color = [float(min(1.0, max(0.0, v))) for v in color]
        params = {
            "name": name,
            "color": color,
            "material_path": material_path,
            "parameter_name": parameter_name,
            "material_slot": material_slot,
        }
        return unreal.send_command("set_actor_material_color", params) or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"set_actor_material_color error: {e}")
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
                
                if building_result.get("success"):
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
    dungeons, and surrounding village. Perfect for dramatic TikTok reveals showing
    the scale and detail of a complete medieval fortress.
    """
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Creating {castle_size} {architectural_style} castle fortress")
        all_actors = []
        
        # Define castle dimensions based on size - MUCH BIGGER AND MORE COMPLEX
        size_params = {
            "small": {"outer_width": 6000, "outer_depth": 6000, "inner_width": 3000, "inner_depth": 3000, "wall_height": 800, "tower_count": 8, "tower_height": 1200},
            "medium": {"outer_width": 8000, "outer_depth": 8000, "inner_width": 4000, "inner_depth": 4000, "wall_height": 1000, "tower_count": 12, "tower_height": 1600},
            "large": {"outer_width": 12000, "outer_depth": 12000, "inner_width": 6000, "inner_depth": 6000, "wall_height": 1200, "tower_count": 16, "tower_height": 2000},
            "epic": {"outer_width": 16000, "outer_depth": 16000, "inner_width": 8000, "inner_depth": 8000, "wall_height": 1600, "tower_count": 24, "tower_height": 2800}
        }
        
        params = size_params.get(castle_size, size_params["large"])
        # Global scale/complexity multipliers applied across ALL styles and sizes
        # This makes every variant ~4x larger and adds denser details everywhere.
        scale_factor: float = 2.0
        complexity_multiplier: int = max(1, int(round(scale_factor)))

        outer_width = int(params["outer_width"] * scale_factor)
        outer_depth = int(params["outer_depth"] * scale_factor)
        inner_width = int(params["inner_width"] * scale_factor)
        inner_depth = int(params["inner_depth"] * scale_factor)
        wall_height = int(params["wall_height"] * scale_factor)
        tower_count = int(params["tower_count"] * complexity_multiplier)
        tower_height = int(params["tower_height"] * scale_factor)

        # Frequently reused scaled offsets
        gate_tower_offset = int(700 * scale_factor)
        barbican_offset = int(400 * scale_factor)
        drawbridge_offset = int(600 * scale_factor)
        
        # MASSIVE COMPLEX CASTLE - BUILD OUTER BAILEY WALLS FIRST
        logger.info("Constructing massive outer bailey walls...")
        wall_thickness = int(300 * max(1.0, scale_factor * 0.75))
        
        # OUTER BAILEY - North wall
        for i in range(int(outer_width / 200)):
            wall_x = location[0] - outer_width/2 + i * 200 + 100
            wall_name = f"{name_prefix}_WallNorth_{i}"
            wall_result = unreal.send_command("spawn_actor", {
                "name": wall_name,
                "type": "StaticMeshActor",
                "location": [wall_x, location[1] - outer_depth/2, location[2] + wall_height/2],
                "scale": [2.0, wall_thickness/100, wall_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if wall_result and wall_result.get("status") == "success":
                all_actors.append(wall_result.get("result"))
            
            # Dense battlements
            if i % 2 == 0:
                battlement_name = f"{name_prefix}_BattlementNorth_{i}"
                battlement_result = unreal.send_command("spawn_actor", {
                    "name": battlement_name,
                    "type": "StaticMeshActor",
                    "location": [wall_x, location[1] - outer_depth/2, location[2] + wall_height + 50],
                    "scale": [1.0, wall_thickness/100, 1.0],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if battlement_result and battlement_result.get("status") == "success":
                    all_actors.append(battlement_result.get("result"))
        
        # OUTER BAILEY - South wall
        for i in range(int(outer_width / 200)):
            wall_x = location[0] - outer_width/2 + i * 200 + 100
            wall_name = f"{name_prefix}_WallSouth_{i}"
            wall_result = unreal.send_command("spawn_actor", {
                "name": wall_name,
                "type": "StaticMeshActor",
                "location": [wall_x, location[1] + outer_depth/2, location[2] + wall_height/2],
                "scale": [2.0, wall_thickness/100, wall_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if wall_result and wall_result.get("status") == "success":
                all_actors.append(wall_result.get("result"))
            
            if i % 2 == 0:
                battlement_name = f"{name_prefix}_BattlementSouth_{i}"
                battlement_result = unreal.send_command("spawn_actor", {
                    "name": battlement_name,
                    "type": "StaticMeshActor",
                    "location": [wall_x, location[1] + outer_depth/2, location[2] + wall_height + 50],
                    "scale": [1.0, wall_thickness/100, 1.0],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if battlement_result and battlement_result.get("status") == "success":
                    all_actors.append(battlement_result.get("result"))
        
        # OUTER BAILEY - East wall
        for i in range(int(outer_depth / 200)):
            wall_y = location[1] - outer_depth/2 + i * 200 + 100
            wall_name = f"{name_prefix}_WallEast_{i}"
            wall_result = unreal.send_command("spawn_actor", {
                "name": wall_name,
                "type": "StaticMeshActor",
                "location": [location[0] + outer_width/2, wall_y, location[2] + wall_height/2],
                "scale": [wall_thickness/100, 2.0, wall_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if wall_result and wall_result.get("status") == "success":
                all_actors.append(wall_result.get("result"))
        
        # OUTER BAILEY - West wall with main gate (much more elaborate)
        for i in range(int(outer_depth / 200)):
            wall_y = location[1] - outer_depth/2 + i * 200 + 100
            # Skip middle sections for massive gate
            if abs(wall_y - location[1]) > 700:
                wall_name = f"{name_prefix}_WallWest_{i}"
                wall_result = unreal.send_command("spawn_actor", {
                    "name": wall_name,
                    "type": "StaticMeshActor",
                    "location": [location[0] - outer_width/2, wall_y, location[2] + wall_height/2],
                    "scale": [wall_thickness/100, 2.0, wall_height/100],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if wall_result and wall_result.get("status") == "success":
                    all_actors.append(wall_result.get("result"))
        
        # BUILD INNER BAILEY WALLS (HIGHER AND STRONGER)
        logger.info("Building inner bailey fortifications...")
        inner_wall_height = wall_height * 1.3
        
        # Inner North wall
        for i in range(int(inner_width / 200)):
            wall_x = location[0] - inner_width/2 + i * 200 + 100
            wall_name = f"{name_prefix}_InnerWallNorth_{i}"
            wall_result = unreal.send_command("spawn_actor", {
                "name": wall_name,
                "type": "StaticMeshActor",
                "location": [wall_x, location[1] - inner_depth/2, location[2] + inner_wall_height/2],
                "scale": [2.0, wall_thickness/100, inner_wall_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if wall_result and wall_result.get("status") == "success":
                all_actors.append(wall_result.get("result"))
        
        # Inner South wall  
        for i in range(int(inner_width / 200)):
            wall_x = location[0] - inner_width/2 + i * 200 + 100
            wall_name = f"{name_prefix}_InnerWallSouth_{i}"
            wall_result = unreal.send_command("spawn_actor", {
                "name": wall_name,
                "type": "StaticMeshActor",
                "location": [wall_x, location[1] + inner_depth/2, location[2] + inner_wall_height/2],
                "scale": [2.0, wall_thickness/100, inner_wall_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if wall_result and wall_result.get("status") == "success":
                all_actors.append(wall_result.get("result"))
        
        # Inner East and West walls
        for i in range(int(inner_depth / 200)):
            wall_y = location[1] - inner_depth/2 + i * 200 + 100
            
            # East inner wall
            wall_name = f"{name_prefix}_InnerWallEast_{i}"
            wall_result = unreal.send_command("spawn_actor", {
                "name": wall_name,
                "type": "StaticMeshActor",
                "location": [location[0] + inner_width/2, wall_y, location[2] + inner_wall_height/2],
                "scale": [wall_thickness/100, 2.0, inner_wall_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if wall_result and wall_result.get("status") == "success":
                all_actors.append(wall_result.get("result"))
            
            # West inner wall
            wall_name = f"{name_prefix}_InnerWallWest_{i}"
            wall_result = unreal.send_command("spawn_actor", {
                "name": wall_name,
                "type": "StaticMeshActor",
                "location": [location[0] - inner_width/2, wall_y, location[2] + inner_wall_height/2],
                "scale": [wall_thickness/100, 2.0, inner_wall_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if wall_result and wall_result.get("status") == "success":
                all_actors.append(wall_result.get("result"))
        
        # Build MASSIVE main gate complex
        logger.info("Building elaborate main gate complex...")
        
        # OUTER Gate towers (much larger)
        for side in [-1, 1]:
            gate_tower_name = f"{name_prefix}_GateTower_{side}"
            gate_tower_result = unreal.send_command("spawn_actor", {
                "name": gate_tower_name,
                "type": "StaticMeshActor",
                "location": [
                    location[0] - outer_width/2,
                    location[1] + side * gate_tower_offset,
                    location[2] + tower_height/2
                ],
                "scale": [4.0, 4.0, tower_height/100],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if gate_tower_result and gate_tower_result.get("status") == "success":
                all_actors.append(gate_tower_result.get("result"))
            
            # Massive tower tops
            tower_top_name = f"{name_prefix}_GateTowerTop_{side}"
            tower_top_result = unreal.send_command("spawn_actor", {
                "name": tower_top_name,
                "type": "StaticMeshActor",
                "location": [
                    location[0] - outer_width/2,
                    location[1] + side * gate_tower_offset,
                    location[2] + tower_height + 200
                ],
                "scale": [5.0, 5.0, 0.8],
                "static_mesh": "/Engine/BasicShapes/Cone.Cone"
            })
            if tower_top_result and tower_top_result.get("status") == "success":
                all_actors.append(tower_top_result.get("result"))
        
        # BARBICAN (outer gate structure) 
        barbican_name = f"{name_prefix}_Barbican"
        barbican_result = unreal.send_command("spawn_actor", {
            "name": barbican_name,
            "type": "StaticMeshActor",
            "location": [location[0] - outer_width/2 - barbican_offset, location[1], location[2] + wall_height/2],
            "scale": [8.0, 12.0, wall_height/100],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if barbican_result and barbican_result.get("status") == "success":
            all_actors.append(barbican_result.get("result"))
        
        # Main Portcullis (gate)
        portcullis_name = f"{name_prefix}_Portcullis"
        portcullis_result = unreal.send_command("spawn_actor", {
            "name": portcullis_name,
            "type": "StaticMeshActor",
            "location": [location[0] - outer_width/2, location[1], location[2] + 200],
            "scale": [0.5, 12.0, 8.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if portcullis_result and portcullis_result.get("status") == "success":
            all_actors.append(portcullis_result.get("result"))
        
        # Inner gate for inner bailey
        inner_portcullis_name = f"{name_prefix}_InnerPortcullis"
        inner_portcullis_result = unreal.send_command("spawn_actor", {
            "name": inner_portcullis_name,
            "type": "StaticMeshActor",
            "location": [location[0] - inner_width/2, location[1], location[2] + 200],
            "scale": [0.5, 8.0, 6.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if inner_portcullis_result and inner_portcullis_result.get("status") == "success":
            all_actors.append(inner_portcullis_result.get("result"))
        
        # Build MASSIVE corner towers - OUTER BAILEY
        logger.info("Constructing massive corner towers...")
        outer_corners = [
            [location[0] - outer_width/2, location[1] - outer_depth/2],  # NW
            [location[0] + outer_width/2, location[1] - outer_depth/2],  # NE
            [location[0] + outer_width/2, location[1] + outer_depth/2],  # SE
            [location[0] - outer_width/2, location[1] + outer_depth/2],  # SW
        ]
        
        # INNER BAILEY corner towers (even bigger)
        inner_corners = [
            [location[0] - inner_width/2, location[1] - inner_depth/2],  # NW
            [location[0] + inner_width/2, location[1] - inner_depth/2],  # NE
            [location[0] + inner_width/2, location[1] + inner_depth/2],  # SE
            [location[0] - inner_width/2, location[1] + inner_depth/2],  # SW
        ]
        
        # Build MASSIVE outer bailey corner towers
        for i, corner in enumerate(outer_corners):
            # HUGE Tower base (much wider)
            tower_base_name = f"{name_prefix}_TowerBase_{i}"
            tower_base_result = unreal.send_command("spawn_actor", {
                "name": tower_base_name,
                "type": "StaticMeshActor",
                "location": [corner[0], corner[1], location[2] + 150],
                "scale": [6.0, 6.0, 3.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if tower_base_result and tower_base_result.get("status") == "success":
                all_actors.append(tower_base_result.get("result"))
            
            # MASSIVE Main tower
            tower_name = f"{name_prefix}_Tower_{i}"
            tower_result = unreal.send_command("spawn_actor", {
                "name": tower_name,
                "type": "StaticMeshActor",
                "location": [corner[0], corner[1], location[2] + tower_height/2],
                "scale": [5.0, 5.0, tower_height/100],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if tower_result and tower_result.get("status") == "success":
                all_actors.append(tower_result.get("result"))
            
            # HUGE Tower top (cone roof)
            if architectural_style in ["medieval", "fantasy"]:
                tower_top_name = f"{name_prefix}_TowerTop_{i}"
                tower_top_result = unreal.send_command("spawn_actor", {
                    "name": tower_top_name,
                    "type": "StaticMeshActor",
                    "location": [corner[0], corner[1], location[2] + tower_height + 150],
                    "scale": [6.0, 6.0, 2.5],
                    "static_mesh": "/Engine/BasicShapes/Cone.Cone"
                })
                if tower_top_result and tower_top_result.get("status") == "success":
                    all_actors.append(tower_top_result.get("result"))
            
            # Multiple levels of tower windows (5 levels instead of 3)
            for window_level in range(5):
                window_height = location[2] + 300 + window_level * 300
                for angle in [0, 90, 180, 270]:
                    window_x = corner[0] + 350 * math.cos(angle * math.pi / 180)
                    window_y = corner[1] + 350 * math.sin(angle * math.pi / 180)
                    window_name = f"{name_prefix}_TowerWindow_{i}_{window_level}_{angle}"
                    window_result = unreal.send_command("spawn_actor", {
                        "name": window_name,
                        "type": "StaticMeshActor",
                        "location": [window_x, window_y, window_height],
                        "rotation": [0, angle, 0],
                        "scale": [0.3, 0.5, 0.8],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if window_result and window_result.get("status") == "success":
                        all_actors.append(window_result.get("result"))
        
        # Build INNER BAILEY corner towers (even more massive)
        logger.info("Building inner bailey towers...")
        for i, corner in enumerate(inner_corners):
            # ENORMOUS Tower base 
            tower_base_name = f"{name_prefix}_InnerTowerBase_{i}"
            tower_base_result = unreal.send_command("spawn_actor", {
                "name": tower_base_name,
                "type": "StaticMeshActor",
                "location": [corner[0], corner[1], location[2] + 200],
                "scale": [8.0, 8.0, 4.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if tower_base_result and tower_base_result.get("status") == "success":
                all_actors.append(tower_base_result.get("result"))
            
            # GIGANTIC Main inner tower
            inner_tower_height = tower_height * 1.4
            tower_name = f"{name_prefix}_InnerTower_{i}"
            tower_result = unreal.send_command("spawn_actor", {
                "name": tower_name,
                "type": "StaticMeshActor",
                "location": [corner[0], corner[1], location[2] + inner_tower_height/2],
                "scale": [6.0, 6.0, inner_tower_height/100],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if tower_result and tower_result.get("status") == "success":
                all_actors.append(tower_result.get("result"))
            
            # MASSIVE Tower top
            tower_top_name = f"{name_prefix}_InnerTowerTop_{i}"
            tower_top_result = unreal.send_command("spawn_actor", {
                "name": tower_top_name,
                "type": "StaticMeshActor",
                "location": [corner[0], corner[1], location[2] + inner_tower_height + 200],
                "scale": [8.0, 8.0, 3.0],
                "static_mesh": "/Engine/BasicShapes/Cone.Cone"
            })
            if tower_top_result and tower_top_result.get("status") == "success":
                all_actors.append(tower_top_result.get("result"))
        
        # Add intermediate towers along walls (more complex)
        logger.info("Adding intermediate wall towers...")
        # North wall intermediate towers
        for i in range(max(3, 3 * complexity_multiplier)):
            tower_x = location[0] - outer_width/4 + i * outer_width/4
            tower_name = f"{name_prefix}_NorthWallTower_{i}"
            tower_result = unreal.send_command("spawn_actor", {
                "name": tower_name,
                "type": "StaticMeshActor",
                "location": [tower_x, location[1] - outer_depth/2, location[2] + tower_height * 0.8/2],
                "scale": [3.0, 3.0, tower_height * 0.8/100],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if tower_result and tower_result.get("status") == "success":
                all_actors.append(tower_result.get("result"))
        
        # South wall intermediate towers
        for i in range(max(3, 3 * complexity_multiplier)):
            tower_x = location[0] - outer_width/4 + i * outer_width/4
            tower_name = f"{name_prefix}_SouthWallTower_{i}"
            tower_result = unreal.send_command("spawn_actor", {
                "name": tower_name,
                "type": "StaticMeshActor",
                "location": [tower_x, location[1] + outer_depth/2, location[2] + tower_height * 0.8/2],
                "scale": [3.0, 3.0, tower_height * 0.8/100],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if tower_result and tower_result.get("status") == "success":
                all_actors.append(tower_result.get("result"))
        
        # Build MASSIVE central keep complex 
        logger.info("Building enormous central keep complex...")
        keep_width = inner_width * 0.6
        keep_depth = inner_depth * 0.6
        keep_height = tower_height * 2.0  # Much taller (already scaled)
        
        # MASSIVE Keep base
        keep_base_name = f"{name_prefix}_KeepBase"
        keep_base_result = unreal.send_command("spawn_actor", {
            "name": keep_base_name,
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + keep_height/2],
            "scale": [keep_width/100, keep_depth/100, keep_height/100],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if keep_base_result and keep_base_result.get("status") == "success":
            all_actors.append(keep_base_result.get("result"))
        
        # GIGANTIC central Keep spire/tower
        # Ensure this sits ON TOP of the keep base instead of floating.
        # Compute the spire height explicitly and place its center at (keep_top + spire_height/2).
        keep_spire_height = max(1200.0, tower_height * 1.0)
        keep_top_z = location[2] + keep_height  # top of the keep base cube
        keep_tower_name = f"{name_prefix}_KeepTower"
        keep_tower_result = unreal.send_command("spawn_actor", {
            "name": keep_tower_name,
            "type": "StaticMeshActor",
            "location": [location[0], location[1], keep_top_z + keep_spire_height / 2.0],
            "scale": [4.0, 4.0, keep_spire_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if keep_tower_result and keep_tower_result.get("status") == "success":
            all_actors.append(keep_tower_result.get("result"))
        
        # ENORMOUS Great Hall (throne room)
        great_hall_name = f"{name_prefix}_GreatHall"
        great_hall_result = unreal.send_command("spawn_actor", {
            "name": great_hall_name,
            "type": "StaticMeshActor",
            "location": [location[0], location[1] + keep_depth/3, location[2] + 200],
            "scale": [keep_width/100 * 0.8, keep_depth/100 * 0.5, 6.0],  # Much taller
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if great_hall_result and great_hall_result.get("status") == "success":
            all_actors.append(great_hall_result.get("result"))
        
        # Additional keep towers (4 corner towers of the keep)
        logger.info("Adding keep corner towers...")
        keep_corners = [
            [location[0] - keep_width/3, location[1] - keep_depth/3],
            [location[0] + keep_width/3, location[1] - keep_depth/3],
            [location[0] + keep_width/3, location[1] + keep_depth/3],
            [location[0] - keep_width/3, location[1] + keep_depth/3],
        ]
        
        for i, corner in enumerate(keep_corners):
            keep_corner_tower_name = f"{name_prefix}_KeepCornerTower_{i}"
            keep_corner_tower_result = unreal.send_command("spawn_actor", {
                "name": keep_corner_tower_name,
                "type": "StaticMeshActor",
                "location": [corner[0], corner[1], location[2] + keep_height * 0.8],
                "scale": [3.0, 3.0, keep_height/100 * 0.8],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if keep_corner_tower_result and keep_corner_tower_result.get("status") == "success":
                all_actors.append(keep_corner_tower_result.get("result"))
        
        # Build MASSIVE inner courtyard complex
        logger.info("Adding massive courtyard complex...")
        
        # HUGE Stables complex
        stable_name = f"{name_prefix}_Stables"
        stable_result = unreal.send_command("spawn_actor", {
            "name": stable_name,
            "type": "StaticMeshActor",
            "location": [location[0] - inner_width/3, location[1] + inner_depth/3, location[2] + 150],
            "scale": [8.0, 4.0, 3.0],  # Much larger
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if stable_result and stable_result.get("status") == "success":
            all_actors.append(stable_result.get("result"))
        
        # MASSIVE Barracks
        barracks_name = f"{name_prefix}_Barracks"
        barracks_result = unreal.send_command("spawn_actor", {
            "name": barracks_name,
            "type": "StaticMeshActor",
            "location": [location[0] + inner_width/3, location[1] + inner_depth/3, location[2] + 150],
            "scale": [10.0, 6.0, 3.0],  # Much larger
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if barracks_result and barracks_result.get("status") == "success":
            all_actors.append(barracks_result.get("result"))
        
        # Large Blacksmith complex
        blacksmith_name = f"{name_prefix}_Blacksmith"
        blacksmith_result = unreal.send_command("spawn_actor", {
            "name": blacksmith_name,
            "type": "StaticMeshActor",
            "location": [location[0] + inner_width/3, location[1] - inner_depth/3, location[2] + 100],
            "scale": [6.0, 6.0, 2.0],  # Much larger
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if blacksmith_result and blacksmith_result.get("status") == "success":
            all_actors.append(blacksmith_result.get("result"))
        
        # MASSIVE Well
        well_name = f"{name_prefix}_Well"
        well_result = unreal.send_command("spawn_actor", {
            "name": well_name,
            "type": "StaticMeshActor",
            "location": [location[0] - inner_width/4, location[1], location[2] + 50],
            "scale": [3.0, 3.0, 2.0],  # Much larger
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
        })
        if well_result and well_result.get("status") == "success":
            all_actors.append(well_result.get("result"))
        
        # ADD MANY MORE BUILDINGS FOR COMPLEXITY
        
        # Armory
        armory_name = f"{name_prefix}_Armory"
        armory_result = unreal.send_command("spawn_actor", {
            "name": armory_name,
            "type": "StaticMeshActor",
            "location": [location[0] - inner_width/3, location[1] - inner_depth/3, location[2] + 150],
            "scale": [6.0, 4.0, 3.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if armory_result and armory_result.get("status") == "success":
            all_actors.append(armory_result.get("result"))
        
        # Chapel
        chapel_name = f"{name_prefix}_Chapel"
        chapel_result = unreal.send_command("spawn_actor", {
            "name": chapel_name,
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - inner_depth/3, location[2] + 200],
            "scale": [8.0, 5.0, 4.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if chapel_result and chapel_result.get("status") == "success":
            all_actors.append(chapel_result.get("result"))
        
        # Kitchen complex
        kitchen_name = f"{name_prefix}_Kitchen"
        kitchen_result = unreal.send_command("spawn_actor", {
            "name": kitchen_name,
            "type": "StaticMeshActor",
            "location": [location[0] - inner_width/4, location[1] + inner_depth/4, location[2] + 120],
            "scale": [5.0, 4.0, 2.5],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if kitchen_result and kitchen_result.get("status") == "success":
            all_actors.append(kitchen_result.get("result"))
        
        # Treasury
        treasury_name = f"{name_prefix}_Treasury"
        treasury_result = unreal.send_command("spawn_actor", {
            "name": treasury_name,
            "type": "StaticMeshActor",
            "location": [location[0] + inner_width/4, location[1] + inner_depth/4, location[2] + 100],
            "scale": [3.0, 3.0, 2.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if treasury_result and treasury_result.get("status") == "success":
            all_actors.append(treasury_result.get("result"))
        
        # Granary
        granary_name = f"{name_prefix}_Granary"
        granary_result = unreal.send_command("spawn_actor", {
            "name": granary_name,
            "type": "StaticMeshActor",
            "location": [location[0] + inner_width/4, location[1] - inner_depth/4, location[2] + 180],
            "scale": [4.0, 6.0, 3.5],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if granary_result and granary_result.get("status") == "success":
            all_actors.append(granary_result.get("result"))
        
        # Guard House
        guardhouse_name = f"{name_prefix}_GuardHouse"
        guardhouse_result = unreal.send_command("spawn_actor", {
            "name": guardhouse_name,
            "type": "StaticMeshActor",
            "location": [location[0] - inner_width/4, location[1] - inner_depth/4, location[2] + 150],
            "scale": [4.0, 4.0, 3.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if guardhouse_result and guardhouse_result.get("status") == "success":
            all_actors.append(guardhouse_result.get("result"))

        # NEW: Fill outer bailey with smaller annex structures attached to the inner face of the first wall
        logger.info("Populating bailey with annex rooms and walkways...")
        annex_depth = int(500 * max(1.0, scale_factor))
        annex_width = int(700 * max(1.0, scale_factor))
        annex_height = int(300 * max(1.0, scale_factor))
        walkway_height = 160
        walkway_width = int(300 * max(1.0, scale_factor))
        spacing = int(1200 * max(1.0, scale_factor))

        def _spawn_annex_row(start_x: float, end_x: float, fixed_y: float, align: str, base_name: str):
            nonlocal all_actors
            count = 0
            x = start_x
            while (x <= end_x and start_x <= end_x) or (x >= end_x and start_x > end_x):
                annex_name = f"{name_prefix}_{base_name}_{count}"
                annex_x = x
                annex_y = fixed_y
                # Offset annex inward from the wall along its normal so it sits inside the bailey
                if align == "north":
                    annex_y += walkway_width
                elif align == "south":
                    annex_y -= walkway_width
                elif align == "east":
                    annex_x -= walkway_width
                elif align == "west":
                    annex_x += walkway_width

                result = unreal.send_command("spawn_actor", {
                    "name": annex_name,
                    "type": "StaticMeshActor",
                    "location": [annex_x, annex_y, location[2] + annex_height/2],
                    "scale": [annex_width/100, annex_depth/100, annex_height/100],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if result and result.get("status") == "success":
                    all_actors.append(result.get("result"))

                # Add a doorway arch on each annex
                arch_offset = 0 if align in ["north", "south"] else (annex_width * 0.25)
                door_x = annex_x + (50 if align == "east" else (-50 if align == "west" else arch_offset))
                door_y = annex_y + (50 if align == "south" else (-50 if align == "north" else 0))
                arch_name = f"{annex_name}_Door"
                arch = unreal.send_command("spawn_actor", {
                    "name": arch_name,
                    "type": "StaticMeshActor",
                    "location": [door_x, door_y, location[2] + 120],
                    "scale": [1.0, 0.6, 2.4],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                })
                if arch and arch.get("status") == "success":
                    all_actors.append(arch.get("result"))

                # Next annex position
                x += spacing if start_x <= end_x else -spacing
                count += 1

        # Perimeter walkways just inside the first wall (four sides)
        # North and South
        walkway_z = location[2] + 100
        for side, fixed_y in [("north", location[1] - outer_depth/2 + walkway_width/2),
                              ("south", location[1] + outer_depth/2 - walkway_width/2)]:
            segments = int(outer_width / 400)
            for i in range(segments):
                seg_x = location[0] - outer_width/2 + (i * 400) + 200
                seg_name = f"{name_prefix}_Walkway_{side}_{i}"
                res = unreal.send_command("spawn_actor", {
                    "name": seg_name,
                    "type": "StaticMeshActor",
                    "location": [seg_x, fixed_y, walkway_z],
                    "scale": [4.0, walkway_width/100, walkway_height/100],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if res and res.get("status") == "success":
                    all_actors.append(res.get("result"))

        # East and West
        for side, fixed_x in [("east", location[0] + outer_width/2 - walkway_width/2),
                              ("west", location[0] - outer_width/2 + walkway_width/2)]:
            segments = int(outer_depth / 400)
            for i in range(segments):
                seg_y = location[1] - outer_depth/2 + (i * 400) + 200
                seg_name = f"{name_prefix}_Walkway_{side}_{i}"
                res = unreal.send_command("spawn_actor", {
                    "name": seg_name,
                    "type": "StaticMeshActor",
                    "location": [fixed_x, seg_y, walkway_z],
                    "scale": [walkway_width/100, 4.0, walkway_height/100],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if res and res.get("status") == "success":
                    all_actors.append(res.get("result"))

        # Annex rows along each inner face of the first wall
        # North wall inner face
        _spawn_annex_row(
            start_x=location[0] - outer_width/2 + spacing,
            end_x=location[0] + outer_width/2 - spacing,
            fixed_y=location[1] - outer_depth/2 + walkway_width + annex_depth/2,
            align="north",
            base_name="NorthAnnex"
        )

        # South wall inner face
        _spawn_annex_row(
            start_x=location[0] - outer_width/2 + spacing,
            end_x=location[0] + outer_width/2 - spacing,
            fixed_y=location[1] + outer_depth/2 - walkway_width - annex_depth/2,
            align="south",
            base_name="SouthAnnex"
        )

        # West wall inner face (vertical placement)
        for y in range(int(location[1] - outer_depth/2 + spacing), int(location[1] + outer_depth/2 - spacing) + 1, spacing):
            res = unreal.send_command("spawn_actor", {
                "name": f"{name_prefix}_WestAnnex_{y}",
                "type": "StaticMeshActor",
                "location": [location[0] - outer_width/2 + walkway_width + annex_depth/2, y, location[2] + annex_height/2],
                "scale": [annex_depth/100, annex_width/100, annex_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if res and res.get("status") == "success":
                all_actors.append(res.get("result"))

        # East wall inner face
        for y in range(int(location[1] - outer_depth/2 + spacing), int(location[1] + outer_depth/2 - spacing) + 1, spacing):
            res = unreal.send_command("spawn_actor", {
                "name": f"{name_prefix}_EastAnnex_{y}",
                "type": "StaticMeshActor",
                "location": [location[0] + outer_width/2 - walkway_width - annex_depth/2, y, location[2] + annex_height/2],
                "scale": [annex_depth/100, annex_width/100, annex_height/100],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if res and res.get("status") == "success":
                all_actors.append(res.get("result"))
        
        # Add siege weapons if requested
        if include_siege_weapons:
            logger.info("Deploying siege weapons...")
            
            # MASSIVE Catapults on walls
            catapult_positions = [
                [location[0], location[1] - outer_depth/2 + 200, location[2] + wall_height],
                [location[0], location[1] + outer_depth/2 - 200, location[2] + wall_height],
                [location[0] - outer_width/3, location[1] - outer_depth/2 + 200, location[2] + wall_height],
                [location[0] + outer_width/3, location[1] + outer_depth/2 - 200, location[2] + wall_height],
            ]
            
            for i, pos in enumerate(catapult_positions):
                # MASSIVE Catapult base
                catapult_base_name = f"{name_prefix}_CatapultBase_{i}"
                catapult_base_result = unreal.send_command("spawn_actor", {
                    "name": catapult_base_name,
                    "type": "StaticMeshActor",
                    "location": pos,
                    "scale": [4.0, 3.0, 1.0],  # Much bigger
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if catapult_base_result and catapult_base_result.get("status") == "success":
                    all_actors.append(catapult_base_result.get("result"))
                
                # MASSIVE Catapult arm
                catapult_arm_name = f"{name_prefix}_CatapultArm_{i}"
                catapult_arm_result = unreal.send_command("spawn_actor", {
                    "name": catapult_arm_name,
                    "type": "StaticMeshActor",
                    "location": [pos[0], pos[1], pos[2] + 100],
                    "rotation": [45, 0, 0],
                    "scale": [0.4, 0.4, 6.0],  # Much bigger
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if catapult_arm_result and catapult_arm_result.get("status") == "success":
                    all_actors.append(catapult_arm_result.get("result"))
                
                # MASSIVE Ammunition pile
                for j in range(5):  # More ammo
                    ammo_name = f"{name_prefix}_CatapultAmmo_{i}_{j}"
                    ammo_result = unreal.send_command("spawn_actor", {
                        "name": ammo_name,
                        "type": "StaticMeshActor",
                        "location": [pos[0] + j * 80 - 160, pos[1] + 250, pos[2] + 40],
                        "scale": [0.6, 0.6, 0.6],  # Bigger ammo
                        "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
                    })
                    if ammo_result and ammo_result.get("status") == "success":
                        all_actors.append(ammo_result.get("result"))
            
            # MASSIVE Ballista on towers
            for i in range(4):
                corner = outer_corners[i]
                ballista_name = f"{name_prefix}_Ballista_{i}"
                ballista_result = unreal.send_command("spawn_actor", {
                    "name": ballista_name,
                    "type": "StaticMeshActor",
                    "location": [corner[0], corner[1], location[2] + tower_height],
                    "scale": [0.5, 3.0, 0.5],  # Bigger ballistae
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if ballista_result and ballista_result.get("status") == "success":
                    all_actors.append(ballista_result.get("result"))
        
        # Build MASSIVE DENSE surrounding settlement if requested
        if include_village:
            logger.info("Building massive dense outer settlement...")
            
            # DENSE Village houses (much closer and more numerous)
            village_radius = outer_width * 0.3  # Much closer!
            num_houses = (24 if castle_size == "epic" else 16) * complexity_multiplier  # MANY more houses
            
            # Inner ring of houses (very close)
            for i in range(num_houses):
                angle = (2 * math.pi * i) / num_houses
                house_x = location[0] + (outer_width/2 + village_radius) * math.cos(angle)
                house_y = location[1] + (outer_depth/2 + village_radius) * math.sin(angle)
                
                # Skip houses that would be in front of main gate
                if not (house_x < location[0] - outer_width * 0.4 and abs(house_y - location[1]) < 1000):
                    # BIGGER House base
                    house_name = f"{name_prefix}_VillageHouse_{i}"
                    house_result = unreal.send_command("spawn_actor", {
                        "name": house_name,
                        "type": "StaticMeshActor",
                        "location": [house_x, house_y, location[2] + 100],
                        "rotation": [0, angle * 180/math.pi, 0],
                        "scale": [3.0, 2.5, 2.0],  # Bigger
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if house_result and house_result.get("status") == "success":
                        all_actors.append(house_result.get("result"))
                    
                    # House roof
                    roof_name = f"{name_prefix}_VillageRoof_{i}"
                    roof_result = unreal.send_command("spawn_actor", {
                        "name": roof_name,
                        "type": "StaticMeshActor",
                        "location": [house_x, house_y, location[2] + 250],
                        "rotation": [0, angle * 180/math.pi, 0],
                        "scale": [3.5, 3.0, 0.8],  # Bigger roof
                        "static_mesh": "/Engine/BasicShapes/Cone.Cone"
                    })
                    if roof_result and roof_result.get("status") == "success":
                        all_actors.append(roof_result.get("result"))
            
            # OUTER ring of houses (further out but still close)
            outer_village_radius = outer_width * 0.5
            for i in range(max(1, num_houses // 2)):
                angle = (2 * math.pi * i) / (num_houses // 2)
                house_x = location[0] + (outer_width/2 + outer_village_radius) * math.cos(angle)
                house_y = location[1] + (outer_depth/2 + outer_village_radius) * math.sin(angle)
                
                # BIGGER outer houses
                house_name = f"{name_prefix}_OuterVillageHouse_{i}"
                house_result = unreal.send_command("spawn_actor", {
                    "name": house_name,
                    "type": "StaticMeshActor",
                    "location": [house_x, house_y, location[2] + 100],
                    "rotation": [0, angle * 180/math.pi, 0],
                    "scale": [2.5, 2.0, 2.0],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if house_result and house_result.get("status") == "success":
                    all_actors.append(house_result.get("result"))
                
                roof_name = f"{name_prefix}_OuterVillageRoof_{i}"
                roof_result = unreal.send_command("spawn_actor", {
                    "name": roof_name,
                    "type": "StaticMeshActor",
                    "location": [house_x, house_y, location[2] + 250],
                    "rotation": [0, angle * 180/math.pi, 0],
                    "scale": [3.0, 2.5, 0.6],
                    "static_mesh": "/Engine/BasicShapes/Cone.Cone"
                })
                if roof_result and roof_result.get("status") == "success":
                    all_actors.append(roof_result.get("result"))
            
            # DENSE Market area (much closer to castle)
            market_x_start = location[0] - outer_width/2 - int(800 * scale_factor)  # Much closer!
            for i in range(8 * complexity_multiplier):  # More stalls
                stall_x = market_x_start + i * 150
                stall_y = location[1] + (200 if i % 2 == 0 else -200)  # Staggered
                
                stall_name = f"{name_prefix}_MarketStall_{i}"
                stall_result = unreal.send_command("spawn_actor", {
                    "name": stall_name,
                    "type": "StaticMeshActor",
                    "location": [stall_x, stall_y, location[2] + 80],
                    "scale": [2.0, 1.5, 1.5],  # Bigger stalls
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if stall_result and stall_result.get("status") == "success":
                    all_actors.append(stall_result.get("result"))
                
                # Stall canopy
                canopy_name = f"{name_prefix}_StallCanopy_{i}"
                canopy_result = unreal.send_command("spawn_actor", {
                    "name": canopy_name,
                    "type": "StaticMeshActor",
                    "location": [stall_x, stall_y, location[2] + 180],
                    "scale": [2.5, 2.0, 0.1],  # Bigger canopy
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if canopy_result and canopy_result.get("status") == "success":
                    all_actors.append(canopy_result.get("result"))
                    
            # ADD SMALL OUTBUILDINGS AND EXTENSIONS 
            logger.info("Adding small outbuildings and extensions...")
            
            # Small workshops around the castle
            workshop_positions = []
            ring_offsets = [int(400 * scale_factor), int(600 * scale_factor), int(800 * scale_factor)]
            for offset in ring_offsets:
                workshop_positions.extend([
                    [location[0] - outer_width/2 - offset, location[1] + offset],
                    [location[0] - outer_width/2 - offset, location[1] - offset],
                    [location[0] + outer_width/2 + offset, location[1] + offset],
                    [location[0] + outer_width/2 + offset, location[1] - offset],
                ])
            
            for i, pos in enumerate(workshop_positions):
                workshop_name = f"{name_prefix}_Workshop_{i}"
                workshop_result = unreal.send_command("spawn_actor", {
                    "name": workshop_name,
                    "type": "StaticMeshActor",
                    "location": [pos[0], pos[1], location[2] + 80],
                    "scale": [2.0, 1.8, 1.6],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if workshop_result and workshop_result.get("status") == "success":
                    all_actors.append(workshop_result.get("result"))
        
        # Add MASSIVE drawbridge
        logger.info("Adding massive drawbridge...")
        drawbridge_name = f"{name_prefix}_Drawbridge"
        drawbridge_result = unreal.send_command("spawn_actor", {
            "name": drawbridge_name,
            "type": "StaticMeshActor",
            "location": [location[0] - outer_width/2 - drawbridge_offset, location[1], location[2] + 20],
            "rotation": [0, 0, 0],
            "scale": [12.0 * scale_factor, 10.0 * scale_factor, 0.3],  # Much bigger
            "static_mesh": "/Engine/BasicShapes/Cube.Cube"
        })
        if drawbridge_result and drawbridge_result.get("status") == "success":
            all_actors.append(drawbridge_result.get("result"))
        
        # Add MASSIVE moat around castle
        logger.info("Creating massive moat...")
        moat_width = int(1200 * scale_factor)  # Much wider
        moat_sections = int(30 * complexity_multiplier)  # More sections
        
        for i in range(moat_sections):
            angle = (2 * math.pi * i) / moat_sections
            moat_x = location[0] + (outer_width/2 + moat_width/2) * math.cos(angle)
            moat_y = location[1] + (outer_depth/2 + moat_width/2) * math.sin(angle)
            
            moat_name = f"{name_prefix}_Moat_{i}"
            moat_result = unreal.send_command("spawn_actor", {
                "name": moat_name,
                "type": "StaticMeshActor",
                "location": [moat_x, moat_y, location[2] - 50],
                "scale": [moat_width/100, moat_width/100, 0.1],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if moat_result and moat_result.get("status") == "success":
                all_actors.append(moat_result.get("result"))
        
        # Add flags on towers
        logger.info("Adding decorative flags...")
        flag_colors = []
        for i in range(len(outer_corners) + 2):  # Corner towers + gate towers
            flag_pole_name = f"{name_prefix}_FlagPole_{i}"
            if i < len(outer_corners):
                flag_x = outer_corners[i][0]
                flag_y = outer_corners[i][1]
                flag_z = location[2] + tower_height + 300
            else:
                # Gate tower flags
                side = 1 if i == len(outer_corners) else -1
                flag_x = location[0] - outer_width/2
                flag_y = location[1] + side * gate_tower_offset  # Updated for new gate tower spacing
                flag_z = location[2] + tower_height + 200
            
            # Flag pole
            pole_result = unreal.send_command("spawn_actor", {
                "name": flag_pole_name,
                "type": "StaticMeshActor",
                "location": [flag_x, flag_y, flag_z],
                "scale": [0.05, 0.05, 3.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
            })
            if pole_result and pole_result.get("status") == "success":
                all_actors.append(pole_result.get("result"))
            
            # Flag
            flag_name = f"{name_prefix}_Flag_{i}"
            flag_result = unreal.send_command("spawn_actor", {
                "name": flag_name,
                "type": "StaticMeshActor",
                "location": [flag_x + 100, flag_y, flag_z + 100],
                "scale": [0.05, 2.0, 1.5],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if flag_result and flag_result.get("status") == "success":
                all_actors.append(flag_result.get("result"))
        
        logger.info(f"Castle fortress creation complete! Created {len(all_actors)} actors")
        
        return {
            "success": True,
            "message": f"Epic {castle_size} {architectural_style} castle fortress created with {len(all_actors)} elements!",
            "actors": all_actors,
            "stats": {
                "size": castle_size,
                "style": architectural_style,
                "wall_sections": int(outer_width/200) * 2 + int(outer_depth/200) * 2,
                "towers": tower_count,
                "has_village": include_village,
                "has_siege_weapons": include_siege_weapons,
                "total_actors": len(all_actors)
            }
        }
        
    except Exception as e:
        logger.error(f"create_castle_fortress error: {e}")
        return {"success": False, "message": str(e)}

@mcp.tool()
def generate_building(
    footprint: str = "rectangle",  # "rectangle", "L_shape", "U_shape", "T_shape", "circle", "cross"
    floors: int = 6,
    style: str = "modern",  # "modern", "cottage", "gothic", "art_deco", "brutalist", "glass", "industrial"
    facade_pattern: str = "grid",  # "grid", "bands", "alternating", "random", "columns", "arches"
    roof_type: str = "flat",  # "flat", "gable", "hip", "cone", "dome", "shed", "gambrel"
    width: float = 1600.0,
    depth: float = 1200.0,
    floor_height: float = 350.0,
    location: List[float] = [0.0, 0.0, 0.0],
    seed: int = 0,
    name_prefix: str = "Building",
    include_details: bool = True,  # Windows, doors, decorative elements
    entrance_side: str = "front",  # "front", "back", "left", "right", "corner"
    balcony_chance: float = 0.3,  # Probability of balconies on residential buildings
    color_scheme: str = "auto",  # "auto", "brick", "concrete", "stone", "wood", "glass", "metal", "stucco"
) -> Dict[str, Any]:
    """
    Generate a single dynamic building with extensive customization options.
    
    This is the ultimate building generator that creates realistic, varied structures
    with architectural details, proper proportions, and style-appropriate features.
    Perfect for creating unique buildings for games, architectural visualization,
    or procedural city generation.
    
    Features:
    - Multiple footprint shapes (rectangle, L, U, T, circle, cross)
    - Architectural styles with appropriate details
    - Facade patterns and window arrangements
    - Various roof types with proper geometry
    - Entrance placement and design
    - Balconies, awnings, and decorative elements
    - Color schemes and material variation
    - Deterministic generation with seed control
    
    Args:
        footprint: Shape of the building base
        floors: Number of floors (1-50)
        style: Architectural style affecting proportions and details
        facade_pattern: Window and wall arrangement pattern
        roof_type: Type of roof structure
        width/depth: Base dimensions in Unreal units (cm)
        floor_height: Height per floor in Unreal units (cm)
        location: World position [X, Y, Z]
        seed: Random seed for deterministic generation
        name_prefix: Prefix for all spawned actors
        include_details: Whether to add windows, doors, decorative elements
        entrance_side: Which side to place the main entrance
        balcony_chance: Probability of balconies (0.0-1.0)
        color_scheme: Building material color scheme (auto picks based on style)
    """
    try:
        import random
        import math
        
        # Set deterministic seed
        if seed > 0:
            random.seed(seed)
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Generating {style} building with {footprint} footprint, {floors} floors")
        
        all_actors = []
        total_height = floors * floor_height
        
        # Validate parameters
        floors = max(1, min(50, floors))
        width = max(400.0, width)
        depth = max(400.0, depth)
        floor_height = max(200.0, min(500.0, floor_height))
        balcony_chance = max(0.0, min(1.0, balcony_chance))
        
        # Style-specific adjustments
        style_configs = {
            "modern": {"wall_thickness": 25, "window_ratio": 0.7, "detail_scale": 1.0},
            "cottage": {"wall_thickness": 40, "window_ratio": 0.4, "detail_scale": 0.8},
            "gothic": {"wall_thickness": 60, "window_ratio": 0.3, "detail_scale": 1.2},
            "art_deco": {"wall_thickness": 30, "window_ratio": 0.6, "detail_scale": 1.1},
            "brutalist": {"wall_thickness": 80, "window_ratio": 0.2, "detail_scale": 1.5},
            "glass": {"wall_thickness": 20, "window_ratio": 0.9, "detail_scale": 0.7},
            "industrial": {"wall_thickness": 35, "window_ratio": 0.5, "detail_scale": 0.9}
        }
        
        config = style_configs.get(style, style_configs["modern"])
        wall_thickness = config["wall_thickness"]
        window_ratio = config["window_ratio"]
        detail_scale = config["detail_scale"]
        
        # Generate realistic building color scheme
        def get_building_colors(scheme, building_style):
            """Generate realistic building colors for walls, roof, windows, doors, etc."""
            
            # Define realistic building material colors
            material_colors = {
                "brick": {
                    "walls": [0.7, 0.4, 0.3, 1.0],      # Red brick
                    "roof": [0.3, 0.2, 0.2, 1.0],       # Dark red clay tiles
                    "windows": [0.2, 0.3, 0.5, 0.8],    # Blue-tinted glass
                    "doors": [0.4, 0.2, 0.1, 1.0],      # Dark wood
                    "trim": [0.9, 0.9, 0.8, 1.0],       # Cream trim
                    "foundation": [0.5, 0.5, 0.5, 1.0]  # Concrete gray
                },
                "concrete": {
                    "walls": [0.75, 0.75, 0.7, 1.0],    # Light concrete
                    "roof": [0.4, 0.4, 0.4, 1.0],       # Dark concrete
                    "windows": [0.2, 0.3, 0.4, 0.7],    # Dark blue glass
                    "doors": [0.2, 0.2, 0.2, 1.0],      # Black metal
                    "trim": [0.6, 0.6, 0.6, 1.0],       # Medium gray
                    "foundation": [0.45, 0.45, 0.45, 1.0]
                },
                "stone": {
                    "walls": [0.6, 0.55, 0.5, 1.0],     # Limestone
                    "roof": [0.35, 0.3, 0.25, 1.0],     # Dark slate
                    "windows": [0.1, 0.2, 0.3, 0.8],    # Dark glass
                    "doors": [0.3, 0.2, 0.1, 1.0],      # Oak wood
                    "trim": [0.8, 0.8, 0.75, 1.0],      # Light stone
                    "foundation": [0.4, 0.4, 0.35, 1.0]
                },
                "wood": {
                    "walls": [0.6, 0.45, 0.3, 1.0],     # Natural wood siding
                    "roof": [0.25, 0.2, 0.15, 1.0],     # Dark wood shingles
                    "windows": [0.9, 0.9, 0.85, 0.9],   # Clear glass
                    "doors": [0.5, 0.35, 0.2, 1.0],     # Stained wood
                    "trim": [0.9, 0.9, 0.85, 1.0],      # White trim
                    "foundation": [0.5, 0.5, 0.45, 1.0]
                },
                "glass": {
                    "walls": [0.3, 0.4, 0.5, 0.3],      # Blue-tinted glass curtain wall
                    "roof": [0.2, 0.2, 0.2, 1.0],       # Black membrane roof
                    "windows": [0.2, 0.3, 0.4, 0.4],    # Darker glass
                    "doors": [0.1, 0.1, 0.1, 1.0],      # Black metal frame
                    "trim": [0.7, 0.7, 0.7, 1.0],       # Aluminum trim
                    "foundation": [0.6, 0.6, 0.6, 1.0]
                },
                "metal": {
                    "walls": [0.6, 0.6, 0.65, 1.0],     # Galvanized steel
                    "roof": [0.4, 0.4, 0.45, 1.0],      # Darker metal roof
                    "windows": [0.2, 0.25, 0.3, 0.8],   # Industrial glass
                    "doors": [0.3, 0.3, 0.35, 1.0],     # Steel doors
                    "trim": [0.8, 0.8, 0.85, 1.0],      # Light metal trim
                    "foundation": [0.45, 0.45, 0.45, 1.0]
                },
                "stucco": {
                    "walls": [0.85, 0.8, 0.7, 1.0],     # Cream stucco
                    "roof": [0.6, 0.3, 0.2, 1.0],       # Terra cotta tiles
                    "windows": [0.2, 0.3, 0.4, 0.8],    # Tinted glass
                    "doors": [0.4, 0.25, 0.15, 1.0],    # Wood doors
                    "trim": [0.9, 0.85, 0.75, 1.0],     # Light stucco trim
                    "foundation": [0.7, 0.65, 0.6, 1.0]
                }
            }
            
            # Auto-select based on building style if scheme is "auto"
            if scheme == "auto":
                style_to_material = {
                    "modern": "glass",
                    "cottage": "wood", 
                    "gothic": "stone",
                    "art_deco": "concrete",
                    "brutalist": "concrete",
                    "glass": "glass",
                    "industrial": "metal"
                }
                scheme = style_to_material.get(building_style, "concrete")
            
            return material_colors.get(scheme, material_colors["concrete"])
        
        # Get building colors
        building_colors = get_building_colors(color_scheme, style)
        
        # Helper function to apply color to an actor
        def apply_color_to_actor(actor_result, color, component_type="walls"):
            """Apply a dynamic material color directly to the spawned actor.
            We set both BaseColor and Color to cover common param names.
            """
            try:
                if not (actor_result and actor_result.get("status") == "success"):
                    return None
                result_obj = actor_result.get("result", {})
                actor_name = result_obj.get("name")
                if not actor_name or not color:
                    return actor_result
                # Set both parameter names to ensure the tint takes effect regardless of material
                # Use the engine's BasicShapeMaterial explicitly to guarantee parameter existence
                set_actor_material_color(actor_name, color, parameter_name="BaseColor", material_path="/Engine/BasicShapes/BasicShapeMaterial")
                set_actor_material_color(actor_name, color, parameter_name="Color", material_path="/Engine/BasicShapes/BasicShapeMaterial")
                logger.debug(f"Applied {component_type} color {color} to {actor_name}")
                return actor_result
            except Exception as e:
                logger.warning(f"apply_color_to_actor failed: {e}")
                return actor_result
        
        # Generate building footprint based on shape
        def create_footprint_segments(footprint_type, base_width, base_depth):
            segments = []
            
            if footprint_type == "rectangle":
                segments.append({
                    "center": [0, 0],
                    "width": base_width,
                    "depth": base_depth,
                    "rotation": 0
                })
                
            elif footprint_type == "L_shape":
                # Main rectangle
                segments.append({
                    "center": [base_width * 0.25, 0],
                    "width": base_width * 0.5,
                    "depth": base_depth,
                    "rotation": 0
                })
                # Wing rectangle
                segments.append({
                    "center": [-base_width * 0.25, base_depth * 0.25],
                    "width": base_width * 0.5,
                    "depth": base_depth * 0.5,
                    "rotation": 0
                })
                
            elif footprint_type == "U_shape":
                # Left wing
                segments.append({
                    "center": [-base_width * 0.3, 0],
                    "width": base_width * 0.4,
                    "depth": base_depth,
                    "rotation": 0
                })
                # Right wing
                segments.append({
                    "center": [base_width * 0.3, 0],
                    "width": base_width * 0.4,
                    "depth": base_depth,
                    "rotation": 0
                })
                # Connecting section
                segments.append({
                    "center": [0, -base_depth * 0.3],
                    "width": base_width,
                    "depth": base_depth * 0.4,
                    "rotation": 0
                })
                
            elif footprint_type == "T_shape":
                # Horizontal bar
                segments.append({
                    "center": [0, base_depth * 0.25],
                    "width": base_width,
                    "depth": base_depth * 0.5,
                    "rotation": 0
                })
                # Vertical stem
                segments.append({
                    "center": [0, -base_depth * 0.25],
                    "width": base_width * 0.5,
                    "depth": base_depth * 0.5,
                    "rotation": 0
                })
                
            elif footprint_type == "cross":
                # Horizontal bar
                segments.append({
                    "center": [0, 0],
                    "width": base_width,
                    "depth": base_depth * 0.6,
                    "rotation": 0
                })
                # Vertical bar
                segments.append({
                    "center": [0, 0],
                    "width": base_width * 0.6,
                    "depth": base_depth,
                    "rotation": 0
                })
                
            elif footprint_type == "circle":
                # Approximate circle with octagon
                radius = min(base_width, base_depth) * 0.4
                for i in range(8):
                    angle = i * math.pi / 4
                    segment_width = radius * 0.8
                    segment_depth = radius * 0.4
                    segments.append({
                        "center": [radius * 0.7 * math.cos(angle), radius * 0.7 * math.sin(angle)],
                        "width": segment_width,
                        "depth": segment_depth,
                        "rotation": angle * 180 / math.pi
                    })
                    
            return segments
        
        # Create building segments
        footprint_segments = create_footprint_segments(footprint, width, depth)
        
        # Build each floor
        for floor in range(floors):
            floor_z = location[2] + floor * floor_height
            
            # Create floor slab
            if floor == 0:  # Foundation
                foundation_name = f"{name_prefix}_Foundation"
                foundation_result = unreal.send_command("spawn_actor", {
                    "name": foundation_name,
                    "type": "StaticMeshActor",
                    "location": [location[0], location[1], floor_z - 25],
                    "scale": [(width + 100)/100, (depth + 100)/100, 0.5],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if foundation_result and foundation_result.get("status") == "success":
                    all_actors.append(foundation_result.get("result"))
                    # Apply foundation color
                    apply_color_to_actor(foundation_result, building_colors["foundation"], "foundation")
            
            # Create walls for each segment
            for seg_idx, segment in enumerate(footprint_segments):
                seg_center_x = location[0] + segment["center"][0]
                seg_center_y = location[1] + segment["center"][1]
                seg_width = segment["width"]
                seg_depth = segment["depth"]
                seg_rotation = segment["rotation"]
                
                # Floor slab for this segment
                floor_name = f"{name_prefix}_Floor_{floor}_{seg_idx}"
                floor_result = unreal.send_command("spawn_actor", {
                    "name": floor_name,
                    "type": "StaticMeshActor",
                    "location": [seg_center_x, seg_center_y, floor_z],
                    "rotation": [0, seg_rotation, 0],
                    "scale": [seg_width/100, seg_depth/100, 0.3],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if floor_result and floor_result.get("status") == "success":
                    all_actors.append(floor_result.get("result"))
                    # Apply floor color (use foundation color for floors)
                    apply_color_to_actor(floor_result, building_colors["foundation"], "floor")
                
                # Create walls (4 walls per segment)
                wall_positions = [
                    {"pos": [0, -seg_depth/2], "size": [seg_width, wall_thickness], "name": "North"},
                    {"pos": [0, seg_depth/2], "size": [seg_width, wall_thickness], "name": "South"},
                    {"pos": [-seg_width/2, 0], "size": [wall_thickness, seg_depth], "name": "West"},
                    {"pos": [seg_width/2, 0], "size": [wall_thickness, seg_depth], "name": "East"}
                ]
                
                for wall_idx, wall in enumerate(wall_positions):
                    wall_x = seg_center_x + wall["pos"][0] * math.cos(math.radians(seg_rotation)) - wall["pos"][1] * math.sin(math.radians(seg_rotation))
                    wall_y = seg_center_y + wall["pos"][0] * math.sin(math.radians(seg_rotation)) + wall["pos"][1] * math.cos(math.radians(seg_rotation))
                    
                    wall_name = f"{name_prefix}_Wall_{floor}_{seg_idx}_{wall['name']}"
                    wall_result = unreal.send_command("spawn_actor", {
                        "name": wall_name,
                        "type": "StaticMeshActor",
                        "location": [wall_x, wall_y, floor_z + floor_height/2],
                        "rotation": [0, seg_rotation + (90 if wall_idx >= 2 else 0), 0],
                        "scale": [wall["size"][0]/100, wall["size"][1]/100, floor_height/100],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if wall_result and wall_result.get("status") == "success":
                        all_actors.append(wall_result.get("result"))
                        # Apply wall color
                        apply_color_to_actor(wall_result, building_colors["walls"], "walls")
                    
                    # Add windows based on facade pattern and floor
                    if include_details and floor > 0:  # No windows on ground floor for some walls
                        add_windows = True
                        if floor == 1 and entrance_side == wall["name"].lower() and seg_idx == 0:
                            add_windows = False  # Skip windows where entrance will be
                        
                        if add_windows:
                            window_count = max(1, int(wall["size"][0] / 200)) if wall_idx < 2 else max(1, int(wall["size"][1] / 200))
                            
                            for win_idx in range(window_count):
                                if random.random() < window_ratio:
                                    window_offset = (win_idx - (window_count - 1)/2) * (wall["size"][0] / window_count if wall_idx < 2 else wall["size"][1] / window_count)
                                    
                                    if wall_idx < 2:  # North/South walls
                                        win_x = wall_x + window_offset * math.cos(math.radians(seg_rotation))
                                        win_y = wall_y + window_offset * math.sin(math.radians(seg_rotation))
                                    else:  # East/West walls
                                        win_x = wall_x + window_offset * math.sin(math.radians(seg_rotation))
                                        win_y = wall_y - window_offset * math.cos(math.radians(seg_rotation))
                                    
                                    # Adjust window position to be on the wall surface
                                    normal_offset = wall_thickness * 0.6
                                    if wall["name"] == "North":
                                        win_y -= normal_offset
                                    elif wall["name"] == "South":
                                        win_y += normal_offset
                                    elif wall["name"] == "West":
                                        win_x -= normal_offset
                                    elif wall["name"] == "East":
                                        win_x += normal_offset
                                    
                                    window_name = f"{name_prefix}_Window_{floor}_{seg_idx}_{wall['name']}_{win_idx}"
                                    window_result = unreal.send_command("spawn_actor", {
                                        "name": window_name,
                                        "type": "StaticMeshActor",
                                        "location": [win_x, win_y, floor_z + floor_height * 0.6],
                                        "rotation": [0, seg_rotation + (90 if wall_idx >= 2 else 0), 0],
                                        "scale": [1.2, 0.1, 1.5],
                                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                                    })
                                    if window_result and window_result.get("status") == "success":
                                        all_actors.append(window_result.get("result"))
                                        # Apply window color
                                        apply_color_to_actor(window_result, building_colors["windows"], "windows")
                
                # Add balconies for residential styles
                if include_details and floor > 1 and style in ["cottage", "modern"] and random.random() < balcony_chance:
                    balcony_name = f"{name_prefix}_Balcony_{floor}_{seg_idx}"
                    balcony_x = seg_center_x + (seg_width/2 + 80) * math.cos(math.radians(seg_rotation))
                    balcony_y = seg_center_y + (seg_width/2 + 80) * math.sin(math.radians(seg_rotation))
                    
                    balcony_result = unreal.send_command("spawn_actor", {
                        "name": balcony_name,
                        "type": "StaticMeshActor",
                        "location": [balcony_x, balcony_y, floor_z + floor_height - 50],
                        "rotation": [0, seg_rotation, 0],
                        "scale": [2.0, 1.0, 0.1],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                    })
                    if balcony_result and balcony_result.get("status") == "success":
                        all_actors.append(balcony_result.get("result"))
                        # Apply trim color to balconies
                        apply_color_to_actor(balcony_result, building_colors["trim"], "balcony")
        
        # Create entrance on ground floor
        if include_details:
            entrance_segment = footprint_segments[0]  # Use first segment for entrance
            seg_center_x = location[0] + entrance_segment["center"][0]
            seg_center_y = location[1] + entrance_segment["center"][1]
            
            entrance_positions = {
                "front": [seg_center_x, seg_center_y - entrance_segment["depth"]/2 - wall_thickness],
                "back": [seg_center_x, seg_center_y + entrance_segment["depth"]/2 + wall_thickness],
                "left": [seg_center_x - entrance_segment["width"]/2 - wall_thickness, seg_center_y],
                "right": [seg_center_x + entrance_segment["width"]/2 + wall_thickness, seg_center_y],
                "corner": [seg_center_x - entrance_segment["width"]/3, seg_center_y - entrance_segment["depth"]/3]
            }
            
            entrance_pos = entrance_positions.get(entrance_side, entrance_positions["front"])
            
            # Main door
            door_name = f"{name_prefix}_MainDoor"
            door_result = unreal.send_command("spawn_actor", {
                "name": door_name,
                "type": "StaticMeshActor",
                "location": [entrance_pos[0], entrance_pos[1], location[2] + 120],
                "scale": [1.0, 0.2, 2.4],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            })
            if door_result and door_result.get("status") == "success":
                all_actors.append(door_result.get("result"))
                # Apply door color
                apply_color_to_actor(door_result, building_colors["doors"], "doors")
            
            # Door frame/arch
            if style in ["gothic", "art_deco"]:
                arch_name = f"{name_prefix}_DoorArch"
                arch_result = unreal.send_command("spawn_actor", {
                    "name": arch_name,
                    "type": "StaticMeshActor",
                    "location": [entrance_pos[0], entrance_pos[1], location[2] + 180],
                    "scale": [1.5, 0.3, 1.0],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder"
                })
                if arch_result and arch_result.get("status") == "success":
                    all_actors.append(arch_result.get("result"))
                    # Apply trim color to arch
                    apply_color_to_actor(arch_result, building_colors["trim"], "arch")
        
        # Create roof
        roof_z = location[2] + total_height
        
        if roof_type == "flat":
            for seg_idx, segment in enumerate(footprint_segments):
                seg_center_x = location[0] + segment["center"][0]
                seg_center_y = location[1] + segment["center"][1]
                
                roof_name = f"{name_prefix}_Roof_{seg_idx}"
                roof_result = unreal.send_command("spawn_actor", {
                    "name": roof_name,
                    "type": "StaticMeshActor",
                    "location": [seg_center_x, seg_center_y, roof_z + 25],
                    "rotation": [0, segment["rotation"], 0],
                    "scale": [(segment["width"] + 50)/100, (segment["depth"] + 50)/100, 0.5],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if roof_result and roof_result.get("status") == "success":
                    all_actors.append(roof_result.get("result"))
                    # Apply roof color
                    apply_color_to_actor(roof_result, building_colors["roof"], "roof")
        
        elif roof_type == "gable":
            main_segment = footprint_segments[0]
            seg_center_x = location[0] + main_segment["center"][0]
            seg_center_y = location[1] + main_segment["center"][1]
            
            # Two sloped roof sections
            for side in [-1, 1]:
                roof_name = f"{name_prefix}_GableRoof_{side}"
                roof_result = unreal.send_command("spawn_actor", {
                    "name": roof_name,
                    "type": "StaticMeshActor",
                    "location": [seg_center_x + side * main_segment["width"]/4, seg_center_y, roof_z + 100],
                    "rotation": [0, main_segment["rotation"], side * 25],
                    "scale": [main_segment["width"]/200, (main_segment["depth"] + 100)/100, 0.3],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if roof_result and roof_result.get("status") == "success":
                    all_actors.append(roof_result.get("result"))
                    # Apply roof color
                    apply_color_to_actor(roof_result, building_colors["roof"], "roof")
        
        elif roof_type == "cone":
            main_segment = footprint_segments[0]
            seg_center_x = location[0] + main_segment["center"][0]
            seg_center_y = location[1] + main_segment["center"][1]
            
            roof_name = f"{name_prefix}_ConeRoof"
            roof_result = unreal.send_command("spawn_actor", {
                "name": roof_name,
                "type": "StaticMeshActor",
                "location": [seg_center_x, seg_center_y, roof_z + 150],
                "rotation": [0, main_segment["rotation"], 0],
                "scale": [max(main_segment["width"], main_segment["depth"])/100, max(main_segment["width"], main_segment["depth"])/100, 3.0],
                "static_mesh": "/Engine/BasicShapes/Cone.Cone"
            })
            if roof_result and roof_result.get("status") == "success":
                all_actors.append(roof_result.get("result"))
                # Apply roof color
                apply_color_to_actor(roof_result, building_colors["roof"], "roof")
        
        elif roof_type == "dome":
            main_segment = footprint_segments[0]
            seg_center_x = location[0] + main_segment["center"][0]
            seg_center_y = location[1] + main_segment["center"][1]
            
            roof_name = f"{name_prefix}_DomeRoof"
            roof_result = unreal.send_command("spawn_actor", {
                "name": roof_name,
                "type": "StaticMeshActor",
                "location": [seg_center_x, seg_center_y, roof_z + 100],
                "rotation": [0, main_segment["rotation"], 0],
                "scale": [max(main_segment["width"], main_segment["depth"])/100, max(main_segment["width"], main_segment["depth"])/100, 2.0],
                "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
            })
            if roof_result and roof_result.get("status") == "success":
                all_actors.append(roof_result.get("result"))
                # Apply roof color
                apply_color_to_actor(roof_result, building_colors["roof"], "roof")
        
        # Add style-specific details
        if include_details:
            main_segment = footprint_segments[0]
            seg_center_x = location[0] + main_segment["center"][0]
            seg_center_y = location[1] + main_segment["center"][1]
            
            if style == "gothic":
                # Add spires on corners
                for i, corner_offset in enumerate([[-1, -1], [1, -1], [1, 1], [-1, 1]]):
                    spire_x = seg_center_x + corner_offset[0] * main_segment["width"] * 0.4
                    spire_y = seg_center_y + corner_offset[1] * main_segment["depth"] * 0.4
                    
                    spire_name = f"{name_prefix}_Spire_{i}"
                    spire_result = unreal.send_command("spawn_actor", {
                        "name": spire_name,
                        "type": "StaticMeshActor",
                        "location": [spire_x, spire_y, roof_z + 200],
                        "scale": [0.5, 0.5, 4.0],
                        "static_mesh": "/Engine/BasicShapes/Cone.Cone"
                    })
                    if spire_result and spire_result.get("status") == "success":
                        all_actors.append(spire_result.get("result"))
                        # Apply trim color to spires
                        apply_color_to_actor(spire_result, building_colors["trim"], "spire")
            
            elif style == "art_deco":
                # Add decorative crown
                crown_name = f"{name_prefix}_Crown"
                crown_result = unreal.send_command("spawn_actor", {
                    "name": crown_name,
                    "type": "StaticMeshActor",
                    "location": [seg_center_x, seg_center_y, roof_z + 100],
                    "scale": [(main_segment["width"] + 200)/100, (main_segment["depth"] + 200)/100, 1.0],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube"
                })
                if crown_result and crown_result.get("status") == "success":
                    all_actors.append(crown_result.get("result"))
                    # Apply trim color to crown
                    apply_color_to_actor(crown_result, building_colors["trim"], "crown")
        
        # Get the final material scheme used
        final_color_scheme = color_scheme
        if color_scheme == "auto":
            style_to_material = {
                "modern": "glass",
                "cottage": "wood", 
                "gothic": "stone",
                "art_deco": "concrete",
                "brutalist": "concrete",
                "glass": "glass",
                "industrial": "metal"
            }
            final_color_scheme = style_to_material.get(style, "concrete")
        
        logger.info(f"Generated building with {len(all_actors)} actors using {final_color_scheme} color scheme")
        
        return {
            "success": True,
            "message": f"Generated {style} building with {footprint} footprint, {floors} floors, {final_color_scheme} colors, and {len(all_actors)} components",
            "actors": all_actors,
            "building_stats": {
                "style": style,
                "footprint": footprint,
                "floors": floors,
                "total_height": total_height,
                "facade_pattern": facade_pattern,
                "roof_type": roof_type,
                "entrance_side": entrance_side,
                "has_balconies": balcony_chance > 0,
                "has_details": include_details,
                "color_scheme": final_color_scheme,
                "colors_applied": {
                    "walls": building_colors["walls"],
                    "roof": building_colors["roof"],
                    "windows": building_colors["windows"],
                    "doors": building_colors["doors"],
                    "trim": building_colors["trim"],
                    "foundation": building_colors["foundation"]
                },
                "total_actors": len(all_actors),
                "dimensions": {"width": width, "depth": depth, "height": total_height},
                "seed_used": seed
            }
        }
        
    except Exception as e:
        logger.error(f"generate_building error: {e}")
        return {"success": False, "message": str(e)}

# Run the server
if __name__ == "__main__":
    logger.info("Starting Advanced MCP server with stdio transport")
    mcp.run(transport='stdio') 