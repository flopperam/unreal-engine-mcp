use glam::{Quat, Vec3};

/// A single segment produced by the L-System turtle.
#[derive(Debug, Clone)]
pub struct SplineSegment {
    pub start: [f32; 3],
    pub end: [f32; 3],
}

/// Result of an L-System evaluation.
#[derive(Debug, Clone)]
pub struct LSystemResult {
    pub segments: Vec<SplineSegment>,
    /// The final derived string (for debugging)
    pub derived_string: String,
}

/// Parameters for an L-System.
#[derive(Debug, Clone)]
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
        }
    }
}

/// Turtle state for 3D L-System interpretation.
#[derive(Debug, Clone)]
struct TurtleState {
    pos: Vec3,
    heading: Vec3,
    left: Vec3,
    up: Vec3,
}

/// Evaluate an L-System: derive the string, then interpret it with a 3D turtle.
pub fn evaluate_lsystem(params: &LSystemParams) -> LSystemResult {
    let derived = derive(params);
    let segments = interpret(&derived, params);
    LSystemResult {
        segments,
        derived_string: derived,
    }
}

/// Derive the L-System string by applying production rules for N iterations.
fn derive(params: &LSystemParams) -> String {
    let mut current = params.axiom.clone();
    let iterations = params.iterations.min(10) as usize; // cap to prevent explosion

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

/// Interpret the derived string using a 3D turtle.
/// Supported symbols:
///   F, G — move forward and draw a segment
///   f    — move forward without drawing
///   +    — turn left (yaw)
///   -    — turn right (yaw)
///   &    — pitch down
///   ^    — pitch up
///   \\   — roll left
///   /    — roll right
///   [    — push state
///   ]    — pop state
fn interpret(string: &str, params: &LSystemParams) -> Vec<SplineSegment> {
    let heading = Vec3::from(params.heading).normalize();
    let up = Vec3::from(params.up).normalize();
    let left = up.cross(heading).normalize();

    let mut state = TurtleState {
        pos: Vec3::from(params.origin),
        heading,
        left,
        up,
    };

    let mut stack: Vec<TurtleState> = Vec::new();
    let mut segments = Vec::new();

    let angle = params.angle_degrees.to_radians();

    let mut chars = string.chars().peekable();
    while let Some(ch) = chars.next() {
        match ch {
            'F' | 'G' => {
                let new_pos = state.pos + state.heading * params.step_length;
                segments.push(SplineSegment {
                    start: state.pos.to_array(),
                    end: new_pos.to_array(),
                });
                state.pos = new_pos;
            }
            'f' => {
                state.pos = state.pos + state.heading * params.step_length;
            }
            '+' => {
                // Turn left (yaw around up axis)
                let rot = Quat::from_axis_angle(state.up, angle);
                state.heading = rot * state.heading;
                state.left = rot * state.left;
            }
            '-' => {
                // Turn right (yaw around up axis, negative)
                let rot = Quat::from_axis_angle(state.up, -angle);
                state.heading = rot * state.heading;
                state.left = rot * state.left;
            }
            '&' => {
                // Pitch down (rotate around left axis)
                let rot = Quat::from_axis_angle(state.left, angle);
                state.heading = rot * state.heading;
                state.up = rot * state.up;
            }
            '^' => {
                // Pitch up (rotate around left axis, negative)
                let rot = Quat::from_axis_angle(state.left, -angle);
                state.heading = rot * state.heading;
                state.up = rot * state.up;
            }
            '\\' => {
                // Roll left (rotate around heading axis)
                let rot = Quat::from_axis_angle(state.heading, angle);
                state.left = rot * state.left;
                state.up = rot * state.up;
            }
            '/' => {
                // Roll right (rotate around heading axis, negative)
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

#[cfg(test)]
mod tests {
    use super::*;

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
        };
        let result = evaluate_lsystem(&params);
        assert!(result.segments.len() > 0, "should produce segments");
        // Koch curve with 2 iterations of F → F+F-F-F+F produces 5^2 = 25 F symbols
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
        };
        let result = evaluate_lsystem(&params);
        // F → F[+F]F[-F]F has 5 F symbols
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
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 3);
        // After [, the turtle branches. After ], it returns to the pre-[ position.
        let last = &result.segments[2];
        // The last F starts where the first F ended, not where the branched F ended
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
            iterations: 20, // over the cap
            step_length: 1.0,
            angle_degrees: 90.0,
            ..Default::default()
        };
        let result = evaluate_lsystem(&params);
        // With cap at 10, F→FF^10 = 2^10 = 1024 F symbols
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
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 1);
        // The segment starts at x=1 (after 'f' move), ends at x=2
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
        };
        let result = evaluate_lsystem(&params);
        assert_eq!(result.segments.len(), 3);
        // After F: pos = (1,0,0), heading=(1,0,0)
        // After &: pitch down 90° → heading = (1,0,0) rotated 90° down = (0,0,-1)
        // After F: pos = (1,0,-1)
        // After ^: pitch up 90° → heading back to (1,0,0)
        // After F: pos = (2,0,-1)
        let last = &result.segments[2];
        assert!((last.end[0] - 2.0).abs() < 1e-3);
        assert!((last.end[2] - (-1.0)).abs() < 1e-3);
    }
}
