"""
Unreal Engine MCP Server - Blueprint-focused

This server exposes tools dedicated to interacting with and creating custom Blueprints.
It includes utilities for variables, events, function nodes, node connections, and
simple/interactive Blueprint generation from natural language descriptions.
"""

import logging
import socket
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional, List

from mcp.server.fastmcp import FastMCP


# Configure logging
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp_blueprints.log'),
    ]
)
logger = logging.getLogger("UnrealMCP_Blueprints")


# Connection configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557


class UnrealConnection:
    """Connection to an Unreal Engine instance."""

    def __init__(self):
        self.socket: Optional[socket.socket] = None
        self.connected: bool = False

    def connect(self) -> bool:
        try:
            if self.socket:
                try:
                    self.socket.close()
                except Exception:
                    pass
                self.socket = None

            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(30)

            # Socket options
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
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
        self.socket = None
        self.connected = False

    def _receive_full_response(self, sock: socket.socket, buffer_size: int = 4096) -> bytes:
        chunks: List[bytes] = []
        sock.settimeout(30)
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)

                data = b"".join(chunks)
                decoded = data.decode("utf-8")
                try:
                    json.loads(decoded)
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    logger.debug("Received partial response, waiting for more data...")
                    continue
                except Exception as e:
                    logger.warning(f"Error processing response chunk: {str(e)}")
                    continue
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                data = b"".join(chunks)
                try:
                    json.loads(data.decode("utf-8"))
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except Exception:
                    pass
            raise Exception("Timeout receiving Unreal response")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise

    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        # Reconnect per command for stability
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None
            self.connected = False

        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None

        try:
            payload = {
                "type": command,
                "params": params or {},
            }
            payload_json = json.dumps(payload)
            logger.info(f"Sending command: {payload_json}")
            self.socket.sendall(payload_json.encode("utf-8"))

            response_data = self._receive_full_response(self.socket)
            response = json.loads(response_data.decode("utf-8"))

            logger.info(f"Complete response from Unreal: {response}")

            # Normalize common error shapes
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                response = {"status": "error", "error": error_message}

            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None
            self.connected = False

            return response
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            self.connected = False
            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None
            return {"status": "error", "error": str(e)}


# Global connection state
_unreal_connection: Optional[UnrealConnection] = None


def get_unreal_connection() -> Optional[UnrealConnection]:
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
    global _unreal_connection
    logger.info("UnrealMCP Blueprints server starting up")
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
        logger.info("Unreal MCP Blueprints server shut down")


# Initialize server
mcp = FastMCP(
    "UnrealMCP_Blueprints",
    description="Blueprint-focused MCP server for Unreal Engine: variables, events, nodes, and simple generators",
    lifespan=server_lifespan,
)


# Internal convenience wrappers used by higher-level tools
def _ok(result: Optional[Dict[str, Any]]) -> bool:
    return bool(result) and (result.get("success") is True or result.get("status") == "success")


def _create_blueprint(name: str, parent_class: str) -> Dict[str, Any]:
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    return unreal.send_command("create_blueprint", {"name": name, "parent_class": parent_class}) or {"success": False, "message": "No response from Unreal"}


def _add_component_to_blueprint(
    blueprint_name: str,
    component_type: str,
    component_name: str,
    location: List[float] = None,
    rotation: List[float] = None,
    scale: List[float] = None,
    component_properties: Dict[str, Any] = None,
) -> Dict[str, Any]:
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    params: Dict[str, Any] = {
        "blueprint_name": blueprint_name,
        "component_type": component_type,
        "component_name": component_name,
        "location": location or [],
        "rotation": rotation or [],
        "scale": scale or [],
        "component_properties": component_properties or {},
    }
    return unreal.send_command("add_component_to_blueprint", params) or {"success": False, "message": "No response from Unreal"}


def _set_physics_properties(
    blueprint_name: str,
    component_name: str,
    simulate_physics: bool = True,
    gravity_enabled: bool = True,
    mass: float = 1.0,
    linear_damping: float = 0.01,
    angular_damping: float = 0.0,
) -> Dict[str, Any]:
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    params = {
        "blueprint_name": blueprint_name,
        "component_name": component_name,
        "simulate_physics": simulate_physics,
        "gravity_enabled": gravity_enabled,
        "mass": mass,
        "linear_damping": linear_damping,
        "angular_damping": angular_damping,
    }
    return unreal.send_command("set_physics_properties", params) or {"success": False, "message": "No response from Unreal"}


def _compile_blueprint(blueprint_name: str) -> Dict[str, Any]:
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    return unreal.send_command("compile_blueprint", {"blueprint_name": blueprint_name}) or {"success": False, "message": "No response from Unreal"}


# Blueprint-focused tools
@mcp.tool()
def create_blueprint_variable(blueprint_name: str, variable_name: str, variable_type: str, default_value: Any = None) -> Dict[str, Any]:
    """Create a variable in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    try:
        params = {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
            "variable_type": variable_type,
            "default_value": default_value,
        }
        response = unreal.send_command("create_blueprint_variable", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"create_blueprint_variable error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def add_blueprint_event_node(blueprint_name: str, event_type: str, node_position: List[float] = None) -> Dict[str, Any]:
    """Add an event node to a Blueprint's event graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    try:
        params = {
            "blueprint_name": blueprint_name,
            "event_type": event_type,
            "node_position": node_position or [0, 0],
        }
        response = unreal.send_command("add_blueprint_event_node", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"add_blueprint_event_node error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def add_blueprint_function_node(
    blueprint_name: str,
    function_name: str,
    function_class: str = None,
    node_position: List[float] = None,
) -> Dict[str, Any]:
    """Add a function call node to a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    try:
        params = {
            "blueprint_name": blueprint_name,
            "function_name": function_name,
            "function_class": function_class,
            "node_position": node_position or [300, 0],
        }
        response = unreal.send_command("add_blueprint_function_node", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"add_blueprint_function_node error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def connect_blueprint_nodes(
    blueprint_name: str,
    source_node_id: str,
    source_pin: str,
    target_node_id: str,
    target_pin: str,
) -> Dict[str, Any]:
    """Connect two nodes in a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    try:
        params = {
            "blueprint_name": blueprint_name,
            "source_node_id": source_node_id,
            "source_pin": source_pin,
            "target_node_id": target_node_id,
            "target_pin": target_pin,
        }
        response = unreal.send_command("connect_blueprint_nodes", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"connect_blueprint_nodes error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def add_blueprint_branch_node(blueprint_name: str, node_position: List[float] = None) -> Dict[str, Any]:
    """Add a branch (if-then-else) node to a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    try:
        params = {
            "blueprint_name": blueprint_name,
            "node_position": node_position or [600, 0],
        }
        response = unreal.send_command("add_blueprint_branch_node", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"add_blueprint_branch_node error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def create_blueprint_custom_event(
    blueprint_name: str,
    event_name: str,
    input_params: List[Dict[str, str]] = None,
) -> Dict[str, Any]:
    """Create a custom event in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    try:
        params = {
            "blueprint_name": blueprint_name,
            "event_name": event_name,
            "input_params": input_params or [],
        }
        response = unreal.send_command("create_blueprint_custom_event", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"create_blueprint_custom_event error: {e}")
        return {"success": False, "message": str(e)}


@mcp.tool()
def create_simple_blueprint_from_description(description: str) -> Dict[str, Any]:
    """Create a simple Blueprint from a natural language description."""
    import re

    description_lower = description.lower()

    # Determine basic BP name and parent
    if "player" in description_lower:
        bp_name = "BP_Player"
        parent_class = "Pawn"
    elif "pickup" in description_lower or "collectible" in description_lower:
        bp_name = "BP_Pickup"
        parent_class = "Actor"
    elif "enemy" in description_lower or "ai" in description_lower:
        bp_name = "BP_Enemy"
        parent_class = "Pawn"
    else:
        bp_name = "BP_Custom"
        parent_class = "Actor"

    name_match = re.search(r"called\s+(\w+)|named\s+(\w+)", description_lower)
    if name_match:
        custom_name = name_match.group(1) or name_match.group(2)
        bp_name = f"BP_{custom_name.title()}"

    # Create BP
    result = _create_blueprint(bp_name, parent_class)
    if not _ok(result):
        return result or {"success": False, "message": "Create blueprint failed"}

    components_added: List[str] = []
    main_mesh_added = False

    if "mesh" in description_lower or "visual" in description_lower:
        _add_component_to_blueprint(bp_name, "StaticMeshComponent", "MainMesh")
        components_added.append("StaticMeshComponent")
        main_mesh_added = True

    if "collision" in description_lower or "collide" in description_lower:
        _add_component_to_blueprint(bp_name, "BoxComponent", "CollisionBox")
        components_added.append("BoxComponent")

    if any(k in description_lower for k in ["physics", "fall", "gravity"]) and main_mesh_added:
        _set_physics_properties(bp_name, "MainMesh", simulate_physics=True, gravity_enabled=True)

    if "light" in description_lower:
        _add_component_to_blueprint(bp_name, "PointLightComponent", "Light")
        components_added.append("PointLightComponent")

    if "sound" in description_lower or "audio" in description_lower:
        _add_component_to_blueprint(bp_name, "AudioComponent", "Sound")
        components_added.append("AudioComponent")

    events_added: List[str] = []
    if any(k in description_lower for k in ["start", "begin", "spawn"]):
        add_blueprint_event_node(bp_name, "BeginPlay")
        events_added.append("BeginPlay")

    if any(k in description_lower for k in ["tick", "every frame", "continuously"]):
        add_blueprint_event_node(bp_name, "Tick")
        events_added.append("Tick")

    if any(k in description_lower for k in ["overlap", "touch", "collide"]):
        add_blueprint_event_node(bp_name, "BeginOverlap")
        events_added.append("BeginOverlap")

    variables_added: List[str] = []
    if any(k in description_lower for k in ["health", "hp"]):
        create_blueprint_variable(bp_name, "Health", "float", 100.0)
        variables_added.append("Health")

    if "speed" in description_lower:
        create_blueprint_variable(bp_name, "Speed", "float", 600.0)
        variables_added.append("Speed")

    if "damage" in description_lower:
        create_blueprint_variable(bp_name, "Damage", "float", 10.0)
        variables_added.append("Damage")

    _compile_blueprint(bp_name)

    return {
        "success": True,
        "blueprint_name": bp_name,
        "parent_class": parent_class,
        "components": components_added,
        "events": events_added,
        "variables": variables_added,
        "description": f"Created {bp_name} from description: {description}",
    }


@mcp.tool()
def create_interactive_blueprint(name: str, interaction_type: str = "pickup") -> Dict[str, Any]:
    """Create a Blueprint with built-in interaction logic (pickup, button, trigger, door)."""
    bp_name = f"BP_{name}"

    result = _create_blueprint(bp_name, "Actor")
    if not _ok(result):
        return result or {"success": False, "message": "Create blueprint failed"}

    _add_component_to_blueprint(bp_name, "StaticMeshComponent", "BaseMesh")
    _add_component_to_blueprint(bp_name, "SphereComponent", "TriggerSphere")

    if interaction_type == "pickup":
        create_blueprint_variable(bp_name, "IsCollected", "bool", False)
        create_blueprint_variable(bp_name, "RotationSpeed", "float", 90.0)

        add_blueprint_event_node(bp_name, "Tick", [0, 0])
        add_blueprint_function_node(bp_name, "AddLocalRotation", None, [300, 0])

        add_blueprint_event_node(bp_name, "BeginOverlap", [0, 200])
        add_blueprint_function_node(bp_name, "DestroyActor", None, [300, 200])

    elif interaction_type == "button":
        create_blueprint_variable(bp_name, "IsPressed", "bool", False)
        create_blueprint_custom_event(bp_name, "OnButtonPressed", [])

        add_blueprint_event_node(bp_name, "BeginOverlap", [0, 0])
        add_blueprint_branch_node(bp_name, [300, 0])

    elif interaction_type == "trigger":
        create_blueprint_variable(bp_name, "IsTriggered", "bool", False)
        create_blueprint_custom_event(bp_name, "OnTriggerEnter", [{"name": "Actor", "type": "Actor"}])
        create_blueprint_custom_event(bp_name, "OnTriggerExit", [{"name": "Actor", "type": "Actor"}])

        add_blueprint_event_node(bp_name, "BeginOverlap", [0, 0])
        add_blueprint_event_node(bp_name, "EndOverlap", [0, 200])

    elif interaction_type == "door":
        create_blueprint_variable(bp_name, "IsOpen", "bool", False)
        create_blueprint_variable(bp_name, "OpenAngle", "float", 90.0)
        create_blueprint_custom_event(bp_name, "OpenDoor", [])
        create_blueprint_custom_event(bp_name, "CloseDoor", [])

        add_blueprint_event_node(bp_name, "BeginPlay", [0, 0])

    _compile_blueprint(bp_name)

    return {
        "success": True,
        "blueprint_name": bp_name,
        "interaction_type": interaction_type,
        "message": f"Created interactive {interaction_type} Blueprint: {bp_name}",
    }





@mcp.tool()
def create_blueprint_from_prompt(prompt: str) -> Dict[str, Any]:
    """
    Create any blueprint from a natural language prompt with automatic node connections.
    
    Examples:
    - "Create a tree that grows taller over time"
    - "Make a pickup item called HealthPotion that rotates and can be collected"
    - "Build a door that opens when the player touches it"
    - "Create an enemy AI that moves and attacks the player"
    - "Make a button that spawns objects when pressed"
    
    Args:
        prompt: Natural language description of what the blueprint should do
    """
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    try:
        params = {
            "prompt": prompt,
        }
        response = unreal.send_command("create_blueprint_from_prompt", params)
        return response or {"success": False, "message": "No response from Unreal"}
    except Exception as e:
        logger.error(f"create_blueprint_from_prompt error: {e}")
        return {"success": False, "message": str(e)}


if __name__ == "__main__":
    logger.info("Starting Blueprint-focused MCP server with stdio transport")
    mcp.run(transport='stdio')


