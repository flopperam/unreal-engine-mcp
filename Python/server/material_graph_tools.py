"""Material graph tools for the Unreal MCP server."""

import logging
import json
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def add_material_node(
    material_name: str,
    node_type: str,
    pos_x: float = 0,
    pos_y: float = 0,
    node_params: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """Add a node to a Material graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "material_name": material_name,
            "node_type": node_type,
            "pos_x": pos_x,
            "pos_y": pos_y,
            "node_params": node_params or {}
        }

        result = unreal.send_command("add_material_node", params)
        return result or make_error_response("No response from Unreal")

    except Exception as e:
        logger.error(f"add_material_node error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def connect_material_nodes(
    material_name: str,
    source_node_id: str,
    source_pin_name: str,
    target_node_id: str,
    target_pin_name: str
) -> Dict[str, Any]:
    """Connect two nodes in a Material graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "material_name": material_name,
            "source_node_id": source_node_id,
            "source_pin_name": source_pin_name,
            "target_node_id": target_node_id,
            "target_pin_name": target_pin_name
        }

        result = unreal.send_command("connect_material_nodes", params)
        return result or make_error_response("No response from Unreal")

    except Exception as e:
        logger.error(f"connect_material_nodes error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def apply_material_json(material_name: str, json_data: str) -> Dict[str, Any]:
    """Apply a JSON string to create Material nodes and connections.

    The JSON structure should be:
    {
      "nodes": [
        {"id": "node1", "type": "TextureSample", "params": {"texture": "/Game/Textures/T_Wood"}},
        {"id": "node2", "type": "Multiply", "params": {"const_b": 2.0}}
      ],
      "connections": [
        {"source_id": "node1", "source_pin": "RGB", "target_id": "node2", "target_pin": "A"},
        {"source_id": "node2", "source_pin": "", "target_id": "BaseColor", "target_pin": ""}
      ]
    }
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        data = json.loads(json_data)
        results = {"nodes": [], "connections": [], "errors": []}
        node_id_map = {}

        for node in data.get("nodes", []):
            try:
                params = {
                    "material_name": material_name,
                    "node_type": node.get("type"),
                    "pos_x": node.get("pos_x", 0),
                    "pos_y": node.get("pos_y", 0),
                    "node_params": node.get("params", {})
                }
                res = unreal.send_command("add_material_node", params)
                if res and res.get("success") and res.get("node_id"):
                    node_id_map[node.get("id")] = res.get("node_id")
                results["nodes"].append(res)
            except Exception as e:
                results["errors"].append(f"Node error {node.get('id')}: {str(e)}")

        for conn in data.get("connections", []):
            try:
                source_unreal_id = node_id_map.get(conn.get("source_id"), conn.get("source_id"))
                target_unreal_id = node_id_map.get(conn.get("target_id"), conn.get("target_id"))

                params = {
                    "material_name": material_name,
                    "source_node_id": source_unreal_id,
                    "source_pin_name": conn.get("source_pin", ""),
                    "target_node_id": target_unreal_id,
                    "target_pin_name": conn.get("target_pin", "")
                }
                res = unreal.send_command("connect_material_nodes", params)
                results["connections"].append(res)
            except Exception as e:
                results["errors"].append(f"Connection error: {str(e)}")

        return {
            "success": len(results["errors"]) == 0,
            "results": results,
            "node_mapping": node_id_map
        }

    except Exception as e:
        logger.error(f"apply_material_json error: {e}")
        return make_error_response(str(e))

@mcp.tool()
def export_material_json(material_path: str) -> Dict[str, Any]:
    """Export a Material graph to a standardized JSON representation.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "material_path": material_path,
        }
        response = unreal.send_command("analyze_material_graph", params)
        if response and response.get("success"):
            return {
                "success": True,
                "json_data": json.dumps(response.get("graph_data", {}), indent=2),
                "raw_data": response.get("graph_data", {})
            }
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"export_material_json error: {e}")
        return make_error_response(str(e))
