use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;
use std::collections::HashMap;

pub struct NoDuplicateMcpId;

impl ValidationRule for NoDuplicateMcpId {
    fn code(&self) -> &'static str {
        "NO_DUPLICATE_MCP_ID"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut counts: HashMap<&str, Vec<usize>> = HashMap::new();
        for (idx, obj) in objects.iter().enumerate() {
            counts.entry(&obj.mcp_id).or_default().push(idx);
        }

        let mut results = Vec::new();
        for (mcp_id, indices) in counts {
            if indices.len() > 1 {
                results.push(
                    Diagnostic::error(
                        self.code(),
                        format!(
                            "Duplicate mcp_id '{}' found {} times",
                            mcp_id,
                            indices.len()
                        ),
                    )
                    .with_mcp_id(mcp_id.to_string())
                    .with_suggestion(format!(
                        "Ensure each object has a unique mcp_id; {} duplicates detected",
                        indices.len() - 1
                    )),
                );
            }
        }
        results
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_object(mcp_id: &str) -> SceneObject {
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
    fn detects_duplicate() {
        let rule = NoDuplicateMcpId;
        let objs = vec![make_object("dup"), make_object("uniq"), make_object("dup")];
        let diags = rule.validate(&objs, &[]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "NO_DUPLICATE_MCP_ID");
        assert_eq!(diags[0].mcp_id, Some("dup".to_string()));
    }

    #[test]
    fn unique_ids_pass() {
        let rule = NoDuplicateMcpId;
        let objs = vec![make_object("a"), make_object("b")];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.is_empty());
    }
}
