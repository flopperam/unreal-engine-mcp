"""
Actor Name Management System

Centralized system for managing unique actor names across all MCP functions.
Prevents duplicate name errors by automatically generating unique names and tracking actors.
"""

import logging
import time
import uuid
from typing import Dict, Any, Set, Optional

# Configure logging
logger = logging.getLogger("ActorNameManager")

class ActorNameManager:
    """Centralized system for managing unique actor names across all MCP functions."""
    
    def __init__(self):
        self._known_actors: Set[str] = set()
        self._session_id = str(int(time.time()))[-6:]  # Last 6 digits of timestamp
        self._actor_counters: Dict[str, int] = {}
        logger.info(f"ActorNameManager initialized with session ID: {self._session_id}")
        
    
    def generate_unique_name(self, base_name: str, unreal_connection=None) -> str:
        """
        Generate a unique actor name based on the desired base name.
        
        Strategy:
        1. First try the base name as-is
        2. If that exists, try base_name with session suffix
        3. If that exists, try base_name with counter
        4. If that exists, try base_name with session + counter + unique_id
        """
        # Clean the base name
        base_name = str(base_name).strip()
        if not base_name:
            base_name = f"Actor_{self._session_id}"
        
        # Strategy 1: Try base name as-is
        if not self._actor_exists(base_name, unreal_connection):
            self._known_actors.add(base_name)
            return base_name
        
        # Strategy 2: Try with session ID
        session_name = f"{base_name}_{self._session_id}"
        if not self._actor_exists(session_name, unreal_connection):
            self._known_actors.add(session_name)
            return session_name
        
        # Strategy 3: Try with counter
        counter_key = base_name
        if counter_key not in self._actor_counters:
            self._actor_counters[counter_key] = 0
        
        for attempt in range(1000):  # Prevent infinite loops
            self._actor_counters[counter_key] += 1
            counter_name = f"{base_name}_{self._actor_counters[counter_key]}"
            
            if not self._actor_exists(counter_name, unreal_connection):
                self._known_actors.add(counter_name)
                return counter_name
        
        # Strategy 4: Ultimate fallback - session + counter + UUID
        unique_suffix = str(uuid.uuid4())[:8]
        final_name = f"{base_name}_{self._session_id}_{self._actor_counters[counter_key]}_{unique_suffix}"
        self._known_actors.add(final_name)
        
        logger.info(f"Generated unique name: {base_name} -> {final_name}")
        return final_name
    
    def _actor_exists(self, name: str, unreal_connection=None) -> bool:
        """Check if an actor with the given name exists."""
        # First check our local cache
        if name in self._known_actors:
            return True
        
        # If we have a connection, check with Unreal Engine
        if unreal_connection:
            try:
                response = unreal_connection.send_command("find_actors_by_name", {"pattern": name})
                if response and response.get("success") and "actors" in response:
                    actors = response["actors"]
                    if isinstance(actors, list) and len(actors) > 0:
                        # Found actor(s) with this name
                        self._known_actors.add(name)
                        return True
            except Exception as e:
                logger.debug(f"Error checking actor existence for '{name}': {e}")
        
        return False
    
    def mark_actor_created(self, name: str):
        """Mark an actor as created (add to known actors)."""
        self._known_actors.add(name)
    
    def remove_actor(self, name: str):
        """Remove an actor from known actors (when deleted)."""
        self._known_actors.discard(name)
    

# Global actor name manager instance
_global_actor_name_manager = ActorNameManager()

def get_global_actor_name_manager() -> ActorNameManager:
    """Get the global actor name manager instance."""
    return _global_actor_name_manager

def get_unique_actor_name(base_name: str, unreal_connection=None) -> str:
    """Public interface to get a unique actor name."""
    return _global_actor_name_manager.generate_unique_name(base_name, unreal_connection)

def safe_spawn_actor(unreal_connection, params: Dict[str, Any], auto_unique_name: bool = True) -> Dict[str, Any]:
    """
    Safely spawn an actor with automatic unique name generation.
    
    Args:
        unreal_connection: The Unreal connection to use
        params: Parameters for spawn_actor command
        auto_unique_name: Whether to automatically generate unique names (default True)
    
    Returns:
        Response from Unreal Engine with success/error status
    """
    if not unreal_connection:
        return {"success": False, "status": "error", "error": "No Unreal connection available"}
    
    original_name = params.get("name", "Actor")
    
    if auto_unique_name:
        # Generate unique name
        unique_name = _global_actor_name_manager.generate_unique_name(original_name, unreal_connection)
        params["name"] = unique_name
        
        # Log name change if it occurred
        if unique_name != original_name:
            logger.debug(f"Actor name changed: '{original_name}' -> '{unique_name}'")
    
    try:
        # Attempt to spawn the actor
        response = unreal_connection.send_command("spawn_actor", params)
        
        if response and response.get("success"):
            # Mark the actor as successfully created
            _global_actor_name_manager.mark_actor_created(params["name"])
            
            # Ensure the response includes the final name used
            if "result" in response:
                if isinstance(response["result"], dict):
                    response["result"]["final_name"] = params["name"]
                    response["result"]["original_name"] = original_name
        
        return response or {"success": False, "status": "error", "error": "No response from Unreal"}
        
    except Exception as e:
        logger.error(f"Error in safe_spawn_actor: {e}")
        return {"success": False, "status": "error", "error": str(e)}

def safe_delete_actor(unreal_connection, actor_name: str) -> Dict[str, Any]:
    """
    Safely delete an actor and update the name tracking.
    
    Args:
        unreal_connection: The Unreal connection to use
        actor_name: Name of the actor to delete
        
    Returns:
        Response from Unreal Engine
    """
    if not unreal_connection:
        return {"success": False, "message": "No Unreal connection available"}
    
    try:
        response = unreal_connection.send_command("delete_actor", {"name": actor_name})
        
        if response and response.get("success"):
            # Remove from our tracking
            _global_actor_name_manager.remove_actor(actor_name)
        
        return response or {"success": False, "message": "No response from Unreal"}
        
    except Exception as e:
        logger.error(f"Error in safe_delete_actor: {e}")
        return {"success": False, "message": str(e)}


