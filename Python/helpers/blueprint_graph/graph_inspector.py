"""
Description: Blueprint graph inspection and analysis tools
"""

from typing import Dict, Any, Optional


def read_blueprint_content_helper(
    unreal_connection,
    blueprint_path: str,
    include_event_graph: bool = True,
    include_functions: bool = True,
    include_variables: bool = True
) -> Dict[str, Any]:
    """
    Read comprehensive Blueprint content including graphs, functions, and variables.

    Args:
        unreal_connection: Active Unreal Engine connection
        blueprint_path: Path to the Blueprint asset
        include_event_graph: Include EventGraph details
        include_functions: Include Blueprint functions
        include_variables: Include Blueprint variables

    Returns:
        Dictionary with Blueprint content details
    """
    params = {
        "blueprint_path": blueprint_path,
        "include_event_graph": include_event_graph,
        "include_functions": include_functions,
        "include_variables": include_variables
    }

    return unreal_connection.send_command("read_blueprint_content", params)


def analyze_blueprint_graph_helper(
    unreal_connection,
    blueprint_path: str,
    graph_name: str = "EventGraph",
    include_node_details: bool = True,
    include_pin_connections: bool = True
) -> Dict[str, Any]:
    """
    Analyze Blueprint graph structure with detailed node and connection information.

    Args:
        unreal_connection: Active Unreal Engine connection
        blueprint_path: Path to the Blueprint asset
        graph_name: Name of the graph to analyze (default: "EventGraph")
        include_node_details: Include detailed node information
        include_pin_connections: Include pin connection details

    Returns:
        Dictionary with graph analysis results
    """
    params = {
        "blueprint_path": blueprint_path,
        "graph_name": graph_name,
        "include_node_details": include_node_details,
        "include_pin_connections": include_pin_connections
    }

    return unreal_connection.send_command("analyze_blueprint_graph", params)


def get_blueprint_variable_details_helper(
    unreal_connection,
    blueprint_path: str,
    variable_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Get detailed information about Blueprint variables.

    Args:
        unreal_connection: Active Unreal Engine connection
        blueprint_path: Path to the Blueprint asset
        variable_name: Specific variable name (None for all variables)

    Returns:
        Dictionary with variable details
    """
    params = {
        "blueprint_path": blueprint_path
    }

    if variable_name is not None:
        params["variable_name"] = variable_name

    return unreal_connection.send_command("get_blueprint_variable_details", params)


def get_blueprint_function_details_helper(
    unreal_connection,
    blueprint_path: str,
    function_name: Optional[str] = None,
    include_graph: bool = True
) -> Dict[str, Any]:
    """
    Get detailed information about Blueprint functions.

    Args:
        unreal_connection: Active Unreal Engine connection
        blueprint_path: Path to the Blueprint asset
        function_name: Specific function name (None for all functions)
        include_graph: Include function graph details

    Returns:
        Dictionary with function details
    """
    params = {
        "blueprint_path": blueprint_path,
        "include_graph": include_graph
    }

    if function_name is not None:
        params["function_name"] = function_name

    return unreal_connection.send_command("get_blueprint_function_details", params)
