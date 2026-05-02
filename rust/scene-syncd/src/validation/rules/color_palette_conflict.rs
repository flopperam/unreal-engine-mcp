use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;
use std::collections::HashMap;

/// Warns when objects of the same kind use conflicting color palettes.
/// A conflict is when two objects of the same kind have significantly
/// different primary colors (draft_color or material color).
pub struct ColorPaletteConflictRule;

impl ValidationRule for ColorPaletteConflictRule {
    fn code(&self) -> &'static str {
        "COLOR_PALETTE_CONFLICT"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();

        // Collect primary colors per kind from visual.draft.color
        let mut kind_colors: HashMap<String, Vec<([f64; 4], String)>> = HashMap::new();
        for obj in objects {
            if obj.deleted {
                continue;
            }
            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("unknown")
                .to_string();

            let color = obj
                .visual
                .get("draft")
                .and_then(|d| d.get("color"))
                .and_then(|c| c.as_array())
                .and_then(|arr| {
                    if arr.len() >= 3 {
                        Some([
                            arr[0].as_f64()?,
                            arr[1].as_f64()?,
                            arr[2].as_f64()?,
                            arr.get(3).and_then(|v| v.as_f64()).unwrap_or(1.0),
                        ])
                    } else {
                        None
                    }
                });
            if let Some(c) = color {
                kind_colors
                    .entry(kind)
                    .or_default()
                    .push((c, obj.mcp_id.clone()));
            }
        }

        for (kind, colors) in &kind_colors {
            if colors.len() < 2 {
                continue;
            }
            // Check if any two colors are significantly different.
            let first = &colors[0].0;
            for (i, (color, mcp_id)) in colors.iter().enumerate().skip(1) {
                let diff = color_diff(first, color);
                if diff > 0.3 {
                    diagnostics.push(
                        Diagnostic::warning(
                            self.code(),
                            format!(
                                "Kind '{}' has conflicting color palettes: object '{}' differs significantly from the first object (diff={:.2}).",
                                kind, mcp_id, diff
                            ),
                        )
                        .with_mcp_id(mcp_id.clone())
                        .with_suggestion(
                            format!("Use a consistent color palette for all '{}' objects", kind)
                        ),
                    );
                    break; // Only one warning per kind
                }
            }
        }

        diagnostics
    }
}

/// Compute a simple color difference metric (0.0 = identical, 1.0 = opposite).
fn color_diff(a: &[f64; 4], b: &[f64; 4]) -> f64 {
    let dr = (a[0] - b[0]).abs();
    let dg = (a[1] - b[1]).abs();
    let db = (a[2] - b[2]).abs();
    (dr + dg + db) / 3.0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, Transform, Vec3};
    use serde_json::json;

    fn make_obj_with_color(mcp_id: &str, kind: &str, color: [f64; 4]) -> SceneObject {
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
            visual: json!({"draft": {"color": color}}),
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
    fn warns_on_conflicting_colors() {
        let objects = vec![
            make_obj_with_color("t1", "tower", [0.6, 0.55, 0.5, 1.0]),
            make_obj_with_color("t2", "tower", [0.1, 0.2, 0.8, 1.0]),
        ];
        let rule = ColorPaletteConflictRule;
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.code == "COLOR_PALETTE_CONFLICT"));
    }

    #[test]
    fn no_warning_consistent_colors() {
        let objects = vec![
            make_obj_with_color("t1", "tower", [0.6, 0.55, 0.5, 1.0]),
            make_obj_with_color("t2", "tower", [0.62, 0.54, 0.51, 1.0]),
        ];
        let rule = ColorPaletteConflictRule;
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "COLOR_PALETTE_CONFLICT"));
    }
}
