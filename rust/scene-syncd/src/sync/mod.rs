pub mod applier;
pub mod cell_aware;
pub mod instance_set_planner;
pub mod planner;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "snake_case")]
pub enum SyncAction {
    Create,
    UpdateTransform,
    UpdateVisual,
    Delete,
    Noop,
    Conflict,
    Unsupported,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SyncOperation {
    pub action: SyncAction,
    pub mcp_id: String,
    pub reason: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub desired: Option<serde_json::Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub actual: Option<serde_json::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SyncPlan {
    pub scene_id: String,
    pub summary: SyncPlanSummary,
    pub operations: Vec<SyncOperation>,
    pub warnings: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct SyncPlanSummary {
    pub create: usize,
    pub update_transform: usize,
    pub update_visual: usize,
    pub delete: usize,
    pub noop: usize,
    pub conflict: usize,
    pub unsupported: usize,
    #[serde(default)]
    pub instance_set_creates: usize,
    #[serde(default)]
    pub instance_set_updates: usize,
    #[serde(default)]
    pub instance_set_deletes: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UnrealActorObservation {
    pub name: String,
    pub class: String,
    pub location: [f64; 3],
    pub rotation: [f64; 3],
    pub scale: [f64; 3],
    #[serde(default)]
    pub tags: Vec<String>,
}

impl UnrealActorObservation {
    pub fn mcp_id(&self) -> Option<&str> {
        self.tags
            .iter()
            .find(|t| t.starts_with("mcp_id:"))
            .map(|t| &t[7..])
    }
}
