use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::geom::spatial_index::SpatialSceneIndex;
use crate::geom::units::Cm;
use crate::layout::kind_registry::KindRegistry;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;
use std::collections::HashMap;

pub struct NoSameLayerOverlap;

impl ValidationRule for NoSameLayerOverlap {
    fn code(&self) -> &'static str {
        "NO_SAME_LAYER_OVERLAP"
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
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Objects {} and {} overlap on layer {} (2D footprint intersection)",
                            a.mcp_id, b.mcp_id, a.layer
                        ),
                    )
                    .with_mcp_id(a.mcp_id.clone())
                    .with_suggestion(format!(
                        "Move object {} away from {} by offset vector",
                        a.mcp_id, b.mcp_id
                    )),
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

    fn make_object(mcp_id: &str, x: f64, y: f64, sx: f64, sy: f64, kind_tag: &str) -> SceneObject {
        let mut obj = make_rotated_object(mcp_id, x, y, sx, sy, 0.0, kind_tag);
        obj.transform.rotation = Rotator {
            pitch: 0.0,
            yaw: 0.0,
            roll: 0.0,
        };
        obj
    }

    fn make_rotated_object(
        mcp_id: &str,
        x: f64,
        y: f64,
        sx: f64,
        sy: f64,
        yaw: f64,
        kind_tag: &str,
    ) -> SceneObject {
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
                    yaw,
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
    fn detects_same_layer_overlap() {
        let rule = NoSameLayerOverlap;
        // Two keep objects both on layer 0, overlapping at origin
        let objs = vec![
            make_object("a", 0.0, 0.0, 2.0, 2.0, "keep"),
            make_object("b", 0.0, 0.0, 2.0, 2.0, "keep"),
        ];
        let diags = rule.validate(&objs, &[]);
        assert!(
            diags.iter().any(|d| d.code == "NO_SAME_LAYER_OVERLAP"),
            "expected overlap warning"
        );
    }

    #[test]
    fn detects_rotated_wall_overlap() {
        let rule = NoSameLayerOverlap;
        // One axis-aligned thin wall and one rotated thin wall crossing at the origin.
        let objs = vec![
            make_object("a", 0.0, 0.0, 2.0, 0.2, "curtain_wall"),
            make_rotated_object("b", 0.0, 0.0, 2.0, 0.2, 45.0, "curtain_wall"),
        ];
        let diags = rule.validate(&objs, &[]);
        assert!(
            diags.iter().any(|d| d.code == "NO_SAME_LAYER_OVERLAP"),
            "expected overlap warning for rotated walls"
        );
    }
}
