"""
Filename: node_properties.py
Creation Date: 2025-10-26
Author: Zoscran
Description: Python wrapper for Blueprint node property modification
"""

import json
import logging
from typing import Dict, Any, Optional

logger = logging.getLogger("BlueprintGraph.NodeProperties")


def set_node_property(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    property_name: str,
    property_value: Any,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Set a property on a Blueprint node.

    Modify properties of existing nodes such as Print message text,
    variable names, node positions, or comments.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        node_id: ID of the node to modify
        property_name: Name of the property to set
        property_value: Value to set (will be JSON serialized)
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - updated_property (str): Name of updated property
            - error (str): Error message if failed

    Supported properties by node type:
        - Print nodes: "message", "duration", "text_color"
        - Variable nodes: "variable_name"
        - Event nodes: "event_name"
        - All nodes: "pos_x", "pos_y", "comment"

    Example:
        >>> result = set_node_property(
        ...     unreal,
        ...     "MyActorBlueprint",
        ...     "K2Node_1234567890",
        ...     "message",
        ...     "Hello World!"
        ... )
        >>> if result["success"]:
        ...     print(f"Updated: {result['updated_property']}")
    """
    try:
        params = {
            "blueprint_name": blueprint_name,
            "node_id": node_id,
            "property_name": property_name,
            "property_value": property_value
        }

        if function_name is not None:
            params["function_name"] = function_name

        response = unreal_connection.send_command("set_node_property", params)

        if response.get("success"):
            logger.info(
                f"Successfully set '{property_name}' on node '{node_id}' in {blueprint_name}"
            )
        else:
            logger.error(
                f"Failed to set node property: {response.get('error', 'Unknown error')}"
            )

        return response

    except Exception as e:
        logger.error(f"Exception in set_node_property: {e}")
        return {
            "success": False,
            "error": str(e)
        }
