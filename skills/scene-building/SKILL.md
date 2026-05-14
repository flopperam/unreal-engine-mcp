---
name: scene-building
description: >
  Scene and world building workflow for the Flopperam Unreal Engine MCP. Covers 
  actor management, landscape sculpting, foliage scattering, AI setup, cinematics, 
  and level verification. Use when placing actors, building environments, working 
  with landscapes/foliage, setting up AI characters, or creating Level Sequences.
---

# Scene & World Building Workflow

## Core Pattern: Observe → Execute → Verify

1. **Observe** — `scene_brief` for level overview, `scene_query` for specific actors, `search_assets` for meshes/materials
2. **Execute** — `scene_compose` for batch placement, domain tools for specific systems
3. **Verify** — `scene_query` with spatial filters to confirm, `pie_test_scene` for runtime behavior

## Scene Queries & Placement

### Finding Things
- `scene_brief` — compact level overview (actor counts, classes, bounds)
- `scene_query` — find actors by class/label/tag/folder with spatial filters (`near`, `box`, `frustum`)
- `search_assets` — Content Browser search by name/prefix/class (returns asset paths, NOT actor labels)
- `asset_references` — dependency graphs (what does X reference, what references X)
- `project_context` — engine version, plugins, project settings, maps, content stats

Scene tools work on **actors** (placed instances). `search_assets` works on **assets** (reusable files). Never confuse the two.

### Placing Things
`scene_compose` declaratively spawns/modifies/deletes actors with prefabs + patterns. Prefer it over looping scene_query writes.

### Actor Management
- `actor_inspect` — drill into one actor (components, properties, transform)
- Use `scene_compose` to modify transforms, delete actors, or batch-manage scene objects

## Landscape & Terrain

### Inspection
`landscape_inspect` — height/slope/normal queries, statistics, find flat areas, paint-layer listing.

### Editing
`landscape_edit` handles:
- Lifecycle — create/resize/delete landscapes
- Sculpting — raise, lower, smooth, flatten, erosion
- Semantic features — mountains, valleys, craters, plateaus, terraces
- Paint layers — create and apply landscape material layers
- Heightmap — import/export

Always inspect before editing to understand current terrain state.

## Foliage

- `foliage_inspect` — list types + counts
- `foliage_edit` — create FoliageTypes, scatter (circles/rectangles/by weight layer), place exact transforms, remove by radius or type

## Materials & Shading

1. `material_inspect` — read-only (overview, expressions, connections, parameters, textures, shader stats, parent chain). Run BEFORE `material_edit` on existing materials.
2. `material_edit` — create materials/instances/functions/Parameter Collections, author expression graphs. Auto-retries single-input pin name resolution.

## VFX (Niagara & Chaos)

- `niagara_inspect` → `niagara_edit` for systems/emitters/parameters/renderers
- `niagara_script_edit` for reusable NiagaraScript module graphs
- `chaos_edit` for Geometry Collections (fracture, clustering, damage thresholds)
- `ramp_authoring` for curve assets (UCurveFloat/Vector/LinearColor) — create the curve FIRST, then reference from the consumer

## AI & Behavior Trees

Full setup workflow:
1. `behavior_tree` with `create_blackboard` → create keys (TargetActor, PatrolLocation, etc.)
2. `behavior_tree` with `create_behavior_tree` → create the BT asset
3. `behavior_tree` with `set_blackboard_on_bt` → link Blackboard to BT
4. `behavior_tree` with `build_behavior_tree` → build tree structure (selectors, sequences, tasks, decorators)
5. `behavior_tree` with `create_ai_controller` → create controller referencing the BT
6. `behavior_tree` with `validate_behavior_tree` → validate

Also covers: AI perception, NavMesh, EQS queries, StateTrees, Smart Objects, runtime BT + blackboard inspection in PIE.

### Gameplay Abilities
- `gas_edit` — Gameplay Abilities (tags, cost, cooldown, activation), Effects (duration, modifiers, stacking), Attribute Sets
- `tag_registry_edit` — project-wide Gameplay Tag hierarchy

## Cinematics

`sequencer_edit` creates Level Sequences with:
- Actor bindings
- Transform / animation / audio / camera / event / fade tracks
- Keyframes with interpolation control
- Camera cuts

## Audio

- `metasound_edit` — procedural audio (MetaSound graphs: oscillators, filters, envelopes, mixers, sample players)
- `sound_asset_edit` — SoundCue graphs, SoundClass hierarchy, SoundAttenuation curves

## Procedural Content

`pcg_graph_edit` — PCG graphs with point generators, samplers, filters, density operators, mesh spawners with typed pin wiring.

## Physics

`collision_profile_edit` — collision profiles, physical materials, object type responses.

## Editor Utilities

- `editor_actions` — save, undo, redo, select, focus, build lighting/navigation, compile, PIE start/stop, viewport mode
- `editor_log` — Output Log + Message Log with verbosity filter + time window
- `performance_audit` — triangle counts, actor counts by class, mesh complexity, texture memory, optimization warnings
- `window_capture` — viewport screenshot as PNG

## Runtime Verification

`pie_test_scene` — free-tier PIE harness with:
- Narrow input whitelist (console, wait, spawn_actor, play_sequence, fire_damage)
- Lifecycle + VFX + audio + visual-regression assertions
- Auto-plan mode from task description

Use after any scene change that affects runtime behavior.

## Data Assets

`asset_factory` — single entry for:
- Enums / Structs
- DataTables (CRUD rows, bulk JSON import)
- DataAssets (any subclass)
- Enhanced Input (IA + IMC + key mappings + modifiers + triggers)
- Safe Blueprint deletion (crash-safe path)
