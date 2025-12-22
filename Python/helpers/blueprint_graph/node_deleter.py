"""
Filename: node_deleter.py
Description: Python wrapper for Blueprint node deletion
"""

import json
import logging
from typing import Dict, Any, Optional

logger = logging.getLogger("BlueprintGraph.NodeDeleter")


def delete_node(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Delete a node from a Blueprint graph.

    This function removes a node and all its connections from either the
    EventGraph or a specific function graph.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        node_id: ID of the node to delete (NodeGuid or GetName())
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - deleted_node_id (str): ID of deleted node
            - error (str): Error message if failed

    Example:
        >>> result = delete_node(
        ...     unreal,
        ...     "MyActorBlueprint",
        ...     "K2Node_1234567890"
        ... )
        >>> if result["success"]:
        ...     print(f"Deleted node: {result['deleted_node_id']}")
    """
    try:
        params = {
            "blueprint_name": blueprint_name,
            "node_id": node_id
        }

        if function_name is not None:
            params["function_name"] = function_name

        response = unreal_connection.send_command("delete_node", params)

        if response.get("success"):
            logger.info(
                f"Successfully deleted node '{node_id}' from {blueprint_name}. "
                f"Deleted Node ID: {response.get('deleted_node_id')}"
            )
        else:
            logger.error(
                f"Failed to delete node: {response.get('error', 'Unknown error')}"
            )

        return response

    except Exception as e:
        logger.error(f"Exception in delete_node: {e}")
        return {
            "success": False,
            "error": str(e)
        }
