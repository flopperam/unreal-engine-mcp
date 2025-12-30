"""
Filename: event_manager.py
Description: Python wrapper for Blueprint event node management
"""

import json
import logging
from typing import Dict, Any, Optional

logger = logging.getLogger("BlueprintGraph.EventManager")


def add_event_node(
    unreal_connection,
    blueprint_name: str,
    event_name: str,
    pos_x: float = 0,
    pos_y: float = 0
) -> Dict[str, Any]:
    """
    Add an event node to a Blueprint graph.

    This function creates specialized event nodes (ReceiveBeginPlay, ReceiveTick, etc.)
    in a Blueprint's event graph. Unlike the generic add_node function, this directly
    creates event nodes using the dedicated event node creation system.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        event_name: Name of the event (e.g., "ReceiveBeginPlay", "ReceiveTick", "ReceiveDestroyed")
        pos_x: X position in graph (default: 0)
        pos_y: Y position in graph (default: 0)

    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - node_id (str): GUID of created event node
            - event_name (str): Name of the event
            - pos_x (float): X position
            - pos_y (float): Y position
            - error (str): Error message if failed

    Example:
        >>> result = add_event_node(
        ...     unreal,
        ...     "MyActorBlueprint",
        ...     "ReceiveBeginPlay",
        ...     pos_x=0,
        ...     pos_y=0
        ... )
        >>> if result["success"]:
        ...     print(f"Event node created: {result['node_id']}")

    Common event names:
        - ReceiveBeginPlay: Called when the actor begins play
        - ReceiveTick: Called every frame
        - ReceiveEndPlay: Called when the actor is being destroyed
        - ReceiveDestroyed: Called when the actor is destroyed
        - ReceiveAnyDamage: Called when the actor takes damage
        - ReceiveActorBeginOverlap: Called when actor begins overlap
        - ReceiveActorEndOverlap: Called when actor ends overlap
    """
    try:
        params = {
            "blueprint_name": blueprint_name,
            "event_name": event_name,
            "pos_x": pos_x,
            "pos_y": pos_y
        }

        response = unreal_connection.send_command("add_event_node", params)

        if response.get("success"):
            logger.info(
                f"Successfully added event node '{event_name}' to {blueprint_name}. "
                f"Node ID: {response.get('node_id')}"
            )
        else:
            logger.error(
                f"Failed to add event node: {response.get('error', 'Unknown error')}"
            )

        return response

    except Exception as e:
        logger.error(f"Exception in add_event_node: {e}")
        return {
            "success": False,
            "error": str(e)
        }
