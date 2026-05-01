use serde::{Deserialize, Serialize};

/// Input features for density-based render mode classification.
/// Computed per-object by the DensityPlannerPass (Phase 3).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct DensityInput {
    pub semantic_kind: String,
    pub importance_score: f32,
    pub distance_band: DistanceBand,
    pub repetition_count: usize,
    pub visible_priority: f32,
    pub interaction_required: bool,
    pub collision_required: bool,
}

/// Distance band from camera / hero point of interest.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum DistanceBand {
    Near,
    Mid,
    Far,
    Distant,
}
