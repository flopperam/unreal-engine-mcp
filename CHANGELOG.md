# Changelog

All notable changes in this fork, relative to the upstream [flopperam/unreal-engine-mcp](https://github.com/flopperam/unreal-engine-mcp), are documented in this file.

---

## [2026-05-03] - Jules cloud agent onboarding

### Added

- Added a root `AGENTS.md` with repository operating rules for local agents and Jules cloud tasks.
- Added `scripts/jules-setup.sh` for Jules Initial Setup on Ubuntu VMs, covering Python dependency setup, tool mapping/doc consistency smoke tests, and Rust dependency fetch.
- Added `docs/superpowers/plans/jules-implementation-brief.md` to convert the broad superpowers backlog into Jules-friendly branch slices.
- Updated `.gitignore` so `AGENTS.md`, `docs/superpowers/plans/tasks.md`, and the Jules implementation brief can be committed and read by cloud agents after cloning the repository.

### Notes

- Jules cannot launch the local Windows Unreal Editor or use `C:\...` Unreal paths, so UE build/editor/live MCP verification remains a local Windows responsibility.

---

## [2026-05-03] - PR #6/#8 functional verification fixes

### Fixed

- Fixed Material graph JSON tool wiring from PR #8:
  - added the missing `create_material` Python MCP tool for the C++ `create_material` command
  - aligned Python Material graph tools on the C++ `material_path`, `source_node_id`, `source_pin_name`, `target_node_id`, and `target_pin_name` contract
  - fixed `EpicUnrealMCPBridge.h` after a duplicate `UEpicUnrealMCPBridge` class declaration was introduced locally while wiring Material graph commands
- Implemented real basic Material graph connections in C++ instead of returning success without linking pins. Connections now support expression inputs and Material root pins such as `BaseColor` and `Roughness`.
- Added Material graph export of connection data in addition to node data.

### Added

- Added unit regression coverage for `apply_material_json` parameter mapping and `create_material` package path handling.
- Added Material graph JSON tool documentation to `README.md` and `Guides/tools-reference.md`.

### Verification

- Ran `python -m pytest tests/unit/test_tool_registration_and_mapping.py -v` before the fix; it failed because `create_material` was routable in C++ but unreachable from Python.
- Ran `python -m pytest tests/unit/test_tool_registration_and_mapping.py -v`; 41 passed.
- Ran `python -m pytest tests/unit/test_docs_consistency.py -v`; 6 passed.
- Ran `python -m pytest tests/unit tests/contract -v`; 305 passed.
- Built `FlopperamUnrealMCPEditor Win64 Development` with UE 5.7; build succeeded. Existing warnings remain for the Visual Studio compiler preference, plugin dependency declarations, and deprecated `NetUpdateFrequency`/`ClassDefaultObject` access.
- Launched the canonical `FlopperamUnrealMCP/FlopperamUnrealMCP.uproject`; the MCP bridge listened on `127.0.0.1:55557`, and logs showed `DynamicBandwidthManager Initialized` and `ServerMeshManager Initialized`.
- Ran a live MCP bridge smoke test for PR #8:
  - `create_material`, `apply_material_json`, and `export_material_json` created `/Game/Materials/M_PR8_Verify_*` and exported one node plus one `Roughness` connection.
  - `create_blueprint`, `apply_blueprint_json`, and `export_blueprint_json` created `/Game/Blueprints/BP_PR8_Verify_*` and exported graph nodes after JSON injection.

---

## [2026-05-01] - Procedural mesh visibility fix

### Fixed

- Fixed Unreal C++ procedural mesh binary parsing to read Rust float32 positions, normals, and UVs explicitly instead of copying into `FVector`/`FVector2D` storage directly. On UE5 large-world-coordinate builds, the previous direct copy could corrupt mesh coordinates while still returning a successful spawn response.
- Made spawned procedural meshes visible in the editor by registering the dynamic mesh component as an instance component, applying a default surface material when no material is provided, selecting the actor, and framing the editor viewport by default.

### Added

- Added optional `/procedural/create-mesh` transform controls: `location`, `rotation`, `scale`, and `focus_viewport`.
- Enlarged the 10K terrain demo to a visibly large terrain and documented the visibility smoke test in the scene-sync operations runbook.

---

## [2026-04-29] - Semantic layout graph draft visualization

### Added

- Added relation-aware layout denormalization for `curtain_wall` and `bridge` entities:
  - explicit `from` / `to` spans still work
  - `connected_by`, `connects`, `spans`, `spans_between`, and `attached_to` relations can derive spans from endpoint entity locations
  - diagonal spans now produce yaw-aligned scene objects
- Added wall and bridge expansion with `segments` or `segment_length`, allowing a semantic wall node to generate many reviewable blockout pieces.
- Added optional wall `crenellations` expansion for richer castle blockouts while preserving the original source entity through semantic metadata and tags.
- Added draft visualization metadata to generated objects, including `layout_kind:*`, `layout_entity:*`, and per-kind draft colors.
- Added Python MCP tools:
  - `scene_create_layout` for bulk creating Semantic Layout Graph nodes and edges
  - `scene_show_draft_proxy` for previewing a layout in Unreal as kind-grouped HISM draft proxies
- Documented the layout graph, preview, draft proxy, and denormalization tool flow in `docs/scene-sync/07_mcp_tool_api_spec.md`.

### Verification

- Added Rust unit coverage for relation-derived diagonal walls and wall expansion into segments plus crenellations.

---

## [2026-04-29] - Castle generation E2E reliability

### Fixed

- Fixed `scene-syncd` Unreal bridge calls to retry transient Windows socket aborts and close each request socket instead of reusing stale bridge connections.
- Fixed scene delta apply bookkeeping so partial `apply_scene_delta` failures no longer mark every create as synced.
- Reduced scene delta create chunks to one actor per bridge command with retry, avoiding large-response bridge aborts during castle generation.
- Fixed the dev-stack launcher to avoid starting a stale `scene-syncd.exe` when Rust sources are newer than the built binary.
- Hardened castle E2E verification by bulk-checking actor `mcp_id` tags from `get_actors_in_level` instead of issuing one bridge lookup per actor.
- Allowed non-destructive `apply_safe` sync to proceed with an empty actual-state snapshot when Unreal actor listing is temporarily unavailable; deletes still abort without a valid Unreal snapshot.
- Switched create-only sync application to the lighter `spawn_actor` bridge command instead of `apply_scene_delta`, reducing castle generation timeouts.

### Verification

- Ran `cargo test unreal::client --lib` in `rust/scene-syncd`.
- Ran `cargo test sync::applier --lib` in `rust/scene-syncd`.
- Ran `uv run pytest tests/e2e/test_castle_generation.py --skip-unreal -v` in `Python`.
- Rebuilt and restarted local `scene-syncd`; `GET /health` returned `success: true`.
- Removed stale `castle_*` Unreal actors left by earlier failed runs using `delete_actor_by_mcp_id`.
- Ran full castle E2E successfully once while Unreal MCP Bridge was listening; after later editor/bridge shutdowns, the same test correctly skipped because `127.0.0.1:55557` was no longer listening.

---

## [2026-04-26] - Scene-sync Phase 4/5 hardening

### Added

- Added `scripts/verify_phase5.py` integration test: creates an actor through DB desired state, updates its transform in the DB, applies sync, then verifies via `find_actor_by_mcp_id` and confirms re-plan is a no-op.
- Added `scene_snapshot_create` and `scene_snapshot_restore` via Rust `/snapshots/create` and `/snapshots/restore`; restore changes DB desired state only and requires a later `scene_sync`.
- Added DB desired-state generators `scene_create_wall` and `scene_create_pyramid` that bulk-upsert generated actors without touching Unreal.
- Added `FlopperamUnrealMCP 5.7/DEPRECATED.md` to document that the 5.7 project directory contains no plugin source and lacks `mcp_id` bridge commands required for scene sync.

### Fixed

- Fixed the Unreal C++ automation test helper linkage by restoring the `MakeArrayValue(std::initializer_list<TSharedPtr<FJsonValue>>)` overload used by MCP ID editor tests.
- Fixed `desired_hash` in `scene-syncd` to only include fields that the sync applier can actually apply (`actor_type` and `transform`).
  - `asset_ref`, `visual`, `physics`, and tags are intentionally excluded until their bridge commands are implemented.
  - Previously, changing tags or `asset_ref` produced an `UpdateVisual` operation that the applier skipped with "visual updates not yet implemented", leaving the DB `sync_status` permanently out of alignment with Unreal reality.
- Fixed `scene-syncd` object sync bookkeeping so `mark_object_synced`, tombstone marking, and delete-applied marking update SurrealDB records by typed record key instead of silently missing IDs that contain `scene:mcp_id`.
- Fixed `scene_snapshot` schema to preserve nested `groups` and `objects` array elements under SurrealDB schemafull mode.
- Fixed `plan_sync` response in `scene-syncd` to include Unreal-unreachable warnings in the `warnings` array, making it visible to clients that the plan was generated against an empty actual state.
- Fixed Python drift-detection test to include `scene_sync` in the `skip_tools` set, since it sends commands through the Rust HTTP API rather than directly to the Unreal C++ bridge.
- Fixed duplicate `mcp_id` safety in `scene-syncd` planner: if the same `mcp_id` is found on multiple actual Unreal actors, the planner downgrades any `Delete` operation for that `mcp_id` to `Conflict`, preventing accidental multi-actor deletion.

### Changed

- Updated `AGENTS.md` repository-specific guidance:
  - Documented that `scene_sync` (Phase 4 apply) is now implemented and wired into API routes and Python facade.
  - Added explicit deprecation warning for `FlopperamUnrealMCP 5.7/`.

### Verification

- Ran UE 5.7 editor build for `FlopperamUnrealMCP 5.7 - 3/FlopperamUnrealMCP.uproject`; build succeeded after the automation helper linkage fix.
- Ran `python -m pytest tests/unit/test_tool_registration_and_mapping.py -v` in `Python`; 35 passed.
- Ran `cargo test` in `rust/scene-syncd`; 6 passed. Existing unused-code warnings remain.
- Ran `python scripts/verify_phase5.py` with Unreal Editor, SurrealDB, and `scene-syncd` running; transform update applied and re-plan returned `noop: 1`.
- Verified Phase 7/8 through Python facade against local SurrealDB + `scene-syncd`: `scene_create_wall` upserted 3 objects, `scene_create_pyramid` upserted 5 objects, snapshot captured 8 objects, restore tombstoned 1 extra object and returned active objects to 8.

---

## [2026-04-26] - scenectl CLI MVP

### Added

- Added `scripts/scenectl.py`, a thin CLI over `scene-syncd` for `doctor`, local `start`/`stop`, scene creation, object listing, DB tag add/remove, safe tombstone dry-runs, sync plan, and guarded sync apply.
- Added root `scenectl.cmd` so Windows CMD can run `scenectl ...` from the repository root.
- Added an interactive `scenectl` shell when no arguments are provided, with color output, slash commands (`/help`, `/doctor`, `/object ...`, `/exit`), and Windows candidate display when typing `/` or pressing `Tab`.
- Added project-local OpenCode slash commands under `.opencode/commands/`: `/scenectl`, `/scene-doctor`, `/scene-list`, `/scene-plan`, `/scene-apply`, and `/scene-delete-dry-run`.
- Added `docs/scene-sync/13_scenectl_cli.md` with usage, safety rules, and current limits.
- Added unit coverage for CLI object filtering and upsert payload construction.

### Fixed

- Fixed scene object tag persistence in `scene-syncd` by adding `tags.*` schema typing and explicitly writing tags after object upserts.

### Verification

- Ran `python scripts/scenectl.py --help`.
- Ran `python -m pytest tests/unit/test_scenectl.py -v` in `Python`.
- Ran `cargo build` in `rust/scene-syncd`; existing unused-code warnings remain.
- Ran `python scripts/scenectl.py doctor` successfully against SurrealDB, `scene-syncd`, and Unreal.
- Ran `cmd /c "(echo /help & echo /exit) | scenectl"` and `cmd /c "(echo /doctor & echo /exit) | scenectl"` to verify the interactive shell.
- Tagged the `castle_crown_064013` scene objects by group, then verified `python scripts/scenectl.py object list --scene castle_crown_064013 --tag white_castle_crown` returned 22 objects.
- Verified delete and apply safety guards: `object delete --dry-run` listed targets without writing, and `apply` without `--yes` refused to run.

---

## [2026-04-26] - Scene sync craft-flow regression fix

### Fixed

- Fixed `scene-syncd` object upserts after initial creation by normalizing omitted or JSON `null` object fields (`asset_ref`, `visual`, `physics`, `metadata`) to `{}` before writing to SurrealDB schemafull `object` fields.

### Verification

- Ran `cargo test object_or_empty` in `rust/scene-syncd`.
- Ran `cargo build` in `rust/scene-syncd`; existing unused-code warnings remain.
- Re-ran a DB-driven craft test through SurrealDB and `scene-syncd`: bulk-created 12 Unreal actors for `craft_lab_063003`, then updated the `forge_core` transform through DB state and verified Unreal reported location `[800, 0, 230]` via `find_actor_by_mcp_id`.

---

## [2026-04-25] - OpenCode MCP configuration fix

### Fixed

- Updated the OpenCode MCP configuration sample from the legacy `mcpServers` shape to the current top-level `mcp` schema so OpenCode 1.14.x accepts the config.
- Added `.opencode/opencode.jsonc` as a project-local OpenCode config that starts the Unreal MCP server through the repository Python virtual environment.

### Verification

- Ran `opencode debug config` successfully against `C:\Users\arat2\.config\opencode\opencode.jsonc`.
- Ran `opencode mcp list` successfully; `unreal-engine-mcp` connected with 57 tools.
- Verified the direct `.venv` Python command imports `unreal_mcp_server_advanced` successfully.

---

## [2026-04-25] - Phase 4 scene-syncd verification

### Fixed

- Fixed `scene-syncd` Unreal bridge framing to use newline-delimited JSON, matching the current Unreal MCP plugin and Python client behavior.
- Fixed SurrealDB persistence for Phase 4 create sync by aligning timestamp serialization with SurrealDB `datetime` fields, avoiding serialized string timestamps in schemafull tables.
- Fixed `scene-syncd` record creation for scenes, scene objects, sync runs, and operation logs so SurrealDB record IDs are not serialized as ordinary string fields.
- Updated the local SurrealDB schema definitions used by `scene-syncd` to match the current Rust domain model for string scene/object references and nested transform fields.

### Added

- Added `scripts/verify_phase4.py` to verify the Phase 4 create-only flow end to end: create desired state through `scene-syncd`, apply sync, then confirm the actor exists in Unreal.

### Verification

- Installed SurrealDB v2.4.0 locally under `tools/surrealdb/surreal.exe`.
- Started SurrealDB on `127.0.0.1:8000` and `scene-syncd` on `127.0.0.1:8787`.
- Ran `cargo build` in `rust/scene-syncd` successfully. Existing unused-code warnings remain.
- Ran `python scripts/verify_phase4.py` successfully; it created `Phase4VerifyCube_20260425191854` in Unreal through `/sync/apply`.

### Notes

- The currently running Unreal plugin responds `Unknown command` for `find_actor_by_mcp_id`, so the active editor binary does not appear to include the repository's newer mcp_id command handlers. Create-only actor generation is verified, but mcp_id-based post-apply reconciliation requires launching a rebuilt plugin binary with those handlers.

---

## [2026-04-25] - Unreal project cleanup and shutdown crash fix

### Fixed

- Fixed an Unreal Editor shutdown crash in `UEpicUnrealMCPBridge::StopServer()` caused by owning `FSocket` instances with `TSharedPtr` while also destroying them through `ISocketSubsystem::DestroySocket()`. The bridge and server runnable now keep socket ownership explicit and release sockets through the Unreal socket subsystem only.
- Fixed UE 5.7 build errors in undo transaction setup by passing `FText` labels to `FScopedTransaction`, and replaced the unavailable `FIPv4Address::Loopback` reference with an explicitly parsed loopback address.
- Removed the project startup action that tried to import `StarterContent.upack`; UE 5.7 installs in this environment do not include that feature pack, and this MCP project does not require Starter Content.

### Changed

- Consolidated the local Unreal project copies back to the canonical `FlopperamUnrealMCP/` tree. The duplicate `FlopperamUnrealMCP 5.7/` and `FlopperamUnrealMCP 5.7 - 2/` project copies were local untracked copies and should not be used as source trees.

### Verification

- Verified the crash reports pointed to `EpicUnrealMCPBridge::StopServer()` during `EditorExit` from the duplicate `FlopperamUnrealMCP 5.7 - 2` project path.
- Built `FlopperamUnrealMCPEditor Win64 Development` with UE 5.7 successfully.
- Launched the canonical project with UE 5.7, confirmed the MCP bridge initialized, then closed the editor process without a new crash report.

---

## [2026-04-24] - Safety, batching, and undo support

### Added

#### New MCP tools

- **`batch_spawn_actors`** (`Python/server/actor_tools.py`): Spawn multiple actors in a single call with per-actor validation, a configurable batch limit (default 500), and a `dry_run` mode that returns the planned actor list without executing. (`Python/server/actor_tools.py`)
- **`get_blueprint_material_info`** (`Python/server/material_tools.py`): Proper `@mcp.tool()`-decorated function that sends `get_blueprint_material_info` to Unreal, replacing the bare alias `get_blueprint_material_info = get_actor_material_info` that was not registered with FastMCP. A backward-compatible alias `_get_blueprint_material_info_alias` is kept for internal use.

#### Input validation layer

- **`Python/server/validation.py`**: New module providing reusable validation helpers:
  - `validate_vector3()`, `validate_color()`, `validate_string()`, `validate_float()`, `validate_int()`, `validate_positive_int()`, `validate_nonneg_int()`, `validate_unreal_path()`
  - `ValidationError` exception class with `field` and `message` attributes.
  - `make_validation_error_response()` and `make_validation_error_response_from_exception()` for uniform error responses.
  - Constants: `MAX_ACTORS_PER_BATCH = 500`, `MAX_WORLD_EXTENT = 1000000.0`.
- Validation applied to:
  - `actor_tools.py`: `find_actors_by_name`, `delete_actor`, `spawn_actor`, `set_actor_transform`
  - `world_building_tools.py`: `create_pyramid`, `create_wall`, `create_maze`
- `create_maze` now rejects requests where the estimated actor count exceeds `MAX_ACTORS_PER_BATCH` before querying Unreal.

#### Batch spawning for world-building tools

- `create_pyramid` and `create_wall` now pre-compute all actor specifications and delegate to `batch_spawn_actors()` instead of spawning actors one at a time via `safe_spawn_actor()`. Both accept a `dry_run` parameter.
- This reduces per-actor validation overhead and centralizes error handling, though each actor still uses a separate TCP command (true C++ batch command not yet implemented).

#### Undo support in C++ plugin

- Added `FScopedTransaction` wrappers to 21 C++ command handlers across three files. This enables Undo in the Unreal Editor for all destructive Blueprint, graph, and editor operations:
  - `EpicUnrealMCPEditorCommands.cpp`: `HandleSpawnActor`, `HandleDeleteActor`, `HandleSetActorTransform`
  - `EpicUnrealMCPBlueprintCommands.cpp`: `HandleCreateBlueprint`, `HandleAddComponentToBlueprint`, `HandleSetPhysicsProperties`, `HandleSetStaticMeshProperties`, `HandleSetMeshMaterialColor`, `HandleSpawnBlueprintActor`, `HandleApplyMaterialToActor`, `HandleApplyMaterialToBlueprint`
  - `EpicUnrealMCPBlueprintGraphCommands.cpp`: `HandleAddBlueprintNode`, `HandleConnectNodes`, `HandleCreateVariable`, `HandleSetVariableProperties`, `HandleAddEventNode`, `HandleDeleteNode`, `HandleSetNodeProperty`, `HandleCreateFunction`, `HandleAddFunctionInput`, `HandleAddFunctionOutput`, `HandleDeleteFunction`, `HandleRenameFunction`
- Added `Actor->Modify()` call in `HandleSetActorTransform` before modifying the transform, so the transaction records the previous state correctly.

#### Tests — Python/C++ command mapping

- Added `TestPythonToCppCommandMapping` class in `Python/tests/unit/test_tool_registration_and_mapping.py` with four tests:
  - `test_python_commands_are_handled_in_cpp`: every command that Python sends to Unreal has a matching C++ dispatcher route.
  - `test_cpp_commands_are_used_by_python`: every C++ command (except whitelisted entries like `ping`) is invoked by at least one Python tool.
  - `test_each_mcp_tool_sends_exactly_one_cpp_command`: structural check that each MCP tool (except known orchestrators) maps to a single C++ command.
  - `test_tool_name_to_command_mapping_is_complete`: verifies a hardcoded 31-entry mapping dict covers all registered tools.
- Added `get_blueprint_material_info` to the existing `TestToolCommandMapping` parameterized test entries.

### Changed

#### README

- Updated tool count from ~38 to 46.
- Added `batch_spawn_actors`, `add_event_node`, `get_actor_material_info`, `get_blueprint_material_info` to the tool table.
- Changed Python version requirement from "3.12+" to "3.10+ (3.12 recommended; 3.10–3.13 supported)" to match `pyproject.toml`.

#### `unreal_mcp_server_advanced.py`

- Re-exports `batch_spawn_actors` from `server.actor_tools`.

### Migration notes

- The bare alias `get_blueprint_material_info = get_actor_material_info` in `material_tools.py` has been replaced with a proper `@mcp.tool()` function. The old alias still works as `_get_blueprint_material_info_alias` for internal use, but MCP clients will now see `get_blueprint_material_info` as a registered tool with its own schema.
- `create_pyramid` and `create_wall` now return batch-style result objects (with `spawned_count`, `failed_count`, `actors`, and optionally `failed` keys) instead of the previous `{"success": True, "actors": [...]}` format. Callers that depend on the exact response shape should be updated.

---

## [2026-04-24] - Fork compliance update

- Added MIT license text as `LICENSE`.
- Added explicit credit to the original repository in `README.md`.
- Added a clear statement that this repository is an unofficial fork and not the official Flopperam project.
- Added a clear statement that this repository is separate from the paid Flopperam Agent product.
- Added guidance for a non-confusing fork display name: "Unreal Engine MCP Community Fork (Unofficial)".

---

## Known differences from upstream (2026-04-24)

This section summarizes all structural and behavioral differences between this fork and the upstream `flopperam/unreal-engine-mcp` repository as of this date.

### Python server

| Area | Upstream | This fork |
|------|----------|-----------|
| `material_tools.py` | `get_blueprint_material_info` is a bare alias for `get_actor_material_info`, not registered as an MCP tool | `get_blueprint_material_info` is a proper `@mcp.tool()` function that sends `get_blueprint_material_info` to Unreal |
| `actor_tools.py` | No `batch_spawn_actors` tool; no input validation on tool functions | `batch_spawn_actors` added with validation, dry_run, and batch limit; validation on `find_actors_by_name`, `delete_actor`, `spawn_actor`, `set_actor_transform` |
| `world_building_tools.py` | `create_pyramid` and `create_wall` spawn one actor at a time via `safe_spawn_actor`; `create_maze` has no actor limit guard | Both delegate to `batch_spawn_actors()` with pre-computed specs and `dry_run` parameter; `create_maze` rejects requests exceeding `MAX_ACTORS_PER_BATCH=500` |
| `validation.py` | Does not exist | New module with vector/color/string/float/int/path validators, `ValidationError`, batch limits |
| Test mapping | `TestToolCommandMapping` has 25 entries; no `TestPythonToCppCommandMapping` | 26 entries (adds `get_blueprint_material_info`); new `TestPythonToCppCommandMapping` class with 4 drift-detection tests |

### C++ plugin

| Area | Upstream | This fork |
|------|----------|-----------|
| `EpicUnrealMCPEditorCommands.cpp` | No `FScopedTransaction` on any handler | `FScopedTransaction` on `HandleSpawnActor`, `HandleDeleteActor`, `HandleSetActorTransform`; `Actor->Modify()` added before transform changes |
| `EpicUnrealMCPBlueprintCommands.cpp` | No `FScopedTransaction` | `FScopedTransaction` on 8 handlers (create blueprint, add component, set physics/mesh/material properties, spawn blueprint actor, apply material to actor/blueprint) |
| `EpicUnrealMCPBlueprintGraphCommands.cpp` | No `FScopedTransaction` | `FScopedTransaction` on 9 handlers (add node, connect nodes, create/set variable, add event node, delete node, set node property, create/rename/delete function, add function input/output) |

### Documentation

| Area | Upstream | This fork |
|------|----------|-----------|
| `README.md` | Tool count ~38; Python 3.12+; no fork notice | Tool count 46; Python 3.10+ (3.12 recommended); fork notice at top |
| `CHANGELOG.md` | Does not exist or is empty | Full fork changelog and diff table |
| `AGENTS.md` | Does not exist or is upstream version | Fork-specific with repository guidance, known risks, required checks |

### Not yet changed (planned)

The following improvements are identified but not yet implemented:

- **C++ `batch_spawn_actors` command**: Currently `batch_spawn_actors` sends individual `spawn_actor` commands over separate TCP connections. A single C++ handler accepting an actor array would eliminate per-actor connection overhead.
- **Persistent TCP connection**: `send_command()` still opens and closes a TCP socket per request. Connection reuse or pooling would reduce latency.
- **`safe_spawn_actor` N+1 name-check problem**: Each actor spawn calls `find_actors_by_name` first. Batch spawn should pre-fetch existing names once.
- **Response envelope standardization**: Responses do not yet follow a uniform `{success, data, error, warnings, duration_ms, command_id}` schema.
- **Authentication/authorization**: No auth token, no allowlist for destructive commands, defaults bind to 127.0.0.1 but does not warn on 0.0.0.0.
- **Remaining world-building tools migration**: `create_tower`, `create_staircase`, `create_town`, `create_castle_fortress`, `create_suspension_bridge`, `create_aqueduct`, `create_arch`, `construct_house`, `construct_mansion` still spawn one actor at a time.
- **Structured logging**: No JSONL audit log; no command-level duration tracking.
- **Idempotency keys**: No retry-safety mechanism for write commands.
- **Blueprint compile results**: `set_node_property`, `connect_nodes`, etc. do not return compile status.
- **C++ command registry / `list_capabilities`**: No dynamic capability query or version negotiation.
- **Pydantic input models**: Tools use raw `Dict[str, Any]` instead of typed models.
