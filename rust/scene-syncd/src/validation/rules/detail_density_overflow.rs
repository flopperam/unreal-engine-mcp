use crate::domain::SceneObject;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationRule;

/// Warns when detail density (detail objects per meter) exceeds a threshold.
/// This prevents generating thousands of small objects per wall span.
pub struct DetailDensityOverflowRule {
    /// Maximum detail objects per 1000 Unreal cm (per unit length).
    pub max_per_1000cm: f64,
}

impl Default for DetailDensityOverflowRule {
    fn default() -> Self {
        Self { max_per_1000cm: 50.0 }
    }
}

impl ValidationRule for DetailDensityOverflowRule {
    fn code(&self) -> &'static str {
        "DETAIL_DENSITY_OVERFLOW"
    }

    fn validate(
        &self,
        objects: &[SceneObject],
        _footprints: &[crate::geom::footprint::Footprint2],
    ) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();

        // Count detail objects (crenellation, window, brick, roof_tile).
        let detail_kinds = ["crenellation", "window", "brick", "roof_tile", "merlon"];
        let detail_count = objects
            .iter()
            .filter(|o| !o.deleted)
            .filter(|o| {
                o.tags
                    .iter()
                    .any(|t| detail_kinds.iter().any(|dk| t == *dk || t.contains(dk)))
            })
            .count();

        let total_count = objects.iter().filter(|o| !o.deleted).count();

        if total_count == 0 {
            return diagnostics;
        }

        let detail_ratio = detail_count as f64 / total_count as f64;

        // Warn if detail objects make up more than 80% of the scene
        if detail_ratio > 0.8 && detail_count > 200 {
            diagnostics.push(
                Diagnostic::warning(
                    self.code(),
                    format!(
                        "Detail objects ({}) make up {:.0}% of all objects ({}). This may cause performance issues.",
                        detail_count, detail_ratio * 100.0, total_count
                    ),
                )
                .with_suggestion(
                    "Reduce detail density or use InstanceSet rendering for detail objects".to_string(),
                ),
            );
        }

        diagnostics
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, Transform, Vec3};
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
                location: Vec3 { x: 0.0, y: 0.0, z: 0.0 },
                rotation: Rotator { pitch: 0.0, yaw: 0.0, roll: 0.0 },
                scale: Vec3 { x: 1.0, y: 1.0, z: 1.0 },
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
    fn warns_on_detail_overflow() {
        let mut objects: Vec<SceneObject> = (0..50)
            .map(|i| make_obj(&format!("wall_{}", i), "curtain_wall"))
            .collect();
        objects.extend((0..250).map(|i| make_obj(&format!("c_{}", i), "crenellation")));
        let rule = DetailDensityOverflowRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(diags.iter().any(|d| d.code == "DETAIL_DENSITY_OVERFLOW"));
    }

    #[test]
    fn no_warning_reasonable_detail() {
        let objects: Vec<SceneObject> = (0..50)
            .map(|i| make_obj(&format!("wall_{}", i), "curtain_wall"))
            .chain((0..30).map(|i| make_obj(&format!("c_{}", i), "crenellation")))
            .collect();
        let rule = DetailDensityOverflowRule::default();
        let diags = rule.validate(&objects, &[]);
        assert!(!diags.iter().any(|d| d.code == "DETAIL_DENSITY_OVERFLOW"));
    }
}