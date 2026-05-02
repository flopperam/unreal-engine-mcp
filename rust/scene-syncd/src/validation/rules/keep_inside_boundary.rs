use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

pub struct KeepInsideBoundary;

impl ValidationRule for KeepInsideBoundary {
    fn code(&self) -> &'static str {
        "KEEP_INSIDE_BOUNDARY"
    }

    fn validate(&self, _objects: &[SceneObject], footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();

        let keeps: Vec<&Footprint2> = footprints.iter().filter(|fp| fp.kind == "keep").collect();

        let boundary_parts: Vec<&Footprint2> = footprints
            .iter()
            .filter(|fp| fp.kind == "curtain_wall" || fp.kind == "tower" || fp.kind == "gatehouse")
            .collect();

        if keeps.is_empty() || boundary_parts.is_empty() {
            return results;
        }

        // Compute collective bounding box of the castle boundary.
        let mut min_x = f64::INFINITY;
        let mut max_x = f64::NEG_INFINITY;
        let mut min_y = f64::INFINITY;
        let mut max_y = f64::NEG_INFINITY;

        for part in &boundary_parts {
            min_x = min_x.min(part.min_x);
            max_x = max_x.max(part.max_x);
            min_y = min_y.min(part.min_y);
            max_y = max_y.max(part.max_y);
        }

        for keep in &keeps {
            let kcx = (keep.min_x + keep.max_x) / 2.0;
            let kcy = (keep.min_y + keep.max_y) / 2.0;

            if kcx < min_x || kcx > max_x || kcy < min_y || kcy > max_y {
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Keep {} center ({:.1}, {:.1}) lies outside the castle boundary bounding box. Keep should be inside the walls.",
                            keep.mcp_id, kcx, kcy
                        ),
                    )
                    .with_mcp_id(keep.mcp_id.clone())
                    .with_suggestion(
                        "Move the keep inside the perimeter formed by walls and towers.".to_string(),
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

    fn make_obj(
        mcp_id: &str,
        x: f64,
        y: f64,
        sx: f64,
        sy: f64,
        kind: &str,
    ) -> (SceneObject, Footprint2) {
        let obj = SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: mcp_id.to_string(),
            desired_name: mcp_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: Transform {
                location: Vec3 { x, y, z: 0.0 },
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
        };
        let fp = Footprint2::from_scene_object(&obj, 0);
        (obj, fp)
    }

    #[test]
    fn keep_inside_boundary_passes() {
        let rule = KeepInsideBoundary;
        let (_w, fw) = make_obj("wall", 0.0, 0.0, 20.0, 20.0, "curtain_wall");
        let (_k, fk) = make_obj("keep", 0.0, 0.0, 4.0, 4.0, "keep");
        let diags = rule.validate(&[], &[fw, fk]);
        assert!(diags.is_empty());
    }

    #[test]
    fn keep_outside_boundary_warns() {
        let rule = KeepInsideBoundary;
        let (_w, fw) = make_obj("wall", 0.0, 0.0, 4.0, 4.0, "curtain_wall");
        let (_k, fk) = make_obj("keep", 5000.0, 0.0, 4.0, 4.0, "keep");
        let diags = rule.validate(&[], &[fw, fk]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "KEEP_INSIDE_BOUNDARY");
    }
}
