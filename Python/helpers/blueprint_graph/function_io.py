"""
Nom du fichier: function_io.py
Description: MCP tool wrappers for Blueprint function parameter management (input/output)
"""

from typing import Dict, Any


def add_function_input_handler(
    unreal_connection,
    blueprint_name: str,
    function_name: str,
    param_name: str,
    param_type: str,
    is_array: bool = False
) -> Dict[str, Any]:
    """
    Handler for add_function_input command

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        function_name: Name of the function
        param_name: Name of the input parameter
        param_type: Type of the parameter (bool, int, float, string, vector, etc.)
        is_array: Whether the parameter is an array (default: False)

    Returns:
        Dictionary with param_name, param_type, and direction or error
    """
    try:
        response = unreal_connection.send_command("add_function_input", {
            "blueprint_name": blueprint_name,
            "function_name": function_name,
            "param_name": param_name,
            "param_type": param_type,
            "is_array": is_array
        })
        return response
    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }


def add_function_output_handler(
    unreal_connection,
    blueprint_name: str,
    function_name: str,
    param_name: str,
    param_type: str,
    is_array: bool = False
) -> Dict[str, Any]:
    """
    Handler for add_function_output command

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        function_name: Name of the function
        param_name: Name of the output parameter
        param_type: Type of the parameter (bool, int, float, string, vector, etc.)
        is_array: Whether the parameter is an array (default: False)

    Returns:
        Dictionary with param_name, param_type, and direction or error
    """
    try:
        response = unreal_connection.send_command("add_function_output", {
            "blueprint_name": blueprint_name,
            "function_name": function_name,
            "param_name": param_name,
            "param_type": param_type,
            "is_array": is_array
        })
        return response
    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }


def send_command(command_dict: Dict[str, Any]) -> Dict[str, Any]:
    """
    Send command to Unreal Engine MCP Bridge
    This function will be called by the MCP tool decorators

    Args:
        command_dict: Dictionary containing command and parameters

    Returns:
        Response from Unreal Engine
    """
    # This is a placeholder - actual implementation handles TCP communication
    # to the Unreal Engine plugin
    return {
        "success": True,
        "message": "Command sent to Unreal Engine"
    }
