use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::ir::geometric::GeometricIr;
use crate::layout::constraint::{HardConstraint, SoftConstraint};

/// Extract hard and soft constraints from scene objects, footprints, and geometric IR.
pub fn extract_constraints(
    objects: &[SceneObject],
    footprints: &[Footprint2],
    _geometric_ir: Option<&GeometricIr>,
) -> (Vec<HardConstraint>, Vec<SoftConstraint>) {
    let mut hard = Vec::new();
    let mut soft = Vec::new();

    let mut tower_ids = Vec::new();
    let mut wall_ids = Vec::new();
    let mut keep_id: Option<String> = None;
    let mut gatehouse_id: Option<String> = None;
    let mut bridge_id: Option<String> = None;
    let mut road_id: Option<String> = None;

    for obj in objects {
        if obj.deleted {
            continue;
        }
        let kind = obj
            .tags
            .iter()
            .find_map(|t| t.strip_prefix("layout_kind:"))
            .unwrap_or("");
        match kind {
            "tower" => tower_ids.push(obj.mcp_id.clone()),
            "curtain_wall" => wall_ids.push(obj.mcp_id.clone()),
            "keep" => keep_id = Some(obj.mcp_id.clone()),
            "gatehouse" => gatehouse_id = Some(obj.mcp_id.clone()),
            "bridge" => bridge_id = Some(obj.mcp_id.clone()),
            "road" => road_id = Some(obj.mcp_id.clone()),
            _ => {}
        }
    }

    // Hard: TowerConnectedToWall for each tower.
    for tid in &tower_ids {
        hard.push(HardConstraint::TowerConnectedToWall {
            tower_id: tid.clone(),
        });
    }

    // Hard: KeepInsideBoundary if keep exists and walls exist.
    if let Some(ref kid) = keep_id {
        if !wall_ids.is_empty() {
            hard.push(HardConstraint::KeepInsideBoundary {
                keep_id: kid.clone(),
            });
        }
    }

    // Hard: GatehouseOnWall if gatehouse exists and walls exist.
    if let Some(ref gid) = gatehouse_id {
        if !wall_ids.is_empty() {
            hard.push(HardConstraint::GatehouseOnWall {
                gatehouse_id: gid.clone(),
            });
        }
    }

    // Hard: BridgeCrossesMoat if bridge exists.
    if let Some(ref bid) = bridge_id {
        hard.push(HardConstraint::BridgeCrossesMoat {
            bridge_id: bid.clone(),
        });
    }

    // Hard: NoOverlap for volume pairs on same layer.
    for i in 0..footprints.len() {
        for j in (i + 1)..footprints.len() {
            if footprints[i].layer == footprints[j].layer
                && footprints[i].intersects_2d(&footprints[j], crate::geom::units::Cm::ZERO)
            {
                hard.push(HardConstraint::NoOverlap {
                    entity_a: footprints[i].mcp_id.clone(),
                    entity_b: footprints[j].mcp_id.clone(),
                });
            }
        }
    }

    // Soft: GateFacing if gatehouse exists.
    if let Some(ref gid) = gatehouse_id {
        soft.push(SoftConstraint::GateFacing {
            gatehouse_id: gid.clone(),
            preferred_yaw: 180.0, // south-facing default
        });
    }

    // Soft: KeepNearCenter if keep exists.
    if let Some(ref kid) = keep_id {
        // Compute centroid of wall footprints as preferred center.
        let wall_fps: Vec<&Footprint2> = footprints
            .iter()
            .filter(|fp| {
                objects
                    .iter()
                    .find(|o| o.mcp_id == fp.mcp_id)
                    .and_then(|o| o.tags.iter().find_map(|t| t.strip_prefix("layout_kind:")))
                    == Some("curtain_wall")
            })
            .collect();
        let (sum_x, sum_y, count) = wall_fps.iter().fold((0.0, 0.0, 0usize), |acc, fp| {
            let cx = (fp.min_x + fp.max_x) / 2.0;
            let cy = (fp.min_y + fp.max_y) / 2.0;
            (acc.0 + cx, acc.1 + cy, acc.2 + 1)
        });
        let center_x = if count > 0 { sum_x / count as f64 } else { 0.0 };
        let center_y = if count > 0 { sum_y / count as f64 } else { 0.0 };
        soft.push(SoftConstraint::KeepNearCenter {
            keep_id: kid.clone(),
            center_x,
            center_y,
        });
    }

    // Soft: RoadConnects if road exists and both gatehouse + keep exist.
    if let Some(ref rid) = road_id {
        if let (Some(gid), Some(kid)) = (gatehouse_id.as_ref(), keep_id.as_ref()) {
            soft.push(SoftConstraint::RoadConnects {
                road_id: rid.clone(),
                from: gid.clone(),
                to: kid.clone(),
            });
        }
    }

    // Soft: NavCoverage if ground exists.
    if objects
        .iter()
        .any(|o| !o.deleted && o.tags.iter().any(|t| t == "layout_kind:ground"))
    {
        soft.push(SoftConstraint::NavCoverage {
            region_id: "main_ground".to_string(),
        });
    }

    (hard, soft)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_obj(mcp_id: &str, kind: &str) -> SceneObject {
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
    fn one_tower_one_wall_extracts_constraints() {
        let objects = vec![make_obj("t1", "tower"), make_obj("w1", "curtain_wall")];
        let (hard, soft) = extract_constraints(&objects, &[], None);
        assert_eq!(hard.len(), 1); // TowerConnectedToWall
        assert!(hard
            .iter()
            .any(|c| matches!(c, HardConstraint::TowerConnectedToWall { .. })));
    }

    #[test]
    fn keep_with_walls_extracts_boundary() {
        let objects = vec![make_obj("k1", "keep"), make_obj("w1", "curtain_wall")];
        let (hard, soft) = extract_constraints(&objects, &[], None);
        assert!(hard
            .iter()
            .any(|c| matches!(c, HardConstraint::KeepInsideBoundary { .. })));
    }

    #[test]
    fn empty_scene_no_constraints() {
        let (hard, soft) = extract_constraints(&[], &[], None);
        assert!(hard.is_empty());
        assert!(soft.is_empty());
    }
}
