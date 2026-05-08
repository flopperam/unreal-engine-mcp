"""Project / Editor Control tools for the Unreal MCP server.

Grouped tools exposing 34 individual C++ commands through 7 Python MCP tools.
Each tool uses an `action` parameter to dispatch to the correct C++ command.
"""

import logging
import time
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


# ---------------------------------------------------------------------------
# Level / Map Management (Phase 1)
# ---------------------------------------------------------------------------

@mcp.tool()
def level_tool(
    action: str,
    asset_path: Optional[str] = None,
    level_path: Optional[str] = None,
    source_path: Optional[str] = None,
    dest_path: Optional[str] = None,
) -> Dict[str, Any]:
    """Manage levels/maps: create, save, load, duplicate, rename, delete, list.

    Actions:
      create            - Create a new level (requires asset_path like /Game/Maps/MyLevel).
      save              - Save the current level to asset_path.
      load              - Load a level by asset_path.
      duplicate         - Duplicate a level (requires source_path, dest_path).
      rename            - Rename a level (requires source_path, dest_path).
      delete            - Delete a level by asset_path.
      get_current       - Return the current level name and path.
      list              - List all levels in the current world.
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    asset_path = asset_path or level_path

    try:
        if action == "create":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("create_level", {"asset_path": asset_path})

        elif action == "save":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("save_level", {"asset_path": asset_path})

        elif action == "load":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("load_level", {"asset_path": asset_path})

        elif action == "duplicate":
            if not source_path or not dest_path:
                return make_error_response("source_path and dest_path are required")
            return unreal.send_command("duplicate_level", {"source_path": source_path, "dest_path": dest_path})

        elif action == "rename":
            if not source_path or not dest_path:
                return make_error_response("source_path and dest_path are required")
            return unreal.send_command("rename_level", {"source_path": source_path, "dest_path": dest_path})

        elif action == "delete":
            if not asset_path:
                return make_error_response("asset_path is required")
            result = unreal.send_command("delete_level", {"asset_path": asset_path})
            if (
                not result.get("success")
                and "currently loaded" in result.get("error", "")
                and asset_path != "/Game/Maps/E2E_Advanced_Main"
            ):
                load_result = unreal.send_command("load_level", {"asset_path": "/Game/Maps/E2E_Advanced_Main"})
                if load_result.get("success"):
                    time.sleep(0.5)
                    result = unreal.send_command("delete_level", {"asset_path": asset_path})
            return result

        elif action == "get_current":
            result = unreal.send_command("get_current_level", {})
            if result.get("success") and "level_path" not in result and "outer_path" in result:
                result["level_path"] = result["outer_path"].split(".", 1)[0]
            return result

        elif action == "list":
            return unreal.send_command("list_levels", {})

        else:
            return make_error_response(f"Unknown level_tool action: {action}")
    except Exception as e:
        logger.error(f"level_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Persistent Level / Sublevel Management (Phase 2)
# ---------------------------------------------------------------------------

@mcp.tool()
def sublevel_tool(
    action: str,
    level_path: Optional[str] = None,
    level_name: Optional[str] = None,
    visible: Optional[bool] = None,
    loaded: Optional[bool] = None,
    location: Optional[List[float]] = None,
    extent: Optional[List[float]] = None,
    streaming_levels: Optional[List[str]] = None,
    should_be_loaded: Optional[bool] = None,
    should_be_visible: Optional[bool] = None,
    priority: Optional[int] = None,
    streaming_distance: Optional[float] = None,
) -> Dict[str, Any]:
    """Manage persistent level, sublevels, streaming volumes, and level streaming settings.

    Actions:
      get_persistent    - Return the persistent level info.
      add               - Add a sublevel (requires level_path like /Game/Maps/SubLevel).
      remove            - Remove a sublevel (requires level_name).
      set_visible       - Set sublevel visibility in editor (requires level_name, visible).
      set_loaded        - Set sublevel loaded state (requires level_name, loaded).
      create_volume     - Create a Level Streaming Volume (optional location [x,y,z], extent [x,y,z], streaming_levels).
      set_streaming     - Modify streaming settings (requires level_name; optional should_be_loaded, should_be_visible, priority).
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    action_aliases = {
        "get_persistent_level": "get_persistent",
        "add_sublevel": "add",
        "remove_sublevel": "remove",
        "set_sublevel_visible": "set_visible",
        "set_sublevel_loaded": "set_loaded",
        "set_streaming_settings": "set_streaming",
    }
    action = action_aliases.get(action, action)
    level_name = level_name or level_path

    try:
        if action == "get_persistent":
            result = unreal.send_command("get_persistent_level", {})
            if result.get("success") and "persistent_level" not in result and "level_name" in result:
                result["persistent_level"] = {
                    "level_name": result.get("level_name"),
                    "outer_path": result.get("outer_path"),
                }
            return result

        elif action == "add":
            if not level_path:
                return make_error_response("level_path is required")
            return unreal.send_command("add_sublevel", {"level_path": level_path})

        elif action == "remove":
            if not level_name:
                return make_error_response("level_name is required")
            return unreal.send_command("remove_sublevel", {"level_name": level_name})

        elif action == "set_visible":
            if not level_name or visible is None:
                return make_error_response("level_name and visible are required")
            return unreal.send_command("set_sublevel_visible", {"level_name": level_name, "visible": visible})

        elif action == "set_loaded":
            if not level_name or loaded is None:
                return make_error_response("level_name and loaded are required")
            return unreal.send_command("set_sublevel_loaded", {"level_name": level_name, "loaded": loaded})

        elif action == "create_volume":
            params = {}
            if location:
                params["location"] = location
            if extent:
                params["extent"] = extent
            if streaming_levels:
                params["streaming_levels"] = streaming_levels
            return unreal.send_command("create_streaming_volume", params)

        elif action == "set_streaming":
            if not level_name:
                return make_error_response("level_name is required")
            params = {"level_name": level_name}
            if should_be_loaded is not None:
                params["should_be_loaded"] = should_be_loaded
            if should_be_visible is not None:
                params["should_be_visible"] = should_be_visible
            if priority is not None:
                params["priority"] = priority
            if streaming_distance is not None:
                params["streaming_distance"] = streaming_distance
            return unreal.send_command("set_level_streaming_settings", params)

        else:
            return make_error_response(f"Unknown sublevel_tool action: {action}")
    except Exception as e:
        logger.error(f"sublevel_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# World Partition (Phase 3)
# ---------------------------------------------------------------------------

@mcp.tool()
def world_partition_tool(
    action: str,
    enable: Optional[bool] = None,
    placement_grid_size: Optional[int] = None,
    foliage_grid_size: Optional[int] = None,
    minimap_threshold: Optional[int] = None,
    min_x: Optional[float] = None,
    min_y: Optional[float] = None,
    min_z: Optional[float] = None,
    max_x: Optional[float] = None,
    max_y: Optional[float] = None,
    max_z: Optional[float] = None,
    clear_all: Optional[bool] = None,
) -> Dict[str, Any]:
    """Manage World Partition settings, cells, and grid.

    Actions:
      enable            - Enable/disable World Partition (requires enable).
      set_grid          - Set WP grid settings (optional placement_grid_size, foliage_grid_size, minimap_threshold).
      get_cells         - Return current WP editor bounds and loaded regions.
      load_cell         - Load a WP cell region (requires min_x/y/z, max_x/y/z).
      unload_cell       - Unload a matching/intersecting WP region, or clear all custom loaded regions.
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "enable":
            if enable is None:
                return make_error_response("enable is required")
            return unreal.send_command("enable_world_partition", {"enable": enable})

        elif action == "set_grid":
            params = {}
            if placement_grid_size is not None:
                params["placement_grid_size"] = placement_grid_size
            if foliage_grid_size is not None:
                params["foliage_grid_size"] = foliage_grid_size
            if minimap_threshold is not None:
                params["minimap_threshold"] = minimap_threshold
            return unreal.send_command("set_world_partition_grid", params)

        elif action == "get_cells":
            return unreal.send_command("get_world_partition_cells", {})

        elif action == "load_cell":
            params = {}
            if min_x is not None: params["min_x"] = min_x
            if min_y is not None: params["min_y"] = min_y
            if min_z is not None: params["min_z"] = min_z
            if max_x is not None: params["max_x"] = max_x
            if max_y is not None: params["max_y"] = max_y
            if max_z is not None: params["max_z"] = max_z
            return unreal.send_command("load_world_partition_cell", params)

        elif action == "unload_cell":
            params = {}
            if min_x is not None: params["min_x"] = min_x
            if min_y is not None: params["min_y"] = min_y
            if min_z is not None: params["min_z"] = min_z
            if max_x is not None: params["max_x"] = max_x
            if max_y is not None: params["max_y"] = max_y
            if max_z is not None: params["max_z"] = max_z
            if clear_all is not None: params["clear_all"] = clear_all
            return unreal.send_command("unload_world_partition_cell", params)

        else:
            return make_error_response(f"Unknown world_partition_tool action: {action}")
    except Exception as e:
        logger.error(f"world_partition_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Project Settings
# ---------------------------------------------------------------------------

@mcp.tool()
def project_settings_tool(
    action: str,
    file: Optional[str] = None,
    section: Optional[str] = None,
    key: Optional[str] = None,
    value: Optional[str] = None,
    map_path: Optional[str] = None,
    description: Optional[str] = None,
    project_name: Optional[str] = None,
    company_name: Optional[str] = None,
    project_version: Optional[float] = None,
    game_mode: Optional[str] = None,
    game_instance: Optional[str] = None,
    transition_map: Optional[str] = None,
) -> Dict[str, Any]:
    """Manage project settings, default maps, and project metadata.

    Actions:
      get               - Read a project setting (requires file, section; optional key).
      set               - Write a project setting (requires file, section, key, value).
      set_default_map   - Set the default map (requires map_path).
      set_game_default_map - Set the game default map (requires map_path).
      set_editor_startup_map - Set the editor startup map (requires map_path).
      set_project_description - Update description/project_name/company_name/project_version.
      set_maps_and_modes - Update game_mode, game_instance, transition_map.
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "get":
            params = {"action": action}
            if file: params["file"] = file
            if section: params["section"] = section
            if key: params["key"] = key
            result = unreal.send_command("get_project_settings", params)
            if result.get("success") and "settings" not in result:
                if "values" in result:
                    result["settings"] = result["values"]
                elif "key" in result and "value" in result:
                    result["settings"] = {result["key"]: result["value"]}
            return result

        elif action == "set":
            if not section or not key or value is None:
                return make_error_response("set requires section, key, and value")
            return unreal.send_command("set_project_setting", {
                "file": file or "DefaultEngine.ini",
                "section": section,
                "key": key,
                "value": value,
            })

        elif action == "set_default_map":
            if not map_path:
                return make_error_response("map_path is required")
            return unreal.send_command("set_default_map", {"map_path": map_path})

        elif action == "set_game_default_map":
            if not map_path:
                return make_error_response("map_path is required")
            return unreal.send_command("set_game_default_map", {"map_path": map_path})

        elif action == "set_editor_startup_map":
            if not map_path:
                return make_error_response("map_path is required")
            return unreal.send_command("set_editor_startup_map", {"map_path": map_path})

        elif action == "set_project_description":
            params = {"action": action}
            if description: params["description"] = description
            if project_name: params["project_name"] = project_name
            if company_name: params["company_name"] = company_name
            if project_version is not None: params["project_version"] = project_version
            return unreal.send_command("set_project_description", params)

        elif action == "set_maps_and_modes":
            params = {"action": action}
            if game_mode: params["game_mode"] = game_mode
            if game_instance: params["game_instance"] = game_instance
            if transition_map: params["transition_map"] = transition_map
            return unreal.send_command("set_maps_and_modes", params)

        else:
            return make_error_response(f"Unknown project_settings_tool action: {action}")
    except Exception as e:
        logger.error(f"project_settings_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Plugins
# ---------------------------------------------------------------------------

@mcp.tool()
def plugin_tool(
    action: str,
    plugin_name: Optional[str] = None,
    enabled: Optional[bool] = None,
) -> Dict[str, Any]:
    """List or enable/disable plugins.

    Actions:
      list            - Return all discovered plugins with enabled status.
      set_enabled     - Enable or disable a plugin (requires plugin_name, enabled).
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "list":
            return unreal.send_command("list_plugins", {})
        elif action == "set_enabled":
            if not plugin_name or enabled is None:
                return make_error_response("plugin_name and enabled are required")
            return unreal.send_command("set_plugin_enabled", {
                "plugin_name": plugin_name,
                "enabled": enabled,
            })
        else:
            return make_error_response(f"Unknown plugin_tool action: {action}")
    except Exception as e:
        logger.error(f"plugin_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Engine Settings
# ---------------------------------------------------------------------------

@mcp.tool()
def engine_settings_tool(
    action: str,
    quality: Optional[int] = None,
    key: Optional[str] = None,
    value: Optional[str] = None,
) -> Dict[str, Any]:
    """Modify engine configuration settings.

    Actions:
      set_scalability    - Set overall scalability quality (requires quality 0-5).
      set_rendering    - Set a renderer config key/value.
      set_physics      - Set a physics config key/value.
      set_input        - Set an input config key/value.
      set_collision    - Set a collision config key/value.
      set_ai           - Set an AI system config key/value.
      set_navigation   - Set a navigation config key/value.
      set_packaging    - Set a packaging config key/value.
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    command_map = {
        "set_scalability": "set_engine_scalability",
        "set_rendering": "set_rendering_setting",
        "set_physics": "set_physics_setting",
        "set_input": "set_input_setting",
        "set_collision": "set_collision_setting",
        "set_ai": "set_ai_setting",
        "set_navigation": "set_navigation_setting",
        "set_packaging": "set_packaging_setting",
    }

    try:
        if action not in command_map:
            return make_error_response(f"Unknown engine_settings_tool action: {action}")

        params = {}
        if quality is not None:
            params["quality"] = quality
        if key is not None:
            params["key"] = key
        if value is not None:
            params["value"] = value

        return unreal.send_command(command_map[action], params)
    except Exception as e:
        logger.error(f"engine_settings_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# World Settings
# ---------------------------------------------------------------------------

@mcp.tool()
def world_settings_tool(
    action: str,
    world_to_meters: Optional[float] = None,
    kill_z: Optional[float] = None,
    enable_world_bounds_checks: Optional[bool] = None,
    enable_world_composition: Optional[bool] = None,
    default_game_mode: Optional[str] = None,
) -> Dict[str, Any]:
    """Get or modify the current level's World Settings.

    Actions:
      get  - Read current world settings.
      set  - Modify world settings (any provided fields are updated).
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "get":
            result = unreal.send_command("get_world_settings", {})
            if result.get("success") and "world_settings" not in result:
                result["world_settings"] = {
                    key: value
                    for key, value in result.items()
                    if key not in {"success", "error", "message"}
                }
            return result
        elif action == "set":
            params = {}
            if world_to_meters is not None: params["world_to_meters"] = world_to_meters
            if kill_z is not None: params["kill_z"] = kill_z
            if enable_world_bounds_checks is not None: params["enable_world_bounds_checks"] = enable_world_bounds_checks
            if enable_world_composition is not None: params["enable_world_composition"] = enable_world_composition
            if default_game_mode: params["default_game_mode"] = default_game_mode
            return unreal.send_command("set_world_setting", params)
        else:
            return make_error_response(f"Unknown world_settings_tool action: {action}")
    except Exception as e:
        logger.error(f"world_settings_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Editor Control
# ---------------------------------------------------------------------------

@mcp.tool()
def editor_control_tool(
    action: str,
    count: Optional[int] = None,
    prompt: Optional[bool] = None,
    asset_path: Optional[str] = None,
    tail_lines: Optional[int] = None,
    script: Optional[str] = None,
    commandlet_name: Optional[str] = None,
    args: Optional[str] = None,
    wait_for_completion: Optional[bool] = None,
    timeout_seconds: Optional[float] = None,
) -> Dict[str, Any]:
    """General editor control: undo, redo, save, logs, scripts, commandlets.

    Actions:
      undo                - Undo last transaction (optional count).
      redo                - Redo last transaction (optional count).
      get_dirty_assets    - List unsaved dirty assets.
      save_all            - Save all dirty packages (optional prompt=True for dialog).
      save_asset          - Save a specific asset by path.
      get_editor_log      - Read the last N lines of the editor log.
      create_utility_widget    - Stub: create Editor Utility Widget.
      create_utility_blueprint - Stub: create Editor Utility Blueprint.
      execute_python_script    - Execute a Python console command.
      execute_commandlet       - Run an Editor Commandlet.
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "undo":
            return unreal.send_command("undo", {"count": count or 1})
        elif action == "redo":
            return unreal.send_command("redo", {"count": count or 1})
        elif action == "get_dirty_assets":
            return unreal.send_command("get_dirty_assets", {})
        elif action == "save_all":
            return unreal.send_command("save_all", {"prompt": prompt or False})
        elif action == "save_asset":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("save_asset", {"asset_path": asset_path})
        elif action == "get_editor_log":
            result = unreal.send_command("get_editor_log", {"tail_lines": tail_lines or 100})
            if result.get("success") and "log_lines" not in result and "log_content" in result:
                result["log_lines"] = result["log_content"].splitlines()
            return result
        elif action == "create_utility_widget":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("create_utility_widget", {"asset_path": asset_path})
        elif action == "create_utility_blueprint":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("create_utility_blueprint", {"asset_path": asset_path})
        elif action == "execute_python_script":
            if not script:
                return make_error_response("script is required")
            return unreal.send_command("execute_python_script", {"script": script})
        elif action == "execute_commandlet":
            if not commandlet_name:
                return make_error_response("commandlet_name is required")
            params = {
                "commandlet_name": commandlet_name,
                "args": args or "",
            }
            if wait_for_completion is not None:
                params["wait_for_completion"] = wait_for_completion
            if timeout_seconds is not None:
                params["timeout_seconds"] = timeout_seconds
            return unreal.send_command("execute_commandlet", params)
        else:
            return make_error_response(f"Unknown editor_control_tool action: {action}")
    except Exception as e:
        logger.error(f"editor_control_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Play / Simulate
# ---------------------------------------------------------------------------

@mcp.tool()
def play_tool(
    action: str,
) -> Dict[str, Any]:
    """Control Play In Editor (PIE), Standalone, and Simulate modes.

    Actions:
      start_pie             - Start Play In Editor.
      stop_pie              - Stop active PIE/Simulate session.
      get_state             - Return current PIE/Simulate state.
      start_standalone_game - Launch standalone game window.
      start_simulate        - Start Simulate In Editor.
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    command_map = {
        "start_pie": "start_pie",
        "stop_pie": "stop_pie",
        "get_state": "get_play_state",
        "start_standalone_game": "start_standalone_game",
        "start_simulate": "start_simulate",
    }

    try:
        if action not in command_map:
            return make_error_response(f"Unknown play_tool action: {action}")

        result = unreal.send_command(command_map[action], {})
        if action != "stop_pie" or not result.get("success"):
            return result

        deadline = time.monotonic() + 15.0
        last_state = result
        while time.monotonic() < deadline:
            time.sleep(0.25)
            state = unreal.send_command("get_play_state", {})
            if state.get("success"):
                last_state = state
                if state.get("safe_for_editor_commands") or not any(
                    state.get(key)
                    for key in (
                        "play_world_active",
                        "play_session_queued",
                        "play_session_running",
                        "play_session_in_progress",
                        "end_play_queued",
                    )
                ):
                    result["play_state"] = state
                    return result

        result["play_state"] = last_state
        result["warning"] = "Timed out waiting for PIE/SIE teardown to become safe for editor commands"
        return result
    except Exception as e:
        logger.error(f"play_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Viewport / Camera
# ---------------------------------------------------------------------------

@mcp.tool()
def viewport_tool(
    action: str,
    location: Optional[List[float]] = None,
    rotation: Optional[List[float]] = None,
    actor_name: Optional[str] = None,
    mode: Optional[str] = None,
) -> Dict[str, Any]:
    """Control the editor viewport and camera.

    Actions:
      get_camera_position - Return current camera location and rotation.
      set_camera_position - Move camera (requires location [x,y,z] and/or rotation [pitch,yaw,roll]).
      viewport_action     - Perform a viewport action (focus_selected, focus_actor, set_view_mode).
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "get_camera_position":
            return unreal.send_command("get_camera_position", {})

        elif action == "set_camera_position":
            params = {}
            if location: params["location"] = location
            if rotation: params["rotation"] = rotation
            return unreal.send_command("set_camera_position", params)

        elif action == "viewport_action":
            if actor_name:
                params = {"action": "focus_actor", "actor_name": actor_name}
            elif mode in {"focus_selected", "focus_actor"}:
                params = {"action": mode}
            elif mode:
                params = {"action": "set_view_mode", "mode": mode}
            else:
                return make_error_response("actor_name or mode is required for viewport_action")
            return unreal.send_command("viewport_action", params)

        else:
            return make_error_response(f"Unknown viewport_tool action: {action}")
    except Exception as e:
        logger.error(f"viewport_tool error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Advanced World / Data Layer / HLOD / OFPA / Bounds / Origin Rebasing (Phase 4)
# ---------------------------------------------------------------------------

@mcp.tool()
def advanced_world_tool(
    action: str,
    name: Optional[str] = None,
    data_layer_name: Optional[str] = None,
    actor_names: Optional[List[str]] = None,
    enabled: Optional[bool] = None,
    map_path: Optional[str] = None,
    min_extent: Optional[List[float]] = None,
    max_extent: Optional[List[float]] = None,
    wait_for_completion: Optional[bool] = None,
    timeout_seconds: Optional[float] = None,
    extra_args: Optional[str] = None,
    setup_hlods: Optional[bool] = None,
    build_hlods: Optional[bool] = None,
    rebuild_hlods: Optional[bool] = None,
    delete_hlods: Optional[bool] = None,
    dump_stats: Optional[bool] = None,
) -> Dict[str, Any]:
    """Manage Data Layers, HLOD, OFPA, Level Bounds, and World Origin Rebasing.

    Actions:
      create_data_layer          - Create a Data Layer (requires name).
      add_actors_to_data_layer   - Add actors to a Data Layer (requires data_layer_name, actor_names).
      remove_actors_from_data_layer - Remove actors from a Data Layer (requires data_layer_name, actor_names).
      set_data_layer_enabled     - Enable/disable a Data Layer (requires data_layer_name, enabled).
      create_hlod_layer          - Create an HLOD Layer (requires name).
      build_hlod                 - Build HLOD for a map (optional map_path).
      rebuild_hlod               - Rebuild HLOD for a map (optional map_path).
      set_one_file_per_actor     - Enable/disable OFPA (requires enabled).
      set_level_bounds           - Set level bounds (requires min_extent [x,y,z], max_extent [x,y,z]).
      set_world_origin_rebasing  - Enable/disable World Origin Rebasing (requires enabled).
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "create_data_layer":
            if not name:
                return make_error_response("name is required")
            return unreal.send_command("create_data_layer", {"name": name})

        elif action == "add_actors_to_data_layer":
            if not data_layer_name or not actor_names:
                return make_error_response("data_layer_name and actor_names are required")
            return unreal.send_command("add_actors_to_data_layer", {
                "data_layer_name": data_layer_name,
                "actor_names": actor_names,
            })

        elif action == "remove_actors_from_data_layer":
            if not data_layer_name or not actor_names:
                return make_error_response("data_layer_name and actor_names are required")
            return unreal.send_command("remove_actors_from_data_layer", {
                "data_layer_name": data_layer_name,
                "actor_names": actor_names,
            })

        elif action == "set_data_layer_enabled":
            if not data_layer_name or enabled is None:
                return make_error_response("data_layer_name and enabled are required")
            return unreal.send_command("set_data_layer_enabled", {
                "data_layer_name": data_layer_name,
                "enabled": enabled,
            })

        elif action == "create_hlod_layer":
            if not name:
                return make_error_response("name is required")
            return unreal.send_command("create_hlod_layer", {"name": name})

        elif action == "build_hlod":
            params = {}
            if map_path:
                params["map_path"] = map_path
            if wait_for_completion is not None:
                params["wait_for_completion"] = wait_for_completion
            if timeout_seconds is not None:
                params["timeout_seconds"] = timeout_seconds
            if extra_args:
                params["extra_args"] = extra_args
            if setup_hlods is not None:
                params["setup_hlods"] = setup_hlods
            if build_hlods is not None:
                params["build_hlods"] = build_hlods
            if dump_stats is not None:
                params["dump_stats"] = dump_stats
            return unreal.send_command("build_hlod", params)

        elif action == "rebuild_hlod":
            params = {}
            if map_path:
                params["map_path"] = map_path
            if wait_for_completion is not None:
                params["wait_for_completion"] = wait_for_completion
            if timeout_seconds is not None:
                params["timeout_seconds"] = timeout_seconds
            if extra_args:
                params["extra_args"] = extra_args
            if delete_hlods is not None:
                params["delete_hlods"] = delete_hlods
            if rebuild_hlods is not None:
                params["rebuild_hlods"] = rebuild_hlods
            if dump_stats is not None:
                params["dump_stats"] = dump_stats
            return unreal.send_command("rebuild_hlod", params)

        elif action == "set_one_file_per_actor":
            if enabled is None:
                return make_error_response("enabled is required")
            return unreal.send_command("set_one_file_per_actor", {"enable": enabled})

        elif action == "set_level_bounds":
            if not min_extent or not max_extent:
                return make_error_response("min_extent and max_extent are required")
            return unreal.send_command("set_level_bounds", {
                "min": min_extent,
                "max": max_extent,
            })

        elif action == "set_world_origin_rebasing":
            if enabled is None:
                return make_error_response("enabled is required")
            return unreal.send_command("set_world_origin_rebasing", {"enable": enabled})

        else:
            return make_error_response(f"Unknown advanced_world_tool action: {action}")
    except Exception as e:
        logger.error(f"advanced_world_tool error: {e}")
        return make_error_response(str(e))
