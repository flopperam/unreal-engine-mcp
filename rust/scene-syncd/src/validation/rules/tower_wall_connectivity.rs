use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Maximum distance from a curtain wall to its nearest tower for valid connectivity.
const CONNECTIVITY_DISTANCE_CM: f64 = 600.0;

pub struct TowerWallConnectivity;

impl ValidationRule for TowerWallConnectivity {
    fn code(&self) -> &'static str {
        "TOWER_WALL_CONNECTIVITY"
    }

    fn validate(&self, _objects: &[SceneObject], footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();

        let towers: Vec<&Footprint2> = footprints.iter().filter(|fp| fp.kind == "tower").collect();

        let walls: Vec<&Footprint2> = footprints
            .iter()
            .filter(|fp| fp.kind == "curtain_wall")
            .collect();

        if walls.is_empty() {
            return results;
        }

        if towers.is_empty() {
            results.push(
                Diagnostic::error(
                    self.code(),
                    "Curtain walls exist but no towers found. Towers must anchor wall endpoints."
                        .to_string(),
                )
                .with_suggestion("Add tower entities at wall corners or endpoints.".to_string()),
            );
            return results;
        }

        for wall in &walls {
            let wall_cx = (wall.min_x + wall.max_x) / 2.0;
            let wall_cy = (wall.min_y + wall.max_y) / 2.0;

            let nearby = towers.iter().any(|t| {
                let tcx = (t.min_x + t.max_x) / 2.0;
                let tcy = (t.min_y + t.max_y) / 2.0;
                let dx = wall_cx - tcx;
                let dy = wall_cy - tcy;
                (dx * dx + dy * dy).sqrt() < CONNECTIVITY_DISTANCE_CM
            });

            if !nearby {
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Curtain wall {} is not near any tower (distance > {} cm).",
                            wall.mcp_id, CONNECTIVITY_DISTANCE_CM
                        ),
                    )
                    .with_mcp_id(wall.mcp_id.clone())
                    .with_suggestion(
                        "Place a tower within 600 cm of each wall segment endpoint.".to_string(),
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

    fn make_object(mcp_id: &str, x: f64, y: f64, sx: f64, sy: f64, kind: &str) -> SceneObject {
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
        }
    }

    fn footprint_for(obj: &SceneObject) -> Footprint2 {
        Footprint2::from_scene_object(obj, 0)
    }

    #[test]
    fn wall_near_tower_passes() {
        let rule = TowerWallConnectivity;
        let tower = make_object("tower_1", 0.0, 0.0, 4.0, 4.0, "tower");
        let wall = make_object("wall_1", 200.0, 0.0, 6.0, 1.0, "curtain_wall");
        let footprints = vec![footprint_for(&tower), footprint_for(&wall)];
        let diags = rule.validate(&[tower, wall], &footprints);
        assert!(diags.is_empty());
    }

    #[test]
    fn wall_far_from_tower_warns() {
        let rule = TowerWallConnectivity;
        let tower = make_object("tower_1", 0.0, 0.0, 4.0, 4.0, "tower");
        let wall = make_object("wall_1", 5000.0, 0.0, 6.0, 1.0, "curtain_wall");
        let footprints = vec![footprint_for(&tower), footprint_for(&wall)];
        let diags = rule.validate(&[tower, wall], &footprints);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "TOWER_WALL_CONNECTIVITY");
    }

    #[test]
    fn walls_without_towers_error() {
        let rule = TowerWallConnectivity;
        let wall = make_object("wall_1", 0.0, 0.0, 6.0, 1.0, "curtain_wall");
        let footprints = vec![footprint_for(&wall)];
        let diags = rule.validate(&[wall], &footprints);
        assert_eq!(diags.len(), 1);
        assert_eq!(
            diags[0].severity,
            crate::validation::diagnostic::Severity::Error
        );
    }
}
