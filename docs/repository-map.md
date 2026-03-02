# Unreal Engine MCP Repository Map

## Scope and snapshot
- Repository purpose: Unreal Engine MCP stack with a Python MCP server and a C++ Unreal Editor plugin.
- Architecture path: MCP host/client -> `Python/unreal_mcp_server_advanced.py` -> TCP (`127.0.0.1:55557`) -> Unreal plugin (`UnrealMCP`) -> Unreal Editor APIs.
- Duplicate plugin note: `FlopperamUnrealMCP/Plugins/UnrealMCP` is byte-identical to `UnrealMCP` (verified: 0 missing files, 0 content differences).

## Top-level map
- `README.md`: Main project docs, setup, architecture, and capability overview.
- `DEBUGGING.md`: Troubleshooting for MCP config, Python dependencies, and startup issues.
- `.gitignore`: Python/Unreal build, cache, and editor-generated excludes.
- `.cursor/rules/unreal-mcp-shapes-guide.mdc`: Cursor rulebook for colored-shape and physics workflows.
- `assets/`: Screenshot assets used in docs.
- `Guides/`: User guides and prompt/tool references.
- `Python/`: MCP server runtime and helper modules.
- `UnrealMCP/`: Standalone Unreal plugin source.
- `FlopperamUnrealMCP/`: Example Unreal project that embeds the plugin.

## Full file inventory

### Root
- `README.md`
- `DEBUGGING.md`
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

### Unreal plugin (canonical source): `UnrealMCP`
- `UnrealMCP/UnrealMCP.uplugin`
- `UnrealMCP/Source/UnrealMCP/UnrealMCP.Build.cs`
- `UnrealMCP/Source/UnrealMCP/Public/EpicUnrealMCPBridge.h`
- `UnrealMCP/Source/UnrealMCP/Public/EpicUnrealMCPModule.h`
- `UnrealMCP/Source/UnrealMCP/Public/MCPServerRunnable.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPBlueprintCommands.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPBlueprintGraphCommands.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPCommonUtils.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPEditorCommands.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/BPConnector.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/BPVariables.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/EventManager.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/NodeDeleter.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/NodeManager.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/NodePropertyManager.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Function/FunctionIO.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Function/FunctionManager.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/AnimationNodes.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/CastingNodes.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/ControlFlowNodes.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/DataNodes.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/ExecutionSequenceEditor.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/MakeArrayEditor.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/SpecializedNodes.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/SwitchEnumEditor.h`
- `UnrealMCP/Source/UnrealMCP/Public/Commands/BlueprintGraph/Nodes/UtilityNodes.h`
- `UnrealMCP/Source/UnrealMCP/Private/EpicUnrealMCPBridge.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/EpicUnrealMCPModule.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/MCPServerRunnable.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPBlueprintCommands.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPBlueprintGraphCommands.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPCommonUtils.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPEditorCommands.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/BPConnector.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/BPVariables.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/EventManager.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/NodeDeleter.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/NodeManager.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/NodePropertyManager.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Function/FunctionIO.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Function/FunctionManager.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/AnimationNodes.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/CastingNodes.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/ControlFlowNodes.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/DataNodes.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/ExecutionSequenceEditor.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/MakeArrayEditor.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/NodeCreatorUtils.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/SpecializedNodes.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/SwitchEnumEditor.cpp`
- `UnrealMCP/Source/UnrealMCP/Private/Commands/BlueprintGraph/Nodes/UtilityNodes.cpp`

### Example Unreal project: `FlopperamUnrealMCP`
- `FlopperamUnrealMCP/FlopperamUnrealMCP.uproject`
- `FlopperamUnrealMCP/Source/FlopperamUnrealMCP.Target.cs`
- `FlopperamUnrealMCP/Source/FlopperamUnrealMCPEditor.Target.cs`
- `FlopperamUnrealMCP/Source/FlopperamUnrealMCP/FlopperamUnrealMCP.Build.cs`
- `FlopperamUnrealMCP/Source/FlopperamUnrealMCP/FlopperamUnrealMCP.cpp`
- `FlopperamUnrealMCP/Source/FlopperamUnrealMCP/FlopperamUnrealMCP.h`
- `FlopperamUnrealMCP/Config/DefaultEngine.ini`
- `FlopperamUnrealMCP/Config/DefaultGame.ini`
- `FlopperamUnrealMCP/Config/DefaultInput.ini`
- `FlopperamUnrealMCP/Config/DefaultEditor.ini`
- `FlopperamUnrealMCP/Plugins/UnrealMCP/UnrealMCP.uplugin`
- `FlopperamUnrealMCP/Plugins/UnrealMCP/Source/UnrealMCP/UnrealMCP.Build.cs`
- `FlopperamUnrealMCP/Plugins/UnrealMCP/Source/UnrealMCP/...` (full mirror of every file listed in the canonical `UnrealMCP` plugin section above)

## Runtime capability map

### Python MCP tools currently exposed (`Python/unreal_mcp_server_advanced.py`)
- Actor and transform: `get_actors_in_level`, `find_actors_by_name`, `delete_actor`, `set_actor_transform`
- Blueprint class and component: `create_blueprint`, `add_component_to_blueprint`, `set_static_mesh_properties`, `set_physics_properties`, `compile_blueprint`
- Blueprint analysis: `read_blueprint_content`, `analyze_blueprint_graph`, `get_blueprint_variable_details`, `get_blueprint_function_details`
- Procedural geometry/world: `create_pyramid`, `create_wall`, `create_tower`, `create_staircase`, `construct_house`, `construct_mansion`, `create_arch`, `spawn_physics_blueprint_actor`, `create_maze`, `create_town`, `create_castle_fortress`, `create_suspension_bridge`, `create_aqueduct`
- Material control: `get_available_materials`, `apply_material_to_actor`, `apply_material_to_blueprint`, `get_actor_material_info`, `set_mesh_material_color`
- Blueprint graph editing: `add_node`, `connect_nodes`, `create_variable`, `set_blueprint_variable_properties`, `add_event_node`, `delete_node`, `set_node_property`, `create_function`, `add_function_input`, `add_function_output`, `delete_function`, `rename_function`

### C++ command dispatch (`UEpicUnrealMCPBridge::ExecuteCommand`)
- Editor commands: actor listing/search/spawn/delete/transform and blueprint actor spawn.
- Blueprint commands: class/component/physics/material/graph inspection commands.
- Blueprint graph commands: node add/connect/delete, variable properties, event/function operations, semantic node editing.

## Observations from mapping
- Docs in `Guides/tools-reference.md` still describe a smaller tool subset; the server now exposes a larger set including advanced world generation and blueprint graph editing.
- The mirrored plugin copy under `FlopperamUnrealMCP/Plugins/UnrealMCP` increases maintenance overhead unless synchronized automatically.
- Transport is direct TCP JSON with command routing in C++ on the game thread and wrappers in Python.
