use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::domain::transform::compute_desired_hash;
use crate::error::AppError;

pub struct NormalizePass;

impl Pass for NormalizePass {
    fn name(&self) -> &'static str {
        "normalize"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        let mut deferred_diags = Vec::new();
        for obj in &mut ctx.objects {
            // Stable tag ordering for deterministic hashing
            obj.tags.sort();
            obj.tags.dedup();

            // Compute desired_hash if missing
            if obj.desired_hash.is_empty() {
                match compute_desired_hash(obj) {
                    Ok(hash) => obj.desired_hash = hash,
                    Err(e) => {
                        // Non-fatal: defer diagnostic to avoid borrow conflict
                        deferred_diags.push(
                            crate::validation::diagnostic::Diagnostic::warning(
                                "HASH_COMPUTE_FAILED",
                                format!("Failed to compute desired_hash for {}: {}", obj.mcp_id, e),
                            )
                            .with_mcp_id(obj.mcp_id.clone()),
                        );
                    }
                }
            }
        }
        if !deferred_diags.is_empty() {
            ctx.add_diagnostics(deferred_diags);
        }
        Ok(())
    }
}
