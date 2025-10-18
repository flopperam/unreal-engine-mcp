"""
Filename: variable_manager.py
Creation Date: 2025-10-11
Author: Zoscran
Description: Python wrapper for Blueprint variable management
"""

import json
import logging
from typing import Dict, Any, Optional

logger = logging.getLogger("BlueprintGraph.VariableManager")


def create_variable(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    variable_type: str,
    default_value: Any = None,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Create a variable in a Blueprint.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        variable_name: Name of the variable to create
        variable_type: Type of the variable ("bool", "int", "float", "string", "vector", "rotator")
        default_value: Default value for the variable (optional)
        is_public: Whether the variable should be public/editable (default: False)
        tooltip: Tooltip text for the variable (optional)
        category: Category for organizing variables (default: "Default")
    
    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - variable (dict): Variable details if successful
            - error (str): Error message if failed
    
    Example:
        >>> result = create_variable(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "Health",
        ...     "float",
        ...     100.0,
        ...     True,
        ...     "Player health points",
        ...     "Stats"
        ... )
        >>> print(result["variable"]["name"])
        'Health'
    """
    try:
        params = {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
            "variable_type": variable_type
        }
        
        if default_value is not None:
            params["default_value"] = default_value
        if is_public:
            params["is_public"] = is_public
        if tooltip:
            params["tooltip"] = tooltip
        if category != "Default":
            params["category"] = category
        
        response = unreal_connection.send_command("create_variable", params)
        
        if response.get("success"):
            logger.info(
                f"Successfully created variable '{variable_name}' ({variable_type}) in {blueprint_name}"
            )
        else:
            logger.error(
                f"Failed to create variable: {response.get('error', 'Unknown error')}"
            )
        
        return response
        
    except Exception as e:
        logger.error(f"Exception in create_variable: {e}")
        return {
            "success": False,
            "error": str(e)
        }


def create_float_variable(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    default_value: float = 0.0,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Convenience function to create a float variable.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable
        default_value: Default float value
        is_public: Whether the variable is public
        tooltip: Tooltip text
        category: Category name
    
    Returns:
        Dictionary containing variable details and status
        
    Example:
        >>> create_float_variable(unreal, "MyActor", "Speed", 10.5, True, "Movement speed")
    """
    return create_variable(
        unreal_connection,
        blueprint_name,
        variable_name,
        "float",
        default_value,
        is_public,
        tooltip,
        category
    )


def create_int_variable(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    default_value: int = 0,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Convenience function to create an integer variable.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable
        default_value: Default integer value
        is_public: Whether the variable is public
        tooltip: Tooltip text
        category: Category name
    
    Returns:
        Dictionary containing variable details and status
        
    Example:
        >>> create_int_variable(unreal, "MyActor", "Score", 0, True, "Player score")
    """
    return create_variable(
        unreal_connection,
        blueprint_name,
        variable_name,
        "int",
        default_value,
        is_public,
        tooltip,
        category
    )


def create_bool_variable(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    default_value: bool = False,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Convenience function to create a boolean variable.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable
        default_value: Default boolean value
        is_public: Whether the variable is public
        tooltip: Tooltip text
        category: Category name
    
    Returns:
        Dictionary containing variable details and status
        
    Example:
        >>> create_bool_variable(unreal, "MyActor", "IsAlive", True, True, "Alive status")
    """
    return create_variable(
        unreal_connection,
        blueprint_name,
        variable_name,
        "bool",
        default_value,
        is_public,
        tooltip,
        category
    )


def create_string_variable(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    default_value: str = "",
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Convenience function to create a string variable.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable
        default_value: Default string value
        is_public: Whether the variable is public
        tooltip: Tooltip text
        category: Category name
    
    Returns:
        Dictionary containing variable details and status
        
    Example:
        >>> create_string_variable(unreal, "MyActor", "Name", "Player", True, "Player name")
    """
    return create_variable(
        unreal_connection,
        blueprint_name,
        variable_name,
        "string",
        default_value,
        is_public,
        tooltip,
        category
    )


def create_vector_variable(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    default_value: list = None,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Convenience function to create a vector variable.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable
        default_value: Default vector value as [x, y, z] (optional)
        is_public: Whether the variable is public
        tooltip: Tooltip text
        category: Category name
    
    Returns:
        Dictionary containing variable details and status
        
    Example:
        >>> create_vector_variable(unreal, "MyActor", "Position", [0, 0, 100], True, "Object position")
    """
    if default_value is None:
        default_value = [0.0, 0.0, 0.0]
    
    return create_variable(
        unreal_connection,
        blueprint_name,
        variable_name,
        "vector",
        default_value,
        is_public,
        tooltip,
        category
    )


def create_rotator_variable(
    unreal_connection,
    blueprint_name: str,
    variable_name: str,
    default_value: list = None,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Convenience function to create a rotator variable.
    
    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        variable_name: Name of the variable
        default_value: Default rotator value as [pitch, yaw, roll] (optional)
        is_public: Whether the variable is public
        tooltip: Tooltip text
        category: Category name
    
    Returns:
        Dictionary containing variable details and status
        
    Example:
        >>> create_rotator_variable(unreal, "MyActor", "Rotation", [0, 90, 0], True, "Object rotation")
    """
    if default_value is None:
        default_value = [0.0, 0.0, 0.0]
    
    return create_variable(
        unreal_connection,
        blueprint_name,
        variable_name,
        "rotator",
        default_value,
        is_public,
        tooltip,
        category
    )