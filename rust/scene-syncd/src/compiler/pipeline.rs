use crate::compiler::context::CompilerContext;
use crate::compiler::ir::{CompileResult, CompileSummary};
use crate::compiler::passes::diff::DiffPlanningPass;
use crate::compiler::passes::graph_build::GraphBuildPass;
use crate::compiler::passes::import::ImportPass;
use crate::compiler::passes::infer_anchors::InferAnchorsPass;
use crate::compiler::passes::lower_geometry::GeometryLoweringPass;
use crate::compiler::passes::normalize::NormalizePass;
use crate::compiler::passes::plan_density_lod::DensityPlannerPass;
use crate::compiler::passes::realize::RealizePass;
use crate::compiler::passes::solve_layout::ConstraintSolvePass;
use crate::compiler::passes::validate::ValidatePass;
use crate::compiler::passes::Pass;
use crate::db::SurrealSceneRepository;
use crate::error::AppError;
use crate::layout::denormalizer::denormalize_layout;
use crate::layout::entity_resolver::resolve_span;
use crate::layout::kind_registry::KindRegistry;
use crate::ir::render_plan::RenderPlan;
use crate::ir::world_cell::partition_into_cells;
use std::collections::HashMap;

pub struct CompilerPipeline {
    pub context: CompilerContext,
    passes: Vec<Box<dyn Pass>>,
}

impl CompilerPipeline {
    pub fn new(context: CompilerContext, passes: Vec<Box<dyn Pass>>) -> Self {
        Self { context, passes }
    }

    /// Async preparation: load entities/relations from DB and run denormalization.
    pub async fn prepare(
        repo: &SurrealSceneRepository,
        scene_id: &str,
    ) -> Result<Self, AppError> {
        let entities = repo.list_entities(scene_id, None).await?;
        let relations = repo.list_relations(scene_id, None).await?;
        let registry = KindRegistry::default();
        let objects = denormalize_layout(scene_id, &entities, &relations, &registry)?;

        // Compute spans for entities that have them (curtain_wall, bridge, etc.)
        let entity_by_id: HashMap<&str, &crate::domain::SceneEntity> = entities
            .iter()
            .map(|e| (e.entity_id.as_str(), e))
            .collect();
        let spans: Vec<(String, crate::layout::span::Span)> = entities
            .iter()
            .filter_map(|e| {
                resolve_span(e, &relations, &entity_by_id)
                    .map(|span| (e.entity_id.clone(), span))
            })
            .collect();

        let mut context = CompilerContext::new(scene_id.to_string());
        context.objects = objects;
        context.entities = entities;
        context.spans = spans;

        let passes: Vec<Box<dyn Pass>> = vec![
            Box::new(ImportPass),
            Box::new(GraphBuildPass),
            Box::new(InferAnchorsPass),
            Box::new(GeometryLoweringPass),
            Box::new(NormalizePass),
            Box::new(RealizePass::blockout()),
            Box::new(ConstraintSolvePass),
            Box::new(ValidatePass::default()),
            Box::new(DensityPlannerPass),
            Box::new(DiffPlanningPass::new()),
        ];

        Ok(Self { context, passes })
    }

    /// Run the prepared pipeline synchronously.
    pub fn run(&mut self,
        stage: &str,
    ) -> Result<CompileResult, AppError> {
        let mut failed_validation = false;

        for pass in &self.passes {
            pass.run(&mut self.context)?;

            // Safety stop: if validation produced errors, skip downstream passes
            // (diff, apply planning) but still return diagnostics to the caller.
            if pass.name() == "validate" {
                let errors = self
                    .context
                    .diagnostics
                    .iter()
                    .filter(|d| {
                        matches!(
                            d.severity,
                            crate::validation::diagnostic::Severity::Error
                        )
                    })
                    .count();
                if errors > 0 {
                    failed_validation = true;
                    break;
                }
            }
        }

        let stage = if failed_validation {
            "failed_validation"
        } else {
            stage
        };

        let errors = self
            .context
            .diagnostics
            .iter()
            .filter(|d| {
                matches!(
                    d.severity,
                    crate::validation::diagnostic::Severity::Error
                )
            })
            .count();
        let warnings = self
            .context
            .diagnostics
            .iter()
            .filter(|d| {
                matches!(
                    d.severity,
                    crate::validation::diagnostic::Severity::Warning
                )
            })
            .count();
        let infos = self
            .context
            .diagnostics
            .iter()
            .filter(|d| {
                matches!(
                    d.severity,
                    crate::validation::diagnostic::Severity::Info
                )
            })
            .count();

        let object_count = self.context.objects.len();
        let objects = std::mem::take(&mut self.context.objects);
        let diagnostics = std::mem::take(&mut self.context.diagnostics);

        // Phase 3: Density/LOD planner produces render_plan + instance_sets.
        // If validation failed early, the pass never ran: fall back to all-actors.
        let render_plan = self
            .context
            .render_plan
            .take()
            .unwrap_or_else(|| RenderPlan::all_actors(&self.context.scene_id, &objects));
        let instance_sets = std::mem::take(&mut self.context.instance_sets);
        let instance_set_count = instance_sets.len();

        // Phase 6: World Partition cell assignment
        let positions: Vec<(String, f64, f64)> = objects
            .iter()
            .map(|o| (o.mcp_id.clone(), o.transform.location.x, o.transform.location.y))
            .collect();
        let mut world_cells = partition_into_cells(&positions, 5000.0);
        for cell in &mut world_cells {
            let cell_objects: Vec<crate::domain::SceneObject> = objects
                .iter()
                .filter(|o| cell.object_ids.contains(&o.mcp_id))
                .cloned()
                .collect();
            cell.dirty_hash = crate::ir::world_cell::compute_dirty_hash(&cell_objects);
        }
        let world_cell_count = world_cells.len();

        Ok(CompileResult {
            scene_id: self.context.scene_id.clone(),
            stage: stage.to_string(),
            mode: Some(stage.to_string()),
            objects,
            instance_sets,
            world_cells,
            diagnostics,
            summary: CompileSummary {
                errors,
                warnings,
                infos,
                objects: object_count,
                instance_sets: instance_set_count,
                world_cells: world_cell_count,
            },
            render_plan: Some(render_plan),
        })
    }

    /// Convenience: prepare + run in one call.
    pub async fn compile_preview(
        repo: &SurrealSceneRepository,
        scene_id: &str,
    ) -> Result<CompileResult, AppError> {
        let mut pipeline = Self::prepare(repo, scene_id).await?;
        pipeline.run("preview")
    }

    /// Run only up to ValidatePass (lightweight, no DB write).
    pub async fn compile_validate_only(
        repo: &SurrealSceneRepository,
        scene_id: &str,
    ) -> Result<CompileResult, AppError> {
        let mut pipeline = Self::prepare(repo, scene_id).await?;
        // Truncate to only run through validate.
        pipeline.passes.retain(|p| {
            let name = p.name();
            name != "diff"
        });
        pipeline.run("validate")
    }

    /// Full compile + diff with actual snapshot.
    pub async fn compile_plan(
        repo: &SurrealSceneRepository,
        scene_id: &str,
        actual: Vec<crate::domain::SceneObject>,
    ) -> Result<CompileResult, AppError> {
        let mut pipeline = Self::prepare(repo, scene_id).await?;
        // Replace the default diff pass with one that has actual snapshot.
        pipeline.passes.retain(|p| p.name() != "diff");
        pipeline.passes.push(Box::new(DiffPlanningPass::with_actual(actual)));
        pipeline.run("plan")
    }

    /// Compile + validate (stop on error) + plan + apply.
    pub async fn compile_apply(
        repo: &SurrealSceneRepository,
        scene_id: &str,
        _allow_delete: bool,
    ) -> Result<CompileResult, AppError> {
        let mut pipeline = Self::prepare(repo, scene_id).await?;
        let result = pipeline.run("apply")?;
        if result.summary.errors > 0 {
            return Err(AppError::Validation(format!(
                "Compilation failed with {} error(s)",
                result.summary.errors
            )));
        }
        Ok(result)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use crate::validation::diagnostic::Severity;
    use serde_json::json;

    #[test]
    fn pipeline_runs_passes_and_collects_diagnostics() {
        let mut ctx = CompilerContext::new("test_scene".to_string());
        ctx.objects = vec![
            SceneObject {
                id: String::new(),
                scene: "scene:test".to_string(),
                group: None,
                mcp_id: "obj_1".to_string(),
                desired_name: "obj_1".to_string(),
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
                tags: vec!["layout_kind:keep".to_string()],
                metadata: json!({}),
                desired_hash: String::new(),
                last_applied_hash: None,
                sync_status: "pending".to_string(),
                deleted: false,
                revision: 1,
                created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
                updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            },
        ];

        let mut pipeline = CompilerPipeline {
            context: ctx,
            passes: vec![
                Box::new(GeometryLoweringPass),
                Box::new(NormalizePass),
                Box::new(ValidatePass::default()),
            ],
        };

        let result = pipeline.run("preview").unwrap();
        assert_eq!(result.scene_id, "test_scene");
        assert_eq!(result.stage, "preview");
        assert_eq!(result.objects.len(), 1);
        assert_eq!(result.summary.objects, 1);
        // A single keep object should have no errors/warnings
        assert_eq!(result.summary.errors, 0);
    }

    #[test]
    fn pipeline_detects_zero_scale() {
        let mut ctx = CompilerContext::new("test_scene".to_string());
        ctx.objects = vec![SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: "bad".to_string(),
            desired_name: "bad".to_string(),
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
                    x: 0.0,
                    y: 1.0,
                    z: 1.0,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec![],
            metadata: json!({}),
            desired_hash: String::new(),
            last_applied_hash: None,
            sync_status: "pending".to_string(),
            deleted: false,
            revision: 1,
            created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
        }];

        let mut pipeline = CompilerPipeline {
            context: ctx,
            passes: vec![
                Box::new(GeometryLoweringPass),
                Box::new(NormalizePass),
                Box::new(ValidatePass::default()),
            ],
        };

        let result = pipeline.run("preview").unwrap();
        assert!(
            result.diagnostics.iter().any(|d| {
                d.code == "NO_ZERO_OR_NEGATIVE_SCALE"
                    && matches!(d.severity, Severity::Error)
            }),
            "expected zero-scale error"
        );
        assert_eq!(result.summary.errors, 1);
    }

    #[test]
    fn preview_result_summary_matches() {
        let mut ctx = CompilerContext::new("snapshot_scene".to_string());
        ctx.objects = vec![
            SceneObject {
                id: String::new(),
                scene: "scene:test".to_string(),
                group: None,
                mcp_id: "keep_1".to_string(),
                desired_name: "keep_1".to_string(),
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
                tags: vec!["layout_kind:keep".to_string()],
                metadata: json!({}),
                desired_hash: String::new(),
                last_applied_hash: None,
                sync_status: "pending".to_string(),
                deleted: false,
                revision: 1,
                created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
                updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            },
        ];

        let mut pipeline = CompilerPipeline {
            context: ctx,
            passes: vec![
                Box::new(GeometryLoweringPass),
                Box::new(NormalizePass),
                Box::new(ValidatePass::default()),
            ],
        };

        let result = pipeline.run("preview").unwrap();
        assert_eq!(result.scene_id, "snapshot_scene");
        assert_eq!(result.stage, "preview");
        assert_eq!(result.summary.objects, 1);
        assert_eq!(result.summary.errors, 0);
        assert!(!result.objects.is_empty());
    }
}
