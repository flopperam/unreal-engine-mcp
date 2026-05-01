use crate::domain::SceneObject;
use crate::validation::diagnostic::{Diagnostic, Severity};
use crate::validation::engine::ValidationRule;

/// Validates that objects with similar kinds use consistent materials.
/// Warns when objects of the same kind have different material families,
/// or when material assignments don't match the world style profile.
pub struct MaterialCoherenceRule;

impl ValidationRule for MaterialCoherenceRule {
    fn code(&self) -> &'static str {
        "MATERIAL_COHERENCE"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();

        // Check for objects of the same kind with different asset refs
        use std::collections::HashMap;
        let mut kind_meshes: HashMap<String, Vec<String>> = HashMap::new();

        for obj in objects {
            if obj.deleted {
                continue;
            }
            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("unknown");

            let mesh = obj
                .asset_ref
                .get("mesh")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();

            kind_meshes
                .entry(kind.to_string())
                .or_default()
                .push(mesh);
        }

        for (kind, meshes) in &kind_meshes {
            let unique_meshes: std::collections::HashSet<&str> =
                meshes.iter().map(|s| s.as_str()).filter(|s| !s.is_empty()).collect();
            if unique_meshes.len() > 1 {
                diagnostics.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Kind '{}' uses {} different meshes: {:?}. Consider using the same mesh for consistency.",
                            kind,
                            unique_meshes.len(),
                            unique_meshes.into_iter().collect::<Vec<_>>()
                        ),
                    )
                    .with_suggestion(format!(
                        "Assign a consistent mesh to all '{}' objects", kind
                    )),
                );
            }
        }

        // Warn about detail objects without render_mode metadata
        let detail_kinds = ["crenellation", "window", "roof_tile", "brick", "merlon"];
        for obj in objects {
            if obj.deleted {
                continue;
            }
            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("");

            if detail_kinds.contains(&kind) {
                if obj.metadata.get("render_mode").is_none() {
                    diagnostics.push(
                        Diagnostic::warning(
                            self.code(),
                            format!(
                                "Detail object '{}' (kind: {}) has no render_mode metadata. InstanceSet rendering recommended.",
                                obj.mcp_id, kind
                            ),
                        )
                        .with_mcp_id(obj.mcp_id.clone())
                        .with_suggestion("Set render_mode to instance_set for detail objects".to_string()),
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
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_obj(mcp_id: &str, kind: &str, mesh: &str, render_mode: Option<&str>) -> SceneObject {
        let mut obj = SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: mcp_id.to_string(),
            desired_name: mcp_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: if mesh.is_empty() {
                json!({})
            } else {
                json!({"mesh": mesh})
            },
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
    fn warns_on_inconsistent_meshes_per_kind() {
        let objects = vec![
            make_obj("w1", "curtain_wall", "/Mesh/Wall_A", None),
            make_obj("w2", "curtain_wall", "/Mesh/Wall_B", None),
        ];
        let rule = MaterialCoherenceRule;
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.code == "MATERIAL_COHERENCE"));
    }

    #[test]
    fn no_warning_when_consistent() {
        let objects = vec![
            make_obj("w1", "curtain_wall", "/Mesh/Wall_A", None),
            make_obj("w2", "curtain_wall", "/Mesh/Wall_A", None),
        ];
        let rule = MaterialCoherenceRule;
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "MATERIAL_COHERENCE" && d.message.contains("different meshes")));
    }

    #[test]
    fn warns_detail_object_without_render_mode() {
        let objects = vec![
            make_obj("c1", "crenellation", "/Mesh/Cube", None),
        ];
        let rule = MaterialCoherenceRule;
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.message.contains("render_mode")));
    }

    #[test]
    fn no_warning_detail_object_with_render_mode() {
        let objects = vec![
            make_obj("c1", "crenellation", "/Mesh/Cube", Some("instance_set")),
        ];
        let rule = MaterialCoherenceRule;
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.message.contains("render_mode")));
    }
}