# Procedural Generation Architecture for scene-syncd

## Status

Draft — under review for issue #13.

## Goal

Provide a single, consistent architecture that lets every procedural generator (L-System, WFC, SDF, Superformula, terrain, city, biome, etc.) plug into the same request/response pipeline, mode-switching logic, and error envelope.

## Existing Landscape

| Generator | Input Type | Output Type | Transport |
|-----------|-----------|------------|-----------|
| `lsystem` | `LSystemParams` | `LSystemResult` (segments) | HTTP JSON + optional Unreal TCP |
| `sdf` + marching cubes | `SdfTree` + bounds/resolution | `MeshData` → `ProceduralMeshPayload` | HTTP JSON request, binary TCP payload to Unreal |
| `superformula` | `SuperformulaParams` | `ProceduralMeshPayload` | same as SDF |

Common pattern: each generator owns its own `Params`, `Result`, and route-level wiring.

## Proposed Unified Model

### 1. Core Trait Boundary

```rust
/// Every procedural generator implements this trait.
pub trait Generator {
    /// Input parameters — must be `Serialize + Deserialize` for HTTP layer.
    type Params: Serialize + DeserializeOwned + Default + Send + Sync + 'static;

    /// Computation result — what the generator produces.
    type Output: Serialize + Send + Sync + 'static;

    /// Human-readable generator name for logging / diagnostics.
    fn name(&self) -> &'static str;

    /// Run the generator. This is pure computation — no side effects.
    fn generate(&self, params: &Self::Params, ctx: &GenerateContext)
        -> Result<ProceduralOutput<Self::Output>, ProceduralError>;

    /// Estimate cost / bounds without doing full work.
    /// Used for dry-run mode and client-side guardrails.
    fn estimate(&self, params: &Self::Params) -> Result<ProceduralEstimate, ProceduralError> {
        // default: no-op estimate
        Ok(ProceduralEstimate::default())
    }

    /// Validate parameters before generation. Called automatically by the framework.
    fn validate(&self, params: &Self::Params) -> Result<(), ProceduralError> {
        Ok(())
    }
}
```

### 2. Context & Safety Limits

```rust
pub struct GenerateContext {
    /// Deterministic seed for reproducible output.
    pub seed: u64,

    /// Hard limits enforced by the framework.
    pub limits: GenerationLimits,

    /// Request ID for tracing.
    pub request_id: u64,

    /// Start time for timeout checks.
    pub started_at: Instant,
}

pub struct GenerationLimits {
    pub max_iterations: u32,
    pub max_segment_count: usize,
    pub max_actor_count: usize,
    pub max_execution_ms: u64,
    pub max_string_length: usize,
}

impl Default for GenerationLimits {
    fn default() -> Self {
        Self {
            max_iterations: 10,
            max_segment_count: 1_000_000,
            max_actor_count: 100_000,
            max_execution_ms: 30_000,
            max_string_length: 1_000_000,
        }
    }
}
```

### 3. Unified Output Envelope

```rust
pub struct ProceduralOutput<T> {
    pub data: T,
    pub stats: ProceduralStats,
    pub warnings: Vec<ProceduralWarning>,
}

pub struct ProceduralStats {
    pub execution_ms: u64,
    pub seed_used: u64,
    pub derived_length: Option<usize>,     // L-System derived string length
    pub segment_count: Option<usize>,      // spline segments, mesh triangles, etc.
    pub actor_count: Option<usize>,        // estimated spawned actors
    pub bounds: Option<SdfBounds>,         // AABB of generated geometry
}

pub enum ProceduralWarning {
    IterationCapped { requested: u32, applied: u32 },
    SegmentCountCapped { requested: usize, applied: usize },
    ActorCountCapped { requested: usize, applied: usize },
    LargeBounds { estimated_size: [f32; 3] },
    UnrecognizedSymbols { symbols: Vec<char> },
}
```

### 4. Error Type

```rust
#[derive(Debug, thiserror::Error)]
pub enum ProceduralError {
    #[error("Parameter validation failed: {0}")]
    Validation(String),

    #[error("Generation exceeded time limit: {elapsed_ms}ms > {limit_ms}ms")]
    Timeout { elapsed_ms: u64, limit_ms: u64 },

    #[error("Generation exceeded output size limit")]
    OutputTooLarge,

    #[error("Computation produced no output")]
    EmptyResult,

    #[error("Contradiction encountered in constraint solver: {details}")]
    Contradiction { details: String },

    #[error("Scene DB persistence failed: {0}")]
    SceneDb(String),

    #[error("Unreal realization failed: {0}")]
    Unreal(String),

    #[error("Unsupported mode or configuration: {0}")]
    Unsupported(String),
}
```

### 5. Output Variants

Each generator declares which output variant it produces:

```rust
pub enum ProceduralResultVariant {
    /// Pure geometry: vertices, normals, indices.
    Mesh(ProceduralMeshPayload<'static>),

    /// Spline segments for roads, rivers, vines.
    Spline { segments: Vec<SplineSegment>, closed_loop: bool },

    /// Tile occupancy grid for WFC / terrain.
    TileGrid { width: u32, height: u32, tiles: Vec<TileCell> },

    /// Actor placement hints (position, rotation, scale, class).
    ActorPlacements(Vec<ActorPlacementHint>),

    /// Mixed / metadata-only output.
    Metadata(serde_json::Value),
}

pub struct ActorPlacementHint {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
    pub actor_class: String,
    pub mcp_id: String,
    pub tags: Vec<String>,
}

pub struct TileCell {
    pub x: u32,
    pub y: u32,
    pub tile_id: String,
    pub rotation_degrees: f32,
}
```

### 6. Behavior Modes (preview / scene_db / unreal / dry_run)

The framework layer (HTTP route) handles mode switching, not individual generators.

```rust
pub enum GenerationMode {
    /// Compute only — return JSON, no persistence, no Unreal.
    Preview,

    /// Persist desired state into Scene DB (SurrealDB).
    SceneDb,

    /// Realize directly in Unreal Editor via TCP bridge.
    Unreal,

    /// Compute + estimate — return stats without mutating anything.
    DryRun,
}

pub struct ProceduralRequest<P> {
    pub params: P,
    pub mode: GenerationMode,
    pub seed: Option<u64>,
    pub limits: Option<GenerationLimits>,
    pub unreal: Option<UnrealRealizationConfig>,
    pub scene_db: Option<SceneDbConfig>,
}

pub struct UnrealRealizationConfig {
    pub actor_name: String,
    pub mcp_id: Option<String>,
    pub material_path: Option<String>,
    pub location: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
    pub focus_viewport: bool,
}

pub struct SceneDbConfig {
    pub scene_id: String,
    pub group_tag: String,
}
```

Route-level flow:

```rust
async fn procedural_route<G: Generator>(
    State(state): State<AppState>,
    Json(req): Json<ProceduralRequest<G::Params>>,
) -> Result<Json<Value>, AppError> {
    // 1. Validate
    let generator = G::default();
    generator.validate(&req.params)?;

    // 2. Dry-run shortcut
    if req.mode == GenerationMode::DryRun {
        let estimate = generator.estimate(&req.params)?;
        return Ok(Json(success_response(json!({ "estimate": estimate }))));
    }

    // 3. Build context
    let ctx = GenerateContext::new(req.seed, req.limits);

    // 4. Pure compute
    let output = generator.generate(&req.params, &ctx)?;

    // 5. Mode dispatch
    match req.mode {
        GenerationMode::Preview => {
            Ok(Json(success_response(json!({
                "data": output.data,
                "stats": output.stats,
                "warnings": output.warnings,
            }))))
        }
        GenerationMode::SceneDb => {
            persist_to_scene_db(state.db, &req.scene_db, &output).await?;
            Ok(Json(success_response(json!({ "persisted": true, "stats": output.stats }))))
        }
        GenerationMode::Unreal => {
            let unreal_resp = realize_in_unreal(&state.unreal_client, &req.unreal, &output).await?;
            Ok(Json(success_response(json!({
                "unreal_response": unreal_resp,
                "stats": output.stats,
            }))))
        }
        _ => unreachable!(),
    }
}
```

## Backward Compatibility Strategy

Existing routes (`/procedural/lsystem-spline`, `/procedural/sdf-mesh`, `/procedural/superformula-mesh`) keep their current request/response shapes. Internally they delegate to the new `Generator` trait, but the HTTP contract does not change until a v2 migration is planned.

Migration plan:
1. Implement `Generator` for `LSystemParams` + `LSystemResult`.
2. Refactor `lsystem_spline_route` to call `LSystemGenerator::default().generate(...)` internally.
3. Same for SDF and Superformula.
4. Add new v2 routes (`/v2/procedural/lsystem`, `/v2/procedural/wfc`, etc.) that expose the unified envelope.
5. Deprecate v1 routes in a later release.

## L-System v2 Specifics

`#14` requires named presets, 2D/3D modes, richer outputs, and safety caps.

Presets:

```rust
pub enum LSystemPreset {
    KochCurve,
    Tree,
    Road,
    River,
    Cave,
    Vine,
    CityStreet,
    Custom { axiom: String, rules: Vec<(char, String)> },
}
```

Extended `LSystemParams` (v2) keeps backward compatibility by making new fields optional:

```rust
pub struct LSystemParamsV2 {
    pub preset: Option<LSystemPreset>,
    // v1 fields kept for compatibility
    pub axiom: Option<String>,
    pub rules: Option<Vec<(char, String)>>,
    pub iterations: u32,
    pub step_length: f32,
    pub angle_degrees: f32,
    pub origin: [f32; 3],
    pub heading: [f32; 3],
    pub up: [f32; 3],
    // v2 additions
    pub mode: LSystemMode, // Turtle2D | Turtle3D
    pub output_format: LSystemOutputFormat, // Segments | GraphNodes | TileOccupancy
}
```

## WFC Specifics

`#16` will implement `Generator` for WFC.

```rust
pub struct WfcParams {
    pub width: u32,
    pub height: u32,
    pub seed: u64,
    pub tiles: Vec<WfcTileDef>,
    pub adjacency_rules: Vec<WfcAdjacencyRule>,
    pub asset_mapping: Option<Vec<WfcAssetMapping>>,
}

pub struct WfcOutput {
    pub grid: Vec<Vec<String>>, // tile_id per cell
    pub placements: Vec<ActorPlacementHint>,
    pub contradiction: Option<WfcContradictionInfo>,
    pub backtrack_count: u32,
}
```

WFC implements `Generator` with `Output = WfcOutput`. The framework handles conversion from `WfcOutput` to `ProceduralResultVariant::TileGrid` / `ActorPlacements`.

## Deterministic Seed Handling

All generators receive a `seed: u64`. The framework guarantees:

- Same `seed` + same `Params` → same `Output`.
- Seed is derived from request timestamp if not provided, but logged for reproducibility.
- `ProceduralStats.seed_used` always reflects the effective seed.

Generators that internally use `rand` must instantiate a `StdRng::seed_from_u64(ctx.seed)` rather than the global thread_rng.

## Python MCP Tool Contract

Python tools should expose a single shape:

```python
@tool
def scene_generate_procedural(
    generator: str,           # "lsystem" | "wfc" | "sdf" | "superformula" | ...
    params: dict,             # generator-specific parameters
    mode: str,               # "preview" | "scene_db" | "unreal" | "dry_run"
    seed: int | None = None,
    limits: dict | None = None,
    unreal_config: dict | None = None,
    scene_db_config: dict | None = None,
) -> dict:
    ...
```

This avoids one-tool-per-generator explosion and routes everything through the unified Rust API.

## C++ Unreal Realization Contract

The C++ plugin receives a structured command per output variant:

- `Mesh` → `upsert_procedural_mesh` (existing)
- `Spline` → `create_spline_from_points` (existing)
- `TileGrid` / `ActorPlacements` → new commands:
  - `spawn_tile_grid`
  - `spawn_procedural_actor_batch`
  - `create_data_layer_for_generation`
  - `clear_generated_group`

All commands return `{ success: bool, error?: string, data?: {...} }`.

## Acceptance Criteria

- [ ] `Generator` trait exists and compiles.
- [ ] `LSystemGenerator` implements `Generator` and passes all existing tests.
- [ ] `SdfGenerator` and `SuperformulaGenerator` implement `Generator` without breaking v1 routes.
- [ ] WFC prototype (#16) can implement `Generator` without trait changes.
- [ ] `ProceduralRequest` / `ProceduralOutput` envelope is used by at least one v2 route.
- [ ] Dry-run mode returns estimated actor count and bounds.
- [ ] Scene DB persistence path exists (may be no-op stub if Surreal schema not ready).
- [ ] Integration test covers at least one L-System end-to-end flow through the new envelope.

## Open Questions

1. Should `Generator::generate` be `async`? (SurrealDB and Unreal client calls are async, but pure compute is sync.)
   **Decision:** `generate` stays sync. The route wrapper handles async dispatch for persistence / realization.
2. Should the framework support cancellation mid-generation?
   **Decision:** Phase 1 uses timeout only. Cancellation tokens in Phase 2 if needed.
3. How do we version the HTTP API? (`/v2/procedural/...` or content negotiation?)
   **Decision:** `/v2/procedural/{generator}` for now.

## References

- Issue #13 (this document)
- Issue #14 (L-System v2)
- Issue #16 (WFC prototype)
- Issue #17 (Unreal C++ commands)
- Issue #18 (Python MCP tools)
- Existing code: `rust/scene-syncd/src/procedural/`
