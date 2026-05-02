use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::geom::units::Cm;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

pub struct BridgeCrossesMoat;

impl ValidationRule for BridgeCrossesMoat {
    fn code(&self) -> &'static str {
        "BRIDGE_CROSSES_MOAT"
    }

    fn validate(&self, _objects: &[SceneObject], footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();

        let bridges: Vec<&Footprint2> =
            footprints.iter().filter(|fp| fp.kind == "bridge").collect();

        let moats: Vec<&Footprint2> = footprints.iter().filter(|fp| fp.kind == "moat").collect();

        if bridges.is_empty() || moats.is_empty() {
            return results;
        }

        for bridge in &bridges {
            let crosses = moats.iter().any(|moat| bridge.intersects_2d(moat, Cm(1.0)));
            if !crosses {
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Bridge {} does not cross any moat. Bridges should span the moat to provide entry.",
                            bridge.mcp_id
                        ),
                    )
                    .with_mcp_id(bridge.mcp_id.clone())
                    .with_suggestion(
                        "Position the bridge so its footprint intersects the moat footprint.".to_string(),
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
    fn bridge_crossing_moat_passes() {
        let rule = BridgeCrossesMoat;
        let (_b, fb) = make_obj("bridge", 0.0, 0.0, 10.0, 2.0, "bridge");
        let (_m, fm) = make_obj("moat", 0.0, 0.0, 4.0, 4.0, "moat");
        let diags = rule.validate(&[], &[fb, fm]);
        assert!(diags.is_empty());
    }

    #[test]
    fn bridge_not_crossing_moat_warns() {
        let rule = BridgeCrossesMoat;
        let (_b, fb) = make_obj("bridge", 5000.0, 0.0, 10.0, 2.0, "bridge");
        let (_m, fm) = make_obj("moat", 0.0, 0.0, 4.0, 4.0, "moat");
        let diags = rule.validate(&[], &[fb, fm]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "BRIDGE_CROSSES_MOAT");
    }
}
