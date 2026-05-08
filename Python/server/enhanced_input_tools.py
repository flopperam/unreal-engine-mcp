"""Enhanced Input tools for Unreal Engine 5.7.

The public MCP surface is intentionally one action-dispatched tool because the
Enhanced Input workflow shares the same asset paths, triggers, modifiers, and
runtime options across many operations.
"""

import logging
from typing import Any, Dict, List, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


def _compact(params: Dict[str, Any]) -> Dict[str, Any]:
    return {key: value for key, value in params.items() if value is not None}


def _single_modifier(
    modifier_type: str,
    lower_threshold: Optional[float] = None,
    upper_threshold: Optional[float] = None,
    dead_zone_type: Optional[str] = None,
    swizzle_order: Optional[str] = None,
    negate_x: Optional[bool] = None,
    negate_y: Optional[bool] = None,
    negate_z: Optional[bool] = None,
) -> Dict[str, Any]:
    modifier: Dict[str, Any] = {"type": modifier_type}
    if lower_threshold is not None:
        modifier["lower_threshold"] = lower_threshold
    if upper_threshold is not None:
        modifier["upper_threshold"] = upper_threshold
    if dead_zone_type is not None:
        modifier["dead_zone_type"] = dead_zone_type
    if swizzle_order is not None:
        modifier["order"] = swizzle_order
    if negate_x is not None:
        modifier["x"] = negate_x
    if negate_y is not None:
        modifier["y"] = negate_y
    if negate_z is not None:
        modifier["z"] = negate_z
    return modifier


@mcp.tool()
def enhanced_input_tool(
    action: str,
    asset_path: Optional[str] = None,
    input_action_path: Optional[str] = None,
    mapping_context_path: Optional[str] = None,
    blueprint_path: Optional[str] = None,
    target_type: Optional[str] = None,
    value_type: Optional[str] = None,
    description: Optional[str] = None,
    key: Optional[str] = None,
    new_key: Optional[str] = None,
    priority: Optional[int] = None,
    player_index: Optional[int] = None,
    mapping_index: Optional[int] = None,
    mappings: Optional[List[Dict[str, Any]]] = None,
    bindings: Optional[List[Dict[str, Any]]] = None,
    triggers: Optional[List[Dict[str, Any]]] = None,
    modifiers: Optional[List[Dict[str, Any]]] = None,
    trigger_type: Optional[str] = None,
    trigger_event: Optional[str] = None,
    hold_time_threshold: Optional[float] = None,
    tap_release_time_threshold: Optional[float] = None,
    actuation_threshold: Optional[float] = None,
    lower_threshold: Optional[float] = None,
    upper_threshold: Optional[float] = None,
    dead_zone_type: Optional[str] = None,
    swizzle_order: Optional[str] = None,
    negate_x: Optional[bool] = None,
    negate_y: Optional[bool] = None,
    negate_z: Optional[bool] = None,
    player_mappable: Optional[bool] = None,
    mapping_name: Optional[str] = None,
    display_name: Optional[str] = None,
    display_category: Optional[str] = None,
    slot: Optional[str] = None,
    profile_id: Optional[str] = None,
    hardware_device_id: Optional[str] = None,
    mapping_context_paths: Optional[List[str]] = None,
    overwrite: Optional[bool] = None,
    clear_all: Optional[bool] = None,
    remove_all_for_action: Optional[bool] = None,
    compile: Optional[bool] = None,
    save: Optional[bool] = None,
    save_settings: Optional[bool] = None,
    handler_function_name: Optional[str] = None,
    enable_user_settings: Optional[bool] = None,
    enable_default_mapping_contexts: Optional[bool] = None,
    filter_input_by_platform_user: Optional[bool] = None,
    create_local_players: Optional[int] = None,
    replace_default_contexts: Optional[bool] = None,
    force_immediately: Optional[bool] = None,
    notify_user_settings: Optional[bool] = None,
    unmap: Optional[bool] = None,
    create_matching_slot_if_needed: Optional[bool] = None,
) -> Dict[str, Any]:
    """Create, configure, bind, debug, and rebind UE5 Enhanced Input.

    Actions:
      create_input_action, create_input_mapping_context
      add_key_mapping, remove_key_mapping
      set_trigger, set_modifier, set_dead_zone, set_swizzle_axis, set_negate
      configure_input_action, configure_key_mapping
      add_runtime_mapping_context, remove_runtime_mapping_context
      generate_player_controller_binding, generate_character_binding
      get_debug_info, setup_rebind_ui, rebind_key, configure_local_multiplayer
      list
    """
    try:
        validate_string(action, "action")
    except ValidationError as exc:
        return make_validation_error_response_from_exception(exc)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    aliases = {
        "create_action": "create_input_action",
        "create_ia": "create_input_action",
        "create_mapping_context": "create_input_mapping_context",
        "create_imc": "create_input_mapping_context",
        "add_mapping": "add_key_mapping",
        "add_keyboard_mapping": "add_key_mapping",
        "add_mouse_mapping": "add_key_mapping",
        "add_gamepad_mapping": "add_key_mapping",
        "delete_key_mapping": "remove_key_mapping",
        "set_triggers": "set_trigger",
        "set_modifiers": "set_modifier",
        "configure_mapping": "configure_key_mapping",
        "configure_action": "configure_input_action",
        "runtime_add_mapping_context": "add_runtime_mapping_context",
        "runtime_remove_mapping_context": "remove_runtime_mapping_context",
        "generate_binding": "setup_binding",
        "generate_player_controller_binding": "setup_binding",
        "generate_character_binding": "setup_binding",
        "input_debug": "get_debug_info",
        "debug": "get_debug_info",
        "rebind_ui": "setup_rebind_ui",
        "rebind": "rebind_key",
        "local_multiplayer": "configure_local_multiplayer",
    }
    action = aliases.get(action, action)

    try:
        if action == "create_input_action":
            path = asset_path or input_action_path
            if not path:
                return make_error_response("asset_path or input_action_path is required")
            return unreal.send_command("create_input_action", _compact({
                "asset_path": path,
                "value_type": value_type,
                "description": description,
                "triggers": triggers,
                "modifiers": modifiers,
                "overwrite": overwrite,
            }))

        if action == "create_input_mapping_context":
            path = asset_path or mapping_context_path
            if not path:
                return make_error_response("asset_path or mapping_context_path is required")
            return unreal.send_command("create_input_mapping_context", _compact({
                "asset_path": path,
                "description": description,
                "mappings": mappings,
                "overwrite": overwrite,
            }))

        if action == "add_key_mapping":
            if not mapping_context_path or not input_action_path or not key:
                return make_error_response("mapping_context_path, input_action_path, and key are required")
            return unreal.send_command("add_enhanced_input_mapping", _compact({
                "mapping_context_path": mapping_context_path,
                "input_action_path": input_action_path,
                "key": key,
                "triggers": triggers,
                "modifiers": modifiers,
                "player_mappable": player_mappable,
                "mapping_name": mapping_name,
                "display_name": display_name,
                "display_category": display_category,
            }))

        if action == "remove_key_mapping":
            if not mapping_context_path:
                return make_error_response("mapping_context_path is required")
            return unreal.send_command("remove_enhanced_input_mapping", _compact({
                "mapping_context_path": mapping_context_path,
                "input_action_path": input_action_path,
                "key": key,
                "clear_all": clear_all,
                "remove_all_for_action": remove_all_for_action,
            }))

        if action == "configure_input_action":
            if not input_action_path:
                return make_error_response("input_action_path is required")
            return unreal.send_command("configure_enhanced_input_action", _compact({
                "input_action_path": input_action_path,
                "value_type": value_type,
                "description": description,
                "triggers": triggers,
                "modifiers": modifiers,
            }))

        if action in {"configure_key_mapping", "set_trigger", "set_modifier", "set_dead_zone", "set_swizzle_axis", "set_negate"}:
            if not mapping_context_path:
                return make_error_response("mapping_context_path is required")

            final_triggers = triggers
            final_modifiers = modifiers
            if action == "set_trigger":
                if final_triggers is None:
                    trig: Dict[str, Any] = {"type": trigger_type or "pressed"}
                    if hold_time_threshold is not None:
                        trig["hold_time_threshold"] = hold_time_threshold
                    if tap_release_time_threshold is not None:
                        trig["tap_release_time_threshold"] = tap_release_time_threshold
                    if actuation_threshold is not None:
                        trig["actuation_threshold"] = actuation_threshold
                    final_triggers = [trig]
            elif action == "set_dead_zone":
                final_modifiers = [_single_modifier("dead_zone", lower_threshold, upper_threshold, dead_zone_type)]
            elif action == "set_swizzle_axis":
                final_modifiers = [_single_modifier("swizzle_axis", swizzle_order=swizzle_order or "YXZ")]
            elif action == "set_negate":
                final_modifiers = [_single_modifier("negate", negate_x=negate_x, negate_y=negate_y, negate_z=negate_z)]

            return unreal.send_command("configure_enhanced_input_mapping", _compact({
                "mapping_context_path": mapping_context_path,
                "input_action_path": input_action_path,
                "key": key,
                "new_key": new_key,
                "mapping_index": mapping_index,
                "triggers": final_triggers,
                "modifiers": final_modifiers,
                "player_mappable": player_mappable,
                "mapping_name": mapping_name,
                "display_name": display_name,
                "display_category": display_category,
            }))

        if action == "list":
            return unreal.send_command("list_enhanced_input_assets", _compact({"path": asset_path}))

        if action == "get_debug_info":
            return unreal.send_command("get_enhanced_input_debug_info", _compact({
                "player_index": player_index,
                "mapping_context_path": mapping_context_path,
                "mapping_context_paths": mapping_context_paths,
            }))

        if action == "add_runtime_mapping_context":
            if not mapping_context_path:
                return make_error_response("mapping_context_path is required")
            return unreal.send_command("add_runtime_mapping_context", _compact({
                "mapping_context_path": mapping_context_path,
                "priority": priority,
                "player_index": player_index,
                "force_immediately": force_immediately,
                "notify_user_settings": notify_user_settings,
            }))

        if action == "remove_runtime_mapping_context":
            if not mapping_context_path:
                return make_error_response("mapping_context_path is required")
            return unreal.send_command("remove_runtime_mapping_context", _compact({
                "mapping_context_path": mapping_context_path,
                "player_index": player_index,
                "force_immediately": force_immediately,
                "notify_user_settings": notify_user_settings,
            }))

        if action == "setup_binding":
            if not blueprint_path:
                return make_error_response("blueprint_path is required")
            return unreal.send_command("setup_enhanced_input_binding", _compact({
                "blueprint_path": blueprint_path,
                "target_type": target_type,
                "input_action_path": input_action_path,
                "bindings": bindings,
                "trigger_event": trigger_event,
                "handler_function_name": handler_function_name,
                "compile": compile,
                "save": save,
            }))

        if action == "setup_rebind_ui":
            paths = mapping_context_paths or ([mapping_context_path] if mapping_context_path else None)
            if not paths:
                return make_error_response("mapping_context_path or mapping_context_paths is required")
            return unreal.send_command("setup_rebind_ui", _compact({
                "mapping_context_paths": paths,
                "enable_user_settings": enable_user_settings,
                "player_index": player_index,
                "profile_id": profile_id,
                "save_settings": save_settings,
            }))

        if action == "rebind_key":
            if not mapping_name or not key:
                return make_error_response("mapping_name and key are required")
            return unreal.send_command("rebind_enhanced_input_key", _compact({
                "mapping_name": mapping_name,
                "key": key,
                "slot": slot,
                "profile_id": profile_id,
                "hardware_device_id": hardware_device_id,
                "mapping_context_path": mapping_context_path,
                "mapping_context_paths": mapping_context_paths,
                "player_index": player_index,
                "save_settings": save_settings,
                "unmap": unmap,
                "create_matching_slot_if_needed": create_matching_slot_if_needed,
            }))

        if action == "configure_local_multiplayer":
            return unreal.send_command("configure_local_multiplayer_input", _compact({
                "mapping_context_path": mapping_context_path,
                "mapping_context_paths": mapping_context_paths,
                "priority": priority,
                "filter_input_by_platform_user": filter_input_by_platform_user,
                "enable_user_settings": enable_user_settings,
                "enable_default_mapping_contexts": enable_default_mapping_contexts,
                "replace_default_contexts": replace_default_contexts,
                "create_local_players": create_local_players,
            }))

        return make_error_response(f"Unknown enhanced_input_tool action: {action}")
    except Exception as exc:
        logger.error("enhanced_input_tool error: %s", exc)
        return make_error_response(str(exc))
