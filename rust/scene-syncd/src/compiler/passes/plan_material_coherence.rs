use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::error::AppError;
use crate::ir::material::{compute_material_plan, MaterialPlan};
use crate::ir::style::WorldStyleProfile;

/// Material coherence planner pass.
///
/// Reads the world style profile (from scene metadata or defaults) and
/// computes a MaterialPlan for each object. The material plans are stored
/// in CompilerContext for downstream passes to reference.
pub struct MaterialCoherencePass;

impl Pass for MaterialCoherencePass {
    fn name(&self) -> &'static str {
        "material_coherence"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        // Derive the world style profile from scene metadata, or use defaults.
        let style = WorldStyleProfile::from_metadata(&serde_json::json!({}))
            .unwrap_or_default();

        // Compute material plans for each non-deleted object.
        let mut material_plans: Vec<MaterialPlan> = Vec::new();

        for obj in &ctx.objects {
            if obj.deleted {
                continue;
            }

            let kind = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("unknown");

            let plan = compute_material_plan(&obj.mcp_id, kind, &style);
            material_plans.push(plan);
        }

        // Store material plans in context metadata for downstream passes.
        if !material_plans.is_empty() {
            ctx.semantic_ir = Some(crate::ir::semantic::SemanticScene {
                scene_id: ctx.scene_id.clone(),
                entities: vec![],
                metadata: serde_json::json!({}),
                style_profile: Some(style),
                material_plans,
            });
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_obj(kind: &str, mcp_id: &str) -> SceneObject {
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
    fn material_coherence_assigns_materials() {
        let mut ctx = CompilerContext::new("test_scene".to_string());
        ctx.objects = vec![make_obj("keep", "keep_1"), make_obj("curtain_wall", "wall_1")];
        let pass = MaterialCoherencePass;
        pass.run(&mut ctx).unwrap();

        assert!(ctx.semantic_ir.is_some());
        let semantic = ctx.semantic_ir.as_ref().unwrap();
        assert_eq!(semantic.material_plans.len(), 2);
        assert_eq!(semantic.material_plans[0].material_family, "limestone");
        assert!(semantic.style_profile.is_some());
    }

    #[test]
    fn material_coherence_skips_deleted() {
        let mut ctx = CompilerContext::new("test_scene".to_string());
        let mut obj = make_obj("keep", "keep_1");
        obj.deleted = true;
        ctx.objects = vec![obj];
        let pass = MaterialCoherencePass;
        pass.run(&mut ctx).unwrap();

        // semantic_ir should not be set when all objects are deleted
        assert!(ctx.semantic_ir.is_none() || ctx.semantic_ir.as_ref().unwrap().material_plans.is_empty());
    }
}