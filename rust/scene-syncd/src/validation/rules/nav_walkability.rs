use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Navigation walkability skeleton: ground/road objects should have coverage per cell.
/// Full NavMesh integration is future work.
pub struct NavWalkability;

impl ValidationRule for NavWalkability {
    fn code(&self) -> &'static str {
        "NAV_WALKABILITY"
    }

    fn validate(&self, objects: &[SceneObject], _footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut ground_area = 0.0;
        let mut road_area = 0.0;
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
            let area = obj.transform.scale.x.abs() * obj.transform.scale.y.abs();
            match kind {
                "ground" => ground_area += area,
                "road" => road_area += area,
                _ => {}
            }
        }

        if ground_area > 0.0 && road_area == 0.0 {
            results.push(
                Diagnostic::info(
                    self.code(),
                    "Scene has ground but no roads. Consider adding road segments for patrol routes."
                        .to_string(),
                )
                .with_suggestion("Add road entities connecting keep to gatehouse.".to_string()),
            );
        }

        results
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_obj(kind: &str, sx: f64, sy: f64) -> SceneObject {
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
                    x: sx,
                    y: sy,
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
    fn ground_without_road_info() {
        let rule = NavWalkability;
        let objs = vec![make_obj("ground", 10.0, 10.0)];
        let diags = rule.validate(&objs, &[]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "NAV_WALKABILITY");
    }

    #[test]
    fn ground_with_road_passes() {
        let rule = NavWalkability;
        let objs = vec![make_obj("ground", 10.0, 10.0), make_obj("road", 2.0, 10.0)];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.is_empty());
    }
}
