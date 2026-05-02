use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Wall span validity: length > 0 and thickness >= minimum.
pub struct WallSpanValid;

const MIN_WALL_THICKNESS: f64 = 10.0; // cm

impl ValidationRule for WallSpanValid {
    fn code(&self) -> &'static str {
        "WALL_SPAN_INVALID"
    }

    fn validate(&self, objects: &[SceneObject], _footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut results = Vec::new();
        for obj in objects {
            if obj.deleted {
                continue;
            }
            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("");
            if kind != "curtain_wall" {
                continue;
            }
            let sx = obj.transform.scale.x.abs();
            let sy = obj.transform.scale.y.abs();
            if sx <= 0.0 || sy <= 0.0 {
                results.push(
                    Diagnostic::error(
                        self.code(),
                        format!("Wall {} has zero or negative span", obj.mcp_id),
                    )
                    .with_mcp_id(obj.mcp_id.clone())
                    .with_suggestion("Use a positive scale for both wall axes".to_string()),
                );
                continue;
            }
            let (length, thickness) = if sx >= sy { (sx, sy) } else { (sy, sx) };
            if thickness < MIN_WALL_THICKNESS {
                results.push(
                    Diagnostic::warning(
                        self.code(),
                        format!(
                            "Wall {} thickness ({:.1} cm) is below minimum {:.1} cm",
                            obj.mcp_id, thickness, MIN_WALL_THICKNESS
                        ),
                    )
                    .with_mcp_id(obj.mcp_id.clone())
                    .with_suggestion(format!(
                        "Increase the smaller scale axis to at least {:.1}",
                        MIN_WALL_THICKNESS
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

    fn make_wall(sx: f64, sy: f64) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: "wall_1".to_string(),
            desired_name: "wall_1".to_string(),
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
        }
    }

    #[test]
    fn thin_wall_warns() {
        let rule = WallSpanValid;
        // thickness = 5 cm < MIN_WALL_THICKNESS
        let objs = vec![make_wall(100.0, 0.1)];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.iter().any(|d| d.code == "WALL_SPAN_INVALID"));
    }

    #[test]
    fn thick_wall_passes() {
        let rule = WallSpanValid;
        let objs = vec![make_wall(100.0, 50.0)];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.is_empty());
    }

    #[test]
    fn zero_length_error() {
        let rule = WallSpanValid;
        let objs = vec![make_wall(0.0, 50.0)];
        let diags = rule.validate(&objs, &[]);
        assert!(diags.iter().any(|d| {
            d.code == "WALL_SPAN_INVALID"
                && matches!(d.severity, crate::validation::diagnostic::Severity::Error)
        }));
    }
}
