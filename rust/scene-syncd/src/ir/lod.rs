use serde::{Deserialize, Serialize};

/// LOD policy assigned by the DensityPlannerPass.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum LodPolicy {
    High,
    Medium,
    Low,
    Billboard,
    Omit,
}

/// Re-export from render_plan.rs so there is a single canonical type.
pub use crate::ir::render_plan::RenderCostEstimate;
