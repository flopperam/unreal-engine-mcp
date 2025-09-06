"""
Actor utility functions for Unreal MCP Server.
Contains helper functions for spawning and managing actors.
"""
from typing import Dict, Any, List
import logging

logger = logging.getLogger(__name__)

# Import actor name manager functions
try:
    from .actor_name_manager import get_unique_actor_name, get_global_actor_name_manager
except ImportError:
    logger.warning("Could not import actor_name_manager, unique name generation disabled")
    def get_unique_actor_name(base_name: str, unreal_connection=None) -> str:
        return base_name
    def get_global_actor_name_manager():
        return None


def spawn_blueprint_actor(
    unreal_connection,
    blueprint_name: str,
    actor_name: str,
    location: List[float] = [0, 0, 0],
    rotation: List[float] = [0, 0, 0],
    auto_unique_name: bool = True
) -> Dict[str, Any]:
    """
    Spawn an actor from a Blueprint using the provided Unreal connection.
    
    Args:
        unreal_connection: The Unreal Engine connection object
        blueprint_name: Name of the blueprint to spawn
        actor_name: Name to give the spawned actor
        location: [x, y, z] position to spawn at
        rotation: [roll, pitch, yaw] rotation to apply
        auto_unique_name: Whether to automatically generate unique names (default True)
        
    Returns:
        Dict containing success status and result data
    """
    try:
        if not unreal_connection:
            return {"success": False, "message": "No Unreal connection provided"}
        
        original_name = actor_name
        
        # Generate unique name if requested
        if auto_unique_name:
            unique_name = get_unique_actor_name(actor_name, unreal_connection)
            if unique_name != actor_name:
                logger.debug(f"Blueprint actor name changed: '{actor_name}' -> '{unique_name}'")
                actor_name = unique_name
        
        params = {
            "blueprint_name": blueprint_name,
            "actor_name": actor_name,
            "location": location,
            "rotation": rotation
        }
        
        response = unreal_connection.send_command("spawn_blueprint_actor", params)
        
        # Mark actor as created if successful
        if response and response.get("success"):
            manager = get_global_actor_name_manager()
            if manager:
                manager.mark_actor_created(actor_name)
            
            # Add name information to response
            if "result" in response and isinstance(response["result"], dict):
                response["result"]["final_name"] = actor_name
                response["result"]["original_name"] = original_name
        
        return response or {"success": False, "message": "No response from Unreal"}
        
    except Exception as e:
        logger.error(f"spawn_blueprint_actor helper error: {e}")
        return {"success": False, "message": str(e)}


def get_blueprint_material_info(
    unreal_connection,
    blueprint_name: str,
    component_name: str
) -> Dict[str, Any]:
    """
    Get information about the material slots available on a blueprint component.
    
    Args:
        unreal_connection: The Unreal Engine connection object
        blueprint_name: Name of the blueprint to query
        component_name: Name of the component to check material slots for
        
    Returns:
        Dict containing success status and material slot information
    """
    try:
        if not unreal_connection:
            return {"success": False, "message": "No Unreal connection provided"}
        
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name
        }
        
        response = unreal_connection.send_command("get_blueprint_material_info", params)
        return response or {"success": False, "message": "No response from Unreal"}
        
    except Exception as e:
        logger.error(f"get_blueprint_material_info helper error: {e}")
        return {"success": False, "message": str(e)}