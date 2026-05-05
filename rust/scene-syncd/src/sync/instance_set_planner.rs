use crate::ir::instance_set::InstanceSet;
use serde::{Deserialize, Serialize};

/// Plan instance set sync by comparing desired sets against actual ISM state.
pub fn plan_instance_set_sync(
    desired_sets: &[InstanceSet],
    actual_ism: &[InstanceSetState],
) -> Vec<InstanceSetOperation> {
    use std::collections::HashMap;

    let mut ops = Vec::new();
    let desired_map: HashMap<&str, &InstanceSet> = desired_sets
        .iter()
        .map(|s| (s.set_id.as_str(), s))
        .collect();
    let actual_map: HashMap<&str, &InstanceSetState> =
        actual_ism.iter().map(|s| (s.set_id.as_str(), s)).collect();

    // Creates and updates
    for (id, desired) in &desired_map {
        match actual_map.get(id) {
            Some(actual) => {
                let desired_mesh = normalize_unreal_asset_path(&desired.mesh);
                let actual_mesh = normalize_unreal_asset_path(&actual.mesh);
                if desired.transforms.len() != actual.instance_count || desired_mesh != actual_mesh
                {
                    ops.push(InstanceSetOperation::Update {
                        set_id: id.to_string(),
                        mesh: desired.mesh.clone(),
                        material: desired.material.clone(),
                        transforms: desired.transforms.clone(),
                        cell_id: desired.cell_id.clone(),
                    });
                }
            }
            None => {
                ops.push(InstanceSetOperation::Create {
                    set_id: id.to_string(),
                    mesh: desired.mesh.clone(),
                    material: desired.material.clone(),
                    transforms: desired.transforms.clone(),
                    cell_id: desired.cell_id.clone(),
                });
            }
        }
    }

    // Deletes
    for (id, _actual) in &actual_map {
        if !desired_map.contains_key(id) {
            ops.push(InstanceSetOperation::Delete {
                set_id: id.to_string(),
            });
        }
    }

    ops
}

fn normalize_unreal_asset_path(path: &str) -> String {
    let trimmed = path.trim();
    if let Some((package_path, object_name)) = trimmed.rsplit_once('.') {
        if package_path.rsplit('/').next() == Some(object_name) {
            return package_path.to_string();
        }
    }
    trimmed.to_string()
}

/// Observed state of an ISM/HISM component in Unreal.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct InstanceSetState {
    pub set_id: String,
    pub mesh: String,
    pub material: Option<String>,
    pub instance_count: usize,
    pub cell_id: Option<String>,
}

/// Operations to sync instance sets with Unreal.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(tag = "op")]
pub enum InstanceSetOperation {
    Create {
        set_id: String,
        mesh: String,
        material: Option<String>,
        transforms: Vec<crate::domain::Transform>,
        cell_id: Option<String>,
    },
    Update {
        set_id: String,
        mesh: String,
        material: Option<String>,
        transforms: Vec<crate::domain::Transform>,
        cell_id: Option<String>,
    },
    Delete {
        set_id: String,
    },
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, Transform, Vec3};

    fn make_set(set_id: &str, mesh: &str, count: usize) -> InstanceSet {
        InstanceSet {
            set_id: set_id.to_string(),
            mesh: mesh.to_string(),
            material: None,
            transforms: (0..count)
                .map(|i| Transform {
                    location: Vec3 {
                        x: i as f64 * 100.0,
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
                })
                .collect(),
            custom_data: serde_json::json!({}),
            tags: vec![],
            cell_id: None,
            lod_policy: "default".to_string(),
            source_mcp_ids: vec![],
        }
    }

    fn make_state(set_id: &str, mesh: &str, count: usize) -> InstanceSetState {
        InstanceSetState {
            set_id: set_id.to_string(),
            mesh: mesh.to_string(),
            material: None,
            instance_count: count,
            cell_id: None,
        }
    }

    #[test]
    fn all_create_when_no_actual() {
        let desired = vec![make_set("a", "/Engine/Cube", 2)];
        let ops = plan_instance_set_sync(&desired, &[]);
        assert_eq!(ops.len(), 1);
        assert!(matches!(ops[0], InstanceSetOperation::Create { .. }));
    }

    #[test]
    fn noop_when_matches() {
        let desired = vec![make_set("a", "/Engine/Cube", 2)];
        let actual = vec![make_state("a", "/Engine/Cube", 2)];
        let ops = plan_instance_set_sync(&desired, &actual);
        assert!(ops.is_empty());
    }

    #[test]
    fn noop_when_unreal_reports_object_qualified_mesh_path() {
        let desired = vec![make_set("a", "/Engine/BasicShapes/Cube", 2)];
        let actual = vec![make_state("a", "/Engine/BasicShapes/Cube.Cube", 2)];
        let ops = plan_instance_set_sync(&desired, &actual);
        assert!(ops.is_empty());
    }

    #[test]
    fn update_when_count_differs() {
        let desired = vec![make_set("a", "/Engine/Cube", 3)];
        let actual = vec![make_state("a", "/Engine/Cube", 2)];
        let ops = plan_instance_set_sync(&desired, &actual);
        assert_eq!(ops.len(), 1);
        assert!(matches!(ops[0], InstanceSetOperation::Update { .. }));
    }

    #[test]
    fn delete_when_missing_in_desired() {
        let desired: Vec<InstanceSet> = vec![];
        let actual = vec![make_state("a", "/Engine/Cube", 2)];
        let ops = plan_instance_set_sync(&desired, &actual);
        assert_eq!(ops.len(), 1);
        assert!(matches!(ops[0], InstanceSetOperation::Delete { .. }));
    }
}
