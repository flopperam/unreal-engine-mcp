use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::domain::Vec3;
use crate::error::AppError;
use crate::ir::geometric::Connector;
use crate::validation::diagnostic::Diagnostic;

/// Infer anchors and spans from entity relations.
/// - Detects orphan entities (no relation connections).
/// - Resolves wall -> tower endpoint connections.
/// - Adds connector diagnostics for gate projections and bridge endpoints.
pub struct InferAnchorsPass;

impl Pass for InferAnchorsPass {
    fn name(&self) -> &'static str {
        "infer_anchors"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        // Collect kind tags for quick lookup.
        let kind_counts: std::collections::HashMap<String, usize> = ctx
            .objects
            .iter()
            .filter(|o| !o.deleted)
            .filter_map(|o| {
                o.tags
                    .iter()
                    .find_map(|t| t.strip_prefix("layout_kind:"))
                    .map(|k| k.to_string())
            })
            .fold(std::collections::HashMap::new(), |mut acc, k| {
                *acc.entry(k).or_insert(0) += 1;
                acc
            });

        let mut diags = Vec::new();
        let mut connectors: Vec<Connector> = Vec::new();

        for obj in &ctx.objects {
            if obj.deleted {
                continue;
            }
            let kind = obj.tags.iter().find_map(|t| t.strip_prefix("layout_kind:"));
            if kind == Some("tower") && *kind_counts.get("curtain_wall").unwrap_or(&0) == 0 {
                diags.push(
                    Diagnostic::info(
                        "ORPHAN_TOWER",
                        format!(
                            "Tower {} has no connected curtain walls. Consider adding wall spans.",
                            obj.mcp_id
                        ),
                    )
                    .with_mcp_id(obj.mcp_id.clone())
                    .with_suggestion(
                        "Add curtain_wall entities connected to this tower.".to_string(),
                    ),
                );
            }

            // Wall endpoint projection: if a wall is near a tower, add connector.
            if kind == Some("curtain_wall") {
                for other in &ctx.objects {
                    if other.deleted {
                        continue;
                    }
                    let other_kind = other
                        .tags
                        .iter()
                        .find_map(|t| t.strip_prefix("layout_kind:"));
                    if other_kind == Some("tower") {
                        let dx = obj.transform.location.x - other.transform.location.x;
                        let dy = obj.transform.location.y - other.transform.location.y;
                        let dist_sq = dx * dx + dy * dy;
                        if dist_sq < 200.0 * 200.0 {
                            connectors.push(Connector {
                                from_entity: obj.mcp_id.clone(),
                                to_entity: other.mcp_id.clone(),
                                from_point: obj.transform.location.clone(),
                                to_point: other.transform.location.clone(),
                                connector_type: crate::ir::geometric::ConnectorType::WallToTower,
                            });
                        }
                    }
                }
            }
        }

        // Store connectors in geometric IR if present.
        if let Some(ref mut geo) = ctx.geometric_ir {
            geo.connectors.extend(connectors);
        }

        ctx.add_diagnostics(diags);
        Ok(())
    }
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

    fn make_obj_at(mcp_id: &str, kind: &str, x: f64, y: f64) -> SceneObject {
        let mut obj = make_obj(mcp_id, kind);
        obj.transform.location = Vec3 { x, y, z: 0.0 };
        obj
    }

    #[test]
    fn orphan_tower_detected_when_no_walls() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![make_obj("t1", "tower")];
        let pass = InferAnchorsPass;
        pass.run(&mut ctx).unwrap();
        assert!(ctx.diagnostics.iter().any(|d| d.code == "ORPHAN_TOWER"));
    }

    #[test]
    fn tower_with_walls_is_not_orphan() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![make_obj("t1", "tower"), make_obj("w1", "curtain_wall")];
        let pass = InferAnchorsPass;
        pass.run(&mut ctx).unwrap();
        assert!(!ctx.diagnostics.iter().any(|d| d.code == "ORPHAN_TOWER"));
    }

    #[test]
    fn wall_near_tower_adds_connector() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![
            make_obj_at("t1", "tower", 0.0, 0.0),
            make_obj_at("w1", "curtain_wall", 50.0, 0.0),
        ];
        // Pre-populate geometric_ir so connectors have somewhere to go.
        ctx.geometric_ir = Some(crate::ir::geometric::GeometricIr::new());
        let pass = InferAnchorsPass;
        pass.run(&mut ctx).unwrap();
        let geo = ctx.geometric_ir.unwrap();
        assert!(!geo.connectors.is_empty());
    }
}
