use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Maximum reasonable Z height for a bridge endpoint above ground.
const MAX_BRIDGE_Z_CM: f64 = 800.0;
/// Minimum reasonable Z height (should not be buried deeply).
const MIN_BRIDGE_Z_CM: f64 = -200.0;

pub struct BridgeEndpointGrounded;

impl ValidationRule for BridgeEndpointGrounded {
    fn code(&self) -> &'static str {
        "BRIDGE_ENDPOINT_GROUNDED"
    }

    fn validate(&self, objects: &[SceneObject], _footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();

        for obj in objects {
            if obj.deleted {
                continue;
            }
            let is_bridge = obj.tags.iter().any(|t| t == "layout_kind:bridge");
            if !is_bridge {
                continue;
            }
            let z = obj.transform.location.z;
            if z < MIN_BRIDGE_Z_CM || z > MAX_BRIDGE_Z_CM {
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Bridge {} Z={:.1} cm is outside reasonable ground range [{:.1}, {:.1}] cm. Bridge may be floating or buried.",
                            obj.mcp_id, z, MIN_BRIDGE_Z_CM, MAX_BRIDGE_Z_CM
                        ),
                    )
                    .with_mcp_id(obj.mcp_id.clone())
                    .with_suggestion(
                        "Set bridge Z near ground level (0–200 cm) or adjust layer grounding.".to_string(),
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

    fn make_bridge(mcp_id: &str, z: f64) -> SceneObject {
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
                location: Vec3 { x: 0.0, y: 0.0, z },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw: 0.0,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: 10.0,
                    y: 2.0,
                    z: 1.0,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec!["layout_kind:bridge".to_string()],
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
    fn bridge_at_ground_level_passes() {
        let rule = BridgeEndpointGrounded;
        let obj = make_bridge("bridge_1", 100.0);
        let diags = rule.validate(&[obj], &[]);
        assert!(diags.is_empty());
    }

    #[test]
    fn floating_bridge_warns() {
        let rule = BridgeEndpointGrounded;
        let obj = make_bridge("bridge_1", 5000.0);
        let diags = rule.validate(&[obj], &[]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "BRIDGE_ENDPOINT_GROUNDED");
    }

    #[test]
    fn buried_bridge_warns() {
        let rule = BridgeEndpointGrounded;
        let obj = make_bridge("bridge_1", -500.0);
        let diags = rule.validate(&[obj], &[]);
        assert_eq!(diags.len(), 1);
        assert_eq!(diags[0].code, "BRIDGE_ENDPOINT_GROUNDED");
    }
}
