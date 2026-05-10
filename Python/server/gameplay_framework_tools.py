"""Gameplay Framework tools for the Unreal MCP server.
Implements UE5 Gameplay Framework: GameMode, GameState, PlayerController, Pawn, Character, etc.
"""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from utils.responses import make_error_response, is_success_response

logger = logging.getLogger("UnrealMCP_Advanced")


# ============================================================================
# GameMode Commands
# ============================================================================

@mcp.tool()
def create_gamemode_blueprint(name: str) -> Dict[str, Any]:
    """Create a new GameMode Blueprint class.
    
    Args:
        name: Name for the new GameMode blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_gamemode_blueprint", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_gamemode_blueprint error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_gamemode_cpp_class(name: str) -> Dict[str, Any]:
    """Create a new GameMode C++ class.
    
    Args:
        name: Name for the new GameMode C++ class
        
    Returns:
        Response with created class file paths
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_gamemode_cpp_class", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_gamemode_cpp_class error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_default_gamemode(gamemode_path: str) -> Dict[str, Any]:
    """Set the default GameMode in DefaultEngine.ini.
    
    Args:
        gamemode_path: Path to the GameMode blueprint (e.g., /Game/GameModes/BP_MyGameMode)
        
    Returns:
        Response confirming the default GameMode was set
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"gamemode_path": gamemode_path}
        response = unreal.send_command("set_default_gamemode", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_default_gamemode error: {e}")
        return make_error_response(str(e))


# ============================================================================
# GameState Commands
# ============================================================================

@mcp.tool()
def create_gamestate(name: str) -> Dict[str, Any]:
    """Create a new GameState Blueprint class.
    
    Args:
        name: Name for the new GameState blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_gamestate", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_gamestate error: {e}")
        return make_error_response(str(e))


# ============================================================================
# PlayerState Commands
# ============================================================================

@mcp.tool()
def create_playerstate(name: str) -> Dict[str, Any]:
    """Create a new PlayerState Blueprint class.
    
    Args:
        name: Name for the new PlayerState blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_playerstate", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_playerstate error: {e}")
        return make_error_response(str(e))


# ============================================================================
# Controller Commands
# ============================================================================

@mcp.tool()
def create_playercontroller(name: str) -> Dict[str, Any]:
    """Create a new PlayerController Blueprint class.
    
    Args:
        name: Name for the new PlayerController blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_playercontroller", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_playercontroller error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_aicontroller(name: str) -> Dict[str, Any]:
    """Create a new AIController Blueprint class.
    
    Args:
        name: Name for the new AIController blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_aicontroller", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_aicontroller error: {e}")
        return make_error_response(str(e))


# ============================================================================
# Pawn / Character Commands
# ============================================================================

@mcp.tool()
def create_pawn(name: str) -> Dict[str, Any]:
    """Create a new Pawn Blueprint class.
    
    Args:
        name: Name for the new Pawn blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_pawn", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_pawn error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_character(
    name: str,
    add_movement_component: bool = False,
    max_walk_speed: Optional[float] = None
) -> Dict[str, Any]:
    """Create a new Character Blueprint class.
    
    Args:
        name: Name for the new Character blueprint
        add_movement_component: Whether to configure movement component
        max_walk_speed: Maximum walk speed for the character
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        if add_movement_component:
            params["add_movement_component"] = True
        if max_walk_speed is not None:
            params["max_walk_speed"] = max_walk_speed
        response = unreal.send_command("create_character", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_character error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_default_pawn(pawn_path: str, gamemode_path: Optional[str] = None) -> Dict[str, Any]:
    """Set the default Pawn class on the configured GameMode Blueprint.
    
    Args:
        pawn_path: Path to the Pawn blueprint (e.g., /Game/Pawns/BP_MyPawn)
        gamemode_path: Optional GameMode blueprint path. Uses the configured default GameMode when omitted.
        
    Returns:
        Response confirming the default pawn path
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"pawn_path": pawn_path}
        if gamemode_path:
            params["gamemode_path"] = gamemode_path
        response = unreal.send_command("set_default_pawn", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_default_pawn error: {e}")
        return make_error_response(str(e))


# ============================================================================
# HUD / Spectator Commands
# ============================================================================

@mcp.tool()
def set_hud_class(name: str, gamemode_path: Optional[str] = None) -> Dict[str, Any]:
    """Create a new HUD Blueprint class and apply it to a GameMode when available.
    
    Args:
        name: Name for the new HUD blueprint
        gamemode_path: Optional GameMode blueprint path. Uses the configured default GameMode when omitted.
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        if gamemode_path:
            params["gamemode_path"] = gamemode_path
        response = unreal.send_command("set_hud_class", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_hud_class error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_spectator_pawn(name: str, gamemode_path: Optional[str] = None) -> Dict[str, Any]:
    """Create a new Spectator Pawn Blueprint class and apply it to a GameMode when available.
    
    Args:
        name: Name for the new Spectator Pawn blueprint
        gamemode_path: Optional GameMode blueprint path. Uses the configured default GameMode when omitted.
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        if gamemode_path:
            params["gamemode_path"] = gamemode_path
        response = unreal.send_command("set_spectator_pawn", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_spectator_pawn error: {e}")
        return make_error_response(str(e))


# ============================================================================
# Player Start / Spawn Commands
# ============================================================================

@mcp.tool()
def place_player_start(
    location: Optional[List[float]] = None,
    rotation: Optional[List[float]] = None,
    tag: Optional[str] = None,
    name: Optional[str] = None
) -> Dict[str, Any]:
    """Place a PlayerStart actor in the level.
    
    Args:
        location: [x, y, z] location coordinates
        rotation: [pitch, yaw, roll] rotation values
        tag: Optional tag to assign to the PlayerStart
        name: Optional custom name for the actor
        
    Returns:
        Response with placed PlayerStart details
    """
    if location is None:
        location = []
    if rotation is None:
        rotation = []
        
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {}
        if location:
            params["location"] = {"value": location}
        if rotation:
            params["rotation"] = {"value": rotation}
        if tag:
            params["tag"] = tag
        if name:
            params["name"] = name
        response = unreal.send_command("place_player_start", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"place_player_start error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_spawn_rules(
    spawn_method: str = "PlayerStart",
    location: Optional[List[float]] = None
) -> Dict[str, Any]:
    """Configure spawn rules for the game.
    
    Args:
        spawn_method: Spawn method - PlayerStart, Transform, or Custom
        location: [x, y, z] spawn location for Transform method
        
    Returns:
        Response with spawn rule configuration
    """
    if location is None:
        location = []
        
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"spawn_method": spawn_method}
        if location:
            params["location"] = {"value": location}
        response = unreal.send_command("set_spawn_rules", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_spawn_rules error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def set_possess_rules(
    possess_method: str = "Auto",
    auto_possess: bool = True,
    delay_seconds: Optional[float] = None
) -> Dict[str, Any]:
    """Configure possession rules for pawns.
    
    Args:
        possess_method: Possession method - Auto, Delayed, or Manual
        auto_possess: Whether to auto-possess on spawn
        delay_seconds: Delay in seconds for Delayed method
        
    Returns:
        Response with possess rule configuration
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "possess_method": possess_method,
            "auto_possess": auto_possess
        }
        if delay_seconds is not None:
            params["delay_seconds"] = delay_seconds
        response = unreal.send_command("set_possess_rules", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_possess_rules error: {e}")
        return make_error_response(str(e))


# ============================================================================
# Camera Commands
# ============================================================================

@mcp.tool()
def set_camera_manager(name: str, playercontroller_path: Optional[str] = None) -> Dict[str, Any]:
    """Create a new PlayerCameraManager Blueprint class and apply it to a PlayerController when provided.
    
    Args:
        name: Name for the new CameraManager blueprint
        playercontroller_path: Optional PlayerController blueprint path to receive PlayerCameraManagerClass.
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        if playercontroller_path:
            params["playercontroller_path"] = playercontroller_path
        response = unreal.send_command("set_camera_manager", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"set_camera_manager error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def setup_camera_component(
    blueprint_name: str,
    field_of_view: Optional[float] = None,
    use_pawn_control_rotation: Optional[bool] = None,
    offset: Optional[List[float]] = None
) -> Dict[str, Any]:
    """Setup or configure a CameraComponent on a blueprint.
    
    Args:
        blueprint_name: Name of the blueprint to add camera to
        field_of_view: Camera field of view (default 90)
        use_pawn_control_rotation: Whether camera uses pawn control rotation
        offset: [x, y, z] relative location offset
        
    Returns:
        Response with camera configuration details
    """
    if offset is None:
        offset = []
        
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"blueprint_name": blueprint_name}
        if field_of_view is not None:
            params["field_of_view"] = field_of_view
        if use_pawn_control_rotation is not None:
            params["use_pawn_control_rotation"] = use_pawn_control_rotation
        if offset:
            params["offset"] = {"value": offset}
        response = unreal.send_command("setup_camera_component", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"setup_camera_component error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def setup_spring_arm(
    blueprint_name: str,
    target_arm_length: Optional[float] = None,
    socket_offset: Optional[List[float]] = None,
    target_offset: Optional[List[float]] = None,
    use_pawn_control_rotation: Optional[bool] = None,
    inherit_pitch: Optional[bool] = None,
    inherit_yaw: Optional[bool] = None,
    inherit_roll: Optional[bool] = None,
    do_collision_test: Optional[bool] = None
) -> Dict[str, Any]:
    """Setup or configure a SpringArmComponent on a blueprint.
    
    Args:
        blueprint_name: Name of the blueprint to add spring arm to
        target_arm_length: Length of the spring arm
        socket_offset: [x, y, z] socket offset
        target_offset: [x, y, z] target offset
        use_pawn_control_rotation: Whether spring arm uses pawn control rotation
        inherit_pitch: Whether to inherit pitch
        inherit_yaw: Whether to inherit yaw
        inherit_roll: Whether to inherit roll
        do_collision_test: Whether to perform collision test
        
    Returns:
        Response with spring arm configuration details
    """
    if socket_offset is None:
        socket_offset = []
    if target_offset is None:
        target_offset = []
        
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"blueprint_name": blueprint_name}
        if target_arm_length is not None:
            params["target_arm_length"] = target_arm_length
        if socket_offset:
            params["socket_offset"] = {"value": socket_offset}
        if target_offset:
            params["target_offset"] = {"value": target_offset}
        if use_pawn_control_rotation is not None:
            params["use_pawn_control_rotation"] = use_pawn_control_rotation
        if inherit_pitch is not None:
            params["inherit_pitch"] = inherit_pitch
        if inherit_yaw is not None:
            params["inherit_yaw"] = inherit_yaw
        if inherit_roll is not None:
            params["inherit_roll"] = inherit_roll
        if do_collision_test is not None:
            params["do_collision_test"] = do_collision_test
        response = unreal.send_command("setup_spring_arm", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"setup_spring_arm error: {e}")
        return make_error_response(str(e))


# ============================================================================
# SaveGame Commands
# ============================================================================

@mcp.tool()
def create_savegame_class(name: str) -> Dict[str, Any]:
    """Create a new SaveGame Blueprint class.
    
    Args:
        name: Name for the new SaveGame blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_savegame_class", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_savegame_class error: {e}")
        return make_error_response(str(e))


# ============================================================================
# GameInstance Commands
# ============================================================================

@mcp.tool()
def create_gameinstance(name: str) -> Dict[str, Any]:
    """Create a new GameInstance Blueprint class.
    
    Args:
        name: Name for the new GameInstance blueprint
        
    Returns:
        Response with created blueprint details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_gameinstance", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_gameinstance error: {e}")
        return make_error_response(str(e))


# ============================================================================
# Subsystem Commands
# ============================================================================

@mcp.tool()
def create_gameinstance_subsystem(name: str) -> Dict[str, Any]:
    """Create a new GameInstance Subsystem C++ class.
    
    Args:
        name: Name for the new GameInstance Subsystem class
        
    Returns:
        Response with created subsystem file paths
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_gameinstance_subsystem", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_gameinstance_subsystem error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_world_subsystem(name: str) -> Dict[str, Any]:
    """Create a new World Subsystem C++ class.
    
    Args:
        name: Name for the new World Subsystem class
        
    Returns:
        Response with created subsystem file paths
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_world_subsystem", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_world_subsystem error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_localplayer_subsystem(name: str) -> Dict[str, Any]:
    """Create a new LocalPlayer Subsystem C++ class.
    
    Args:
        name: Name for the new LocalPlayer Subsystem class
        
    Returns:
        Response with created subsystem file paths
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"name": name}
        response = unreal.send_command("create_localplayer_subsystem", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_localplayer_subsystem error: {e}")
        return make_error_response(str(e))


# ============================================================================
# Gameplay Tags Commands
# ============================================================================

@mcp.tool()
def setup_gameplay_tags(tags: List[str]) -> Dict[str, Any]:
    """Setup multiple Gameplay Tags at once.
    
    Args:
        tags: List of tag strings to register
        
    Returns:
        Response with registered tag details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"tags": tags}
        response = unreal.send_command("setup_gameplay_tags", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"setup_gameplay_tags error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_gameplay_tag(tag: str, comment: Optional[str] = None) -> Dict[str, Any]:
    """Add a single Gameplay Tag to the project.
    
    Args:
        tag: The tag string to add (e.g., "State.Dead")
        comment: Optional comment for the tag
        
    Returns:
        Response with added tag details
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {"tag": tag}
        if comment:
            params["comment"] = comment
        response = unreal.send_command("add_gameplay_tag", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_gameplay_tag error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def create_gameplay_tag_query(
    name: str = "TagQuery",
    query_type: str = "Any",
    tags: Optional[List[str]] = None
) -> Dict[str, Any]:
    """Create a Gameplay Tag Query configuration.
    
    Args:
        name: Name for the query
        query_type: Query type - Any, All, or None
        tags: List of tags to include in the query
        
    Returns:
        Response with query configuration details
    """
    if tags is None:
        tags = []
        
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "name": name,
            "query_type": query_type,
            "tags": tags
        }
        response = unreal.send_command("create_gameplay_tag_query", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_gameplay_tag_query error: {e}")
        return make_error_response(str(e))


# ============================================================================
# Save / Load Commands
# ============================================================================

@mcp.tool()
def save_game_to_slot(
    slot_name: str = "save0",
    user_index: int = 0,
) -> Dict[str, Any]:
    """Save the current game state to a slot.

    Args:
        slot_name: Name of the save slot
        user_index: Player controller user index (default 0)

    Returns:
        Response with save success status
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {
            "slot_name": slot_name,
            "user_index": user_index,
        }
        response = unreal.send_command("save_game_to_slot", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"save_game_to_slot error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def load_game_from_slot(
    slot_name: str = "save0",
    user_index: int = 0,
) -> Dict[str, Any]:
    """Load game state from a save slot.

    Args:
        slot_name: Name of the save slot
        user_index: Player controller user index (default 0)

    Returns:
        Response with loaded data or error
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {
            "slot_name": slot_name,
            "user_index": user_index,
        }
        response = unreal.send_command("load_game_from_slot", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"load_game_from_slot error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def delete_save_slot(
    slot_name: str = "save0",
    user_index: int = 0,
) -> Dict[str, Any]:
    """Delete a save slot.

    Args:
        slot_name: Name of the save slot
        user_index: Player controller user index (default 0)

    Returns:
        Response with deletion success status
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {
            "slot_name": slot_name,
            "user_index": user_index,
        }
        response = unreal.send_command("delete_save_slot", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"delete_save_slot error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def has_save_game(
    slot_name: str = "save0",
    user_index: int = 0,
) -> Dict[str, Any]:
    """Check whether a save slot exists.

    Args:
        slot_name: Name of the save slot
        user_index: Player controller user index (default 0)

    Returns:
        Response with existence flag
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params: Dict[str, Any] = {
            "slot_name": slot_name,
            "user_index": user_index,
        }
        response = unreal.send_command("has_save_game", params)
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"has_save_game error: {e}")
        return make_error_response(str(e))
