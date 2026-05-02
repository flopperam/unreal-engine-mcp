use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::geom::units::Cm;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

pub struct WallSelfIntersection;

impl ValidationRule for WallSelfIntersection {
    fn code(&self) -> &'static str {
        "WALL_SELF_INTERSECTION"
    }

    fn validate(&self, _objects: &[SceneObject], footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();
        let walls: Vec<&Footprint2> = footprints
            .iter()
            .filter(|fp| fp.kind == "curtain_wall")
            .collect();

        for i in 0..walls.len() {
            for j in (i + 1)..walls.len() {
                let a = walls[i];
                let b = walls[j];
                if a.intersects_2d(b, Cm(1.0)) {
                    results.push(
                        Diagnostic::warning(
                            self.code(),
                            format!(
                                "Curtain walls {} and {} intersect in 2D footprint. Wall segments should not cross each other.",
                                a.mcp_id, b.mcp_id
                            ),
                        )
                        .with_mcp_id(a.mcp_id.clone())
                        .with_suggestion(
                            "Separate wall segments or join them at a shared tower.".to_string(),
                        ),
                    );
                }
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

    fn make_wall(mcp_id: &str, x: f64, y: f64, sx: f64, sy: f64) -> (SceneObject, Footprint2) {
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
            tags: vec!["layout_kind:curtain_wall".to_string()],
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
    fn crossing_walls_warn() {
        let rule = WallSelfIntersection;
        let (_a, fa) = make_wall("wall_a", 0.0, 0.0, 10.0, 1.0);
        let (_b, fb) = make_wall("wall_b", 0.0, 0.0, 1.0, 10.0);
        let diags = rule.validate(&[], &[fa, fb]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "WALL_SELF_INTERSECTION");
    }

    #[test]
    fn separated_walls_pass() {
        let rule = WallSelfIntersection;
        let (_a, fa) = make_wall("wall_a", 0.0, 0.0, 2.0, 1.0);
        let (_b, fb) = make_wall("wall_b", 500.0, 500.0, 2.0, 1.0);
        let diags = rule.validate(&[], &[fa, fb]);
        assert!(diags.is_empty());
    }
}
