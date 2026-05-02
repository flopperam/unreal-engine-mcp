use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Ground contact rule: towers and keeps should have their bottom at or near Z=0.
pub struct GroundContact;

const GROUND_EPSILON: f64 = 1.0; // 1 cm tolerance

impl ValidationRule for GroundContact {
    fn code(&self) -> &'static str {
        "GROUND_CONTACT"
    }

    fn validate(&self, objects: &[SceneObject], _footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();
        for obj in objects {
            if obj.deleted {
                continue;
            }
            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("");
            if !matches!(kind, "tower" | "keep" | "gatehouse") {
                continue;
            }
            let z = obj.transform.location.z;
            let half_height = obj.transform.scale.z.abs() * 50.0;
            let bottom_z = z - half_height;
            if bottom_z.abs() > GROUND_EPSILON {
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Object {} (kind={}) bottom is at Z={:.1}, expected near 0",
                            obj.mcp_id, kind, bottom_z
                        ),
                    )
                    .with_mcp_id(obj.mcp_id.clone())
                    .with_suggestion(format!(
                        "Set location.z to {:.1} so the object rests on the ground",
                        half_height
                    ))
                    .with_suggested_transform(crate::domain::Transform {
                        location: crate::domain::Vec3 {
                            x: obj.transform.location.x,
                            y: obj.transform.location.y,
                            z: half_height,
                        },
                        rotation: obj.transform.rotation.clone(),
                        scale: obj.transform.scale.clone(),
                    }),
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

    fn make_obj(kind: &str, z: f64, sz: f64) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: format!("{}_1", kind),
            desired_name: format!("{}_1", kind),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: Transform {
                location: Vec3 { x: 0.0, y: 0.0, z },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw: 0.0,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: 1.0,
                    y: 1.0,
                    z: sz,
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
    fn floating_tower_warns() {
        let rule = GroundContact;
        // scale.z=2.0 → half_height=100.0; location.z=200.0 → bottom_z=100.0
        let objs = vec![make_obj("tower", 200.0, 2.0)];
        let diags = rule.validate(&objs, &[]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "GROUND_CONTACT");
        assert!(diags[0].suggested_transform.is_some());
    }

    #[test]
    fn grounded_tower_passes() {
        let rule = GroundContact;
        // scale.z=2.0 → half_height=100.0; location.z=100.0 → bottom_z=0.0
        let objs = vec![make_obj("tower", 100.0, 2.0)];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.is_empty());
    }

    #[test]
    fn ground_kind_ignored() {
        let rule = GroundContact;
        let objs = vec![make_obj("ground", 200.0, 2.0)];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.is_empty());
    }
}
