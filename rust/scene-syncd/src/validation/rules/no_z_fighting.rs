use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::geom::spatial_index::SpatialSceneIndex;
use crate::geom::units::Cm;
use crate::layout::kind_registry::{KindRegistry, LAYER_GAP};
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;
use std::collections::HashMap;

/// Z difference threshold for z-fighting detection (cm).
const Z_FIGHT_EPSILON: f64 = 5.0;

pub struct NoSameLayerZFight;

impl ValidationRule for NoSameLayerZFight {
    fn code(&self) -> &'static str {
        "NO_SAME_LAYER_Z_FIGHT"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let registry = KindRegistry::default();
        let mut layer_groups: HashMap<i32, Vec<Footprint2>> = HashMap::new();

        for obj in objects {
            if obj.deleted {
                continue;
            }
            let kind = obj.tags.iter().find_map(|t| t.strip_prefix("layout_kind:"));
            let layer = kind
                .and_then(|k| registry.get(k))
                .map(|s| s.layer)
                .unwrap_or(0);
            let fp = Footprint2::from_scene_object(obj, layer);
            layer_groups.entry(layer).or_default().push(fp);
        }

        let mut results = Vec::new();
        for (_layer, footprints) in layer_groups {
            let index = SpatialSceneIndex::from_footprints(footprints);
            for (a, b) in index.overlapping_pairs(Cm(1.0)) {
                let z_diff = (a.z - b.z).abs();
                if z_diff < Z_FIGHT_EPSILON && a.is_surface_like() && b.is_surface_like() {
                    results.push(
                        Diagnostic::warning(
                            self.code(),
                            format!(
                                "Z-fighting risk between {} and {}: z_diff={:.1} cm on layer {}. Suggest increasing layer offset or using volume primitives.",
                                a.mcp_id, b.mcp_id, z_diff, a.layer
                            ),
                        )
                        .with_mcp_id(a.mcp_id.clone())
                        .with_suggestion(format!(
                            "Increase vertical separation (current gap={:.1} cm, minimum recommended={:.1} cm) or convert to volumetric mesh",
                            z_diff,
                            LAYER_GAP
                        )),
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

    fn make_surface_object(mcp_id: &str, x: f64, y: f64, z: f64, kind_tag: &str) -> SceneObject {
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
                location: Vec3 { x, y, z },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw: 0.0,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: 100.0,
                    y: 100.0,
                    z: 1.0,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec![format!("layout_kind:{}", kind_tag)],
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
    fn detects_z_fighting_between_same_layer_surface_objects() {
        let rule = NoSameLayerZFight;
        // bridge is layer=1 and surface_like; two bridges overlapping closely in Z
        let objs = vec![
            make_surface_object("bridge_a", 0.0, 0.0, 0.0, "bridge"),
            make_surface_object("bridge_b", 0.0, 0.0, 2.0, "bridge"),
        ];
        let diags = rule.validate(&objs, &[]);
        assert!(
            diags.iter().any(|d| d.code == "NO_SAME_LAYER_Z_FIGHT"),
            "expected z-fighting warning"
        );
    }

    #[test]
    fn no_z_fighting_when_well_separated() {
        let rule = NoSameLayerZFight;
        let objs = vec![
            make_surface_object("ground", 0.0, 0.0, 0.0, "ground"),
            make_surface_object("moat", 0.0, 0.0, 200.0, "moat"),
        ];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.is_empty());
    }
}
