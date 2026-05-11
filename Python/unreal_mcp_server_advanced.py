"""
Unreal Engine Advanced MCP Server

A streamlined MCP server focused on advanced composition tools for Unreal Engine.
Contains only the advanced tools from the expanded MCP tool system to keep tool count manageable.
"""

import logging

# Core infrastructure (re-exported for tests and helpers)
from server.core import (
    mcp,
    configure_logging,
    server_lifespan,
    get_unreal_connection,
    reset_unreal_connection,
    UnrealConnection,
)

# Re-export tool functions for backwards-compatible direct imports and tests.
from server.actor_tools import (
    get_actors_in_level,
    find_actors_by_name,
    delete_actor,
    spawn_actor,
    set_actor_transform,
    batch_spawn_actors,
    find_actor_by_mcp_id,
    set_actor_transform_by_mcp_id,
    delete_actor_by_mcp_id,
)

from server.blueprint_tools import (
    create_blueprint,
    add_component_to_blueprint,
    set_static_mesh_properties,
    set_physics_properties,
    compile_blueprint,
    read_blueprint_content,
    analyze_blueprint_graph,
    get_blueprint_variable_details,
    get_blueprint_function_details,
)

from server.blueprint_graph_tools import (
    add_node,
    connect_nodes,
    create_variable,
    set_blueprint_variable_properties,
    add_event_node,
    delete_node,
    set_node_property,
    create_function,
    add_function_input,
    add_function_output,
    delete_function,
    rename_function,
    apply_blueprint_json,
    export_blueprint_json,
)

from server.material_graph_tools import (
    create_material,
    add_material_node,
    connect_material_nodes,
    apply_material_json,
    export_material_json,
)

from server.material_tools import (
    get_available_materials,
    apply_material_to_actor,
    apply_material_to_blueprint,
    get_actor_material_info,
    get_blueprint_material_info,
    set_mesh_material_color,
)

from server.world_building_tools import (
    create_pyramid,
    create_wall,
    create_tower,
    create_staircase,
    construct_house,
    construct_mansion,
    create_arch,
    spawn_physics_blueprint_actor,
    create_maze,
    create_town,
    create_castle_fortress,
    create_suspension_bridge,
    create_aqueduct,
)

from server.scene_tools import (
    scene_create,
    scene_upsert_actor,
    scene_upsert_actors,
    scene_delete_actor,
    scene_snapshot_create,
    scene_snapshot_restore,
    scene_list_objects,
    scene_create_wall,
    scene_create_pyramid,
    scene_health,
    scene_plan_sync,
    scene_sync,
    scene_upsert_procedural_mesh,
    scene_create_sdf_mesh,
    scene_create_superformula_mesh,
    scene_create_lsystem_spline,
    scene_create_wfc_grid,
)

from server.project_editor_tools import (
    project_settings_tool,
    plugin_tool,
    engine_settings_tool,
    world_settings_tool,
    editor_control_tool,
    play_tool,
    viewport_tool,
)

from server.asset_management_tools import (
    asset_management_tool,
)

from server.asset_import_tools import (
    fbx_mesh_import_tool,
    texture_import_tool,
    audio_import_tool,
    asset_export_tool,
)

from server.mesh_editing_tools import (
    asset_mesh_editing_tool,
)

from server.enhanced_input_tools import (
    enhanced_input_tool,
)

from server.gameplay_framework_tools import (
    create_gamemode_blueprint,
    create_gamemode_cpp_class,
    set_default_gamemode,
    create_gamestate,
    create_playerstate,
    create_playercontroller,
    create_aicontroller,
    create_pawn,
    create_character,
    set_default_pawn,
    set_hud_class,
    set_spectator_pawn,
    place_player_start,
    set_spawn_rules,
    set_possess_rules,
    set_camera_manager,
    setup_camera_component,
    setup_spring_arm,
    create_savegame_class,
    create_gameinstance,
    create_gameinstance_subsystem,
    create_world_subsystem,
    create_localplayer_subsystem,
    setup_gameplay_tags,
    add_gameplay_tag,
    create_gameplay_tag_query,
)

from server.umg_tools import (
    umg_tool,
)

from server.lighting_tools import (
    set_light_intensity,
    set_light_color,
    set_light_temperature,
    set_light_mobility,
    set_light_shadow_enabled,
    set_light_shadow_bias,
    set_light_contact_shadows,
    set_light_volumetric_scattering,
    set_light_attenuation_radius,
    set_light_cone_angles,
    set_light_source_radius,
    set_light_ies_profile,
    set_light_channel,
    set_rect_light_properties,
    set_sky_light_properties,
    set_sky_atmosphere_properties,
    set_height_fog_properties,
    set_volumetric_fog,
    set_directional_light_as_sun,
    set_sun_position,
    create_hdri_backdrop,
    create_reflection_capture,
    set_reflection_capture_settings,
    build_reflection_captures,
    create_lightmass_importance_volume,
    build_lighting,
    set_lighting_scenario,
    set_megaliights,
)

from server.vertical_test_tools import (
    vertical_test_tool,
)

from server.vroid_tools import (
    vroid_check_plugin,
    vroid_import_vrm,
    vroid_spawn_avatar,
    vroid_validate_avatar_asset,
)


# Explicitly bootstrap tool registration to avoid heavy import side-effects.
from server import bootstrap
bootstrap()

# Re-export the mcp instance for any external consumers
__all__ = [
    "mcp",
    "get_unreal_connection",
    "reset_unreal_connection",
    "UnrealConnection",
    "configure_logging",
    "server_lifespan",
    "get_actors_in_level",
    "find_actors_by_name",
    "delete_actor",
    "spawn_actor",
    "set_actor_transform",
    "batch_spawn_actors",
    "create_blueprint",
    "add_component_to_blueprint",
    "set_static_mesh_properties",
    "set_physics_properties",
    "compile_blueprint",
    "read_blueprint_content",
    "analyze_blueprint_graph",
    "get_blueprint_variable_details",
    "get_blueprint_function_details",
    "add_node",
    "connect_nodes",
    "create_variable",
    "set_blueprint_variable_properties",
    "add_event_node",
    "delete_node",
    "set_node_property",
    "create_function",
    "add_function_input",
    "add_function_output",
    "delete_function",
    "rename_function",
    "apply_blueprint_json",
    "export_blueprint_json",
    "get_available_materials",
    "apply_material_to_actor",
    "apply_material_to_blueprint",
    "get_actor_material_info",
    "get_blueprint_material_info",
    "set_mesh_material_color",
    "create_material",
    "add_material_node",
    "connect_material_nodes",
    "apply_material_json",
    "export_material_json",
    "create_pyramid",
    "create_wall",
    "create_tower",
    "create_staircase",
    "construct_house",
    "construct_mansion",
    "create_arch",
    "spawn_physics_blueprint_actor",
    "create_maze",
    "create_town",
    "create_castle_fortress",
    "create_suspension_bridge",
    "create_aqueduct",
    "scene_create",
    "scene_upsert_actor",
    "scene_upsert_actors",
    "scene_delete_actor",
    "scene_snapshot_create",
    "scene_snapshot_restore",
    "scene_list_objects",
    "scene_create_wall",
    "scene_create_pyramid",
    "scene_health",
    "scene_plan_sync",
    "scene_sync",
    "scene_upsert_procedural_mesh",
    "scene_create_sdf_mesh",
    "scene_create_superformula_mesh",
    "scene_create_lsystem_spline",
    "scene_create_wfc_grid",
    "project_settings_tool",
    "plugin_tool",
    "engine_settings_tool",
    "world_settings_tool",
    "editor_control_tool",
    "play_tool",
    "viewport_tool",
    "asset_management_tool",
    "fbx_mesh_import_tool",
    "texture_import_tool",
    "audio_import_tool",
    "asset_export_tool",
    "asset_mesh_editing_tool",
    "enhanced_input_tool",
    "create_gamemode_blueprint",
    "create_gamemode_cpp_class",
    "set_default_gamemode",
    "create_gamestate",
    "create_playerstate",
    "create_playercontroller",
    "create_aicontroller",
    "create_pawn",
    "create_character",
    "set_default_pawn",
    "set_hud_class",
    "set_spectator_pawn",
    "place_player_start",
    "set_spawn_rules",
    "set_possess_rules",
    "set_camera_manager",
    "setup_camera_component",
    "setup_spring_arm",
    "create_savegame_class",
    "create_gameinstance",
    "create_gameinstance_subsystem",
    "create_world_subsystem",
    "create_localplayer_subsystem",
    "setup_gameplay_tags",
    "add_gameplay_tag",
    "create_gameplay_tag_query",
    "umg_tool",
    "set_light_intensity",
    "set_light_color",
    "set_light_temperature",
    "set_light_mobility",
    "set_light_shadow_enabled",
    "set_light_shadow_bias",
    "set_light_contact_shadows",
    "set_light_volumetric_scattering",
    "set_light_attenuation_radius",
    "set_light_cone_angles",
    "set_light_source_radius",
    "set_light_ies_profile",
    "set_light_channel",
    "set_rect_light_properties",
    "set_sky_light_properties",
    "set_sky_atmosphere_properties",
    "set_height_fog_properties",
    "set_volumetric_fog",
    "set_directional_light_as_sun",
    "set_sun_position",
    "create_hdri_backdrop",
    "create_reflection_capture",
    "set_reflection_capture_settings",
    "build_reflection_captures",
    "create_lightmass_importance_volume",
    "build_lighting",
    "set_lighting_scenario",
    "set_megaliights",
    "vertical_test_tool",
    "vroid_check_plugin",
    "vroid_import_vrm",
    "vroid_spawn_avatar",
    "vroid_validate_avatar_asset",
]


if __name__ == "__main__":
    configure_logging()
    logger = logging.getLogger("UnrealMCP_Advanced")
    logger.info("Starting Advanced MCP server with stdio transport")
    mcp.run(transport='stdio')
