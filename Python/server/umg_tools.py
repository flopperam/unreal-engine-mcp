"""UI / UMG / Common UI tools for the Unreal MCP server.

This module exposes Widget Blueprint creation, WidgetTree hierarchy editing,
slot/layout mutation, event binding, runtime viewport operations, and common
menu templates through one dynamic MCP tool.
"""

import logging
from typing import Any, Dict, List, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


def _put(params: Dict[str, Any], key: str, value: Any) -> None:
    if value is not None:
        params[key] = value


def _resolve_bp(widget_blueprint: Optional[str], blueprint_path: Optional[str], asset_path: Optional[str]) -> Optional[str]:
    return widget_blueprint or blueprint_path or asset_path


@mcp.tool()
def umg_tool(
    action: str,
    widget_blueprint: Optional[str] = None,
    blueprint_path: Optional[str] = None,
    asset_path: Optional[str] = None,
    parent_class: Optional[str] = None,
    common_ui: Optional[bool] = None,
    save: Optional[bool] = None,
    widget_type: Optional[str] = None,
    widget_name: Optional[str] = None,
    parent_name: Optional[str] = None,
    replace_existing: Optional[bool] = None,
    is_variable: Optional[bool] = None,
    text: Optional[str] = None,
    options: Optional[List[str]] = None,
    percent: Optional[float] = None,
    value: Optional[float] = None,
    checked: Optional[bool] = None,
    anchors: Optional[List[float]] = None,
    position: Optional[List[float]] = None,
    size: Optional[List[float]] = None,
    alignment: Optional[List[float]] = None,
    padding: Optional[List[float]] = None,
    horizontal_alignment: Optional[str] = None,
    vertical_alignment: Optional[str] = None,
    auto_size: Optional[bool] = None,
    z_order: Optional[int] = None,
    row: Optional[int] = None,
    column: Optional[int] = None,
    color: Optional[List[float]] = None,
    tint: Optional[List[float]] = None,
    resource_path: Optional[str] = None,
    image_size: Optional[List[float]] = None,
    font_size: Optional[int] = None,
    typeface: Optional[str] = None,
    style: Optional[Dict[str, Any]] = None,
    button_name: Optional[str] = None,
    print_string: Optional[str] = None,
    property_name: Optional[str] = None,
    function_name: Optional[str] = None,
    source_property: Optional[str] = None,
    binding_kind: Optional[str] = None,
    animation_name: Optional[str] = None,
    template_name: Optional[str] = None,
    instance_name: Optional[str] = None,
    input_mode: Optional[str] = None,
    show_mouse_cursor: Optional[bool] = None,
    visible: Optional[bool] = None,
    remove_root: Optional[bool] = None,
    force_gc: Optional[bool] = None,
    compile_after: Optional[bool] = None,
) -> Dict[str, Any]:
    """Create and edit UMG Widget Blueprints and runtime UI.

    Actions include: create_widget_blueprint, add_widget, remove_widget,
    set_slot, set_text, set_font, set_color, set_brush, set_style,
    bind_on_clicked, bind_property, create_animation, compile, inspect,
    add_to_viewport, remove_from_parent, click_button, set_input_mode,
    set_mouse_cursor, and create_template.
    """
    try:
        validate_string(action, "action")
    except ValidationError as exc:
        return make_validation_error_response_from_exception(exc)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    action_aliases = {
        "create": "create_widget_blueprint",
        "create_widget": "create_widget_blueprint",
        "create_widget_blueprint": "create_widget_blueprint",
        "add": "add_widget",
        "add_widget": "add_widget",
        "add_child": "add_widget",
        "remove": "remove_widget",
        "remove_widget": "remove_widget",
        "delete_widget": "remove_widget",
        "set_slot": "set_slot",
        "set_layout": "set_slot",
        "set_slot_properties": "set_slot",
        "set_text": "set_text",
        "set_font": "set_font",
        "set_color": "set_color",
        "set_brush": "set_brush",
        "set_style": "set_style",
        "bind_on_clicked": "bind_on_clicked",
        "button_onclicked": "bind_on_clicked",
        "bind_property": "bind_property",
        "create_animation": "create_animation",
        "compile": "compile",
        "inspect": "inspect",
        "add_to_viewport": "add_to_viewport",
        "hud_add": "add_to_viewport",
        "remove_from_parent": "remove_from_parent",
        "click_button": "click_button",
        "set_input_mode": "set_input_mode",
        "set_mouse_cursor": "set_mouse_cursor",
        "create_template": "create_template",
        "create_main_menu": "create_template",
        "create_pause_menu": "create_template",
        "create_settings_menu": "create_template",
        "create_dialogue_ui": "create_template",
        "create_inventory_ui": "create_template",
    }
    normalized_action = action_aliases.get(action, action)
    bp_path = _resolve_bp(widget_blueprint, blueprint_path, asset_path)

    try:
        if normalized_action == "create_widget_blueprint":
            if not bp_path:
                return make_error_response("asset_path is required")
            params: Dict[str, Any] = {"asset_path": bp_path}
            _put(params, "parent_class", parent_class)
            _put(params, "common_ui", common_ui)
            _put(params, "save", save)
            return unreal.send_command("create_widget_blueprint", params)

        if normalized_action == "add_widget":
            if not bp_path or not widget_type:
                return make_error_response("widget_blueprint/asset_path and widget_type are required")
            params = {
                "widget_blueprint": bp_path,
                "widget_type": widget_type,
            }
            _put(params, "widget_name", widget_name)
            _put(params, "parent_name", parent_name)
            _put(params, "replace_existing", replace_existing)
            _put(params, "is_variable", is_variable)
            _put(params, "text", text)
            _put(params, "options", options)
            _put(params, "percent", percent)
            _put(params, "value", value)
            _put(params, "checked", checked)
            _put(params, "compile_after", compile_after)
            return unreal.send_command("add_widget_to_widget_blueprint", params)

        if normalized_action == "remove_widget":
            if not bp_path:
                return make_error_response("widget_blueprint/asset_path is required")
            params = {"widget_blueprint": bp_path}
            _put(params, "widget_name", widget_name)
            _put(params, "remove_root", remove_root)
            _put(params, "force_gc", force_gc)
            return unreal.send_command("remove_widget_from_widget_blueprint", params)

        if normalized_action == "set_slot":
            if not bp_path or not widget_name:
                return make_error_response("widget_blueprint/asset_path and widget_name are required")
            params = {"widget_blueprint": bp_path, "widget_name": widget_name}
            _put(params, "anchors", anchors)
            _put(params, "position", position)
            _put(params, "size", size)
            _put(params, "alignment", alignment)
            _put(params, "padding", padding)
            _put(params, "horizontal_alignment", horizontal_alignment)
            _put(params, "vertical_alignment", vertical_alignment)
            _put(params, "auto_size", auto_size)
            _put(params, "z_order", z_order)
            _put(params, "row", row)
            _put(params, "column", column)
            return unreal.send_command("set_widget_slot_properties", params)

        if normalized_action == "set_text":
            if not bp_path or not widget_name or text is None:
                return make_error_response("widget_blueprint/asset_path, widget_name, and text are required")
            return unreal.send_command("set_widget_text", {
                "widget_blueprint": bp_path,
                "widget_name": widget_name,
                "text": text,
            })

        if normalized_action == "set_font":
            if not bp_path or not widget_name:
                return make_error_response("widget_blueprint/asset_path and widget_name are required")
            params = {"widget_blueprint": bp_path, "widget_name": widget_name}
            _put(params, "font_size", font_size)
            _put(params, "typeface", typeface)
            return unreal.send_command("set_widget_font", params)

        if normalized_action == "set_color":
            if not bp_path or not widget_name or color is None:
                return make_error_response("widget_blueprint/asset_path, widget_name, and color are required")
            return unreal.send_command("set_widget_color", {
                "widget_blueprint": bp_path,
                "widget_name": widget_name,
                "color": color,
            })

        if normalized_action == "set_brush":
            if not bp_path or not widget_name:
                return make_error_response("widget_blueprint/asset_path and widget_name are required")
            params = {"widget_blueprint": bp_path, "widget_name": widget_name}
            _put(params, "resource_path", resource_path)
            _put(params, "image_size", image_size)
            _put(params, "size", size)
            _put(params, "tint", tint)
            _put(params, "color", color)
            return unreal.send_command("set_widget_brush", params)

        if normalized_action == "set_style":
            if not bp_path or not widget_name or style is None:
                return make_error_response("widget_blueprint/asset_path, widget_name, and style are required")
            return unreal.send_command("set_widget_style", {
                "widget_blueprint": bp_path,
                "widget_name": widget_name,
                "style": style,
            })

        if normalized_action == "bind_on_clicked":
            if not bp_path:
                return make_error_response("widget_blueprint/asset_path is required")
            target_button = button_name or widget_name
            if not target_button:
                return make_error_response("button_name or widget_name is required")
            params = {"widget_blueprint": bp_path, "button_name": target_button}
            _put(params, "print_string", print_string)
            return unreal.send_command("bind_widget_button_on_clicked", params)

        if normalized_action == "bind_property":
            if not bp_path or not widget_name or not property_name:
                return make_error_response("widget_blueprint/asset_path, widget_name, and property_name are required")
            params = {
                "widget_blueprint": bp_path,
                "widget_name": widget_name,
                "property_name": property_name,
            }
            _put(params, "function_name", function_name)
            _put(params, "source_property", source_property)
            _put(params, "binding_kind", binding_kind)
            return unreal.send_command("bind_widget_property", params)

        if normalized_action == "create_animation":
            if not bp_path:
                return make_error_response("widget_blueprint/asset_path is required")
            params = {"widget_blueprint": bp_path}
            _put(params, "animation_name", animation_name)
            return unreal.send_command("create_widget_animation", params)

        if normalized_action == "compile":
            if not bp_path:
                return make_error_response("widget_blueprint/asset_path is required")
            return unreal.send_command("compile_widget_blueprint", {"widget_blueprint": bp_path})

        if normalized_action == "inspect":
            if not bp_path:
                return make_error_response("widget_blueprint/asset_path is required")
            return unreal.send_command("inspect_widget_blueprint", {"widget_blueprint": bp_path})

        if normalized_action == "add_to_viewport":
            if not bp_path:
                return make_error_response("widget_blueprint/asset_path is required")
            params = {"widget_blueprint": bp_path}
            _put(params, "instance_name", instance_name)
            _put(params, "z_order", z_order)
            _put(params, "show_mouse_cursor", show_mouse_cursor)
            return unreal.send_command("add_widget_to_viewport", params)

        if normalized_action == "remove_from_parent":
            if not instance_name:
                return make_error_response("instance_name is required")
            return unreal.send_command("remove_widget_from_parent", {"instance_name": instance_name})

        if normalized_action == "click_button":
            target_button = button_name or widget_name
            if not instance_name or not target_button:
                return make_error_response("instance_name and button_name/widget_name are required")
            return unreal.send_command("click_widget_button", {
                "instance_name": instance_name,
                "button_name": target_button,
            })

        if normalized_action == "set_input_mode":
            params = {}
            _put(params, "input_mode", input_mode)
            _put(params, "show_mouse_cursor", show_mouse_cursor)
            return unreal.send_command("set_ui_input_mode", params)

        if normalized_action == "set_mouse_cursor":
            return unreal.send_command("set_mouse_cursor_visible", {"visible": True if visible is None else visible})

        if normalized_action == "create_template":
            resolved_template = template_name
            if not resolved_template:
                if action == "create_main_menu":
                    resolved_template = "main_menu"
                elif action == "create_pause_menu":
                    resolved_template = "pause_menu"
                elif action == "create_settings_menu":
                    resolved_template = "settings_menu"
                elif action == "create_dialogue_ui":
                    resolved_template = "dialogue_ui"
                elif action == "create_inventory_ui":
                    resolved_template = "inventory_ui"
            params = {}
            _put(params, "template_name", resolved_template)
            _put(params, "asset_path", bp_path)
            _put(params, "common_ui", common_ui)
            return unreal.send_command("create_ui_template", params)

        return make_error_response(f"Unknown umg_tool action: {action}")
    except Exception as exc:
        logger.error("umg_tool error: %s", exc)
        return make_error_response(str(exc))
