use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;
use std::collections::HashMap;

/// Warns when there are too many individual actors that could be grouped
/// into InstanceSets based on repeated mesh+material combinations.
pub struct TooManyActorsRule {
    /// Minimum repetition count that triggers a warning.
    pub repetition_threshold: usize,
}

impl Default for TooManyActorsRule {
    fn default() -> Self {
        Self {
            repetition_threshold: 20,
        }
    }
}

impl ValidationRule for TooManyActorsRule {
    fn code(&self) -> &'static str {
        "TOO_MANY_ACTORS"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();

        // Count objects by (mesh, kind) to find candidates for InstanceSet grouping.
        let mut group_counts: HashMap<(String, String), usize> = HashMap::new();
        for obj in objects {
            if obj.deleted {
                continue;
            }
            let mesh = obj
                .asset_ref
                .get("mesh")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("unknown")
                .to_string();
            *group_counts.entry((mesh, kind)).or_insert(0) += 1;
        }

        for ((mesh, kind), count) in &group_counts {
            if *count >= self.repetition_threshold {
                diagnostics.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "{} objects share the same mesh '{}' and kind '{}'. Consider using InstanceSet rendering for better performance.",
                            count, mesh, kind
                        ),
                    )
                    .with_suggestion(
                        "Tag these objects with render_mode: instance_set or use the density planner".to_string(),
                    ),
                );
            }
        }

        diagnostics
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, Transform, Vec3};
    use serde_json::json;

    fn make_obj(mcp_id: &str, mesh: &str, kind: &str) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: mcp_id.to_string(),
            desired_name: mcp_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({"mesh": mesh}),
            transform: Transform {
                location: Vec3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw: 0.0,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: 1.0,
                    y: 1.0,
                    z: 1.0,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec![format!("layout_kind:{}", kind)],
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
    fn warns_on_repeated_mesh() {
        let objects: Vec<SceneObject> = (0..25)
            .map(|i| make_obj(&format!("c_{}", i), "/Mesh/Cube", "crenellation"))
            .collect();
        let rule = TooManyActorsRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.code == "TOO_MANY_ACTORS"));
    }

    #[test]
    fn no_warning_below_threshold() {
        let objects: Vec<SceneObject> = (0..10)
            .map(|i| make_obj(&format!("c_{}", i), "/Mesh/Cube", "crenellation"))
            .collect();
        let rule = TooManyActorsRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "TOO_MANY_ACTORS"));
    }
}
