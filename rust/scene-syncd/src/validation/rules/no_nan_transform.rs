use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

pub struct NoNaNTransform;

impl ValidationRule for NoNaNTransform {
    fn code(&self) -> &'static str {
        "NO_NAN_TRANSFORM"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut results = Vec::new();
        for obj in objects {
            let t = &obj.transform;
            if t.location.x.is_nan()
                || t.location.y.is_nan()
                || t.location.z.is_nan()
                || t.rotation.pitch.is_nan()
                || t.rotation.yaw.is_nan()
                || t.rotation.roll.is_nan()
                || t.scale.x.is_nan()
                || t.scale.y.is_nan()
                || t.scale.z.is_nan()
            {
                results.push(
                    Diagnostic::error(
                        self.code(),
                        format!("Object {} has NaN in transform", obj.mcp_id),
                    )
                    .with_mcp_id(obj.mcp_id.clone())
                    .with_suggestion(
                        "Reset transform to default (location=0, rotation=0, scale=1)".to_string(),
                    ),
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

    fn make_object(mcp_id: &str, x: f64) -> SceneObject {
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
                location: Vec3 { x, y: 0.0, z: 0.0 },
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
    fn detects_nan_location() {
        let rule = NoNaNTransform;
        let obj = make_object("obj_1", f64::NAN);
        let diags = rule.validate(&[obj], &[]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "NO_NAN_TRANSFORM");
    }

    #[test]
    fn clean_transform_passes() {
        let rule = NoNaNTransform;
        let obj = make_object("obj_1", 10.0);
        let diags = rule.validate(&[obj], &[]);
        assert!(diags.is_empty());
    }
}
