use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::error::AppError;
use crate::ir::world_cell::WorldCell;
use crate::sync::cell_aware::split_by_cell_availability;
use crate::sync::{SyncPlan, SyncPlanSummary};

/// 10th pass: converts SyncIr into CellAwareSyncPlan using `split_by_cell_availability`.
pub struct ApplyPlanPass {
    pub loaded_cell_ids: Vec<String>,
}

impl ApplyPlanPass {
    pub fn new(loaded_cell_ids: Vec<String>) -> Self {
        Self { loaded_cell_ids }
    }
}

impl Pass for ApplyPlanPass {
    fn name(&self) -> &'static str {
        "apply_plan"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        let sync_ir = match &ctx.sync_ir {
            Some(ir) => ir,
            None => {
                ctx.add_diagnostics(vec![crate::validation::diagnostic::Diagnostic::info(
                    "APPLY_PLAN_NO_SYNC_IR",
                    "No SyncIr available for apply_plan pass".to_string(),
                )]);
                return Ok(());
            }
        };

        let world_cells = &ctx.world_cells;
        let loaded: Vec<&str> = self.loaded_cell_ids.iter().map(|s| s.as_str()).collect();

        // Convert SyncIr operations to SyncPlan operations.
        let operations: Vec<crate::sync::SyncOperation> = sync_ir
            .operations
            .iter()
            .map(|op| match op {
                crate::ir::sync::SyncOperation::Create { mcp_id, object } => {
                    crate::sync::SyncOperation {
                        action: crate::sync::SyncAction::Create,
                        mcp_id: mcp_id.clone(),
                        reason: "Create from SyncIr".to_string(),
                        desired: Some(serde_json::to_value(object).unwrap_or_default()),
                        actual: None,
                    }
                }
                crate::ir::sync::SyncOperation::Update { mcp_id, object } => {
                    crate::sync::SyncOperation {
                        action: crate::sync::SyncAction::UpdateVisual,
                        mcp_id: mcp_id.clone(),
                        reason: "Update from SyncIr".to_string(),
                        desired: Some(serde_json::to_value(object).unwrap_or_default()),
                        actual: None,
                    }
                }
                crate::ir::sync::SyncOperation::Delete { mcp_id, object } => {
                    crate::sync::SyncOperation {
                        action: crate::sync::SyncAction::Delete,
                        mcp_id: mcp_id.clone(),
                        reason: "Delete from SyncIr".to_string(),
                        desired: Some(serde_json::to_value(object).unwrap_or_default()),
                        actual: None,
                    }
                }
                crate::ir::sync::SyncOperation::NoOp { mcp_id } => crate::sync::SyncOperation {
                    action: crate::sync::SyncAction::Noop,
                    mcp_id: mcp_id.clone(),
                    reason: "NoOp from SyncIr".to_string(),
                    desired: None,
                    actual: None,
                },
            })
            .collect();

        let plan = SyncPlan {
            scene_id: ctx.scene_id.clone(),
            summary: SyncPlanSummary::default(),
            operations,
            warnings: vec![],
        };

        let cell_plan = split_by_cell_availability(&plan, world_cells, &loaded);

        ctx.add_diagnostics(vec![crate::validation::diagnostic::Diagnostic::info(
            "APPLY_PLAN_SUMMARY",
            format!(
                "Cell-aware plan: {} loaded, {} deferred, {} cells affected",
                cell_plan.summary.loaded,
                cell_plan.summary.deferred,
                cell_plan.summary.cells_affected
            ),
        )]);

        Ok(())
    }
}
