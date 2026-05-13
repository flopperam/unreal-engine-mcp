use serde::{Deserialize, Serialize};
use std::time::Instant;

use crate::procedural::mesh_buffer::ProceduralMeshPayload;
use crate::procedural::sdf::SdfBounds;

// ── Core Trait ────────────────────────────────────────────────────────

/// Every procedural generator implements this trait.
///
/// The trait separates pure computation (`generate`) from side-effectful
/// dispatch (Unreal TCP, Scene DB persistence) which is handled by the
/// framework layer in HTTP routes.
pub trait Generator {
    /// Input parameters — must be serializable for HTTP layer.
    type Params: Serialize + for<'de> Deserialize<'de> + Default + Send + Sync + 'static;

    /// Computation result — what the generator produces.
    type Output: Serialize + Send + Sync + 'static;

    /// Human-readable generator name for logging / diagnostics.
    fn name(&self) -> &'static str;

    /// Run the generator. Pure computation — no side effects.
    fn generate(
        &self,
        params: &Self::Params,
        ctx: &GenerateContext,
    ) -> Result<ProceduralOutput<Self::Output>, ProceduralError>;

    /// Estimate cost / bounds without doing full work.
    /// Used for dry-run mode and client-side guardrails.
    fn estimate(&self, _params: &Self::Params) -> Result<ProceduralEstimate, ProceduralError> {
        Ok(ProceduralEstimate::default())
    }

    /// Validate parameters before generation.
    /// Called automatically by the framework before `generate`.
    fn validate(&self, _params: &Self::Params) -> Result<(), ProceduralError> {
        Ok(())
    }

    /// Optional secondary progress signal independent of the shared
    /// ctx.progress tracker. Default returns None (use the tracker).
    /// This exists for the user-facing API contract requested in the
    /// async-jobs follow-up; in practice generators report progress via
    /// ctx.progress.set(...) because the trait is &self.
    fn progress(&self) -> Option<f32> {
        None
    }
}

// ── Generation Context ──────────────────────────────────────────────

/// Shared progress tracker that long-running generators can update
/// concurrently. The job framework (procedural::jobs) reads from this so
/// that polling clients see real-time progress instead of just "running".
#[derive(Debug, Default)]
pub struct ProgressTracker {
    /// Progress current value (e.g. cells collapsed, iterations done).
    current: std::sync::atomic::AtomicU64,
    /// Total expected value (e.g. total cells, max iterations). Zero = unknown.
    total: std::sync::atomic::AtomicU64,
    /// Optional explicit fraction in [0,1] times 1_000_000 (for sub-step granularity).
    /// Set to u64::MAX when unset.
    fraction_micro: std::sync::atomic::AtomicU64,
}

impl ProgressTracker {
    pub fn new() -> Self {
        Self {
            current: std::sync::atomic::AtomicU64::new(0),
            total: std::sync::atomic::AtomicU64::new(0),
            fraction_micro: std::sync::atomic::AtomicU64::new(u64::MAX),
        }
    }

    pub fn set(&self, current: u64, total: u64) {
        self.current.store(current, std::sync::atomic::Ordering::Relaxed);
        self.total.store(total, std::sync::atomic::Ordering::Relaxed);
    }

    pub fn set_fraction(&self, fraction: f32) {
        let clamped = fraction.clamp(0.0, 1.0);
        let micro = (clamped * 1_000_000.0) as u64;
        self.fraction_micro.store(micro, std::sync::atomic::Ordering::Relaxed);
    }

    /// Returns a fraction in [0,1] if any progress is known, otherwise None.
    pub fn read(&self) -> Option<f32> {
        let micro = self.fraction_micro.load(std::sync::atomic::Ordering::Relaxed);
        if micro != u64::MAX {
            return Some((micro as f32 / 1_000_000.0).clamp(0.0, 1.0));
        }
        let total = self.total.load(std::sync::atomic::Ordering::Relaxed);
        if total == 0 {
            return None;
        }
        let current = self.current.load(std::sync::atomic::Ordering::Relaxed);
        Some((current as f32 / total as f32).clamp(0.0, 1.0))
    }
}

/// Context passed to every generator invocation.
#[derive(Debug, Clone)]
pub struct GenerateContext {
    /// Deterministic seed for reproducible output.
    pub seed: u64,

    /// Hard limits enforced by the framework.
    pub limits: GenerationLimits,

    /// Request ID for tracing.
    pub request_id: u64,

    /// Start time for timeout checks.
    pub started_at: Instant,

    /// Live progress tracker. Generators should call progress.set(...) or
    /// progress.set_fraction(...) from inside hot loops so that the job
    /// registry's polling watchdog can surface real progress to clients.
    pub progress: std::sync::Arc<ProgressTracker>,
}

impl GenerateContext {
    pub fn new(seed: Option<u64>, limits: Option<GenerationLimits>) -> Self {
        let effective_seed = seed.unwrap_or_else(|| {
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64
        });
        Self {
            seed: effective_seed,
            limits: limits.unwrap_or_default(),
            request_id: effective_seed, // reuse seed as request_id baseline
            started_at: Instant::now(),
            progress: std::sync::Arc::new(ProgressTracker::new()),
        }
    }

    /// Check whether generation has exceeded its time budget.
    pub fn check_timeout(&self) -> Result<(), ProceduralError> {
        let elapsed = self.started_at.elapsed().as_millis() as u64;
        if elapsed >= self.limits.max_execution_ms {
            Err(ProceduralError::Timeout {
                elapsed_ms: elapsed,
                limit_ms: self.limits.max_execution_ms,
            })
        } else {
            Ok(())
        }
    }
}

/// Hard limits for generation safety.
#[derive(Debug, Clone, Serialize, Deserialize)]
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

// ── Unified Output Envelope ───────────────────────────────────────────

/// Wrapper around every generator's raw output.
#[derive(Debug, Clone, Serialize)]
pub struct ProceduralOutput<T> {
    pub data: T,
    pub stats: ProceduralStats,
    pub warnings: Vec<ProceduralWarning>,
}

/// Statistics produced by every generator run.
#[derive(Debug, Clone, Default, Serialize)]
pub struct ProceduralStats {
    pub execution_ms: u64,
    pub seed_used: u64,
    /// L-System derived string length, WFC backtrack count, etc.
    pub derived_length: Option<usize>,
    /// Spline segments, mesh triangles, tile count, etc.
    pub segment_count: Option<usize>,
    /// Estimated spawned actors.
    pub actor_count: Option<usize>,
    /// AABB of generated geometry.
    pub bounds: Option<SdfBounds>,
}

/// Warnings that do not abort generation but should be surfaced to callers.
#[derive(Debug, Clone, Serialize)]
#[serde(tag = "type", content = "details")]
pub enum ProceduralWarning {
    IterationCapped { requested: u32, applied: u32 },
    SegmentCountCapped { requested: usize, applied: usize },
    ActorCountCapped { requested: usize, applied: usize },
    LargeBounds { estimated_size: [f32; 3] },
    UnrecognizedSymbols { symbols: Vec<char> },
}

// ── Error Type ──────────────────────────────────────────────────────

#[derive(Debug, thiserror::Error, Serialize)]
#[serde(tag = "error", content = "message")]
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

// ── Output Variants ─────────────────────────────────────────────────

/// Discriminated union of everything a generator can produce.
///
/// The framework uses this to dispatch the correct Unreal C++ command
/// or Scene DB persistence path.
#[derive(Debug, Clone, Serialize)]
#[serde(tag = "variant")]
pub enum ProceduralResultVariant {
    Mesh {
        payload: ProceduralMeshPayload<'static>,
    },
    Spline {
        segments: Vec<SplineSegment>,
        closed_loop: bool,
    },
    TileGrid {
        width: u32,
        height: u32,
        tiles: Vec<TileCell>,
    },
    ActorPlacements {
        placements: Vec<ActorPlacementHint>,
    },
    Metadata {
        value: serde_json::Value,
    },
}

/// A single spline segment (used by L-System turtle output).
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct SplineSegment {
    pub start: [f32; 3],
    pub end: [f32; 3],
}

/// A single cell in a tile grid (WFC output).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TileCell {
    pub x: u32,
    pub y: u32,
    pub tile_id: String,
    pub rotation_degrees: f32,
}

/// Hint for placing an actor in Unreal.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ActorPlacementHint {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
    pub actor_class: String,
    pub mcp_id: String,
    pub tags: Vec<String>,
}

// ── Behavior Modes ────────────────────────────────────────────────────

/// What the framework should do with the generator output.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
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

/// Unified request shape for all procedural generators.
///
/// Individual HTTP routes may wrap this in a generator-specific
/// request struct, but internally they convert to this form.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProceduralRequest<P> {
    pub params: P,
    pub mode: GenerationMode,
    #[serde(default)]
    pub seed: Option<u64>,
    #[serde(default)]
    pub limits: Option<GenerationLimits>,
    #[serde(default)]
    pub unreal: Option<UnrealRealizationConfig>,
    #[serde(default)]
    pub scene_db: Option<SceneDbConfig>,
}

/// Configuration for Unreal realization.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UnrealRealizationConfig {
    #[serde(default = "default_actor_name")]
    pub actor_name: String,
    pub mcp_id: Option<String>,
    #[serde(default)]
    pub material_path: Option<String>,
    #[serde(default = "default_origin")]
    pub location: [f32; 3],
    #[serde(default)]
    pub rotation: [f32; 3],
    #[serde(default = "default_scale")]
    pub scale: [f32; 3],
    #[serde(default = "default_focus_viewport")]
    pub focus_viewport: bool,
}

/// Configuration for Scene DB persistence.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SceneDbConfig {
    pub scene_id: String,
    pub group_tag: String,
}

fn default_actor_name() -> String {
    "ProceduralActor".to_string()
}

fn default_origin() -> [f32; 3] {
    [0.0, 0.0, 0.0]
}

fn default_scale() -> [f32; 3] {
    [1.0, 1.0, 1.0]
}

fn default_focus_viewport() -> bool {
    true
}

// ── Dry-Run Estimate ──────────────────────────────────────────────────

/// Lightweight estimation returned by `Generator::estimate`.
#[derive(Debug, Clone, Default, Serialize)]
pub struct ProceduralEstimate {
    pub estimated_actor_count: usize,
    pub estimated_bounds: Option<SdfBounds>,
    pub estimated_execution_ms: u64,
    pub warnings: Vec<String>,
}

// ── Tests ─────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generation_limits_default() {
        let limits = GenerationLimits::default();
        assert_eq!(limits.max_iterations, 10);
        assert_eq!(limits.max_execution_ms, 30_000);
    }

    #[test]
    fn test_generate_context_new_with_seed() {
        let ctx = GenerateContext::new(Some(42), None);
        assert_eq!(ctx.seed, 42);
        assert_eq!(ctx.request_id, 42);
    }

    #[test]
    fn test_generate_context_check_timeout() {
        let ctx = GenerateContext::new(None, Some(GenerationLimits {
            max_execution_ms: 0,
            ..Default::default()
        }));
        // Should immediately exceed 0ms limit
        assert!(ctx.check_timeout().is_err());
    }

    #[test]
    fn test_generate_context_no_timeout() {
        let ctx = GenerateContext::new(None, Some(GenerationLimits {
            max_execution_ms: 1_000_000,
            ..Default::default()
        }));
        assert!(ctx.check_timeout().is_ok());
    }

    #[test]
    fn test_procedural_error_serialization() {
        let err = ProceduralError::Validation("bad param".to_string());
        let json = serde_json::to_string(&err).unwrap();
        assert!(json.contains("Validation"));
        assert!(json.contains("bad param"));
    }

    #[test]
    fn test_procedural_request_deserialization() {
        let json = r#"{
            "params": {"iterations": 3},
            "mode": "Preview",
            "seed": 123
        }"#;
        let req: ProceduralRequest<serde_json::Value> = serde_json::from_str(json).unwrap();
        assert_eq!(req.mode, GenerationMode::Preview);
        assert_eq!(req.seed, Some(123));
    }

    #[test]
    fn test_procedural_output_stats() {
        let output = ProceduralOutput {
            data: "test",
            stats: ProceduralStats {
                execution_ms: 10,
                seed_used: 99,
                segment_count: Some(25),
                ..Default::default()
            },
            warnings: vec![ProceduralWarning::IterationCapped {
                requested: 20,
                applied: 10,
            }],
        };
        assert_eq!(output.stats.segment_count, Some(25));
        assert_eq!(output.warnings.len(), 1);
    }
}
