"""Blueprint graph tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from utils.responses import make_error_response
from helpers.blueprint_graph import node_manager, variable_manager, connector_manager
from helpers.blueprint_graph import event_manager, node_deleter, node_properties
from helpers.blueprint_graph import function_manager, function_io

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def add_node(
    blueprint_name: str,
    node_type: str,
    pos_x: float = 0,
    pos_y: float = 0,
    message: str = "",
    event_type: str = "BeginPlay",
    variable_name: str = "",
    target_function: str = "",
    target_blueprint: Optional[str] = None,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """Add a node to a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        node_params = {
            "pos_x": pos_x,
            "pos_y": pos_y
        }

        if message:
            node_params["message"] = message
        if event_type:
            node_params["event_type"] = event_type
        if variable_name:
            node_params["variable_name"] = variable_name
        if target_function:
            node_params["target_function"] = target_function
        if target_blueprint:
            node_params["target_blueprint"] = target_blueprint
        if function_name:
            node_params["function_name"] = function_name

        result = node_manager.add_node(
            unreal,
            blueprint_name,
            node_type,
            node_params
        )

        return result

    except Exception as e:
        logger.error(f"add_node error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def connect_nodes(
    blueprint_name: str,
    source_node_id: str,
    source_pin_name: str,
    target_node_id: str,
    target_pin_name: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """Connect two nodes in a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = connector_manager.connect_nodes(
            unreal,
            blueprint_name,
            source_node_id,
            source_pin_name,
            target_node_id,
            target_pin_name,
            function_name
        )

        return result
    except Exception as e:
        logger.error(f"connect_nodes error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_variable(
    blueprint_name: str,
    variable_name: str,
    variable_type: str,
    default_value: Any = None,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """Create a variable in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = variable_manager.create_variable(
            unreal,
            blueprint_name,
            variable_name,
            variable_type,
            default_value,
            is_public,
            tooltip,
            category
        )

        return result
    except Exception as e:
        logger.error(f"create_variable error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_blueprint_variable_properties(
    blueprint_name: str,
    variable_name: str,
    var_name: Optional[str] = None,
    var_type: Optional[str] = None,
    is_blueprint_readable: Optional[bool] = None,
    is_blueprint_writable: Optional[bool] = None,
    is_public: Optional[bool] = None,
    is_editable_in_instance: Optional[bool] = None,
    tooltip: Optional[str] = None,
    category: Optional[str] = None,
    default_value: Any = None,
    expose_on_spawn: Optional[bool] = None,
    expose_to_cinematics: Optional[bool] = None,
    slider_range_min: Optional[str] = None,
    slider_range_max: Optional[str] = None,
    value_range_min: Optional[str] = None,
    value_range_max: Optional[str] = None,
    units: Optional[str] = None,
    bitmask: Optional[bool] = None,
    bitmask_enum: Optional[str] = None,
    replication_enabled: Optional[bool] = None,
    replication_condition: Optional[int] = None,
    is_private: Optional[bool] = None
) -> Dict[str, Any]:
    """Modify properties of an existing Blueprint variable without deleting it."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = variable_manager.set_blueprint_variable_properties(
            unreal,
            blueprint_name,
            variable_name,
            var_name,
            var_type,
            is_blueprint_readable,
            is_blueprint_writable,
            is_public,
            is_editable_in_instance,
            tooltip,
            category,
            default_value,
            expose_on_spawn,
            expose_to_cinematics,
            slider_range_min,
            slider_range_max,
            value_range_min,
            value_range_max,
            units,
            bitmask,
            bitmask_enum,
            replication_enabled,
            replication_condition,
            is_private
        )

        return result
    except Exception as e:
        logger.error(f"set_blueprint_variable_properties error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_event_node(
    blueprint_name: str,
    event_name: str,
    pos_x: float = 0,
    pos_y: float = 0
) -> Dict[str, Any]:
    """Add an event node to a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = event_manager.add_event_node(
            unreal,
            blueprint_name,
            event_name,
            pos_x,
            pos_y
        )

        return result
    except Exception as e:
        logger.error(f"add_event_node error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def delete_node(
    blueprint_name: str,
    node_id: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """Delete a node from a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = node_deleter.delete_node(
            unreal,
            blueprint_name,
            node_id,
            function_name
        )
        return result
    except Exception as e:
        logger.error(f"delete_node error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_node_property(
    blueprint_name: str,
    node_id: str,
    property_name: str = "",
    property_value: Any = None,
    function_name: Optional[str] = None,
    action: Optional[str] = None,
    pin_type: Optional[str] = None,
    pin_name: Optional[str] = None,
    enum_type: Optional[str] = None,
    new_type: Optional[str] = None,
    target_type: Optional[str] = None,
    target_function: Optional[str] = None,
    target_class: Optional[str] = None,
    event_type: Optional[str] = None
) -> Dict[str, Any]:
    """Set a property on a Blueprint node or perform semantic node editing."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        # Build kwargs for semantic actions
        kwargs = {}
        if action is not None:
            if pin_type is not None:
                kwargs["pin_type"] = pin_type
            if pin_name is not None:
                kwargs["pin_name"] = pin_name
            if enum_type is not None:
                kwargs["enum_type"] = enum_type
            if new_type is not None:
                kwargs["new_type"] = new_type
            if target_type is not None:
                kwargs["target_type"] = target_type
            if target_function is not None:
                kwargs["target_function"] = target_function
            if target_class is not None:
                kwargs["target_class"] = target_class
            if event_type is not None:
                kwargs["event_type"] = event_type

        result = node_properties.set_node_property(
            unreal,
            blueprint_name,
            node_id,
            property_name,
            property_value,
            function_name,
            action,
            **kwargs
        )
        return result
    except Exception as e:
        logger.error(f"set_node_property error: {e}", exc_info=True)
        return make_error_response(str(e))


@mcp.tool()
def create_function(
    blueprint_name: str,
    function_name: str,
    return_type: str = "void"
) -> Dict[str, Any]:
    """Create a new function in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = function_manager.create_function_handler(
            unreal,
            blueprint_name,
            function_name,
            return_type
        )
        return result
    except Exception as e:
        logger.error(f"create_function error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_function_input(
    blueprint_name: str,
    function_name: str,
    param_name: str,
    param_type: str,
    is_array: bool = False
) -> Dict[str, Any]:
    """Add an input parameter to a Blueprint function."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = function_io.add_function_input_handler(
            unreal,
            blueprint_name,
            function_name,
            param_name,
            param_type,
            is_array
        )
        return result
    except Exception as e:
        logger.error(f"add_function_input error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_function_output(
    blueprint_name: str,
    function_name: str,
    param_name: str,
    param_type: str,
    is_array: bool = False
) -> Dict[str, Any]:
    """Add an output parameter to a Blueprint function."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = function_io.add_function_output_handler(
            unreal,
            blueprint_name,
            function_name,
            param_name,
            param_type,
            is_array
        )
        return result
    except Exception as e:
        logger.error(f"add_function_output error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def delete_function(
    blueprint_name: str,
    function_name: str
) -> Dict[str, Any]:
    """Delete a function from a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = function_manager.delete_function_handler(
            unreal,
            blueprint_name,
            function_name
        )
        return result
    except Exception as e:
        logger.error(f"delete_function error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def rename_function(
    blueprint_name: str,
    old_function_name: str,
    new_function_name: str
) -> Dict[str, Any]:
    """Rename a function in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        result = function_manager.rename_function_handler(
            unreal,
            blueprint_name,
            old_function_name,
            new_function_name
        )
        return result
    except Exception as e:
        logger.error(f"rename_function error: {e}")
        return make_error_response(str(e))

import json

@mcp.tool()
def apply_blueprint_json(blueprint_name: str, json_data: str) -> Dict[str, Any]:
    """Apply a JSON string to create Blueprint variables, nodes, and connections.

    The JSON structure should be:
    {
      "variables": [
        {"name": "VarName", "type": "Boolean", "default": false}
      ],
      "nodes": [
        {"id": "node1", "type": "Event", "params": {"event_type": "BeginPlay", "pos_x": 0, "pos_y": 0}},
        {"id": "node2", "type": "Print", "params": {"message": "Hello", "pos_x": 200, "pos_y": 0}}
      ],
      "connections": [
        {"source_id": "node1", "source_pin": "then", "target_id": "node2", "target_pin": "execute"}
      ]
    }
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        data = json.loads(json_data)
        results = {"variables": [], "nodes": [], "connections": [], "errors": []}
        node_id_map = {} # Maps JSON string IDs to Unreal GUIDs

        # 1. Create variables
        for var in data.get("variables", []):
            try:
                res = variable_manager.create_variable(
                    unreal,
                    blueprint_name,
                    var.get("name"),
                    var.get("type", "Boolean"),
                    var.get("default"),
                    var.get("is_public", False)
                )
                results["variables"].append(res)
            except Exception as e:
                results["errors"].append(f"Variable error {var.get('name')}: {str(e)}")

        # 2. Create nodes
        for node in data.get("nodes", []):
            try:
                # Use node_manager.add_node
                res = node_manager.add_node(
                    unreal,
                    blueprint_name,
                    node.get("type"),
                    node.get("params", {})
                )
                if res and res.get("success") and res.get("node_id"):
                    node_id_map[node.get("id")] = res.get("node_id")
                results["nodes"].append(res)
            except Exception as e:
                results["errors"].append(f"Node error {node.get('id')}: {str(e)}")

        # 3. Create connections
        for conn in data.get("connections", []):
            try:
                source_unreal_id = node_id_map.get(conn.get("source_id"), conn.get("source_id"))
                target_unreal_id = node_id_map.get(conn.get("target_id"), conn.get("target_id"))

                res = connector_manager.connect_nodes(
                    unreal,
                    blueprint_name,
                    source_unreal_id,
                    conn.get("source_pin"),
                    target_unreal_id,
                    conn.get("target_pin"),
                    conn.get("function_name")
                )
                results["connections"].append(res)
            except Exception as e:
                results["errors"].append(f"Connection error: {str(e)}")

        return {
            "success": len(results["errors"]) == 0,
            "results": results,
            "node_mapping": node_id_map
        }

    except Exception as e:
        logger.error(f"apply_blueprint_json error: {e}")
        return make_error_response(str(e))

@mcp.tool()
def export_blueprint_json(blueprint_path: str, graph_name: str = "EventGraph") -> Dict[str, Any]:
    """Export a Blueprint graph to a standardized JSON representation.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        # We reuse the analyze_blueprint_graph command which already returns graph data
        params = {
            "blueprint_path": blueprint_path,
            "graph_name": graph_name,
            "include_node_details": True,
            "include_pin_connections": True
        }
        response = unreal.send_command("analyze_blueprint_graph", params)
        if response and response.get("success"):
            # Format as JSON string for easy copying
            return {
                "success": True,
                "json_data": json.dumps(response.get("graph_data", {}), indent=2),
                "raw_data": response.get("graph_data", {})
            }
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"export_blueprint_json error: {e}")
        return make_error_response(str(e))
