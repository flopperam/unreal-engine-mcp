use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Minimum gate opening width in Unreal cm.
/// Scale 1.0 corresponds to 100 cm width in footprint.rs, so 3.0 = 300 cm.
const MIN_GATE_SCALE: f64 = 3.0;

pub struct GateOpeningWidth;

impl ValidationRule for GateOpeningWidth {
    fn code(&self) -> &'static str {
        "GATE_OPENING_WIDTH"
    }

    fn validate(&self, objects: &[SceneObject], _footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();

        for obj in objects {
            if obj.deleted {
                continue;
            }
            let is_gate = obj.tags.iter().any(|t| t == "layout_kind:gatehouse");
            if !is_gate {
                continue;
            }
            let sx = obj.transform.scale.x.abs();
            let sy = obj.transform.scale.y.abs();
            let width = sx.max(sy);
            if width < MIN_GATE_SCALE {
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Gatehouse {} width scale {:.1} is below minimum {:.1} ({} cm). Gate may be too narrow for passage.",
                            obj.mcp_id,
                            width,
                            MIN_GATE_SCALE,
                            MIN_GATE_SCALE * 100.0
                        ),
                    )
                    .with_mcp_id(obj.mcp_id.clone())
                    .with_suggestion(
                        format!(
                            "Increase gatehouse scale to at least {:.1} in the wider axis ({} cm).",
                            MIN_GATE_SCALE,
                            MIN_GATE_SCALE * 100.0
                        ),
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

    fn make_gate(mcp_id: &str, scale: f64) -> SceneObject {
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
                    x: scale,
                    y: scale,
                    z: 1.0,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec!["layout_kind:gatehouse".to_string()],
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
    fn wide_gate_passes() {
        let rule = GateOpeningWidth;
        let obj = make_gate("gate_1", 5.0);
        let diags = rule.validate(&[obj], &[]);
        assert!(diags.is_empty());
    }

    #[test]
    fn narrow_gate_warns() {
        let rule = GateOpeningWidth;
        let obj = make_gate("gate_1", 1.5);
        let diags = rule.validate(&[obj], &[]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "GATE_OPENING_WIDTH");
    }
}
