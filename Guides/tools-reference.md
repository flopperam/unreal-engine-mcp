# Tools Reference

Complete documentation for all 46 tools in the Unreal MCP Advanced Server.

## Actor Management

### get_actors_in_level

List all actors in the current level.

**Parameters:**
- `random_string` (str, optional): Ignored placeholder.

**Returns:** Array of actor information including names, types, and transforms.

---

### find_actors_by_name

Search for actors using name patterns (supports wildcards).

**Parameters:**
- `pattern` (str, required): Search pattern. Must be at least 1 character.

**Returns:** Matching actors with details.

**Validation:** `pattern` is validated as a non-empty string.

---

### spawn_actor

Create a single actor in the level.

**Parameters:**
- `name` (str, required): Unique actor name.
- `type` (str, required): Actor class name (e.g., `"StaticMeshActor"`).
- `location` (list[float], optional): `[X, Y, Z]` spawn position. Default `None` → `[0, 0, 0]`.
- `rotation` (list[float], optional): `[Pitch, Yaw, Roll]` in degrees. Default `None` → `[0, 0, 0]`.
- `scale` (list[float], optional): `[X, Y, Z]` scale factors. Default `None` → `[1, 1, 1]`.
- `static_mesh` (str, optional): Unreal asset path for the mesh (must start with `/`).

**Returns:** Spawn result with actor details.

**Validation:** `name`, `type` must be non-empty strings. `location`/`rotation`/`scale` validated as 3-element numeric vectors. `static_mesh` validated as Unreal path.

---

### batch_spawn_actors

Spawn multiple actors with per-actor validation and optional dry-run.

**Parameters:**
- `actors` (list[dict], required): Each dict must have `"name"` (str) and `"type"` (str), and may include `"location"`, `"rotation"`, `"scale"`, `"static_mesh"`.
- `dry_run` (bool, default `False`): If `True`, return the planned actor list without spawning.

**Constraints:**
- Maximum 500 actors per batch (`MAX_ACTORS_PER_BATCH`).
- Each actor is validated individually (see `spawn_actor` validation).

**Returns (dry_run=False):**
```json
{
  "success": true,
  "spawned_count": 3,
  "failed_count": 0,
  "actors": [...],
  "message": "Successfully spawned all 3 actors."
}
```

**Returns (dry_run=True):**
```json
{
  "success": true,
  "dry_run": true,
  "actor_count": 3,
  "actors": [...],
  "message": "Would spawn 3 actors. Set dry_run=False to execute."
}
```

---

### delete_actor

Remove an actor by name.

**Parameters:**
- `name` (str, required): Name of the actor to delete.

**Validation:** `name` must be a non-empty string.

---

### set_actor_transform

Modify actor position, rotation, or scale.

**Parameters:**
- `name` (str, required): Actor name.
- `location` (list[float], optional): New `[X, Y, Z]` position.
- `rotation` (list[float], optional): New `[Pitch, Yaw, Roll]` in degrees.
- `scale` (list[float], optional): New `[X, Y, Z]` scale.

**Validation:** `name` is validated. Vectors validated as 3-element numeric arrays.

**Note:** Undoable in the Unreal Editor (uses `FScopedTransaction` and `Actor->Modify()`).

---

## Blueprint System

### create_blueprint

Create a new Blueprint class.

**Parameters:**
- `name` (str, required): Blueprint name (must be unique).
- `parent_class` (str, required): Base class, typically `"Actor"`.

**Note:** Undoable (uses `FScopedTransaction`).

---

### add_component_to_blueprint

Add a component to a Blueprint.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `component_type` (str, required): Component class name (e.g., `"StaticMeshComponent"`).
- `component_name` (str, required): Name for the new component.
- `location` (list[float], optional): Relative position. Default `None` → `[0, 0, 0]`.
- `rotation` (list[float], optional): Relative rotation. Default `None` → `[0, 0, 0]`.
- `scale` (list[float], optional): Relative scale. Default `None` → `[1, 1, 1]`.
- `component_properties` (dict, optional): Additional settings.

**Common component types:** `StaticMeshComponent`, `CameraComponent`, `SpringArmComponent`, `PointLightComponent`, `AudioComponent`.

**Note:** Undoable (uses `FScopedTransaction`).

---

### set_static_mesh_properties

Set the mesh asset on a StaticMeshComponent.

**Parameters:**
- `blueprint_name` (str, required): Blueprint containing the component.
- `component_name` (str, required): StaticMeshComponent name.
- `static_mesh` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Asset path for the mesh.

**Available basic meshes:**
- `/Engine/BasicShapes/Cube.Cube`
- `/Engine/BasicShapes/Sphere.Sphere`
- `/Engine/BasicShapes/Cylinder.Cylinder`
- `/Engine/BasicShapes/Plane.Plane`

**Note:** Undoable (uses `FScopedTransaction`).

---

### set_physics_properties

Configure physics simulation on a component.

**Parameters:**
- `blueprint_name` (str, required): Blueprint name.
- `component_name` (str, required): Component to configure.
- `simulate_physics` (bool, default `True`)
- `gravity_enabled` (bool, default `True`)
- `mass` (float, default `1.0`)
- `linear_damping` (float, default `0.01`)
- `angular_damping` (float, default `0.0`)

**Note:** Undoable (uses `FScopedTransaction`).

---

### compile_blueprint

Compile a Blueprint to apply pending changes.

**Parameters:**
- `blueprint_name` (str, required): Blueprint to compile.

---

### read_blueprint_content

Read and analyze the complete content of a Blueprint.

**Parameters:**
- `blueprint_path` (str, required): Blueprint asset path (e.g., `"/Game/Blueprints/BP_Test"`).
- `include_event_graph` (bool, default `True`)
- `include_functions` (bool, default `True`)
- `include_variables` (bool, default `True`)
- `include_components` (bool, default `True`)
- `include_interfaces` (bool, default `True`)

---

### analyze_blueprint_graph

Analyze a specific graph with node and connection details.

**Parameters:**
- `blueprint_path` (str, required): Blueprint asset path.
- `graph_name` (str, default `"EventGraph"`): Graph to analyze.
- `include_node_details` (bool, default `True`)
- `include_pin_connections` (bool, default `True`)
- `trace_execution_flow` (bool, default `True`)

---

### get_blueprint_variable_details

Inspect variable type, default value, and metadata.

**Parameters:**
- `blueprint_path` (str, required): Blueprint asset path.
- `variable_name` (str, optional): Specific variable, or `None` for all.

---

### get_blueprint_function_details

Inspect function parameters, return values, and graph content.

**Parameters:**
- `blueprint_path` (str, required): Blueprint asset path.
- `function_name` (str, optional): Specific function, or `None` for all.
- `include_graph` (bool, default `True`)

---

## Blueprint Graph

### add_node

Add a node to a Blueprint graph. Supports 23+ node types.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `node_type` (str, required): Type of node to add (e.g., `"PrintString"`, `"SetTimerByFunctionName"`).
- `pos_x` (float, default `0`): X position in the graph.
- `pos_y` (float, default `0`): Y position in the graph.
- `message` (str, default `""`): Message for `PrintString` nodes.
- `event_type` (str, default `"BeginPlay"`): Event type for event nodes.
- `variable_name` (str, default `""`): Variable name for get/set nodes.
- `target_function` (str, default `""`): Target function name.
- `target_blueprint` (str, optional): Blueprint for cross-Blueprint calls.
- `function_name` (str, optional): Function name for call nodes.

**Note:** Undoable (uses `FScopedTransaction`).

---

### add_event_node

Add an event node to a Blueprint graph.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `event_name` (str, required): Event name (e.g., `"EventBeginPlay"`, `"EventTick"`).
- `pos_x` (float, default `0`)
- `pos_y` (float, default `0`)

**Note:** Undoable (uses `FScopedTransaction`).

---

### connect_nodes

Connect two pins in a Blueprint graph.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `source_node_id` (str, required): Source node ID.
- `source_pin_name` (str, required): Source pin name.
- `target_node_id` (str, required): Target node ID.
- `target_pin_name` (str, required): Target pin name.
- `function_name` (str, optional): Function graph to operate in.

**Note:** Undoable (uses `FScopedTransaction`).

---

### delete_node

Delete a node from a Blueprint graph.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `node_id` (str, required): Node ID to delete.
- `function_name` (str, optional): Function graph to operate in.

**Note:** Undoable (uses `FScopedTransaction`).

---

### set_node_property

Set a property on a Blueprint node, or perform semantic node editing.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `node_id` (str, required): Target node.
- `property_name` (str, default `""`): Property to set.
- `property_value` (any, optional): New value.
- `function_name` (str, optional): Function context.
- Additional optional parameters: `action`, `pin_type`, `pin_name`, `enum_type`, `new_type`, `target_type`, `target_function`, `target_class`, `event_type`.

**Note:** Undoable (uses `FScopedTransaction`).

---

### create_variable

Create a variable in a Blueprint.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `variable_name` (str, required): Name for the new variable.
- `variable_type` (str, required): Type (e.g., `"Bool"`, `"Float"`, `"String"`, `"Vector"`).
- `default_value` (any, optional): Default value.
- `is_public` (bool, default `False`)
- `tooltip` (str, default `""`)
- `category` (str, default `"Default"`)

**Note:** Undoable (uses `FScopedTransaction`).

---

### set_blueprint_variable_properties

Modify properties of an existing Blueprint variable.

**Parameters:**
- `blueprint_name` (str, required)
- `variable_name` (str, required)
- `var_name` (str, optional): Rename the variable.
- `var_type` (str, optional): Change the type.
- `is_blueprint_readable` (bool, optional)
- `is_blueprint_writable` (bool, optional)
- `is_public` (bool, optional)
- `is_editable_in_instance` (bool, optional)
- `tooltip` (str, optional)
- `category` (str, optional)
- `default_value` (any, optional)
- `expose_on_spawn` (bool, optional)
- `expose_to_cinematics` (bool, optional)
- `slider_range_min` (str, optional)
- `slider_range_max` (str, optional)
- `value_range_min` (str, optional)
- `value_range_max` (str, optional)
- `units` (str, optional)
- `bitmask` (bool, optional)
- `bitmask_enum` (str, optional)
- `replication_enabled` (bool, optional)
- `replication_condition` (int, optional)
- `is_private` (bool, optional)

**Note:** Undoable (uses `FScopedTransaction`).

---

### create_function

Create a new function in a Blueprint.

**Parameters:**
- `blueprint_name` (str, required)
- `function_name` (str, required)
- `return_type` (str, default `"void"`)

**Note:** Undoable (uses `FScopedTransaction`).

---

### add_function_input

Add an input parameter to a function.

**Parameters:**
- `blueprint_name` (str, required)
- `function_name` (str, required)
- `param_name` (str, required)
- `param_type` (str, required)
- `is_array` (bool, default `False`)

**Note:** Undoable (uses `FScopedTransaction`).

---

### add_function_output

Add an output parameter to a function.

**Parameters:**
- `blueprint_name` (str, required)
- `function_name` (str, required)
- `param_name` (str, required)
- `param_type` (str, required)
- `is_array` (bool, default `False`)

**Note:** Undoable (uses `FScopedTransaction`).

---

### delete_function

Delete a function from a Blueprint.

**Parameters:**
- `blueprint_name` (str, required)
- `function_name` (str, required)

**Note:** Undoable (uses `FScopedTransaction`).

---

### rename_function

Rename a function in a Blueprint.

**Parameters:**
- `blueprint_name` (str, required)
- `old_function_name` (str, required)
- `new_function_name` (str, required)

**Note:** Undoable (uses `FScopedTransaction`).

---

## Materials

### get_available_materials

List available materials that can be applied to objects.

**Parameters:**
- `search_path` (str, default `"/Game/"`): Asset path to search.
- `include_engine_materials` (bool, default `True`): Include built-in engine materials.

---

### apply_material_to_actor

Apply a material to an actor.

**Parameters:**
- `actor_name` (str, required): Target actor.
- `material_path` (str, required): Material asset path.
- `material_slot` (int, default `0`): Material index.

**Note:** Undoable (uses `FScopedTransaction`).

---

### apply_material_to_blueprint

Apply a material to a Blueprint component.

**Parameters:**
- `blueprint_name` (str, required): Target Blueprint.
- `component_name` (str, required): Target component.
- `material_path` (str, required): Material asset path.
- `material_slot` (int, default `0`): Material index.

**Note:** Undoable (uses `FScopedTransaction`).

---

### get_actor_material_info

Get material information for an actor's components.

**Parameters:**
- `actor_name` (str, required): Actor name.

**Returns:** Component name, material slots, and material paths.

---

### get_blueprint_material_info

Get material information for a Blueprint component.

**Parameters:**
- `blueprint_name` (str, required): Blueprint name.
- `component_name` (str, required): Component name.

**Returns:** Material slots and paths for the specified component.

---

### set_mesh_material_color

Set a color on a mesh component via material parameter.

**Parameters:**
- `blueprint_name` (str, required): Blueprint containing the component.
- `component_name` (str, required): StaticMeshComponent to color.
- `color` (list[float], required): `[R, G, B, A]` in 0.0–1.0 range.
- `material_path` (str, default `"/Engine/BasicShapes/BasicShapeMaterial"`): Material asset.
- `parameter_name` (str, default `"BaseColor"`): Material parameter name.
- `material_slot` (int, default `0`): Material index.

**Common colors:**

| Name   | Value                              |
|--------|------------------------------------|
| Red    | `[1.0, 0.0, 0.0, 1.0]`            |
| Green  | `[0.0, 1.0, 0.0, 1.0]`            |
| Blue   | `[0.0, 0.0, 1.0, 1.0]`            |
| Yellow | `[1.0, 1.0, 0.0, 1.0]`            |
| Purple | `[1.0, 0.0, 1.0, 1.0]`            |
| White  | `[1.0, 1.0, 1.0, 1.0]`            |

**Note:** Undoable (uses `FScopedTransaction`).

---

## Material Graph

### create_material

Create an empty Material asset.

**Parameters:**
- `name` (str, required): Material asset name.
- `package_path` (str, default `"/Game/Materials/"`): Package folder for the new asset.

**Returns:** `success` and the created Material `path`.

---

### add_material_node

Add a node to a Material graph.

**Parameters:**
- `material_path` (str, required): Material asset path, for example `"/Game/Materials/M_Test"`.
- `node_type` (str, required): Material expression type such as `"Constant"`, `"Multiply"`, `"Add"`, `"VectorParameter"`, or `"TextureSample"`.
- `pos_x` (float, default `0`): Editor graph X position.
- `pos_y` (float, default `0`): Editor graph Y position.
- `node_params` (dict, optional): Node-specific parameters. Supported examples include `{"value": 0.8}` for `Constant`, `{"parameter_name": "Tint", "default_value": [1, 0, 0, 1]}` for `VectorParameter`, and `{"texture": "/Game/Textures/T_Wood"}` for `TextureSample`.

---

### connect_material_nodes

Connect Material expression outputs to expression inputs or Material root properties.

**Parameters:**
- `material_path` (str, required): Material asset path.
- `source_node_id` (str, required): Source expression object name returned by `add_material_node` or `export_material_json`.
- `source_pin_name` (str, default `""`): Source output pin. Empty string uses output index `0`.
- `target_node_id` (str, required): Target expression object name, or `"Material"` for root Material properties.
- `target_pin_name` (str, required): Target expression input or root property such as `"A"`, `"B"`, `"BaseColor"`, or `"Roughness"`.

---

### apply_material_json

Apply a JSON Material graph description by creating nodes first, then connecting them.

**Parameters:**
- `material_path` (str, required): Material asset path.
- `json_data` (str, required): JSON string with `nodes` and `connections` arrays.

**JSON shape:**
```json
{
  "nodes": [
    {"id": "roughness", "type": "Constant", "params": {"value": 0.8}}
  ],
  "connections": [
    {"source_id": "roughness", "source_pin": "", "target_id": "Material", "target_pin": "Roughness"}
  ]
}
```

---

### export_material_json

Export a Material graph to a standardized JSON representation.

**Parameters:**
- `material_path` (str, required): Material asset path.

**Returns:** `json_data` and `raw_data` containing `nodes` and `connections`.

---

## World Building

### create_pyramid

Build a stepped pyramid from cube actors.

**Parameters:**
- `base_size` (int, default `3`): Blocks along base edge. Max 50.
- `block_size` (float, default `100.0`): Block edge in cm. Range 1–10000.
- `location` (list[float], optional): Center position. Default `[0, 0, 0]`.
- `name_prefix` (str, default `"PyramidBlock"`): Actor name prefix.
- `mesh` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Static mesh asset path.
- `dry_run` (bool, default `False`): Preview without spawning.

**Returns:** Batch spawn result with `spawned_count`, `failed_count`, and actor details.

---

### create_wall

Build a straight wall from cube actors.

**Parameters:**
- `length` (int, default `5`): Blocks along the wall. Max 200.
- `height` (int, default `2`): Block layers vertically. Max 100.
- `block_size` (float, default `100.0`): Block edge in cm. Range 1–10000.
- `location` (list[float], optional): Wall origin position.
- `orientation` (str, default `"x"`): Direction — `"x"` or `"y"`.
- `name_prefix` (str, default `"WallBlock"`): Actor name prefix.
- `mesh` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Static mesh asset path.
- `dry_run` (bool, default `False`): Preview without spawning.

**Returns:** Batch spawn result.

---

### create_maze

Generate a solvable maze using recursive backtracking.

**Parameters:**
- `rows` (int, default `8`): Maze height in cells. Max 100.
- `cols` (int, default `8`): Maze width in cells. Max 100.
- `cell_size` (float, default `300.0`): Cell size in cm. Range 10–10000.
- `wall_height` (int, default `3`): Wall height in block layers. Max 50.
- `location` (list[float], optional): Maze center position.

**Validation:** Rejects requests where estimated actor count exceeds 500.

---

### create_tower

Build a tower with architectural style options.

**Parameters:**
- `height` (int, default `10`): Number of vertical levels.
- `base_size` (int, default `4`): Base diameter/width.
- `block_size` (float, default `100.0`): Block size in cm.
- `location` (list[float], optional): Tower base position.
- `name_prefix` (str, default `"TowerBlock"`): Actor name prefix.
- `mesh` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Static mesh asset.
- `tower_style` (str, default `"cylindrical"`): `"cylindrical"`, `"square"`, or `"tapered"`.

---

### create_staircase

Build a stepped staircase.

**Parameters:**
- `steps` (int, default `5`): Number of steps.
- `step_size` (list[float], optional): `[width, depth, height]` per step. Default `[100, 100, 50]`.
- `location` (list[float], optional): Starting position.
- `name_prefix` (str, default `"Stair"`): Actor name prefix.
- `mesh` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Static mesh asset.

---

### create_arch

Build an arch from blocks arranged in a semicircle.

**Parameters:**
- `radius` (float, default `300.0`): Arch radius in cm.
- `segments` (int, default `6`): Number of arch blocks.
- `location` (list[float], optional): Arch center position.
- `name_prefix` (str, default `"ArchBlock"`): Actor name prefix.
- `mesh` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Static mesh asset.

---

### construct_house

Build a house with rooms, windows, and a pitched roof.

**Parameters:**
- `width` (int, default `1200`): House width in cm.
- `depth` (int, default `1000`): House depth in cm.
- `height` (int, default `600`): Wall height in cm.
- `location` (list[float], optional): Center position.
- `name_prefix` (str, default `"House"`): Actor name prefix.
- `mesh` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Static mesh asset.
- `house_style` (str, default `"modern"`): `"modern"`, `"cottage"`, or `"mansion"`.

---

### construct_mansion

Build a mansion with multiple wings and luxury features.

**Parameters:**
- `mansion_scale` (str, default `"large"`): Scale — `"small"`, `"medium"`, `"large"`.
- `location` (list[float], optional): Center position.
- `name_prefix` (str, default `"Mansion"`): Actor name prefix.

---

### create_town

Build a town with buildings, streets, and infrastructure.

**Parameters:**
- `town_size` (str, default `"medium"`): `"small"`, `"medium"`, `"large"`, or `"metropolis"`.
- `building_density` (float, default `0.7`): 0.0–1.0 building packing.
- `location` (list[float], optional): Town center position.
- `name_prefix` (str, default `"Town"`): Actor name prefix.
- `include_infrastructure` (bool, default `True`): Add streets, lights, vehicles.
- `architectural_style` (str, default `"mixed"`): `"modern"`, `"medieval"`, `"suburban"`, `"downtown"`, `"mixed"`, `"futuristic"`.

---

### create_castle_fortress

Build a castle with walls, towers, courtyards, and surrounding village.

**Parameters:**
- `castle_size` (str, default `"large"`): `"small"`, `"medium"`, or `"large"`.
- `location` (list[float], optional): Castle position.
- `name_prefix` (str, default `"Castle"`): Actor name prefix.
- `include_siege_weapons` (bool, default `True`): Add siege equipment.
- `include_village` (bool, default `True`): Add surrounding village.
- `architectural_style` (str, default `"medieval"`): Style theme.

---

### create_suspension_bridge

Build a suspension bridge with towers, deck, cables, and suspenders.

**Parameters:**
- `span_length` (float, default `6000.0`): Bridge span in cm.
- `deck_width` (float, default `800.0`): Deck width in cm.
- `tower_height` (float, default `4000.0`): Tower height in cm.
- `cable_sag_ratio` (float, default `0.12`): Cable sag as fraction of span.
- `module_size` (float, default `200.0`): Block size in cm.
- `location` (list[float], optional): Bridge center position.
- `orientation` (str, default `"x"`): Bridge direction.
- `name_prefix` (str, default `"Bridge"`): Actor name prefix.
- `deck_mesh`, `tower_mesh`, `cable_mesh`, `suspender_mesh` (str): Asset paths.
- `dry_run` (bool, default `False`): Preview without spawning.

---

### create_aqueduct

Build a multi-tier Roman-style aqueduct.

**Parameters:**
- `arches` (int, default `18`): Number of arches.
- `arch_radius` (float, default `600.0`): Arch radius in cm.
- `pier_width` (float, default `200.0`): Pier width in cm.
- `tiers` (int, default `2`): Number of vertical tiers.
- `deck_width` (float, default `600.0`): Deck width in cm.
- `module_size` (float, default `200.0`): Block size in cm.
- `location` (list[float], optional): Aqueduct position.
- `orientation` (str, default `"x"`): Direction.
- `name_prefix` (str, default `"Aqueduct"`): Actor name prefix.
- `arch_mesh`, `pier_mesh`, `deck_mesh` (str): Asset paths.
- `dry_run` (bool, default `False`): Preview without spawning.

---

### spawn_physics_blueprint_actor

Spawn a single actor with physics, color, and a specified mesh.

**Parameters:**
- `name` (str, required): Actor name.
- `mesh_path` (str, default `"/Engine/BasicShapes/Cube.Cube"`): Mesh asset path.
- `location` (list[float], optional): Spawn position.
- `mass` (float, default `1.0`): Mass in kg.
- `simulate_physics` (bool, default `True`)
- `gravity_enabled` (bool, default `True`)
- `color` (list[float], optional): `[R, G, B, A]` color.
- `scale` (list[float], optional): `[X, Y, Z]` scale.

---

## Scene Synchronization & AI

### scene_create

Initialize a new scene for synchronization.

**Parameters:**
- `scene_id` (str, required): Unique scene identifier.
- `name` (str, required): Display name for the scene.

---

### scene_create_navmesh_volume

Create a NavMesh Bounds Volume and sync with DB.

**Parameters:**
- `scene_id` (str, required): Target scene.
- `volume_name` (str, required): Name for the volume.
- `location` (dict, required): `{"x": X, "y": Y, "z": Z}`.
- `extent` (dict, required): `{"x": X, "y": Y, "z": Z}`.

---

### scene_create_patrol_route

Create a patrol route spline actor and sync with DB.

**Parameters:**
- `scene_id` (str, required): Target scene.
- `route_name` (str, required): Name for the route.
- `points` (list[dict], required): List of `{"x": X, "y": Y, "z": Z}` points.
- `closed_loop` (bool, default `True`)

---

### scene_set_ai_behavior

Configure AI behavior properties for an entity.

**Parameters:**
- `scene_id` (str, required): Target scene.
- `entity_id` (str, required): Entity to configure.
- `perception_radius` (float, optional)
- `faction` (str, optional)

---

### scene_sync

Perform a full synchronization between DB and Unreal.

**Parameters:**
- `scene_id` (str, required): Scene to sync.
- `dry_run` (bool, default `False`)

---

## Usage Tips

### Performance

- Use `batch_spawn_actors` or world-building tools instead of calling `spawn_actor` in a loop.
- Use `dry_run=True` on batch tools to preview actor counts before executing.
- Keep total actor counts reasonable (< 1000 actors). `create_maze` rejects requests exceeding 500 estimated actors.

### Undo

- All destructive operations are undoable in the Unreal Editor (Ctrl+Z).
- Batch operations (pyramid, wall, maze) create one transaction per actor.
- A future C++ `batch_spawn_actors` command will enable single-transaction batch undo.

### Coordinate Guidelines

- Place objects at Z > 0 to avoid ground clipping.
- Use large separation distances for multiple structures.
- Unreal uses centimeters (100 = 1 meter).

### Blueprint Workflow

1. Create Blueprint class (`create_blueprint`)
2. Add components (`add_component_to_blueprint`)
3. Set component properties (mesh, physics, materials)
4. Compile Blueprint (`compile_blueprint`)
5. Spawn actors from it (`spawn_blueprint_actor`)

### Naming Conventions

- Use descriptive, unique names for all actors.
- Include prefixes for grouped objects (e.g., `"House1_Wall"`, `"House1_Roof"`).
- Avoid special characters in actor names.
