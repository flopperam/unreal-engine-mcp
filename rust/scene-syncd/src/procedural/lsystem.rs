use glam::{Quat, Vec3};
use serde::{Deserialize, Serialize};

use crate::procedural::generator::{
    GenerateContext, Generator, ProceduralError, ProceduralEstimate, ProceduralOutput,
    ProceduralStats, ProceduralWarning, SplineSegment,
};

// ── Data Types ──────────────────────────────────────────────────────────

/// Dimension mode for turtle interpretation.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum DimensionMode {
    /// Constrain drawing to the XY plane (Z is fixed to origin.z).
    TwoD,
    /// Full 3D movement.
    #[default]
    ThreeD,
}

/// Result of an L-System evaluation.
#[derive(Debug, Clone, Serialize)]
pub struct LSystemResult {
    pub segments: Vec<SplineSegment>,
    /// The final derived string (for debugging)
    pub derived_string: String,
}

/// Parameters for an L-System.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LSystemParams {
    /// Axiom (initial string)
    pub axiom: String,
    /// Production rules: symbol → replacement string
    pub rules: Vec<(char, String)>,
    /// Number of derivation iterations
    pub iterations: u32,
    /// Step length for the turtle
    pub step_length: f32,
    /// Turn angle in degrees
    pub angle_degrees: f32,
    /// Initial position of the turtle
    pub origin: [f32; 3],
    /// Initial heading direction (normalized internally)
    pub heading: [f32; 3],
    /// Initial up direction (normalized internally)
    pub up: [f32; 3],
    /// Dimension mode (2D or 3D)
    #[serde(default)]
    pub dimension_mode: DimensionMode,
}

impl Default for LSystemParams {
    fn default() -> Self {
        Self {
            axiom: "F".to_string(),
            rules: vec![('F', "F+F-F-F+F".to_string())],
            iterations: 3,
            step_length: 1.0,
            angle_degrees: 90.0,
            origin: [0.0, 0.0, 0.0],
            heading: [1.0, 0.0, 0.0],
            up: [0.0, 0.0, 1.0],
            dimension_mode: DimensionMode::ThreeD,
        }
    }
}

// ── Generator Implementation ────────────────────────────────────────────

/// L-System generator conforming to the unified `Generator` trait.
#[derive(Debug, Clone, Default)]
pub struct LSystemGenerator;

impl Generator for LSystemGenerator {
    type Params = LSystemParams;
    type Output = LSystemResult;

    fn name(&self) -> &'static str {
        "lsystem"
    }

    fn validate(&self, params: &Self::Params) -> Result<(), ProceduralError> {
        if params.axiom.is_empty() {
            return Err(ProceduralError::Validation(
                "Axiom cannot be empty".to_string(),
            ));
        }
        if params.step_length <= 0.0 {
            return Err(ProceduralError::Validation(
                "step_length must be positive".to_string(),
            ));
        }
        Ok(())
    }

    fn estimate(&self, params: &Self::Params) -> Result<ProceduralEstimate, ProceduralError> {
        let iterations = params.iterations.min(10) as usize;
        let estimated_segments = params
            .axiom
            .len()
            .saturating_mul(5usize.pow(iterations as u32));
        let _capped_segments = estimated_segments.min(1_000_000);
        Ok(ProceduralEstimate {
            estimated_actor_count: 1, // one spline actor
            estimated_execution_ms: 10,
            ..Default::default()
        })
    }

    fn generate(
        &self,
        params: &Self::Params,
        ctx: &GenerateContext,
    ) -> Result<ProceduralOutput<Self::Output>, ProceduralError> {
        let start = std::time::Instant::now();
        let effective_iterations = params.iterations.min(ctx.limits.max_iterations);

        let derived = derive(params, effective_iterations);
        let segments = interpret(&derived, params);

        if segments.is_empty() {
            return Err(ProceduralError::EmptyResult);
        }

        let mut warnings = Vec::new();
        if effective_iterations < params.iterations {
            warnings.push(ProceduralWarning::IterationCapped {
                requested: params.iterations,
                applied: effective_iterations,
            });
        }

        let segment_count = segments.len();
        let derived_length = derived.len();
        let bounds = compute_bounds(&segments);
        let result = LSystemResult {
            segments,
            derived_string: derived,
        };

        let elapsed = start.elapsed().as_millis() as u64;

        Ok(ProceduralOutput {
            data: result,
            stats: ProceduralStats {
                execution_ms: elapsed,
                seed_used: ctx.seed,
                derived_length: Some(derived_length),
                segment_count: Some(segment_count),
                bounds,
                ..Default::default()
            },
            warnings,
        })
    }
}

// ── Core Evaluation (backward-compatible free function) ─────────────────

/// Evaluate an L-System: derive the string, then interpret it with a 3D turtle.
/// This function is kept for backward compatibility; new code should use
/// `LSystemGenerator::default().generate(...)`.
pub fn evaluate_lsystem(params: &LSystemParams) -> LSystemResult {
    let derived = derive(params, params.iterations.min(10));
    let segments = interpret(&derived, params);
    LSystemResult {
        segments,
        derived_string: derived,
    }
}

// ── Internal Implementation ─────────────────────────────────────────────

fn derive(params: &LSystemParams, effective_iterations: u32) -> String {
    let mut current = params.axiom.clone();
    let iterations = effective_iterations as usize;

    for _ in 0..iterations {
        let mut next = String::with_capacity(current.len() * 4);
        for ch in current.chars() {
            if let Some(rule) = params.rules.iter().find(|(sym, _)| *sym == ch) {
                next.push_str(&rule.1);
            } else {
                next.push(ch);
            }
        }
        current = next;
    }
    current
}

/// Turtle state for 3D L-System interpretation.
#[derive(Debug, Clone)]
struct TurtleState {
    pos: Vec3,
    heading: Vec3,
    left: Vec3,
    up: Vec3,
}

/// Interpret the derived string using a 3D turtle.
/// Supported symbols:
///   F, G — move forward and draw a segment
///   f    — move forward without drawing
///   +    — turn left (yaw)
///   -    — turn right (yaw)
///        &    — pitch down
///        ^    — pitch up
///        \   — roll left
///        /    — roll right
///        [    — push state
///   ]    — pop state
fn interpret(string: &str, params: &LSystemParams) -> Vec<SplineSegment> {
    let heading = Vec3::from(params.heading).normalize();
    let up = Vec3::from(params.up).normalize();
    let left = up.cross(heading).normalize();

    let origin_z = params.origin[2];
    let is_2d = matches!(params.dimension_mode, DimensionMode::TwoD);

    let mut state = TurtleState {
        pos: Vec3::from(params.origin),
        heading,
        left,
        up,
    };

    let mut stack: Vec<TurtleState> = Vec::new();
    let mut segments = Vec::new();

    let angle = params.angle_degrees.to_radians();

    for ch in string.chars() {
        match ch {
            'F' | 'G' => {
                let new_pos = state.pos + state.heading * params.step_length;
                let clamped_pos = if is_2d {
                    Vec3::new(new_pos.x, new_pos.y, origin_z)
                } else {
                    new_pos
                };
                segments.push(SplineSegment {
                    start: state.pos.to_array(),
                    end: clamped_pos.to_array(),
                });
                state.pos = clamped_pos;
            }
            'f' => {
                let new_pos = state.pos + state.heading * params.step_length;
                state.pos = if is_2d {
                    Vec3::new(new_pos.x, new_pos.y, origin_z)
                } else {
                    new_pos
                };
            }
            '+' => {
                let rot = Quat::from_axis_angle(state.up, angle);
                state.heading = rot * state.heading;
                state.left = rot * state.left;
            }
            '-' => {
                let rot = Quat::from_axis_angle(state.up, -angle);
                state.heading = rot * state.heading;
                state.left = rot * state.left;
            }
            '&' => {
                let rot = Quat::from_axis_angle(state.left, angle);
                state.heading = rot * state.heading;
                state.up = rot * state.up;
            }
            '^' => {
                let rot = Quat::from_axis_angle(state.left, -angle);
                state.heading = rot * state.heading;
                state.up = rot * state.up;
            }
            '\\' => {
                let rot = Quat::from_axis_angle(state.heading, angle);
                state.left = rot * state.left;
                state.up = rot * state.up;
            }
            '/' => {
                let rot = Quat::from_axis_angle(state.heading, -angle);
                state.left = rot * state.left;
                state.up = rot * state.up;
            }
            '[' => {
                stack.push(state.clone());
            }
            ']' => {
                if let Some(popped) = stack.pop() {
                    state = popped;
                }
            }
            _ => {} // ignore unrecognized symbols
        }
    }

    segments
}

fn compute_bounds(segments: &[SplineSegment]) -> Option<crate::procedural::sdf::SdfBounds> {
    if segments.is_empty() {
        return None;
    }
    let mut min = Vec3::splat(f32::INFINITY);
    let mut max = Vec3::splat(f32::NEG_INFINITY);
    for seg in segments {
        let start = Vec3::from(seg.start);
        let end = Vec3::from(seg.end);
        min = min.min(start).min(end);
        max = max.max(start).max(end);
    }
    Some(crate::procedural::sdf::SdfBounds::new(min, max))
}

// ── Tests ───────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generator_trait() {
        let gen = LSystemGenerator;
        assert_eq!(gen.name(), "lsystem");

        let ctx = GenerateContext::new(None, None);
        let params = LSystemParams::default();
        let output = gen.generate(&params, &ctx).unwrap();
        assert!(!output.data.segments.is_empty());
        assert!(output.stats.segment_count.unwrap() > 0);
    }

    #[test]
    fn test_generator_validate_empty_axiom() {
        let gen = LSystemGenerator;
        let params = LSystemParams {
            axiom: "".to_string(),
            ..Default::default()
        };
        assert!(gen.validate(&params).is_err());
    }

    #[test]
    fn test_generator_validate_negative_step() {
        let gen = LSystemGenerator;
        let params = LSystemParams {
            step_length: -1.0,
            ..Default::default()
        };
        assert!(gen.validate(&params).is_err());
    }

    #[test]
    fn test_generator_estimate() {
        let gen = LSystemGenerator;
        let params = LSystemParams::default();
        let estimate = gen.estimate(&params).unwrap();
        assert_eq!(estimate.estimated_actor_count, 1);
    }

    #[test]
    fn test_generator_empty_result() {
        let gen = LSystemGenerator;
        let params = LSystemParams {
            axiom: "f".to_string(),
            rules: vec![],
            iterations: 0,
            step_length: 1.0,
            ..Default::default()
        };
        let ctx = GenerateContext::new(None, None);
        assert!(matches!(
            gen.generate(&params, &ctx),
            Err(ProceduralError::EmptyResult)
        ));
    }

    #[test]
    fn test_koch_curve() {
        let params = LSystemParams {
            axiom: "F".to_string(),
            rules: vec![('F', "F+F-F-F+F".to_string())],
            iterations: 2,
            step_length: 1.0,
            angle_degrees: 90.0,
            origin: [0.0, 0.0, 0.0],
            heading: [1.0, 0.0, 0.0],
            up: [0.0, 0.0, 1.0],
            dimension_mode: DimensionMode::ThreeD,
        };
        let result = evaluate_lsystem(&params);
        assert!(!result.segments.is_empty(), "should produce segments");
        assert_eq!(result.segments.len(), 25);
    }

    #[test]
    fn test_simple_forward() {
        let params = LSystemParams {
            axiom: "F".to_string(),
            rules: vec![],
            iterations: 0,
            step_length: 2.0,
            angle_degrees: 90.0,
            origin: [0.0, 0.0, 0.0],
            heading: [1.0, 0.0, 0.0],
            up: [0.0, 0.0, 1.0],
            dimension_mode: DimensionMode::ThreeD,
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 1);
        assert!((result.segments[0].end[0] - 2.0).abs() < 1e-4);
    }

    #[test]
    fn test_branching() {
        let params = LSystemParams {
            axiom: "F".to_string(),
            rules: vec![('F', "F[+F]F[-F]F".to_string())],
            iterations: 1,
            step_length: 1.0,
            angle_degrees: 25.7,
            origin: [0.0, 0.0, 0.0],
            heading: [0.0, 0.0, 1.0],
            up: [0.0, 1.0, 0.0],
            dimension_mode: DimensionMode::ThreeD,
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 5);
    }

    #[test]
    fn test_push_pop() {
        let params = LSystemParams {
            axiom: "F[+F]F".to_string(),
            rules: vec![],
            iterations: 0,
            step_length: 1.0,
            angle_degrees: 90.0,
            origin: [0.0, 0.0, 0.0],
            heading: [1.0, 0.0, 0.0],
            up: [0.0, 0.0, 1.0],
            dimension_mode: DimensionMode::ThreeD,
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 3);
        let last = &result.segments[2];
        assert!(
            (last.start[0] - 1.0).abs() < 1e-4,
            "should return to main branch"
        );
    }

    #[test]
    fn test_derivation_iteration_cap() {
        let params = LSystemParams {
            axiom: "F".to_string(),
            rules: vec![('F', "FF".to_string())],
            iterations: 20,
            step_length: 1.0,
            angle_degrees: 90.0,
            ..Default::default()
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 1024);
    }

    #[test]
    fn test_move_without_draw() {
        let params = LSystemParams {
            axiom: "fF".to_string(),
            rules: vec![],
            iterations: 0,
            step_length: 1.0,
            angle_degrees: 90.0,
            origin: [0.0, 0.0, 0.0],
            heading: [1.0, 0.0, 0.0],
            up: [0.0, 0.0, 1.0],
            dimension_mode: DimensionMode::ThreeD,
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 1);
        assert!((result.segments[0].start[0] - 1.0).abs() < 1e-4);
        assert!((result.segments[0].end[0] - 2.0).abs() < 1e-4);
    }

    #[test]
    fn test_3d_rotation() {
        let params = LSystemParams {
            axiom: "F&F^F".to_string(),
            rules: vec![],
            iterations: 0,
            step_length: 1.0,
            angle_degrees: 90.0,
            origin: [0.0, 0.0, 0.0],
            heading: [1.0, 0.0, 0.0],
            up: [0.0, 0.0, 1.0],
            dimension_mode: DimensionMode::ThreeD,
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 3);
        let last = &result.segments[2];
        assert!((last.end[0] - 2.0).abs() < 1e-3);
        assert!((last.end[2] - (-1.0)).abs() < 1e-3);
    }
}
