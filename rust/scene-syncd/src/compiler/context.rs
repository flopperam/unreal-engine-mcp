use crate::domain::{SceneEntity, SceneObject};
use crate::geom::footprint::Footprint2;
use crate::ir::geometric::GeometricIr;
use crate::ir::instance_set::InstanceSet;
use crate::ir::render_plan::RenderPlan;
use crate::ir::semantic::SemanticScene;
use crate::ir::source_map::SourceMap;
use crate::ir::sync::SyncIr;
use crate::ir::world_cell::WorldCell;
use crate::layout::span::Span;
use crate::validation::diagnostic::Diagnostic;

/// Mutable context carried through each compiler pass.
pub struct CompilerContext {
    pub scene_id: String,
    pub objects: Vec<SceneObject>,
    pub entities: Vec<SceneEntity>,
    pub spans: Vec<(String, Span)>,
    pub footprints: Vec<Footprint2>,
    pub instance_sets: Vec<InstanceSet>,
    pub world_cells: Vec<WorldCell>,
    pub diagnostics: Vec<Diagnostic>,
    pub semantic_ir: Option<SemanticScene>,
    pub geometric_ir: Option<GeometricIr>,
    pub sync_ir: Option<SyncIr>,
    pub source_map: Option<SourceMap>,
    pub render_plan: Option<RenderPlan>,
}

impl CompilerContext {
    pub fn new(scene_id: String) -> Self {
        Self {
            scene_id,
            objects: Vec::new(),
            entities: Vec::new(),
            spans: Vec::new(),
            footprints: Vec::new(),
            instance_sets: Vec::new(),
            world_cells: Vec::new(),
            diagnostics: Vec::new(),
            semantic_ir: None,
            geometric_ir: None,
            sync_ir: None,
            source_map: None,
            render_plan: None,
        }
    }

    pub fn add_diagnostics(
        &mut self,
        mut diags: Vec<Diagnostic>,
    ) {
        self.diagnostics.append(&mut diags);
    }
}
