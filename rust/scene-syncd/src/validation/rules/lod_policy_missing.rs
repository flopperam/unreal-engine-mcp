use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Warns when objects that should have LOD policy metadata don't.
pub struct LodPolicyMissingRule;

impl ValidationRule for LodPolicyMissingRule {
    fn code(&self) -> &'static str {
        "LOD_POLICY_MISSING"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();

        // Large structural objects that should have LOD policy but don't.
        let lod_required_kinds = ["keep", "tower", "curtain_wall", "gatehouse", "building"];

        for obj in objects {
            if obj.deleted {
                continue;
            }
            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("");

            if lod_required_kinds.contains(&kind) {
                let has_lod = obj.tags.iter().any(|t| t.starts_with("lod:"));
                if !has_lod {
                    diagnostics.push(
                        Diagnostic::warning(
                            self.code(),
                            format!(
                                "Object '{}' (kind: {}) has no LOD policy. Large structures should define distance-based LOD.",
                                obj.mcp_id, kind
                            ),
                        )
                        .with_mcp_id(obj.mcp_id.clone())
                        .with_suggestion(
                            "Add lod:high, lod:medium, or lod:low tags or run the finalize pass".to_string(),
                        ),
                    );
                }
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

    fn make_obj(mcp_id: &str, kind: &str, tags: Vec<&str>) -> SceneObject {
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
            tags: tags.into_iter().map(String::from).collect(),
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
    fn warns_when_lod_missing() {
        let objects = vec![make_obj("keep_1", "keep", vec!["layout_kind:keep"])];
        let rule = LodPolicyMissingRule;
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.code == "LOD_POLICY_MISSING"));
    }

    #[test]
    fn no_warning_when_lod_present() {
        let objects = vec![make_obj(
            "keep_1",
            "keep",
            vec!["layout_kind:keep", "lod:high"],
        )];
        let rule = LodPolicyMissingRule;
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "LOD_POLICY_MISSING"));
    }
}
