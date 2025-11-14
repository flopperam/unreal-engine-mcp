"""
Filename: connector_manager.py
Description: Python wrapper for Blueprint node connection management
"""

import json
import logging
from typing import Dict, Any, Optional

logger = logging.getLogger("BlueprintGraph.ConnectorManager")


def connect_nodes(
    unreal_connection,
    blueprint_name: str,
    source_node_id: str,
    source_pin_name: str,
    target_node_id: str,
    target_pin_name: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Connect two nodes in a Blueprint graph.

    Links a source pin to a target pin between existing nodes in a Blueprint's event graph or function graph.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        source_node_id: ID of the source node
        source_pin_name: Name of the output pin on the source node
        target_node_id: ID of the target node
        target_pin_name: Name of the input pin on the target node
        function_name: Optional name of function graph (if None, uses EventGraph)

    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - connection (dict): Connection details if successful
            - error (str): Error message if failed

    Example:
        >>> result = connect_nodes(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "node_1",
        ...     "execute",
        ...     "node_2",
        ...     "execute"
        ... )
        >>> print(result["connection"]["connection_type"])
        'exec'
    """
    try:
        params = {
            "blueprint_name": blueprint_name,
            "source_node_id": source_node_id,
            "source_pin_name": source_pin_name,
            "target_node_id": target_node_id,
            "target_pin_name": target_pin_name
        }

        if function_name:
            params["function_name"] = function_name

        response = unreal_connection.send_command("connect_nodes", params)

        if response.get("success"):
            logger.info(
                f"Successfully connected nodes in {blueprint_name}: "
                f"{source_node_id}.{source_pin_name} -> {target_node_id}.{target_pin_name}"
            )
        else:
            logger.error(
                f"Failed to connect nodes: {response.get('error', 'Unknown error')}"
            )

        return response

    except Exception as e:
        logger.error(f"Exception in connect_nodes: {e}")
        return {
            "success": False,
            "error": str(e)
        }


def connect_execution_pins(
    unreal_connection,
    blueprint_name: str,
    source_node_id: str,
    target_node_id: str
) -> Dict[str, Any]:
    """
    Convenience function to connect execution pins between two nodes.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        source_node_id: ID of the source node
        target_node_id: ID of the target node

    Returns:
        Dictionary containing connection details and status

    Example:
        >>> connect_execution_pins(unreal, "MyActor", "BeginPlay", "PrintNode")
    """
    return connect_nodes(
        unreal_connection,
        blueprint_name,
        source_node_id,
        "execute",
        target_node_id,
        "execute"
    )


def connect_data_pins(
    unreal_connection,
    blueprint_name: str,
    source_node_id: str,
    source_pin_name: str,
    target_node_id: str,
    target_pin_name: str
) -> Dict[str, Any]:
    """
    Convenience function to connect data pins between two nodes.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        source_node_id: ID of the source node
        source_pin_name: Name of the output data pin
        target_node_id: ID of the target node
        target_pin_name: Name of the input data pin

    Returns:
        Dictionary containing connection details and status

    Example:
        >>> connect_data_pins(unreal, "MyActor", "FloatVar", "Value", "PrintNode", "InString")
    """
    return connect_nodes(
        unreal_connection,
        blueprint_name,
        source_node_id,
        source_pin_name,
        target_node_id,
        target_pin_name
    )


def connect_variable_to_print(
    unreal_connection,
    blueprint_name: str,
    variable_node_id: str,
    print_node_id: str,
    variable_name: str
) -> Dict[str, Any]:
    """
    Convenience function to connect a variable node to a print node.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_node_id: ID of the variable get node
        print_node_id: ID of the print node
        variable_name: Name of the variable

    Returns:
        Dictionary containing connection details and status

    Example:
        >>> connect_variable_to_print(unreal, "MyActor", "VarNode", "PrintNode", "MyFloat")
    """
    return connect_nodes(
        unreal_connection,
        blueprint_name,
        variable_node_id,
        variable_name,
        print_node_id,
        "InString"
    )