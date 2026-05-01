<!--
Project: Unreal MCP Scene Database / Sync System
DB: SurrealDB
Core: Rust SDK
Created: 2026-04-25
Scope: Design documents for a SurrealDB-backed desired-state sync architecture integrated with the existing Python MCP + Unreal C++ bridge codebase.
-->
# 10. Operations Runbook

## 1. Required processes

```text
1. SurrealDB server
2. Rust scene-syncd
3. Python MCP server
4. Unreal Editor + MCP plugin
```

## 2. Recommended paths

```text
/home/arat2/Project-MUSE/
  Python/
  rust/scene-syncd/
  scripts/
  .surreal/
```

## 3. Start SurrealDB

```bash
cd /home/arat2/Project-MUSE && mkdir -p .surreal && surreal start --user root --pass secret --bind 127.0.0.1:8000 rocksdb://.surreal/unreal_mcp.db
```

## 4. Start Rust service

```bash
cd /home/arat2/Project-MUSE/rust/scene-syncd && SURREAL_URL=ws://127.0.0.1:8000 SURREAL_NS=unreal_mcp SURREAL_DB=scene SURREAL_USER=root SURREAL_PASS=secret UNREAL_MCP_HOST=127.0.0.1 UNREAL_MCP_PORT=55557 cargo run
```

Health check:

```bash
curl http://127.0.0.1:8787/health
```

## 5. Start Python MCP

```bash
cd /home/arat2/Project-MUSE/Python && SCENE_SYNCD_URL=http://127.0.0.1:8787 python unreal_mcp_server_advanced.py
```

Adjust if using `uv run`.

## 6. First smoke test

### Create desired cube

Call MCP tool:

```json
{
  "tool": "scene_upsert_actor",
  "args": {
    "scene_id": "main",
    "mcp_id": "cube_001",
    "desired_name": "Cube_001",
    "actor_type": "StaticMeshActor",
    "asset_ref": { "kind": "static_mesh", "path": "/Engine/BasicShapes/Cube.Cube" },
    "transform": {
      "location": { "x": 0, "y": 0, "z": 100 },
      "rotation": { "pitch": 0, "yaw": 0, "roll": 0 },
      "scale": { "x": 1, "y": 1, "z": 1 }
    }
  }
}
```

### Plan

```json
{
  "tool": "scene_plan_sync",
  "args": { "scene_id": "main" }
}
```

Expected:

```text
create = 1
```

### Apply

```json
{
  "tool": "scene_sync",
  "args": { "scene_id": "main", "mode": "apply_safe" }
}
```

Expected:

- Cube appears in Unreal.
- Cube has `mcp_id:cube_001`.

### Re-run

Expected:

```text
noop = 1
create = 0
```

## 7. Inspect DB manually

```bash
surreal sql --user root --pass secret --ns unreal_mcp --db scene --pretty
```

Useful queries:

```sql
SELECT * FROM scene;
SELECT * FROM scene_object;
SELECT * FROM sync_run ORDER BY started_at DESC LIMIT 5;
SELECT * FROM scene_operation ORDER BY created_at DESC LIMIT 20;
```

## 7a. Procedural mesh visibility smoke test

The `/procedural/create-mesh` route forwards float32 vertex buffers from `scene-syncd` to the Unreal C++ bridge. The bridge parses those floats explicitly, creates a `UDynamicMeshComponent`, applies a visible default surface material when `material_path` is empty, selects the spawned actor, and frames the editor viewport by default.

Optional request fields:

- `location`: `[x, y, z]`, default `[0, 0, 0]`
- `rotation`: `[pitch, yaw, roll]`, default `[0, 0, 0]`
- `scale`: `[x, y, z]`, default `[1, 1, 1]`
- `focus_viewport`: `true` by default; selects and frames the spawned actor in Unreal Editor

Large terrain demo:

```bash
cd /home/arat2/Project-MUSE/Python && python tests/e2e/demo_terrain_mesh.py
```

Expected:

- Unreal response has `"success": true`.
- `E2E_Terrain_10K` is selected in the editor and the viewport is framed around it.
- The response includes `bounds_origin` and `bounds_extent` so callers can confirm the spawned mesh has non-zero visible bounds.

## 8. Backup

Preferred logical export, version permitting:

```bash
surreal export --conn ws://127.0.0.1:8000 --user root --pass secret --ns unreal_mcp --db scene backup_scene.surql
```

If command differs by version, check the installed SurrealDB CLI help. Databases love moving the cheese.

## 9. Restore

```bash
surreal import --conn ws://127.0.0.1:8000 --user root --pass secret --ns unreal_mcp --db scene backup_scene.surql
```

## 10. Troubleshooting

### Rust cannot connect to SurrealDB

Check:

```bash
surreal version
```

```bash
curl http://127.0.0.1:8000
```

Fix:

- Start SurrealDB.
- Check `SURREAL_URL`.
- Check credentials.

### Python scene tools fail

Likely Rust service is offline.

Check:

```bash
curl http://127.0.0.1:8787/health
```

### Sync creates duplicates

Likely causes:

- Actor did not get `mcp_id` tag.
- Actor listing does not return tags.
- Planner cannot match actual actor.

Fix:

- Verify C++ bridge tag support.
- Query actor listing.
- Do not use actor display name as identity.

### Delete skipped

Cause:

```text
allow_delete=false
```

Fix:

- Review plan.
- Then run sync with `allow_delete=true`.

### Sync does nothing

Check:

```sql
SELECT * FROM scene_object WHERE scene = scene:main;
```

Also check Unreal bridge is online.

## 11. Safe change procedure

1. Create snapshot.
2. Upsert desired changes.
3. Run plan.
4. Review conflicts/deletes.
5. Apply safe sync.
6. Apply deletes only if intended.
7. Create post-change snapshot.

## 12. Emergency reset

If local DB is disposable:

```bash
cd /home/arat2/Project-MUSE && rm -rf .surreal && mkdir -p .surreal
```

This deletes state. Groundbreaking, I know.

## 13. Logging

```bash
cd /home/arat2/Project-MUSE/rust/scene-syncd && RUST_LOG=scene_syncd=debug,tower_http=info cargo run
```

Log fields to include:

- `scene_id`
- `run_id`
- `mcp_id`
- `operation`
- `duration_ms`
- `error_code`

## 14. Cleanup policy

- Keep snapshots.
- Keep failed operations.
- Compact old successful operations after 30 days.
- Keep tombstones until snapshots and sync history no longer need them.
