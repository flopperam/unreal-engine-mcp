pub mod apply_plan;
pub mod diff;
pub mod graph_build;
pub mod import;
pub mod infer_anchors;
pub mod lower_geometry;
pub mod normalize;
pub mod plan_density_lod;
pub mod plan_material_coherence;
pub mod realize;
pub mod solve_layout;
pub mod validate;

use crate::compiler::context::CompilerContext;
use crate::error::AppError;

/// A single compiler pass.
pub trait Pass {
    fn name(&self) -> &'static str;
    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError>;
}
