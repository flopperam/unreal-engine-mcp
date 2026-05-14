---
name: blueprint-authoring
description: >
  Blueprint authoring workflow for the Flopperam Unreal Engine MCP. Covers the 
  full create, author, commit, verify cycle using the narrow bp_* tool surface. 
  Use when creating or editing Blueprints, adding variables/components/events/functions, 
  wiring graphs, or debugging compile errors.
---

# Blueprint Authoring Workflow

## The Blueprint Authoring Cycle

```
orient (bp_brief / bp_inspect) → probe (bp_dry_run) → create (bp_create) → author (narrow writes) → commit (bp_commit) → verify (bp_inspect / pie_test_bp)
```

Skip steps for trivial edits — a single-variable tweak on an already-read BP goes straight from `bp_variable` to `bp_commit`.

## Step 1: Orient (Read Before Write)

At least one read-class tool MUST fire before any write. This satisfies the write guard.

- `bp_brief(blueprint_path)` — compact one-call overview (parent, components, variables, graphs, dispatchers). Use FIRST.
- `bp_inspect({queries: [...]})` — 21 targeted ops: `overview`, `get_variables`, `get_functions`, `get_nodes`, `get_connections`, `export_spec`, `health_check`, etc. Batched — every op in one call shares loaded BP cache.
- `bp_export(blueprint_path)` — full GraphSpec JSON dump for cloning or diffing.
- `search_assets` — find unknown Blueprint paths by name/prefix.

## Step 2: Probe (Optional, Recommended for Complex Work)

- `bp_dry_run(op=check_intent, intent=[...])` — fast static validation. Catches bogus class paths, wrong K2_ function names, unsupported variable types BEFORE an apply cycle.
- `bp_dry_run(op=check_spec, blueprint_path, spec_json)` — runs the composer's real dry-run (non-destructive, snapshot + rollback). Returns blockers + warnings.

## Step 3: Create Asset

- `bp_create(op=from_parent, blueprint_path, parent_class, components?, variables?, interfaces?)` — create Actor/Pawn/Character/Component/Object BP with inline bootstrap.
- `bp_create(op=from_actor, actor_label, package_path, name)` — bake a placed level actor into a new BP.
- `bp_create(op=interface|enum|struct)` — create BPIs / Enums / Structs.

## Step 4: Author (Narrow Writes)

All narrow writes defer compile — stack many, then commit once.

### Variables
`bp_variable({blueprint_path, variables: [...]})` — batched CRUD.

Type vocabulary: `bool`, `int`, `int64`, `float`, `double`, `string`, `name`, `text`, `byte`, `Vector`, `Rotator`, `Transform`, `Color`, `LinearColor`, object refs (`/Script/Engine.Actor`), BP class refs, enums, containers (`Array<T>`, `Set<T>`, `Map<K,V>`).

Field gotchas:
- `read_only` (NOT `blueprint_read_only` — extra underscore is silently ignored)
- `default_value` or `default` (NOT `value` — silently ignored)
- `category` only (NOT `cat` or `display_category`)

### Components
`bp_component({blueprint_path, components: [...]})` — batched CRUD with inline property writes (collision, transforms, mesh refs, attach sockets).

### Graphs
`bp_graph({blueprint_path, graphs: [...]})` — function/macro/event graph CRUD. Auto-creates FunctionEntry + FunctionResult nodes — reference via `$entry.<pin>` / `$result.<param>`.

### Nodes
`bp_nodes({blueprint_path, graph, nodes: [...]})` — batched add/remove on ONE graph.

~40 node types: `event`, `custom_event`, `function_call`, `variable_get`, `variable_set`, `branch`, `sequence`, `cast`, `make_struct`, `break_struct`, `spawn_actor`, `input_action`, `timeline`, `math`, `comparison`, `select`, `for_each_loop`, `switch_*`, `make_array`, `format_text`, `call_dispatcher`, `bind_dispatcher`.

Declare `stable_id` for each node, reference with `$stable_id` from bp_wire.

### Wiring
`bp_wire({blueprint_path, graph, edges: [...], defaults: [...], pin_ops: [...]})` — batched exec + data edges + pin defaults + pin ops.

**ALWAYS include exec AND data edges in ONE call** — partial wiring is the #1 failure mode.

Edge format: `$<stable_id>.<pin>` for new nodes, `$<32-char-hex-guid>.<pin>` for existing un-tagged nodes.

### Enhanced Input
`bp_input({actions: [...], bindings: [...], bind?: {...}})` — create IAs + IMCs + key mappings + modifiers + triggers in ONE call.

### Class-Level Settings
`bp_class({blueprint_path, operations: [...]})` — reparent, add_interface, set_cdo_property (dot-notation like `"CharacterMovement.MaxWalkSpeed"`).

## Step 5: Commit

`bp_commit(blueprint_path)` — single compile + auto-layout + health report.

Returns: `compile_status`, `errors[]`, `warnings[]`, `orphan_pins`, `dangling_exec_chains`, `unreachable_nodes`.

- **UpToDateWithWarnings is NOT success** — investigate every warning.
- Multi-commit across fix passes is normal (author → commit → read errors → narrow fix → commit again).

## Step 6: Verify

- `bp_brief` / `bp_inspect` for structural confirmation
- `pie_test_bp` for runtime-behavioral verification (30+ assertion types)
- A compile-clean BP that fails PIE is the #1 silent-bug source

## Hard Rules

- **Read before write** — at least one read-class tool must fire first
- **Batch arrays** — `variables[]`, `components[]`, `graphs[]`, `nodes[]`, `edges[]`, `operations[]` are designed for one call with many items
- **One bp_wire call per graph** for the initial wiring pass (exec + data together)
- **One bp_commit per target BP** per authoring pass
- **snake_case node types** — `event`, `function_call`, `variable_get`, `branch`, `spawn_actor`
- **Pure functions have NO exec pins** — `variable_get`, math, `K2_GetActorLocation`, `make_struct`, `break_struct`, `Array_Length`
- **Function inputs are OUTPUT pins** on FunctionEntry — reference via `$entry.<ParamName>`, do NOT create variable_get
- **Latent nodes** (Delay, RetriggerableDelay) only work in EventGraphs
- **`math` node is SCALAR-ONLY** — use KismetMathLibrary function_calls for Vector/Rotator math
- **`function_call` class field is `class`** (NOT `scope` — silently ignored)
- **`comparison` op is lowercase** — `equal`, `not_equal`, `greater`, `less` (NOT PascalCase)

## Compound Escape Hatches

- `bp_author(blueprint_path, spec_json)` — full GraphSpec one-shot when you have the complete spec
- `repair_blueprint(blueprint_path)` — rescue corrupted BPs (duplicate graphs, orphan nodes, broken references)
- `bp_skills({topics: [...]})` — load skill docs before first use of unfamiliar tools (batch 3-8 topics)
