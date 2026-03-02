# Unreal MCP Full-Control Plan

## Goal
Enable MCP clients to control the entire Unreal Engine editor and build pipeline end-to-end, not just world spawning and Blueprint graph edits.

## Current baseline
- Strong coverage already exists for actor manipulation, procedural world building, material application, Blueprint creation, Blueprint graph editing, and function/variable workflows.
- Major gaps remain in project lifecycle, content pipeline, sequencer/cinematics, landscape/foliage, VFX/audio, AI tooling, packaging, automation testing, and secure multi-agent operation.

## Feature backlog (full-control target)

1. Editor/session lifecycle control: `get_editor_state`, `get_engine_version`, `open_project`, `save_all`, `request_exit_editor`, `play_in_editor_start`, `play_in_editor_stop`, `simulate_start`, `simulate_stop`, `pause_game`, `step_frame`.
2. World and level lifecycle: `list_levels`, `open_level`, `save_level`, `create_level`, `duplicate_level`, `delete_level`, `set_current_world`, `add_sublevel`, `remove_sublevel`, `toggle_sublevel_visibility`.
3. World Partition and streaming: `list_world_partition_cells`, `load_partition_cells`, `unload_partition_cells`, `set_data_layer_state`, `list_data_layers`, `set_hlod_state`.
4. Selection and viewport operations: `get_selected_actors`, `select_actors`, `focus_viewport_on_selection`, `set_viewport_camera`, `capture_viewport_screenshot`, `get_viewport_stats`.
5. Generic object reflection API: `get_object_properties`, `set_object_properties`, `get_property_schema`, `call_uobject_function`, `list_callable_functions`.
6. Actor hierarchy and grouping: `attach_actor`, `detach_actor`, `set_actor_parent`, `duplicate_actor`, `group_actors`, `ungroup_actors`, `lock_actor`, `unlock_actor`.
7. Component-level control: `get_actor_components`, `add_component`, `remove_component`, `set_component_transform`, `attach_component`, `set_component_properties`, `set_component_asset_reference`.
8. Content Browser traversal: `list_assets`, `search_assets`, `get_asset_metadata`, `create_folder`, `rename_asset`, `move_asset`, `duplicate_asset`, `delete_asset`, `fixup_redirectors`.
9. Import/export pipeline: `import_asset`, `reimport_asset`, `export_asset`, `batch_import_assets`, `set_import_options`, `get_import_presets`.
10. Asset dependency and validation: `get_asset_dependencies`, `get_asset_referencers`, `find_unused_assets`, `run_asset_validation`, `auto_fix_asset_issues`.
11. Material authoring expansion: `create_material`, `create_material_instance`, `set_material_instance_param`, `add_material_expression`, `connect_material_pins`, `compile_material`, `set_material_usage_flags`.
12. Texture and mesh pipeline: `set_texture_compression`, `set_texture_mip_settings`, `build_static_mesh_lods`, `set_static_mesh_collision`, `add_static_mesh_socket`, `generate_nanite_settings`.
13. Skeletal/animation pipeline: `create_anim_blueprint`, `set_anim_class`, `retarget_animation`, `create_animation_montage`, `add_anim_notify`, `create_control_rig_asset`, `set_control_rig_variable`.
14. Sequencer and cinematics: `create_level_sequence`, `add_sequence_track`, `add_section`, `set_keyframes`, `bind_actor_to_sequence`, `add_camera_cut`, `set_sequence_playback_range`, `render_sequence_movie_queue`.
15. Landscape tooling: `create_landscape`, `import_heightmap`, `sculpt_landscape`, `paint_landscape_layer`, `set_landscape_material`, `create_landscape_spline`.
16. Foliage and procedural world systems: `add_foliage_type`, `paint_foliage_instances`, `remove_foliage_instances`, `scatter_procedural_meshes`, `regenerate_procedural_content`.
17. Niagara and VFX: `create_niagara_system`, `create_niagara_emitter`, `set_niagara_parameter`, `spawn_niagara_in_level`, `bake_niagara_simulation`.
18. Audio systems: `create_sound_cue`, `set_sound_attenuation`, `spawn_audio_actor`, `set_audio_component_parameters`, `create_meta_sound_asset`.
19. UMG/editor utility workflows: `create_widget_blueprint`, `add_widget_to_tree`, `set_widget_binding`, `create_editor_utility_widget`, `run_editor_utility_action`.
20. Gameplay framework tooling: `create_data_asset`, `create_data_table`, `update_data_table_row`, `manage_gameplay_tags`, `set_project_input_mappings`, `create_game_feature_data`.
21. AI and navigation: `build_navmesh`, `set_nav_modifier`, `create_behavior_tree`, `create_blackboard`, `edit_behavior_tree_nodes`, `run_eqs_query_preview`.
22. Physics/Chaos advanced control: `create_physics_asset`, `set_constraint_properties`, `set_collision_profile`, `create_geometry_collection`, `trigger_chaos_fracture`.
23. Blueprint system parity upgrades: `create_blueprint_interface`, `create_blueprint_macro_library`, `add_macro`, `add_timeline_track`, `find_replace_nodes`, `diff_blueprint_versions`.
24. Package/build/deploy operations: `build_project`, `cook_content`, `package_project`, `run_commandlet`, `build_lighting`, `build_hlods`, `generate_project_files`.
25. Automation and test harness: `run_automation_tests`, `run_gauntlet`, `execute_pie_smoke_test`, `assert_actor_state`, `record_test_artifacts`.
26. Source control integration: `source_control_status`, `checkout_asset`, `submit_changelist`, `revert_asset`, `create_shelved_change`, `annotate_asset_history`.
27. Logging, diagnostics, and profiling: `tail_output_log`, `get_recent_errors`, `start_trace_capture`, `stop_trace_capture`, `capture_memreport`, `capture_stat_dump`.
28. Performance-safe operations: `dry_run_command`, `estimate_actor_count_impact`, `estimate_memory_impact`, `apply_operation_budget`, `throttle_spawn_rate`.
29. Transaction and undo model: `begin_editor_transaction`, `end_editor_transaction`, `rollback_transaction`, `undo`, `redo`, `checkpoint_scene_state`.
30. MCP protocol upgrades: `list_tools_dynamic`, `get_tool_json_schema`, `batch_execute_commands`, `stream_progress_events`, `subscribe_editor_events`, `cancel_operation`.
31. Multi-agent coordination: `acquire_scene_lock`, `release_scene_lock`, `lock_asset`, `reserve_name_namespace`, `get_active_agent_sessions`.
32. Security and governance: `authenticate_client`, `authorize_tool_scope`, `sandbox_mode_on_off`, `allowed_path_policies`, `dangerous_command_guardrails`, `audit_log_export`.
33. Reliability hardening: request idempotency keys, connection heartbeat, retry policies with backoff, dead-letter command queue, structured error codes, compatibility version negotiation.

## Delivery phases

1. Phase 1 (critical control plane): items 1, 2, 4, 5, 29, 30, 33.
2. Phase 2 (content and world parity): items 3, 6, 7, 8, 9, 10, 12.
3. Phase 3 (creative pipelines): items 11, 13, 14, 15, 16, 17, 18.
4. Phase 4 (gameplay and production): items 19, 20, 21, 22, 23.
5. Phase 5 (shipping and operations): items 24, 25, 26, 27, 28, 31, 32.

## Immediate implementation priorities

1. Add generic reflection tools (`get_object_properties`, `set_object_properties`, `call_uobject_function`) because this unlocks broad engine coverage fastest.
2. Add editor lifecycle + level lifecycle controls to let agents create repeatable workflows from project open to PIE validation.
3. Add transaction, locking, and structured error/progress APIs before scaling to multi-agent or destructive build/package actions.
