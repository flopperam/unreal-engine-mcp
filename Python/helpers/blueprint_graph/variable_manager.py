"""
Filename: variable_manager.py
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


def set_blueprint_variable_properties(
    unreal_connection,
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
    """
    Modify properties of an existing Blueprint variable without deleting it.
    Preserves all VariableGet and VariableSet nodes connected to this variable.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        variable_name: Name of the variable to modify
        is_blueprint_readable: Allow reading in Blueprint (VariableGet) (optional)
        is_blueprint_writable: Allow writing in Blueprint (Set) (optional)
        is_public: Visible in Blueprint editor (optional)
        is_editable_in_instance: Modifiable on instances (optional)
        tooltip: Variable description (optional)
        category: Variable category (optional)
        default_value: New default value (optional)

    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - variable_name (str): Name of the modified variable
            - properties_updated (dict): Properties that were updated
            - error (str): Error message if failed

    Example:
        >>> result = set_blueprint_variable_properties(
        ...     unreal,
        ...     "TestWorkflowBP",
        ...     "TimerCounter",
        ...     is_blueprint_readable=True,
        ...     is_blueprint_writable=True,
        ...     category="Gameplay"
        ... )
        >>> print(result["properties_updated"])
        {'is_blueprint_readable': True, 'is_blueprint_writable': True, 'category': 'Gameplay'}
    """
    try:
        params = {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name
        }

        # Only include optional parameters if they are specified
        if var_name is not None:
            params["var_name"] = var_name
        if var_type is not None:
            params["var_type"] = var_type
        if is_blueprint_readable is not None:
            params["is_blueprint_readable"] = is_blueprint_readable
        if is_blueprint_writable is not None:
            params["is_blueprint_writable"] = is_blueprint_writable
        if is_public is not None:
            params["is_public"] = is_public
        if is_editable_in_instance is not None:
            params["is_editable_in_instance"] = is_editable_in_instance
        if tooltip is not None:
            params["tooltip"] = tooltip
        if category is not None:
            params["category"] = category
        if default_value is not None:
            params["default_value"] = default_value
        if expose_on_spawn is not None:
            params["expose_on_spawn"] = expose_on_spawn
        if expose_to_cinematics is not None:
            params["expose_to_cinematics"] = expose_to_cinematics
        if slider_range_min is not None:
            params["slider_range_min"] = slider_range_min
        if slider_range_max is not None:
            params["slider_range_max"] = slider_range_max
        if value_range_min is not None:
            params["value_range_min"] = value_range_min
        if value_range_max is not None:
            params["value_range_max"] = value_range_max
        if units is not None:
            params["units"] = units
        if bitmask is not None:
            params["bitmask"] = bitmask
        if bitmask_enum is not None:
            params["bitmask_enum"] = bitmask_enum
        if replication_enabled is not None:
            params["replication_enabled"] = replication_enabled
        if replication_condition is not None:
            params["replication_condition"] = replication_condition
        if is_private is not None:
            params["is_private"] = is_private

        response = unreal_connection.send_command("set_blueprint_variable_properties", params)

        if response.get("success"):
            logger.info(
                f"Successfully updated properties of variable '{variable_name}' in {blueprint_name}"
            )
        else:
            logger.error(
                f"Failed to update variable properties: {response.get('error', 'Unknown error')}"
            )

        return response

    except Exception as e:
        logger.error(f"Exception in set_blueprint_variable_properties: {e}")
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