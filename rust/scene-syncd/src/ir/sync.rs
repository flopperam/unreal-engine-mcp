use crate::domain::SceneObject;
use serde::{Deserialize, Serialize};

/// Intermediate representation for sync planning: desired vs actual diff.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SyncIr {
    pub operations: Vec<SyncOperation>,
}

impl SyncIr {
    pub fn new() -> Self {
        Self {
            operations: Vec::new(),
        }
    }

    /// Build operations by comparing desired objects against an optional actual snapshot.
    pub fn from_desired_and_actual(
        desired: &[SceneObject],
        actual: Option<&[SceneObject]>,
    ) -> Self {
        use std::collections::HashMap;

        let mut ops = Vec::new();
        let desired_map: HashMap<&str, &SceneObject> =
            desired.iter().map(|o| (o.mcp_id.as_str(), o)).collect();

        if let Some(actual_objs) = actual {
            let actual_map: HashMap<&str, &SceneObject> =
                actual_objs.iter().map(|o| (o.mcp_id.as_str(), o)).collect();

            // Creates and updates
            for (id, d) in &desired_map {
                if d.deleted {
                    continue;
                }
                match actual_map.get(id) {
                    Some(a) => {
                        if d.desired_hash != a.desired_hash {
                            ops.push(SyncOperation::Update {
                                mcp_id: id.to_string(),
                                object: (*d).clone(),
                            });
                        }
                    }
                    None => {
                        ops.push(SyncOperation::Create {
                            mcp_id: id.to_string(),
                            object: (*d).clone(),
                        });
                    }
                }
            }

            // Deletes
            for (id, a) in &actual_map {
                if !desired_map.contains_key(id) {
                    ops.push(SyncOperation::Delete {
                        mcp_id: id.to_string(),
                        object: (*a).clone(),
                    });
                }
            }
        } else {
            // No actual snapshot: everything is a create.
            for (id, d) in &desired_map {
                if !d.deleted {
                    ops.push(SyncOperation::Create {
                        mcp_id: id.to_string(),
                        object: (*d).clone(),
                    });
                }
            }
        }

        Self { operations: ops }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(tag = "op")]
pub enum SyncOperation {
    Create { mcp_id: String, object: SceneObject },
    Update { mcp_id: String, object: SceneObject },
    Delete { mcp_id: String, object: SceneObject },
    NoOp { mcp_id: String },
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_obj(mcp_id: &str, hash: &str) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: mcp_id.to_string(),
            desired_name: mcp_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: Transform::default(),
            visual: json!({}),
            physics: json!({}),
            tags: vec![],
            metadata: json!({}),
            desired_hash: hash.to_string(),
            last_applied_hash: None,
            sync_status: "pending".to_string(),
            deleted: false,
            revision: 1,
            created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
        }
    }

    #[test]
    fn all_create_when_no_actual() {
        let desired = vec![make_obj("a", "h1"), make_obj("b", "h2")];
        let sync = SyncIr::from_desired_and_actual(&desired, None);
        assert_eq!(sync.operations.len(), 2);
        assert!(sync
            .operations
            .iter()
            .all(|op| matches!(op, SyncOperation::Create { .. })));
    }

    #[test]
    fn noop_when_hashes_match() {
        let desired = vec![make_obj("a", "h1")];
        let actual = vec![make_obj("a", "h1")];
        let sync = SyncIr::from_desired_and_actual(&desired, Some(&actual));
        assert!(sync.operations.is_empty());
    }

    #[test]
    fn update_when_hash_differs() {
        let desired = vec![make_obj("a", "h2")];
        let actual = vec![make_obj("a", "h1")];
        let sync = SyncIr::from_desired_and_actual(&desired, Some(&actual));
        assert_eq!(sync.operations.len(), 1);
        assert!(matches!(sync.operations[0], SyncOperation::Update { .. }));
    }

    #[test]
    fn delete_when_missing_in_desired() {
        let desired: Vec<SceneObject> = vec![];
        let actual = vec![make_obj("a", "h1")];
        let sync = SyncIr::from_desired_and_actual(&desired, Some(&actual));
        assert_eq!(sync.operations.len(), 1);
        assert!(matches!(sync.operations[0], SyncOperation::Delete { .. }));
    }

    #[test]
    fn skips_deleted_desired() {
        let mut obj = make_obj("a", "h1");
        obj.deleted = true;
        let desired = vec![obj];
        let sync = SyncIr::from_desired_and_actual(&desired, None);
        assert!(sync.operations.is_empty());
    }
}
