"""
Nom du fichier: function_manager.py
Description: MCP tool wrappers for Blueprint function lifecycle management (create, delete, rename)
"""

from typing import Dict, Any
import json


def create_function_handler(unreal_connection, blueprint_name: str, function_name: str, return_type: str = "void") -> Dict[str, Any]:
    """
    Handler for create_function command

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        function_name: Name for the new function
        return_type: Return type of the function (default: "void")

    Returns:
        Dictionary with function_name, graph_id or error
    """
    try:
        response = unreal_connection.send_command("create_function", {
            "blueprint_name": blueprint_name,
            "function_name": function_name,
            "return_type": return_type
        })
        return response
    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }


def delete_function_handler(unreal_connection, blueprint_name: str, function_name: str) -> Dict[str, Any]:
    """
    Handler for delete_function command

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        function_name: Name of the function to delete

    Returns:
        Dictionary with deleted_function_name or error
    """
    try:
        response = unreal_connection.send_command("delete_function", {
            "blueprint_name": blueprint_name,
            "function_name": function_name
        })
        return response
    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }


def rename_function_handler(unreal_connection, blueprint_name: str, old_function_name: str, new_function_name: str) -> Dict[str, Any]:
    """
    Handler for rename_function command

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        old_function_name: Current name of the function
        new_function_name: New name for the function

    Returns:
        Dictionary with new_function_name or error
    """
    try:
        response = unreal_connection.send_command("rename_function", {
            "blueprint_name": blueprint_name,
            "old_function_name": old_function_name,
            "new_function_name": new_function_name
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
