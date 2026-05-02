use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use serde::{Deserialize, Serialize};

/// Hard constraint: must be satisfied for a valid layout.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum HardConstraint {
    /// Tower must connect to a wall endpoint or corner.
    TowerConnectedToWall { tower_id: String },
    /// Gatehouse must sit on a curtain wall segment.
    GatehouseOnWall { gatehouse_id: String },
    /// Bridge must cross the moat.
    BridgeCrossesMoat { bridge_id: String },
    /// Keep must be inside the wall boundary.
    KeepInsideBoundary { keep_id: String },
    /// Major volumes on the same layer must not overlap.
    NoOverlap { entity_a: String, entity_b: String },
}

/// Soft constraint: preferred but not required.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum SoftConstraint {
    /// Gate should face south (or preferred direction).
    GateFacing {
        gatehouse_id: String,
        preferred_yaw: f64,
    },
    /// Keep should be near the weighted center.
    KeepNearCenter {
        keep_id: String,
        center_x: f64,
        center_y: f64,
    },
    /// Road should connect gatehouse to keep.
    RoadConnects {
        road_id: String,
        from: String,
        to: String,
    },
    /// Nav walkable surface should cover required area.
    NavCoverage { region_id: String },
}

/// A repair suggestion emitted when a constraint is violated.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RepairSuggestion {
    pub constraint_code: String,
    pub message: String,
    pub suggested_action: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub suggested_transform: Option<crate::domain::Transform>,
}

/// Maximum distance from a curtain wall to its nearest tower for valid connectivity.
const CONNECTIVITY_DISTANCE_CM: f64 = 600.0;

/// Evaluate hard constraints and return repair suggestions for violations.
/// Now performs actual geometric checks for tower connectivity and keep boundary.
pub fn evaluate_hard_constraints(
    constraints: &[HardConstraint],
    objects: &[SceneObject],
    footprints: &[Footprint2],
) -> Vec<RepairSuggestion> {
    let mut results = Vec::new();

    // Build lookup maps for efficient access
    let footprint_by_id: std::collections::HashMap<&str, &Footprint2> = footprints
        .iter()
        .map(|fp| (fp.mcp_id.as_str(), fp))
        .collect();

    let object_by_id: std::collections::HashMap<&str, &SceneObject> =
        objects.iter().map(|o| (o.mcp_id.as_str(), o)).collect();

    for c in constraints {
        match c {
            HardConstraint::TowerConnectedToWall { tower_id } => {
                if let Some(tower_fp) = footprint_by_id.get(tower_id.as_str()) {
                    let tower_cx = (tower_fp.min_x + tower_fp.max_x) / 2.0;
                    let tower_cy = (tower_fp.min_y + tower_fp.max_y) / 2.0;

                    let connected = footprints.iter().any(|fp| {
                        if fp.mcp_id == *tower_id || fp.kind != "curtain_wall" {
                            return false;
                        }
                        let wall_cx = (fp.min_x + fp.max_x) / 2.0;
                        let wall_cy = (fp.min_y + fp.max_y) / 2.0;
                        let dx = wall_cx - tower_cx;
                        let dy = wall_cy - tower_cy;
                        (dx * dx + dy * dy).sqrt() < CONNECTIVITY_DISTANCE_CM
                    });

                    if !connected {
                        results.push(RepairSuggestion {
                            constraint_code: "TOWER_CONNECTED_TO_WALL".to_string(),
                            message: format!(
                                "Tower {} is not connected to a wall endpoint (no wall within {} cm).",
                                tower_id, CONNECTIVITY_DISTANCE_CM
                            ),
                            suggested_action: format!(
                                "Add a curtain_wall with a span connecting to tower {}.",
                                tower_id
                            ),
                            suggested_transform: None,
                        });
                    }
                }
            }
            HardConstraint::KeepInsideBoundary { keep_id } => {
                if let Some(keep_fp) = footprint_by_id.get(keep_id.as_str()) {
                    // Compute AABB of all castle boundary elements (walls, towers, gatehouse)
                    let boundary_kinds = ["curtain_wall", "tower", "gatehouse"];
                    let boundary_fps: Vec<&Footprint2> = footprints
                        .iter()
                        .filter(|fp| boundary_kinds.contains(&fp.kind.as_str()))
                        .collect();

                    if !boundary_fps.is_empty() {
                        let min_x = boundary_fps
                            .iter()
                            .map(|fp| fp.min_x)
                            .fold(f64::INFINITY, f64::min);
                        let max_x = boundary_fps
                            .iter()
                            .map(|fp| fp.max_x)
                            .fold(f64::NEG_INFINITY, f64::max);
                        let min_y = boundary_fps
                            .iter()
                            .map(|fp| fp.min_y)
                            .fold(f64::INFINITY, f64::min);
                        let max_y = boundary_fps
                            .iter()
                            .map(|fp| fp.max_y)
                            .fold(f64::NEG_INFINITY, f64::max);

                        let keep_cx = (keep_fp.min_x + keep_fp.max_x) / 2.0;
                        let keep_cy = (keep_fp.min_y + keep_fp.max_y) / 2.0;

                        let inside = keep_cx >= min_x
                            && keep_cx <= max_x
                            && keep_cy >= min_y
                            && keep_cy <= max_y;

                        if !inside {
                            results.push(RepairSuggestion {
                                constraint_code: "KEEP_INSIDE_BOUNDARY".to_string(),
                                message: format!(
                                    "Keep {} is outside the castle wall boundary.",
                                    keep_id
                                ),
                                suggested_action:
                                    "Move the keep inside the perimeter or expand walls."
                                        .to_string(),
                                suggested_transform: None,
                            });
                        }
                    }
                }
            }
            _ => {}
        }
    }
    results
}

/// Evaluate soft constraints and return a score (higher = better).
/// Phase 5: now computes gate facing score and keep center score.
pub fn evaluate_soft_score(constraints: &[SoftConstraint]) -> f64 {
    let mut score = 0.0;
    for c in constraints {
        match c {
            SoftConstraint::GateFacing {
                gatehouse_id: _,
                preferred_yaw,
            } => {
                // Reward facing the preferred direction (closer yaw = higher score).
                // Skeleton: neutral if yaw matches exactly, penalty for deviation.
                let deviation = (preferred_yaw - 180.0).abs(); // assume south = 180 deg
                score += (1.0 - (deviation / 360.0)).max(0.0);
            }
            SoftConstraint::KeepNearCenter {
                keep_id: _,
                center_x,
                center_y,
            } => {
                // Reward proximity to center (skeleton: assumes keep is at origin).
                let dx = center_x;
                let dy = center_y;
                let dist = (dx * dx + dy * dy).sqrt();
                let max_dist = 5000.0; // 50m
                score += (1.0 - (dist / max_dist).min(1.0)).max(0.0);
            }
            _ => {}
        }
    }
    score
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
    fn hard_constraints_produce_repairs() {
        let constraints = vec![
            HardConstraint::TowerConnectedToWall {
                tower_id: "t1".to_string(),
            },
            HardConstraint::KeepInsideBoundary {
                keep_id: "k1".to_string(),
            },
        ];
        let objects = vec![
            make_object("t1", 0.0, 0.0, 4.0, 4.0, "tower"),
            make_object("k1", 5000.0, 5000.0, 4.0, 4.0, "keep"),
        ];
        let footprints = vec![footprint_for(&objects[0]), footprint_for(&objects[1])];
        let suggestions = evaluate_hard_constraints(&constraints, &objects, &footprints);
        assert_eq!(suggestions.len(), 2);
        assert_eq!(suggestions[0].constraint_code, "TOWER_CONNECTED_TO_WALL");
        assert_eq!(suggestions[1].constraint_code, "KEEP_INSIDE_BOUNDARY");
    }

    #[test]
    fn tower_near_wall_passes() {
        let tower = make_object("t1", 0.0, 0.0, 4.0, 4.0, "tower");
        let wall = make_object("w1", 200.0, 0.0, 6.0, 1.0, "curtain_wall");
        let objects = vec![tower.clone(), wall.clone()];
        let footprints = vec![footprint_for(&tower), footprint_for(&wall)];
        let constraints = vec![HardConstraint::TowerConnectedToWall {
            tower_id: "t1".to_string(),
        }];
        let suggestions = evaluate_hard_constraints(&constraints, &objects, &footprints);
        assert!(suggestions.is_empty(), "tower near wall should pass");
    }

    #[test]
    fn keep_inside_boundary_passes() {
        let wall1 = make_object("w1", -500.0, 0.0, 10.0, 1.0, "curtain_wall");
        let wall2 = make_object("w2", 500.0, 0.0, 10.0, 1.0, "curtain_wall");
        let wall3 = make_object("w3", 0.0, -500.0, 1.0, 10.0, "curtain_wall");
        let wall4 = make_object("w4", 0.0, 500.0, 1.0, 10.0, "curtain_wall");
        let keep = make_object("k1", 0.0, 0.0, 4.0, 4.0, "keep");
        let objects = vec![wall1, wall2, wall3, wall4, keep.clone()];
        let footprints: Vec<Footprint2> = objects.iter().map(footprint_for).collect();
        let constraints = vec![HardConstraint::KeepInsideBoundary {
            keep_id: "k1".to_string(),
        }];
        let suggestions = evaluate_hard_constraints(&constraints, &objects, &footprints);
        assert!(suggestions.is_empty(), "keep inside boundary should pass");
    }

    #[test]
    fn keep_outside_boundary_fails() {
        let wall = make_object("w1", 0.0, 0.0, 10.0, 1.0, "curtain_wall");
        let keep = make_object("k1", 5000.0, 0.0, 4.0, 4.0, "keep");
        let objects = vec![wall, keep.clone()];
        let footprints = vec![footprint_for(&objects[0]), footprint_for(&keep)];
        let constraints = vec![HardConstraint::KeepInsideBoundary {
            keep_id: "k1".to_string(),
        }];
        let suggestions = evaluate_hard_constraints(&constraints, &objects, &footprints);
        assert_eq!(suggestions.len(), 1);
        assert_eq!(suggestions[0].constraint_code, "KEEP_INSIDE_BOUNDARY");
    }

    #[test]
    fn soft_score_gate_facing() {
        let constraints = vec![SoftConstraint::GateFacing {
            gatehouse_id: "g1".to_string(),
            preferred_yaw: 180.0, // south
        }];
        let score = evaluate_soft_score(&constraints);
        assert!(score > 0.5);
    }

    #[test]
    fn soft_score_keep_near_center() {
        let constraints = vec![SoftConstraint::KeepNearCenter {
            keep_id: "k1".to_string(),
            center_x: 0.0,
            center_y: 0.0,
        }];
        let score = evaluate_soft_score(&constraints);
        assert_eq!(score, 1.0);
    }

    #[test]
    fn soft_score_keep_far_from_center() {
        let constraints = vec![SoftConstraint::KeepNearCenter {
            keep_id: "k1".to_string(),
            center_x: 10000.0,
            center_y: 0.0,
        }];
        let score = evaluate_soft_score(&constraints);
        assert!(score < 1.0);
    }
}
