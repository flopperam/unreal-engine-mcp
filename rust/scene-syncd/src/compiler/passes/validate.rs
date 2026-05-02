use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::error::AppError;
use crate::validation::diagnostic::Diagnostic;
use crate::validation::engine::ValidationEngine;
use crate::validation::rules::bridge_crosses_moat::BridgeCrossesMoat;
use crate::validation::rules::bridge_endpoint_grounded::BridgeEndpointGrounded;
use crate::validation::rules::gate_opening_width::GateOpeningWidth;
use crate::validation::rules::ground_contact::GroundContact;
use crate::validation::rules::keep_inside_boundary::KeepInsideBoundary;
use crate::validation::rules::moat_offset_validity::MoatOffsetValidity;
use crate::validation::rules::nav_walkability::NavWalkability;
use crate::validation::rules::no_duplicate_mcp_id::NoDuplicateMcpId;
use crate::validation::rules::no_nan_transform::NoNaNTransform;
use crate::validation::rules::no_overlap::NoSameLayerOverlap;
use crate::validation::rules::no_z_fighting::NoSameLayerZFight;
use crate::validation::rules::no_zero_scale::NoZeroOrNegativeScale;
use crate::validation::rules::tower_wall_connectivity::TowerWallConnectivity;
use crate::validation::rules::wall_self_intersection::WallSelfIntersection;
use crate::validation::rules::wall_span_valid::WallSpanValid;

pub struct ValidatePass {
    engine: ValidationEngine,
}

impl Default for ValidatePass {
    fn default() -> Self {
        let mut engine = ValidationEngine::new();
        engine.add_rule(Box::new(NoNaNTransform));
        engine.add_rule(Box::new(NoZeroOrNegativeScale));
        engine.add_rule(Box::new(NoDuplicateMcpId));
        engine.add_rule(Box::new(NoSameLayerOverlap));
        engine.add_rule(Box::new(NoSameLayerZFight));
        // Phase 3: Castle Validator rules
        engine.add_rule(Box::new(TowerWallConnectivity));
        engine.add_rule(Box::new(WallSelfIntersection));
        engine.add_rule(Box::new(KeepInsideBoundary));
        engine.add_rule(Box::new(MoatOffsetValidity));
        engine.add_rule(Box::new(BridgeCrossesMoat));
        engine.add_rule(Box::new(BridgeEndpointGrounded));
        engine.add_rule(Box::new(GateOpeningWidth));
        engine.add_rule(Box::new(GroundContact));
        engine.add_rule(Box::new(WallSpanValid));
        engine.add_rule(Box::new(NavWalkability));
        Self { engine }
    }
}

impl Pass for ValidatePass {
    fn name(&self) -> &'static str {
        "validate"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        let diags = self.engine.validate(&ctx.objects, &ctx.footprints);
        ctx.add_diagnostics(diags);
        Ok(())
    }
}

impl ValidatePass {
    pub fn has_errors(&self, diags: &[Diagnostic]) -> bool {
        self.engine.has_errors(diags)
    }
}
