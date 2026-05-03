# Jules Implementation Brief

This brief turns `docs/superpowers/plans/tasks.md` into cloud-agent-friendly implementation slices. The backlog file is the broad feature inventory; this file is the execution guide for Jules branches.

Jules runs on Ubuntu without Unreal Editor. It can implement source changes, Python tests, Rust tests, docs, and static C++ command wiring. It cannot complete live Unreal Editor verification.

## Operating Model

- One Jules task should implement one feature slice.
- Use branch names like `jules/asset-management-tools`, `jules/datatable-tools`, or `jules/umg-widget-tools`.
- Every tool slice should include Python MCP facade, C++ command contract if needed, tests, README/tools-reference updates, and changelog notes.
- If Unreal C++ changes are included, end the task with explicit local Windows UE build/editor verification steps.
- Avoid broad "cover all Unreal features" tasks. They are too large and will create protocol drift.

## Best First Jules Slices

### 1. Content Browser / Asset Management

Goal: give agents a safe way to inspect and organize assets.

Suggested tools:

- `asset_list`
- `asset_search`
- `asset_get_metadata`
- `asset_create_folder`
- `asset_rename`
- `asset_move`
- `asset_duplicate`
- `asset_delete`
- `asset_fix_redirectors`

Implementation notes:

- Start with read-only listing/search/metadata before destructive operations.
- Require explicit `confirm=True` for delete, bulk move, and redirector fixup.
- Add Python mapping tests for every MCP tool.
- Add contract tests for path validation and destructive-operation safeguards.

Local Windows verification required:

- Build the Unreal plugin.
- Launch the canonical project.
- Create a temporary folder and test list/search/rename/move/delete/fixup on disposable assets.

### 2. Data Tables / Data Assets

Goal: let LLMs create gameplay data from CSV/JSON and inspect it again.

Suggested tools:

- `create_data_table_from_csv`
- `create_data_table_from_json`
- `export_data_table_json`
- `get_data_table_rows`
- `upsert_data_table_row`
- `delete_data_table_row`
- `create_primary_data_asset`

Implementation notes:

- Keep schemas explicit. Do not infer row structs silently without documenting the fallback.
- Add tests for JSON payload expansion and invalid row data.
- Document supported field types and unsupported cases.

Local Windows verification required:

- Create a test row struct and DataTable asset.
- Import sample CSV/JSON.
- Re-export and compare rows.

### 3. Project / Editor Control

Goal: add operational editor tools that make later autonomous work safer.

Suggested tools:

- `get_project_settings`
- `set_project_setting`
- `get_world_settings`
- `set_world_setting`
- `list_dirty_assets`
- `save_asset`
- `save_all_assets`
- `get_editor_log_tail`
- `start_pie`
- `stop_pie`

Implementation notes:

- Prefer read-only tools first.
- Validate config section/key names with allowlists before adding write tools.
- Keep PIE commands separate from asset and settings commands.

Local Windows verification required:

- Confirm settings read/write on disposable project settings.
- Confirm dirty asset listing and save behavior.
- Confirm PIE start/stop does not leave the editor in a bad state.

### 4. Testing / Validation

Goal: make future agent-generated work easier to verify.

Suggested tools:

- `compile_all_blueprints`
- `validate_asset_references`
- `validate_missing_materials`
- `validate_missing_meshes`
- `run_map_check`
- `run_automation_tests`
- `get_validation_report`

Implementation notes:

- Prefer structured reports over log text dumps.
- Tests should check response envelope, parameter validation, and error reporting.
- Do not hide failures behind success responses.

Local Windows verification required:

- Run against a project with a known broken Blueprint/reference.
- Confirm failures are reported with actionable asset paths.

### 5. UMG / UI

Goal: let LLMs create minimal playable UI such as HUD, main menu, pause menu, score text, and health bars.

Suggested tools:

- `create_widget_blueprint`
- `add_widget_node`
- `set_widget_layout`
- `set_widget_style`
- `bind_widget_event`
- `add_widget_to_viewport_blueprint_node`
- `apply_widget_json`
- `export_widget_json`

Implementation notes:

- Model the widget tree as JSON, similar to Blueprint and Material graph JSON tools.
- Start with Canvas Panel, Text Block, Button, Image, Progress Bar, Vertical Box, Horizontal Box, and Overlay.
- Keep event binding minimal at first: button click to Blueprint event/function.

Local Windows verification required:

- Create a Widget Blueprint from JSON.
- Compile it.
- Add it to viewport from a Blueprint.
- Confirm it appears in PIE.

## Good Later Slices

- Material Instances and parameter editing.
- Enhanced Input assets and mapping contexts.
- Behavior Tree and Blackboard asset creation.
- Static Mesh import/export and basic collision settings.
- Post Process Volume and Camera look controls.
- Lighting parameter controls beyond actor placement.
- Scene validation commands for layout, navigation, and missing assets.

## Poor First Jules Slices

These areas are valuable but should not be the first cloud-agent branches because they need heavy Unreal runtime/editor validation:

- Landscape sculpting and heightmap workflows.
- Foliage painting and procedural foliage simulation.
- Niagara graph authoring.
- Animation Blueprint, IK Rig, Control Rig, and retargeting.
- Sequencer and Movie Render Queue.
- Multiplayer runtime sessions, dedicated servers, and network profiling.
- Gameplay Ability System.
- Platform packaging, Android/iOS/XR, and shipping builds.

## Done Criteria For A Jules Branch

- Tool names and request fields are documented.
- Python tool mapping tests pass.
- Contract tests cover validation and response shape where applicable.
- Docs and changelog are updated.
- The final note clearly separates what Jules verified from what still needs local Windows Unreal verification.
