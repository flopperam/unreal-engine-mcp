use crate::domain::SceneObject;
use crate::ir::instance_set::InstanceSet;
use crate::ir::world_cell::WorldCell;
use crate::validation::diagnostic::Diagnostic;
use serde::{Deserialize, Serialize};

/// Result of running the compiler pipeline for a scene.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompileResult {
    pub scene_id: String,
    pub stage: String,
    /// Compilation mode: blockout, asset_binding, detail, finalize.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mode: Option<String>,
    pub objects: Vec<SceneObject>,
    pub instance_sets: Vec<InstanceSet>,
    pub world_cells: Vec<WorldCell>,
    pub diagnostics: Vec<Diagnostic>,
    pub summary: CompileSummary,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub render_plan: Option<crate::ir::render_plan::RenderPlan>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompileSummary {
    pub errors: usize,
    pub warnings: usize,
    pub infos: usize,
    pub objects: usize,
    #[serde(default)]
    pub instance_sets: usize,
    #[serde(default)]
    pub world_cells: usize,
}

impl Default for CompileSummary {
    fn default() -> Self {
        Self {
            errors: 0,
            warnings: 0,
            infos: 0,
            objects: 0,
            instance_sets: 0,
            world_cells: 0,
        }
    }
}
