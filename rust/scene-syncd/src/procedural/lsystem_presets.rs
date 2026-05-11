use serde::{Deserialize, Serialize};

use super::lsystem::{DimensionMode, LSystemParams};

// ── Preset Types ────────────────────────────────────────────────────────

/// Named presets for common L-System grammars.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum LSystemPreset {
    #[serde(rename = "koch2d")]
    Koch2D,
    #[serde(rename = "tree3d")]
    Tree3D,
    #[serde(rename = "dragon2d")]
    Dragon2D,
    #[serde(rename = "sierpinski2d")]
    Sierpinski2D,
    #[serde(rename = "hilbert3d")]
    Hilbert3D,
}

impl LSystemPreset {
    /// Human-readable name for diagnostics.
    pub fn name(&self) -> &'static str {
        match self {
            LSystemPreset::Koch2D => "Koch2D",
            LSystemPreset::Tree3D => "Tree3D",
            LSystemPreset::Dragon2D => "Dragon2D",
            LSystemPreset::Sierpinski2D => "Sierpinski2D",
            LSystemPreset::Hilbert3D => "Hilbert3D",
        }
    }

    /// Return the L-System parameters for this preset.
    pub fn params(&self) -> LSystemParams {
        match self {
            LSystemPreset::Koch2D => LSystemParams {
                axiom: "F".to_string(),
                rules: vec![('F', "F+F-F-F+F".to_string())],
                iterations: 3,
                step_length: 1.0,
                angle_degrees: 90.0,
                origin: [0.0, 0.0, 0.0],
                heading: [1.0, 0.0, 0.0],
                up: [0.0, 0.0, 1.0],
                dimension_mode: DimensionMode::TwoD,
            },
            LSystemPreset::Tree3D => LSystemParams {
                axiom: "F".to_string(),
                rules: vec![('F', "F[+F]F[-F]F".to_string())],
                iterations: 3,
                step_length: 1.0,
                angle_degrees: 25.7,
                origin: [0.0, 0.0, 0.0],
                heading: [0.0, 0.0, 1.0],
                up: [0.0, 1.0, 0.0],
                dimension_mode: DimensionMode::ThreeD,
            },
            LSystemPreset::Dragon2D => LSystemParams {
                axiom: "FX".to_string(),
                rules: vec![
                    ('X', "X+YF+".to_string()),
                    ('Y', "-FX-Y".to_string()),
                ],
                iterations: 10,
                step_length: 1.0,
                angle_degrees: 90.0,
                origin: [0.0, 0.0, 0.0],
                heading: [1.0, 0.0, 0.0],
                up: [0.0, 0.0, 1.0],
                dimension_mode: DimensionMode::TwoD,
            },
            LSystemPreset::Sierpinski2D => LSystemParams {
                axiom: "F-G-G".to_string(),
                rules: vec![
                    ('F', "F-G+F+G-F".to_string()),
                    ('G', "GG".to_string()),
                ],
                iterations: 3,
                step_length: 1.0,
                angle_degrees: 120.0,
                origin: [0.0, 0.0, 0.0],
                heading: [1.0, 0.0, 0.0],
                up: [0.0, 0.0, 1.0],
                dimension_mode: DimensionMode::TwoD,
            },
            LSystemPreset::Hilbert3D => LSystemParams {
                axiom: "X".to_string(),
                rules: vec![('X', "^<XF^<XFX-F^>>XFX&F+>>XFX-F>X-".to_string())],
                iterations: 2,
                step_length: 1.0,
                angle_degrees: 90.0,
                origin: [0.0, 0.0, 0.0],
                heading: [1.0, 0.0, 0.0],
                up: [0.0, 0.0, 1.0],
                dimension_mode: DimensionMode::ThreeD,
            },
        }
    }
}

/// Resolve a preset name (case-insensitive) into parameters.
pub fn resolve_preset(name: &str) -> Option<LSystemParams> {
    let lower = name.to_ascii_lowercase();
    match lower.as_str() {
        "koch2d" => Some(LSystemPreset::Koch2D.params()),
        "tree3d" => Some(LSystemPreset::Tree3D.params()),
        "dragon2d" => Some(LSystemPreset::Dragon2D.params()),
        "sierpinski2d" => Some(LSystemPreset::Sierpinski2D.params()),
        "hilbert3d" => Some(LSystemPreset::Hilbert3D.params()),
        _ => None,
    }
}

// ── Tests ───────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use crate::procedural::generator::Generator;

    #[test]
    fn test_resolve_koch2d() {
        let p = resolve_preset("Koch2D").unwrap();
        assert_eq!(p.axiom, "F");
        assert_eq!(p.angle_degrees, 90.0);
        assert!(matches!(p.dimension_mode, DimensionMode::TwoD));
    }

    #[test]
    fn test_resolve_tree3d() {
        let p = resolve_preset("tree3d").unwrap();
        assert_eq!(p.axiom, "F");
        assert_eq!(p.angle_degrees, 25.7);
        assert!(matches!(p.dimension_mode, DimensionMode::ThreeD));
    }

    #[test]
    fn test_resolve_dragon2d() {
        let p = resolve_preset("dragon2d").unwrap();
        assert_eq!(p.axiom, "FX");
        assert_eq!(p.iterations, 10);
        assert!(matches!(p.dimension_mode, DimensionMode::TwoD));
    }

    #[test]
    fn test_resolve_sierpinski2d() {
        let p = resolve_preset("Sierpinski2D").unwrap();
        assert_eq!(p.axiom, "F-G-G");
        assert_eq!(p.angle_degrees, 120.0);
        assert!(matches!(p.dimension_mode, DimensionMode::TwoD));
    }

    #[test]
    fn test_resolve_hilbert3d() {
        let p = resolve_preset("hilbert3d").unwrap();
        assert_eq!(p.axiom, "X");
        assert_eq!(p.iterations, 2);
        assert!(matches!(p.dimension_mode, DimensionMode::ThreeD));
    }

    #[test]
    fn test_resolve_unknown() {
        assert!(resolve_preset("unknown").is_none());
    }

    #[test]
    fn test_preset_name_roundtrip() {
        for preset in [
            LSystemPreset::Koch2D,
            LSystemPreset::Tree3D,
            LSystemPreset::Dragon2D,
            LSystemPreset::Sierpinski2D,
            LSystemPreset::Hilbert3D,
        ] {
            let params = preset.params();
            assert!(!params.axiom.is_empty());
        }
    }

    #[test]
    fn test_preset_generates_segments() {
        use super::super::lsystem::LSystemGenerator;
        use super::super::generator::GenerateContext;

        let ctx = GenerateContext::new(None, None);
        let gen = LSystemGenerator;

        for preset in [
            LSystemPreset::Koch2D,
            LSystemPreset::Tree3D,
            LSystemPreset::Dragon2D,
            LSystemPreset::Sierpinski2D,
            LSystemPreset::Hilbert3D,
        ] {
            let mut params = preset.params();
            // Cap iterations to keep tests fast.
            params.iterations = params.iterations.min(3);
            let output = gen.generate(&params, &ctx).unwrap();
            assert!(
                !output.data.segments.is_empty(),
                "{} should produce segments",
                preset.name()
            );
            assert!(
                output.stats.bounds.is_some(),
                "{} should produce bounds",
                preset.name()
            );
        }
    }
}
