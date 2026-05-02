use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::geom::units::Cm;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

pub struct MoatOffsetValidity;

impl ValidationRule for MoatOffsetValidity {
    fn code(&self) -> &'static str {
        "MOAT_OFFSET_VALIDITY"
    }

    fn validate(&self, _objects: &[SceneObject], footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();

        let moats: Vec<&Footprint2> = footprints.iter().filter(|fp| fp.kind == "moat").collect();

        let structures: Vec<&Footprint2> = footprints
            .iter()
            .filter(|fp| {
                fp.kind == "keep"
                    || fp.kind == "tower"
                    || fp.kind == "curtain_wall"
                    || fp.kind == "gatehouse"
            })
            .collect();

        for moat in &moats {
            for structure in &structures {
                if moat.intersects_2d(structure, Cm(1.0)) {
                    results.push(
                        Diagnostic::warning(
                            self.code(),
                            format!(
                                "Moat {} overlaps with {} {}. Moat should surround structures, not intersect them.",
                                moat.mcp_id, structure.kind, structure.mcp_id
                            ),
                        )
                        .with_mcp_id(moat.mcp_id.clone())
                        .with_suggestion(
                            "Offset the moat outward from walls and keep.".to_string(),
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
    fn moat_overlapping_wall_warns() {
        let rule = MoatOffsetValidity;
        let (_w, fw) = make_obj("wall", 0.0, 0.0, 4.0, 4.0, "curtain_wall");
        let (_m, fm) = make_obj("moat", 0.0, 0.0, 4.0, 4.0, "moat");
        let diags = rule.validate(&[], &[fw, fm]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "MOAT_OFFSET_VALIDITY");
    }

    #[test]
    fn moat_separated_passes() {
        let rule = MoatOffsetValidity;
        let (_w, fw) = make_obj("wall", 0.0, 0.0, 4.0, 4.0, "curtain_wall");
        let (_m, fm) = make_obj("moat", 1000.0, 0.0, 4.0, 4.0, "moat");
        let diags = rule.validate(&[], &[fw, fm]);
        assert!(diags.is_empty());
    }
}
