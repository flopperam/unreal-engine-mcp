use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;
use std::collections::HashMap;

/// Warns when 50+ objects of the same mesh should be rendered as an InstanceSet
/// but are missing the render_mode:instance_set metadata.
pub struct InstanceSetRequiredRule {
    pub threshold: usize,
}

impl Default for InstanceSetRequiredRule {
    fn default() -> Self {
        Self { threshold: 50 }
    }
}

impl ValidationRule for InstanceSetRequiredRule {
    fn code(&self) -> &'static str {
        "INSTANCE_SET_REQUIRED"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();

        // Group objects by (mesh, kind) and track how many have render_mode:instance_set.
        let mut groups: HashMap<(String, String), (usize, usize)> = HashMap::new();
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
            let has_instance_set = obj
                .metadata
                .get("render_mode")
                .and_then(|v| v.as_str())
                == Some("instance_set");
            let entry = groups.entry((mesh, kind)).or_insert((0, 0));
            entry.0 += 1;
            if has_instance_set {
                entry.1 += 1;
            }
        }

        for ((mesh, kind), (total, marked)) in &groups {
            if *total >= self.threshold && *marked < *total {
                diagnostics.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "{} objects of kind '{}' with mesh '{}' should use InstanceSet rendering, but only {} are marked.",
                            total, kind, mesh, marked
                        ),
                    )
                    .with_suggestion(
                        "Run the density planner or add render_mode:instance_set metadata to these objects".to_string(),
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

    fn make_obj(mcp_id: &str, mesh: &str, kind: &str, render_mode: Option<&str>) -> SceneObject {
        let mut obj = SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: mcp_id.to_string(),
            desired_name: mcp_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({"mesh": mesh}),
            transform: Transform {
                location: Vec3 { x: 0.0, y: 0.0, z: 0.0 },
                rotation: Rotator { pitch: 0.0, yaw: 0.0, roll: 0.0 },
                scale: Vec3 { x: 1.0, y: 1.0, z: 1.0 },
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
        };
        if let Some(rm) = render_mode {
            obj.metadata["render_mode"] = json!(rm);
        }
        obj
    }

    #[test]
    fn warns_when_repeated_objects_not_marked() {
        let objects: Vec<SceneObject> = (0..60)
            .map(|i| make_obj(&format!("c_{}", i), "/Mesh/Cube", "crenellation", None))
            .collect();
        let rule = InstanceSetRequiredRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.code == "INSTANCE_SET_REQUIRED"));
    }

    #[test]
    fn no_warning_when_all_marked() {
        let objects: Vec<SceneObject> = (0..60)
            .map(|i| make_obj(&format!("c_{}", i), "/Mesh/Cube", "crenellation", Some("instance_set")))
            .collect();
        let rule = InstanceSetRequiredRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "INSTANCE_SET_REQUIRED"));
    }

    #[test]
    fn no_warning_below_threshold() {
        let objects: Vec<SceneObject> = (0..10)
            .map(|i| make_obj(&format!("c_{}", i), "/Mesh/Cube", "crenellation", None))
            .collect();
        let rule = InstanceSetRequiredRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "INSTANCE_SET_REQUIRED"));
    }
}