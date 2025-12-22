"""
Filename: node_manager.py
Description: Python wrapper for Blueprint node management
"""

import json
import logging
from typing import Dict, Any, Optional, List

logger = logging.getLogger("BlueprintGraph.NodeManager")


def add_node(
    unreal_connection,
    blueprint_name: str,
    node_type: str,
    node_params: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """
    Add a node to a Blueprint graph.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        node_type: Type of node to create ("Print", "Event", "VariableGet", "VariableSet", "CallFunction")
        node_params: Additional parameters for the node:
            - pos_x (float): X position in graph (default: 0)
            - pos_y (float): Y position in graph (default: 0)
            - message (str): For Print nodes, the text to print
            - event_type (str): For Event nodes, the event name (BeginPlay, Tick, etc.)
            - variable_name (str): For Variable nodes, the variable name
            - target_function (str): For CallFunction nodes, the function to call
            - target_blueprint (str): For CallFunction nodes, optional path to target Blueprint
            - function_name (str): Optional name of function graph to add node to (if None, uses EventGraph)

    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - node_id (str): GUID of created node
            - node_type (str): Type of node created
            - pos_x (float): X position
            - pos_y (float): Y position
            - error (str): Error message if failed

    Example:
        >>> result = add_node(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "Print",
        ...     {"pos_x": 100, "pos_y": 200, "message": "Hello World"}
        ... )
        >>> print(result["node_id"])
        '8B3A4F2C-4D5E-6F7A-8B9C-0D1E2F3A4B5C'
    """
    if node_params is None:
        node_params = {}
    
    try:
        response = unreal_connection.send_command("add_blueprint_node", {
            "blueprint_name": blueprint_name,
            "node_type": node_type,
            "node_params": node_params
        })
        
        if response.get("success"):
            logger.info(
                f"Successfully added {node_type} node to {blueprint_name}. "
                f"Node ID: {response.get('node_id')}"
            )
        else:
            logger.error(
                f"Failed to add node: {response.get('error', 'Unknown error')}"
            )
        
        return response
        
    except Exception as e:
        logger.error(f"Exception in add_node: {e}")
        return {
            "success": False,
            "error": str(e)
        }


def add_print_node(
    unreal_connection,
    blueprint_name: str,
    message: str = "Hello World",
    pos_x: float = 0,
    pos_y: float = 0
) -> Dict[str, Any]:
    """
    Convenience function to add a Print String node.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        message: Text to print
        pos_x: X position in graph
        pos_y: Y position in graph
    
    Returns:
        Dictionary containing node_id and status
        
    Example:
        >>> add_print_node(unreal, "MyActor", "Debug message", 100, 200)
    """
    return add_node(
        unreal_connection,
        blueprint_name,
        "Print",
        {
            "pos_x": pos_x,
            "pos_y": pos_y,
            "message": message
        }
    )


def add_event_node(
    unreal_connection,
    blueprint_name: str,
    event_type: str = "BeginPlay",
    pos_x: float = 0,
    pos_y: float = 0
) -> Dict[str, Any]:
    """
    Convenience function to add an Event node.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        event_type: Type of event ("BeginPlay", "Tick", "ActorBeginOverlap", etc.)
        pos_x: X position in graph
        pos_y: Y position in graph
    
    Returns:
        Dictionary containing node_id and status
        
    Example:
        >>> add_event_node(unreal, "MyActor", "BeginPlay", 0, 0)
    """
    return add_node(
        unreal_connection,
        blueprint_name,
        "Event",
        {
            "pos_x": pos_x,
            "pos_y": pos_y,
            "event_type": event_type
        }
    )


def add_variable_get_node(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    pos_x: float = 0,
    pos_y: float = 0
) -> Dict[str, Any]:
    """
    Add a Variable Get node to retrieve a variable's value.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable to get
        pos_x: X position in graph
        pos_y: Y position in graph
    
    Returns:
        Dictionary containing node_id and status
        
    Example:
        >>> add_variable_get_node(unreal, "MyActor", "Speed", 200, 100)
    """
    return add_node(
        unreal_connection,
        blueprint_name,
        "VariableGet",
        {
            "pos_x": pos_x,
            "pos_y": pos_y,
            "variable_name": variable_name
        }
    )


def add_variable_set_node(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    pos_x: float = 0,
    pos_y: float = 0
) -> Dict[str, Any]:
    """
    Add a Variable Set node to modify a variable's value.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable to set
        pos_x: X position in graph
        pos_y: Y position in graph

    Returns:
        Dictionary containing node_id and status

    Example:
        >>> add_variable_set_node(unreal, "MyActor", "Health", 300, 200)
    """
    return add_node(
        unreal_connection,
        blueprint_name,
        "VariableSet",
        {
            "pos_x": pos_x,
            "pos_y": pos_y,
            "variable_name": variable_name
        }
    )


def add_call_function_node(
    unreal_connection,
    blueprint_name: str,
    target_function: str,
    pos_x: float = 0,
    pos_y: float = 0,
    target_blueprint: Optional[str] = None
) -> Dict[str, Any]:
    """
    Add a Call Function node to call a Blueprint function.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint containing the EventGraph
        target_function: Name of the function to call
        pos_x: X position in graph
        pos_y: Y position in graph
        target_blueprint: Optional path to target Blueprint (defaults to same blueprint)

    Returns:
        Dictionary containing node_id and status

    Example:
        >>> add_call_function_node(unreal, "MyActor", "MyFunction", 400, 0)
    """
    node_params = {
        "pos_x": pos_x,
        "pos_y": pos_y,
        "target_function": target_function
    }

    if target_blueprint:
        node_params["target_blueprint"] = target_blueprint

    return add_node(
        unreal_connection,
        blueprint_name,
        "CallFunction",
        node_params
    )
