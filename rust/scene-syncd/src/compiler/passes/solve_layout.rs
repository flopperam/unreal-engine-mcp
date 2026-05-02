use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::error::AppError;
use crate::layout::constraint::{evaluate_hard_constraints, evaluate_soft_score};
use crate::layout::constraint_extract::extract_constraints;
use crate::validation::diagnostic::{Diagnostic, Severity};

/// Constraint Solve Pass (Phase 5).
/// Extracts constraints from context, evaluates hard constraints (emitting Error
/// diagnostics with suggested_transform), and evaluates soft constraints
/// (emitting Info diagnostics with score).
pub struct ConstraintSolvePass;

impl Pass for ConstraintSolvePass {
    fn name(&self) -> &'static str {
        "constraint_solve"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        let geo_ir = ctx.geometric_ir.as_ref();
        let (hard, soft) = extract_constraints(&ctx.objects, &ctx.footprints, geo_ir);

        let repairs = evaluate_hard_constraints(&hard, &ctx.objects, &ctx.footprints);
        for repair in repairs {
            let mut diag = Diagnostic::error(
                &repair.constraint_code,
                format!("{}: {}", repair.message, repair.suggested_action),
            );
            if let Some(t) = repair.suggested_transform {
                diag = diag.with_suggested_transform(t);
            }
            ctx.add_diagnostics(vec![diag]);
        }

        let score = evaluate_soft_score(&soft);
        if score != 0.0 {
            ctx.add_diagnostics(vec![Diagnostic::info(
                "SOFT_CONSTRAINT_SCORE",
                format!("Layout soft constraint score: {:.2}", score),
            )]);
        }

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

    #[test]
    fn constraint_solve_pass_with_tower_emits_error() {
        let mut ctx = CompilerContext::new("test".to_string());
        let tower = make_obj("t1", "tower");
        ctx.objects = vec![tower.clone()];
        ctx.footprints = vec![crate::geom::footprint::Footprint2::from_scene_object(
            &tower, 0,
        )];
        let pass = ConstraintSolvePass;
        pass.run(&mut ctx).unwrap();
        assert!(ctx.diagnostics.iter().any(|d| {
            d.code == "TOWER_CONNECTED_TO_WALL" && matches!(d.severity, Severity::Error)
        }));
    }

    #[test]
    fn empty_scene_silent() {
        let mut ctx = CompilerContext::new("test".to_string());
        let pass = ConstraintSolvePass;
        pass.run(&mut ctx).unwrap();
        assert!(ctx.diagnostics.is_empty());
    }

    #[test]
    fn gate_facing_emits_soft_score() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![make_obj("g1", "gatehouse")];
        let pass = ConstraintSolvePass;
        pass.run(&mut ctx).unwrap();
        assert!(ctx
            .diagnostics
            .iter()
            .any(|d| d.code == "SOFT_CONSTRAINT_SCORE"));
    }
}
