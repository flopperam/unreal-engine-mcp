use crate::domain::SceneObject;
use crate::ir::instance_set::InstanceSet;
use serde::{Deserialize, Serialize};

/// High-level rendering plan for a scene.
/// Produced by the Density/LOD planner (Phase 3) and consumed by the sync applier.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RenderPlan {
    pub scene_id: String,
    pub items: Vec<RenderItem>,
    pub decisions: Vec<RenderDecision>,
    pub estimated_cost: RenderCostEstimate,
}

impl RenderPlan {
    /// Build a fallback RenderPlan where every object is emitted as an individual Actor.
    /// Used in Phase 2 before the DensityPlannerPass is wired up.
    pub fn all_actors(scene_id: &str, objects: &[SceneObject]) -> Self {
        let items: Vec<RenderItem> = objects
            .iter()
            .filter(|o| !o.deleted)
            .map(|o| RenderItem::Actor(o.clone()))
            .collect();

        let decisions: Vec<RenderDecision> = objects
            .iter()
            .filter(|o| !o.deleted)
            .map(|o| RenderDecision {
                source_mcp_id: o.mcp_id.clone(),
                decision: RenderMode::Actor,
                importance_score: 1.0,
                semantic_density: 0.0,
                visual_density: 0.0,
                reason: "Phase 2 fallback: all objects rendered as Actor".to_string(),
            })
            .collect();

        let estimated_cost = RenderCostEstimate {
            actor_cost: items.len() as f32,
            instance_cost: 0.0,
            total_draw_calls: items.len(),
        };

        Self {
            scene_id: scene_id.to_string(),
            items,
            decisions,
            estimated_cost,
        }
    }

    /// Extract only the InstanceSet items from this plan.
    pub fn instance_sets(&self) -> Vec<InstanceSet> {
        self.items
            .iter()
            .filter_map(|item| match item {
                RenderItem::InstanceSet(is) => Some(is.clone()),
                _ => None,
            })
            .collect()
    }

    /// Extract SceneObjects that are rendered as individual Actors.
    pub fn actor_objects(&self) -> Vec<SceneObject> {
        self.items
            .iter()
            .filter_map(|item| match item {
                RenderItem::Actor(obj) => Some(obj.clone()),
                _ => None,
            })
            .collect()
    }
}

/// A single entry in the render plan: how one logical entity should appear in Unreal.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(tag = "type", content = "data")]
#[allow(clippy::large_enum_variant)]
pub enum RenderItem {
    Actor(SceneObject),
    InstanceSet(InstanceSet),
    #[serde(skip)] // Phase 5+ placeholder
    HlodProxy(HlodProxySpec),
    #[serde(skip)] // Phase 7+ placeholder
    Decal(DecalSpec),
    #[serde(skip)] // Phase 9+ placeholder
    Omit(OmitReason),
}

/// Per-object decision made by the Density/LOD planner.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RenderDecision {
    pub source_mcp_id: String,
    pub decision: RenderMode,
    pub importance_score: f32,
    pub semantic_density: f32,
    pub visual_density: f32,
    pub reason: String,
}

/// How an object should be rendered.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum RenderMode {
    Actor,
    InstanceSet,
    HlodProxy,
    Decal,
    Omit,
}

/// Estimated rendering cost for a planned scene.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RenderCostEstimate {
    pub actor_cost: f32,
    pub instance_cost: f32,
    pub total_draw_calls: usize,
}

/// Placeholder: HLOD proxy specification (Phase 5).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct HlodProxySpec {
    pub proxy_id: String,
    pub bounds: serde_json::Value,
    pub mesh: String,
}

/// Placeholder: Decal specification (Phase 7).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct DecalSpec {
    pub decal_id: String,
    pub material: String,
    pub size: serde_json::Value,
}

/// Reason an object was omitted from rendering.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct OmitReason {
    pub mcp_id: String,
    pub reason: String,
}
