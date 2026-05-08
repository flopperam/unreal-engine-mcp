"""Blueprint tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from utils.responses import make_error_response, is_success_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def create_blueprint(name: str, parent_class: str) -> Dict[str, Any]:
    """Create a new Blueprint class."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "name": name,
            "parent_class": parent_class
        }
        response = unreal.send_command("create_blueprint", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_blueprint error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_component_to_blueprint(
    blueprint_name: str,
    component_type: str,
    component_name: str,
    location: Optional[List[float]] = None,
    rotation: Optional[List[float]] = None,
    scale: Optional[List[float]] = None,
    component_properties: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """Add a component to a Blueprint."""
    if location is None:
        location = []
    if rotation is None:
        rotation = []
    if scale is None:
        scale = []
    if component_properties is None:
        component_properties = {}
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_type": component_type,
            "component_name": component_name,
            "location": location,
            "rotation": rotation,
            "scale": scale,
            "component_properties": component_properties
        }
        response = unreal.send_command("add_component_to_blueprint", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_component_to_blueprint error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_static_mesh_properties(
    blueprint_name: str,
    component_name: str,
    static_mesh: str = "/Engine/BasicShapes/Cube.Cube"
) -> Dict[str, Any]:
    """Set static mesh properties on a StaticMeshComponent."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "static_mesh": static_mesh
        }
        response = unreal.send_command("set_static_mesh_properties", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_static_mesh_properties error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_physics_properties(
    blueprint_name: str,
    component_name: str,
    simulate_physics: bool = True,
    gravity_enabled: bool = True,
    mass: float = 1,
    linear_damping: float = 0.01,
    angular_damping: float = 0
) -> Dict[str, Any]:
    """Set physics properties on a component."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "simulate_physics": simulate_physics,
            "gravity_enabled": gravity_enabled,
            "mass": mass,
            "linear_damping": linear_damping,
            "angular_damping": angular_damping
        }
        response = unreal.send_command("set_physics_properties", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_physics_properties error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def compile_blueprint(blueprint_name: str) -> Dict[str, Any]:
    """Compile a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"blueprint_name": blueprint_name}
        response = unreal.send_command("compile_blueprint", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"compile_blueprint error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def read_blueprint_content(
    blueprint_path: str,
    include_event_graph: bool = True,
    include_functions: bool = True,
    include_variables: bool = True,
    include_components: bool = True,
    include_interfaces: bool = True
) -> Dict[str, Any]:
    """
    Read and analyze the complete content of a Blueprint including event graph,
    functions, variables, components, and implemented interfaces.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_path": blueprint_path,
            "include_event_graph": include_event_graph,
            "include_functions": include_functions,
            "include_variables": include_variables,
            "include_components": include_components,
            "include_interfaces": include_interfaces
        }

        logger.info(f"Reading Blueprint content for: {blueprint_path}")
        response = unreal.send_command("read_blueprint_content", params)

        if response and is_success_response(response):
            logger.info(f"Successfully read Blueprint content. Found:")
            if response.get("variables"):
                logger.info(f"  - {len(response['variables'])} variables")
            if response.get("functions"):
                logger.info(f"  - {len(response['functions'])} functions")
            if response.get("event_graph", {}).get("nodes"):
                logger.info(f"  - {len(response['event_graph']['nodes'])} event graph nodes")
            if response.get("components"):
                logger.info(f"  - {len(response['components'])} components")

        return response or make_error_response("No response from Unreal")

    except Exception as e:
        logger.error(f"read_blueprint_content error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def analyze_blueprint_graph(
    blueprint_path: str,
    graph_name: str = "EventGraph",
    include_node_details: bool = True,
    include_pin_connections: bool = True,
    trace_execution_flow: bool = True
) -> Dict[str, Any]:
    """
    Analyze a specific graph within a Blueprint (EventGraph, functions, etc.)
    and provide detailed information about nodes, connections, and execution flow.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_path": blueprint_path,
            "graph_name": graph_name,
            "include_node_details": include_node_details,
            "include_pin_connections": include_pin_connections,
            "trace_execution_flow": trace_execution_flow
        }

        logger.info(f"Analyzing Blueprint graph: {blueprint_path} -> {graph_name}")
        response = unreal.send_command("analyze_blueprint_graph", params)

        if response and is_success_response(response):
            graph_data = response.get("graph_data", {})
            logger.info(f"Graph analysis complete:")
            logger.info(f"  - Graph: {graph_data.get('graph_name', 'Unknown')}")
            logger.info(f"  - Nodes: {len(graph_data.get('nodes', []))}")
            logger.info(f"  - Connections: {len(graph_data.get('connections', []))}")
            if graph_data.get('execution_paths'):
                logger.info(f"  - Execution paths: {len(graph_data['execution_paths'])}")

        return response or make_error_response("No response from Unreal")

    except Exception as e:
        logger.error(f"analyze_blueprint_graph error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def get_blueprint_variable_details(
    blueprint_path: str,
    variable_name: str = None
) -> Dict[str, Any]:
    """
    Get detailed information about Blueprint variables including type,
    default values, metadata, and usage within the Blueprint.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_path": blueprint_path,
            "variable_name": variable_name
        }

        logger.info(f"Getting Blueprint variable details: {blueprint_path}")
        if variable_name:
            logger.info(f"  - Specific variable: {variable_name}")

        response = unreal.send_command("get_blueprint_variable_details", params)
        return response or make_error_response("No response from Unreal")

    except Exception as e:
        logger.error(f"get_blueprint_variable_details error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def get_blueprint_function_details(
    blueprint_path: str,
    function_name: str = None,
    include_graph: bool = True
) -> Dict[str, Any]:
    """
    Get detailed information about Blueprint functions including parameters,
    return values, local variables, and function graph content.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "blueprint_path": blueprint_path,
            "function_name": function_name,
            "include_graph": include_graph
        }

        logger.info(f"Getting Blueprint function details: {blueprint_path}")
        if function_name:
            logger.info(f"  - Specific function: {function_name}")

        response = unreal.send_command("get_blueprint_function_details", params)
        return response or make_error_response("No response from Unreal")

    except Exception as e:
        logger.error(f"get_blueprint_function_details error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Phase 6: Missing Blueprint Features
# ---------------------------------------------------------------------------

@mcp.tool()
def set_blueprint_parent_class(blueprint_path: str, parent_class: str) -> Dict[str, Any]:
    """Change the parent class of a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path, "parent_class": parent_class}
        response = unreal.send_command("set_blueprint_parent_class", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_blueprint_parent_class error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_blueprint_class_settings(
    blueprint_path: str,
    generate_overlap_events: bool = None,
    run_construction_script: bool = None
) -> Dict[str, Any]:
    """Update Blueprint class settings."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path}
        if generate_overlap_events is not None:
            params["generate_overlap_events"] = generate_overlap_events
        if run_construction_script is not None:
            params["run_construction_script"] = run_construction_script
        response = unreal.send_command("set_blueprint_class_settings", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_blueprint_class_settings error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_blueprint_class_defaults(blueprint_path: str, defaults: List[Dict[str, str]]) -> Dict[str, Any]:
    """Update Blueprint class default variable values."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path, "defaults": defaults}
        response = unreal.send_command("set_blueprint_class_defaults", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_blueprint_class_defaults error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_component_defaults(blueprint_path: str, component_name: str, properties: Dict[str, Any]) -> Dict[str, Any]:
    """Update component default property values in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "component_name": component_name,
            "properties": properties
        }
        response = unreal.send_command("set_component_defaults", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_component_defaults error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def edit_construction_script(blueprint_path: str, add_node: str = None) -> Dict[str, Any]:
    """Access and edit the Blueprint Construction Script."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path}
        if add_node:
            params["add_node"] = add_node
        response = unreal.send_command("edit_construction_script", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"edit_construction_script error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_event_dispatcher(blueprint_path: str, dispatcher_name: str) -> Dict[str, Any]:
    """Create an Event Dispatcher in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path, "dispatcher_name": dispatcher_name}
        response = unreal.send_command("create_event_dispatcher", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_event_dispatcher error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def bind_event_dispatcher(
    blueprint_path: str,
    dispatcher_name: str,
    target_function: str
) -> Dict[str, Any]:
    """Configure Event Dispatcher binding to a target function."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "dispatcher_name": dispatcher_name,
            "target_function": target_function
        }
        response = unreal.send_command("bind_event_dispatcher", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"bind_event_dispatcher error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_enum(enum_path: str, values: List[str]) -> Dict[str, Any]:
    """Create a new Blueprint Enum asset."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"enum_path": enum_path, "values": values}
        response = unreal.send_command("create_enum", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_enum error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_struct(struct_path: str) -> Dict[str, Any]:
    """Create a new Blueprint Struct asset."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"struct_path": struct_path}
        response = unreal.send_command("create_struct", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_struct error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def edit_enum(enum_path: str, add_values: List[str] = None) -> Dict[str, Any]:
    """Edit an existing Enum by adding new values."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"enum_path": enum_path}
        if add_values:
            params["add_values"] = add_values
        response = unreal.send_command("edit_enum", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"edit_enum error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def edit_struct(struct_path: str) -> Dict[str, Any]:
    """Get information about a Blueprint Struct."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"struct_path": struct_path}
        response = unreal.send_command("edit_struct", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"edit_struct error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_blueprint_interface(interface_path: str) -> Dict[str, Any]:
    """Create a new Blueprint Interface asset."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"interface_path": interface_path}
        response = unreal.send_command("create_blueprint_interface", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_blueprint_interface error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def implement_interface(blueprint_path: str, interface_path: str) -> Dict[str, Any]:
    """Implement a Blueprint Interface in a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "interface_path": interface_path
        }
        response = unreal.send_command("implement_interface", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"implement_interface error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_function_library(library_path: str) -> Dict[str, Any]:
    """Create a new Blueprint Function Library."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"library_path": library_path}
        response = unreal.send_command("create_function_library", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_function_library error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_macro_library(library_path: str) -> Dict[str, Any]:
    """Create a new Blueprint Macro Library."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"library_path": library_path}
        response = unreal.send_command("create_macro_library", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_macro_library error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_comment_node(blueprint_path: str, graph_name: str = "EventGraph") -> Dict[str, Any]:
    """Add a comment node to a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path, "graph_name": graph_name}
        response = unreal.send_command("add_comment_node", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_comment_node error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_reroute_node(
    blueprint_path: str,
    source_node_id: int,
    graph_name: str = "EventGraph"
) -> Dict[str, Any]:
    """Add a reroute node to a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "source_node_id": source_node_id,
            "graph_name": graph_name
        }
        response = unreal.send_command("add_reroute_node", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_reroute_node error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def format_graph(blueprint_path: str, graph_name: str = "EventGraph") -> Dict[str, Any]:
    """Format/arrange nodes in a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path, "graph_name": graph_name}
        response = unreal.send_command("format_graph", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"format_graph error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_collapsed_graph(
    blueprint_path: str,
    graph_name: str = "EventGraph",
    collapsed_graph_name: str = "CollapsedGraph",
    pos_x: int = 0,
    pos_y: int = 0
) -> Dict[str, Any]:
    """Create a collapsed graph node in a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "graph_name": graph_name,
            "collapsed_graph_name": collapsed_graph_name,
            "pos_x": pos_x,
            "pos_y": pos_y,
        }
        response = unreal.send_command("create_collapsed_graph", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_collapsed_graph error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_macro_graph(blueprint_path: str, macro_name: str) -> Dict[str, Any]:
    """Create a macro graph inside a Blueprint or Macro Library."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path, "macro_name": macro_name}
        response = unreal.send_command("create_macro_graph", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_macro_graph error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_macro_instance(
    blueprint_path: str,
    macro_graph_name: str,
    graph_name: str = "EventGraph",
    macro_blueprint_path: str = None,
    pos_x: int = 0,
    pos_y: int = 0
) -> Dict[str, Any]:
    """Place a macro instance node in a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "graph_name": graph_name,
            "macro_graph_name": macro_graph_name,
            "pos_x": pos_x,
            "pos_y": pos_y,
        }
        if macro_blueprint_path:
            params["macro_blueprint_path"] = macro_blueprint_path
        response = unreal.send_command("create_macro_instance", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_macro_instance error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_timeline(
    blueprint_path: str,
    timeline_name: str,
    graph_name: str = "EventGraph",
    pos_x: int = 0,
    pos_y: int = 0,
    length: float = None,
    autoplay: bool = None,
    loop: bool = None,
    replicated: bool = None,
    ignore_time_dilation: bool = None
) -> Dict[str, Any]:
    """Create a Timeline template and a Timeline node in a Blueprint graph."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "timeline_name": timeline_name,
            "graph_name": graph_name,
            "pos_x": pos_x,
            "pos_y": pos_y,
        }
        if length is not None:
            params["length"] = length
        if autoplay is not None:
            params["autoplay"] = autoplay
        if loop is not None:
            params["loop"] = loop
        if replicated is not None:
            params["replicated"] = replicated
        if ignore_time_dilation is not None:
            params["ignore_time_dilation"] = ignore_time_dilation
        response = unreal.send_command("create_timeline", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_timeline error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def edit_timeline(
    blueprint_path: str,
    timeline_name: str,
    track_action: str = "list",
    track_type: str = "float",
    track_name: str = None,
    curve_path: str = None,
    keys: List[Dict[str, Any]] = None,
    length: float = None,
    autoplay: bool = None,
    loop: bool = None,
    replicated: bool = None,
    ignore_time_dilation: bool = None
) -> Dict[str, Any]:
    """Edit Timeline settings and add/remove Timeline tracks."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "timeline_name": timeline_name,
            "track_action": track_action,
            "track_type": track_type,
        }
        if track_name:
            params["track_name"] = track_name
        if curve_path:
            params["curve_path"] = curve_path
        if keys:
            params["keys"] = keys
        if length is not None:
            params["length"] = length
        if autoplay is not None:
            params["autoplay"] = autoplay
        if loop is not None:
            params["loop"] = loop
        if replicated is not None:
            params["replicated"] = replicated
        if ignore_time_dilation is not None:
            params["ignore_time_dilation"] = ignore_time_dilation
        response = unreal.send_command("edit_timeline", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"edit_timeline error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_blueprint_breakpoint(
    blueprint_path: str,
    node_id: Any,
    graph_name: str = "EventGraph",
    enable: bool = True,
    action: str = "set",
    remove: bool = False
) -> Dict[str, Any]:
    """Set or clear a breakpoint on a Blueprint node."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    if node_id is None and action != "clear_all":
        return make_error_response("node_id is required unless action='clear_all'")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "graph_name": graph_name,
            "enable": enable,
            "action": action,
            "remove": remove,
        }
        if node_id is not None:
            params["node_id"] = node_id
        response = unreal.send_command("set_blueprint_breakpoint", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_blueprint_breakpoint error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_blueprint_watch(
    blueprint_path: str,
    node_id: Any,
    pin_name: str,
    graph_name: str = "EventGraph",
    pin_id: Any = None,
    action: str = "set",
    remove: bool = False
) -> Dict[str, Any]:
    """Set or remove a watch on a Blueprint node pin."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    if node_id is None:
        return make_error_response("node_id is required")
    if not pin_name and pin_id is None:
        return make_error_response("pin_name or pin_id is required")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "graph_name": graph_name,
            "node_id": node_id,
            "action": action,
            "remove": remove,
        }
        if pin_name:
            params["pin_name"] = pin_name
        if pin_id is not None:
            params["pin_id"] = pin_id
        response = unreal.send_command("set_blueprint_watch", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_blueprint_watch error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def clear_blueprint_watches(blueprint_path: str) -> Dict[str, Any]:
    """Clear all pin watches for a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("clear_blueprint_watches", {"blueprint_path": blueprint_path})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"clear_blueprint_watches error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def step_blueprint_debugger(action: str) -> Dict[str, Any]:
    """Request a Blueprint debugger step action: step_in, step_over, or step_out."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command("step_blueprint_debugger", {"action": action})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"step_blueprint_debugger error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def get_blueprint_debug_info(blueprint_path: str) -> Dict[str, Any]:
    """Get debug information about a Blueprint."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {"blueprint_path": blueprint_path}
        response = unreal.send_command("get_blueprint_debug_info", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"get_blueprint_debug_info error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def blueprint_diff(blueprint_path: str, other_blueprint_path: str) -> Dict[str, Any]:
    """Compare two Blueprints and return differences."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        params = {
            "blueprint_path": blueprint_path,
            "other_blueprint_path": other_blueprint_path
        }
        response = unreal.send_command("blueprint_diff", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"blueprint_diff error: {e}")
        return make_error_response(str(e))
