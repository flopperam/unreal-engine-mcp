use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::error::AppError;
use crate::ir::semantic::SemanticScene;
use crate::ir::source_map::SourceMap;

/// Import pass: validates that objects were loaded and records basic diagnostics.
/// The actual denormalization is done during `CompilerPipeline::prepare` (async DB call).
/// This pass serves as the formal entry point in the compiler pipeline.
pub struct ImportPass;

impl Pass for ImportPass {
    fn name(&self) -> &'static str {
        "import"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        if ctx.objects.is_empty() {
            ctx.add_diagnostics(vec![crate::validation::diagnostic::Diagnostic::info(
                "IMPORT_EMPTY",
                format!(
                    "Scene '{}' produced no objects after denormalization",
                    ctx.scene_id
                ),
            )]);
        }

        // Generate SemanticScene and SourceMap from denormalized objects.
        let semantic = SemanticScene::from_objects(ctx.scene_id.clone(), &ctx.objects);
        let mut source_map = SourceMap::new();
        for entity in &semantic.entities {
            source_map.register(&entity.entity_id, &entity.entity_id);
        }
        ctx.semantic_ir = Some(semantic);
        ctx.source_map = Some(source_map);

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn import_pass_warns_on_empty() {
        let mut ctx = CompilerContext::new("empty_scene".to_string());
        let pass = ImportPass;
        pass.run(&mut ctx).unwrap();
        assert_eq!(ctx.diagnostics.len(), 1);
        assert_eq!(ctx.diagnostics[0].code, "IMPORT_EMPTY");
    }

    #[test]
    fn import_pass_silent_with_objects() {
        let mut ctx = CompilerContext::new("test_scene".to_string());
        ctx.objects = vec![crate::domain::SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: "obj".to_string(),
            desired_name: "obj".to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: serde_json::json!({}),
            transform: crate::domain::Transform::default(),
            visual: serde_json::json!({}),
            physics: serde_json::json!({}),
            tags: vec![
                "layout_entity:obj".to_string(),
                "layout_kind:keep".to_string(),
            ],
            metadata: serde_json::json!({}),
            desired_hash: String::new(),
            last_applied_hash: None,
            sync_status: "pending".to_string(),
            deleted: false,
            revision: 1,
            created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
        }];
        let pass = ImportPass;
        pass.run(&mut ctx).unwrap();
        assert!(ctx.diagnostics.is_empty());
        assert!(ctx.semantic_ir.is_some());
        assert!(ctx.source_map.is_some());
    }
}
