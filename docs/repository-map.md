# Unreal Engine MCP Repository Map

## Scope and snapshot
- Repository purpose: Unreal Engine MCP stack with a Python MCP server and a C++ Unreal Editor plugin.
- Architecture path: MCP host/client -> `Python/unreal_mcp_server_advanced.py` -> TCP (`127.0.0.1:55557`) -> Unreal plugin (`UnrealMCP`) -> Unreal Editor APIs.
- Plugin copies: `FlopperamUnrealMCP/Plugins/UnrealMCP` is the authoritative copy; `UnrealMCP` is synced from it.
- Protocol version: **1.2.0** (Phase 1 complete).

## Top-level map
- `README.md`: Main project docs, setup, architecture, and capability overview.
- `DEBUGGING.md`: Troubleshooting for MCP config, Python dependencies, and startup issues.
- `.gitignore`: Python/Unreal build, cache, and editor-generated excludes.
- `.cursor/rules/unreal-mcp-shapes-guide.mdc`: Cursor rulebook for colored-shape and physics workflows.
- `assets/`: Screenshot assets used in docs.
- `Guides/`: User guides and prompt/tool references.
- `Python/`: MCP server runtime and helper modules.
- `UnrealMCP/`: Standalone Unreal plugin source (synced from FlopperamUnrealMCP).
- `FlopperamUnrealMCP/`: Example Unreal project that embeds the plugin (authoritative source).

## Full file inventory

### Root
- `README.md`
- `DEBUGGING.md`
- `plan.md`
- `PHASE_4_REPORT.md`
- `.gitignore`

### Cursor config
- `.cursor/rules/unreal-mcp-shapes-guide.mdc`

### Assets
- `assets/blueprint_modification.png`
- `assets/blueprint_modification2.png`

### Guides
- `Guides/tools-reference.md`
- `Guides/prompt-examples.md`
- `Guides/colored-shapes-tutorial.md`
- `Guides/blueprint-graph-guide.md`

### Python runtime and tooling
- `Python/pyproject.toml`
- `Python/uv.lock`
- `Python/README_advanced.md`
- `Python/unreal_mcp_server_advanced.py`
- `Python/helpers/__init__.py`
- `Python/helpers/error_codes.py`
- `Python/helpers/actor_name_manager.py`
- `Python/helpers/actor_utilities.py`
- `Python/helpers/advanced_buildings.py`
- `Python/helpers/bridge_aqueduct_creation.py`
- `Python/helpers/building_creation.py`
- `Python/helpers/castle_creation.py`
- `Python/helpers/house_construction.py`
- `Python/helpers/infrastructure_creation.py`
- `Python/helpers/mansion_creation.py`
- `Python/helpers/tower_creation.py`
- `Python/helpers/blueprint_graph/__init__.py`
- `Python/helpers/blueprint_graph/connector_manager.py`
- `Python/helpers/blueprint_graph/event_manager.py`
- `Python/helpers/blueprint_graph/function_io.py`
- `Python/helpers/blueprint_graph/function_manager.py`
- `Python/helpers/blueprint_graph/graph_inspector.py`
- `Python/helpers/blueprint_graph/node_deleter.py`
- `Python/helpers/blueprint_graph/node_manager.py`
- `Python/helpers/blueprint_graph/node_properties.py`
- `Python/helpers/blueprint_graph/variable_manager.py`

### Unreal plugin (authoritative): `FlopperamUnrealMCP/Plugins/UnrealMCP`
- `UnrealMCP.uplugin`
- `Source/UnrealMCP/UnrealMCP.Build.cs`
- **Public headers:**
  - `Public/EpicUnrealMCPBridge.h`
  - `Public/EpicUnrealMCPModule.h`
  - `Public/MCPServerRunnable.h`
  - `Public/Commands/EpicUnrealMCPEditorCommands.h`
  - `Public/Commands/EpicUnrealMCPBlueprintCommands.h`
  - `Public/Commands/EpicUnrealMCPBlueprintGraphCommands.h`
  - `Public/Commands/EpicUnrealMCPCommonUtils.h`
  - `Public/Commands/EpicUnrealMCPErrorCodes.h`
  - `Public/Commands/EpicUnrealMCPTransactionCommands.h`
  - `Public/Commands/BlueprintGraph/BPConnector.h`
  - `Public/Commands/BlueprintGraph/BPVariables.h`
  - `Public/Commands/BlueprintGraph/EventManager.h`
  - `Public/Commands/BlueprintGraph/NodeDeleter.h`
  - `Public/Commands/BlueprintGraph/NodeManager.h`
  - `Public/Commands/BlueprintGraph/NodePropertyManager.h`
  - `Public/Commands/BlueprintGraph/Function/FunctionIO.h`
  - `Public/Commands/BlueprintGraph/Function/FunctionManager.h`
  - `Public/Commands/BlueprintGraph/Nodes/AnimationNodes.h`
  - `Public/Commands/BlueprintGraph/Nodes/CastingNodes.h`
  - `Public/Commands/BlueprintGraph/Nodes/ControlFlowNodes.h`
  - `Public/Commands/BlueprintGraph/Nodes/DataNodes.h`
  - `Public/Commands/BlueprintGraph/Nodes/ExecutionSequenceEditor.h`
  - `Public/Commands/BlueprintGraph/Nodes/MakeArrayEditor.h`
  - `Public/Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h`
  - `Public/Commands/BlueprintGraph/Nodes/SpecializedNodes.h`
  - `Public/Commands/BlueprintGraph/Nodes/SwitchEnumEditor.h`
  - `Public/Commands/BlueprintGraph/Nodes/UtilityNodes.h`
- **Private sources:** mirror of Public headers (`.cpp` files in `Private/`)

### Standalone plugin copy: `UnrealMCP`
- Synced from `FlopperamUnrealMCP/Plugins/UnrealMCP/Source/` — identical file set.

### Example Unreal project: `FlopperamUnrealMCP`
- `FlopperamUnrealMCP.uproject`
- `Source/FlopperamUnrealMCP.Target.cs`
- `Source/FlopperamUnrealMCPEditor.Target.cs`
- `Source/FlopperamUnrealMCP/FlopperamUnrealMCP.Build.cs`
- `Source/FlopperamUnrealMCP/FlopperamUnrealMCP.cpp`
- `Source/FlopperamUnrealMCP/FlopperamUnrealMCP.h`
- `Config/DefaultEngine.ini`
- `Config/DefaultGame.ini`
- `Config/DefaultInput.ini`
- `Config/DefaultEditor.ini`

## Runtime capability map

### Python MCP tools (`Python/unreal_mcp_server_advanced.py`) — 90+ tools

#### Actor and transform
- `get_actors_in_level`, `find_actors_by_name`, `delete_actor`, `set_actor_transform`

#### Blueprint class and component
- `create_blueprint`, `add_component_to_blueprint`, `set_static_mesh_properties`, `set_physics_properties`, `compile_blueprint`

#### Blueprint analysis
- `read_blueprint_content`, `analyze_blueprint_graph`, `get_blueprint_variable_details`, `get_blueprint_function_details`

#### Procedural geometry/world
- `create_pyramid`, `create_wall`, `create_tower`, `create_staircase`, `construct_house`, `construct_mansion`, `create_arch`, `spawn_physics_blueprint_actor`, `create_maze`, `create_town`, `create_castle_fortress`, `create_suspension_bridge`, `create_aqueduct`

#### Material control
- `get_available_materials`, `apply_material_to_actor`, `apply_material_to_blueprint`, `get_actor_material_info`, `set_mesh_material_color`

#### Blueprint graph editing
- `add_node`, `connect_nodes`, `create_variable`, `set_blueprint_variable_properties`, `add_event_node`, `delete_node`, `set_node_property`, `create_function`, `add_function_input`, `add_function_output`, `delete_function`, `rename_function`

#### Phase 1: Editor lifecycle (Item 1)
- `get_editor_state`, `get_engine_version`, `save_all`
- `request_exit_editor`, `restart_editor`
- `play_in_editor_start`, `play_in_editor_stop`
- `simulate_start`, `simulate_stop`
- `pause_game`

#### Phase 1: Level management (Item 2)
- `list_levels`, `open_level`, `save_level`, `create_level`
- `duplicate_level`, `delete_level`, `set_current_world`
- `add_sublevel`, `remove_sublevel`, `toggle_sublevel_visibility`

#### Phase 1: Selection and viewport (Item 4)
- `get_selected_actors`, `select_actors`, `focus_viewport_on_selection`
- `set_viewport_camera`, `capture_viewport_screenshot`, `get_viewport_stats`

#### Phase 1: Generic reflection (Item 5)
- `get_object_properties`, `set_object_properties`, `call_uobject_function`, `build_navmesh`

#### Phase 1: Transactions (Item 29)
- `begin_editor_transaction`, `end_editor_transaction`, `rollback_transaction`, `undo`, `redo`

#### Phase 1: Protocol and introspection (Item 30)
- `negotiate_protocol_version`, `batch_execute_commands`
- `ping`, `list_tools_dynamic`, `get_tool_json_schema`

#### Phase 1: Reliability (Item 33)
- `get_dead_letters`, `clear_dead_letters`
- Structured error codes via `Python/helpers/error_codes.py`
- Dead-letter queue infrastructure (module-level `record_dead_letter()`)

#### Phase 4: Gameplay and production (partial)
- Data tables, AI/navigation, widget blueprints, blueprint interfaces (see `PHASE_4_REPORT.md`)

### C++ command dispatch (`UEpicUnrealMCPBridge`)
- **EditorCommands handler**: actor CRUD, transforms, blueprint actor spawn, editor lifecycle, PIE/simulation, level management, selection/viewport, reflection APIs, navmesh build
- **BlueprintCommands handler**: class/component/physics/material/graph inspection
- **BlueprintGraphCommands handler**: node add/connect/delete, variable properties, event/function operations, semantic node editing
- **TransactionCommands handler**: begin/end/rollback transactions, undo/redo

### Error code system
- C++: `EpicUnrealMCPErrorCodes.h` — `MCPErrorCodes` namespace
- Python: `helpers/error_codes.py` — `MCPErrorCode` class + `make_error_response()`/`make_success_response()`

## Observations
- Plugin copies are now synchronized; `FlopperamUnrealMCP/Plugins/UnrealMCP` is authoritative.
- Phase 1 is complete. Phases 2, 3, 5 are not started. Phase 4 is partially done.
- Transport is direct TCP JSON with command routing in C++ on the game thread and wrappers in Python.
- Protocol version 1.2.0 supports: structured error codes, transactions, batch commands, idempotency keys, undo/redo, editor lifecycle, PIE/simulation, level management, selection/viewport, reflection APIs, dead-letter queue, and tool introspection.
