use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Warns when the total actor count exceeds a render budget.
pub struct RenderBudgetRule {
    pub max_actor_count: usize,
    pub max_instance_sets: usize,
    pub max_instances_per_set: usize,
}

impl Default for RenderBudgetRule {
    fn default() -> Self {
        Self {
            max_actor_count: 500,
            max_instance_sets: 50,
            max_instances_per_set: 1000,
        }
    }
}

impl ValidationRule for RenderBudgetRule {
    fn code(&self) -> &'static str {
        "RENDER_BUDGET"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();
        let active_count = objects.iter().filter(|o| !o.deleted).count();

        if active_count > self.max_actor_count {
            diagnostics.push(
                Diagnostic::warning(
                    self.code(),
                    format!(
                        "Scene has {} objects, exceeding render budget of {}. Consider grouping repeated meshes into InstanceSets.",
                        active_count, self.max_actor_count
                    ),
                )
                .with_suggestion(
                    "Use scene_get_instance_sets to see which objects can be grouped".to_string()
                ),
            );
        }

        diagnostics
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, Transform, Vec3};
    use serde_json::json;

    fn make_obj(mcp_id: &str) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: mcp_id.to_string(),
            desired_name: mcp_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: Transform {
                location: Vec3 { x: 0.0, y: 0.0, z: 0.0 },
                rotation: Rotator { pitch: 0.0, yaw: 0.0, roll: 0.0 },
                scale: Vec3 { x: 1.0, y: 1.0, z: 1.0 },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec![],
            metadata: json!({}),
            desired_hash: String::new(),
            last_applied_hash: None,
            sync_status: "pending".to_string(),
            deleted: false,
            revision: 1,
            created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
        }
    }

    #[test]
    fn warns_when_over_budget() {
        let objects: Vec<SceneObject> = (0..600).map(|i| make_obj(&format!("obj_{}", i))).collect();
        let rule = RenderBudgetRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.code == "RENDER_BUDGET"));
    }

    #[test]
    fn no_warning_under_budget() {
        let objects: Vec<SceneObject> = (0..100).map(|i| make_obj(&format!("obj_{}", i))).collect();
        let rule = RenderBudgetRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "RENDER_BUDGET"));
    }
}