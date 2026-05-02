use crate::ir::style::WorldStyleProfile;
use serde::{Deserialize, Serialize};

/// Material plan for a single object, derived from the world style profile.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct MaterialPlan {
    /// The mcp_id of the object this plan applies to.
    pub object_id: String,
    /// Material family (stone, wood, metal, etc.).
    pub material_family: String,
    /// Optional explicit Unreal material asset path.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub material_path: Option<String>,
    /// Color palette for this object.
    pub color_palette: ColorPalette,
    /// Surface modifiers (weathering, moss, soot, etc.).
    #[serde(default)]
    pub surface_modifiers: Vec<SurfaceModifier>,
}

/// Color palette with primary, secondary, and accent colors.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ColorPalette {
    /// Primary color (base material tint).
    pub primary: [f32; 4],
    /// Secondary color (trim, mortar, accent).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub secondary: Option<[f32; 4]>,
    /// Accent color (heraldry, decoration).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub accent: Option<[f32; 4]>,
}

impl Default for ColorPalette {
    fn default() -> Self {
        Self {
            primary: [0.6, 0.55, 0.5, 1.0],
            secondary: None,
            accent: None,
        }
    }
}

/// Surface modifier applied on top of base material.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SurfaceModifier {
    /// Modifier type (weathering, moss, soot, cracks, etc.).
    pub modifier_type: String,
    /// Intensity 0.0–1.0.
    pub intensity: f32,
    /// Optional mask channel for the modifier.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mask_channel: Option<String>,
}

/// Compute a material plan for an object given its kind and the world style profile.
pub fn compute_material_plan(
    object_id: &str,
    kind: &str,
    style: &crate::ir::style::WorldStyleProfile,
) -> MaterialPlan {
    let (family, primary) = material_for_kind(kind, style);

    let mut surface_modifiers = Vec::new();
    let weathering = style.weathering_factor();
    if weathering > 0.2 {
        surface_modifiers.push(SurfaceModifier {
            modifier_type: "weathering".to_string(),
            intensity: weathering,
            mask_channel: None,
        });
    }
    if style.biome == "tropical" || style.biome == "temperate" {
        surface_modifiers.push(SurfaceModifier {
            modifier_type: "moss".to_string(),
            intensity: weathering * 0.5,
            mask_channel: None,
        });
    }

    MaterialPlan {
        object_id: object_id.to_string(),
        material_family: family.to_string(),
        material_path: None,
        color_palette: ColorPalette {
            primary,
            secondary: Some([0.4, 0.35, 0.3, 1.0]),
            accent: style.faction.as_ref().map(|_| [0.8, 0.2, 0.15, 1.0]),
        },
        surface_modifiers,
    }
}

/// Determine material family and primary color for a kind given the world style.
fn material_for_kind(kind: &str, style: &WorldStyleProfile) -> (String, [f32; 4]) {
    let base = style.material_family().to_string();
    let wealth = style.wealth_level;

    match kind {
        "keep" | "tower" => (base.clone(), wealth_color(wealth, 0.65)),
        "curtain_wall" => (base.clone(), wealth_color(wealth, 0.55)),
        "gatehouse" => ("wood".to_string(), wealth_color(wealth, 0.45)),
        "bridge" => ("wood".to_string(), [0.55, 0.4, 0.25, 1.0]),
        "ground" => ("earth".to_string(), [0.35, 0.5, 0.3, 1.0]),
        "moat" => ("water".to_string(), [0.2, 0.4, 0.7, 1.0]),
        "window" => ("glass".to_string(), [0.4, 0.6, 0.85, 0.8]),
        "crenellation" | "merlon" | "battlement" => (base.clone(), wealth_color(wealth, 0.6)),
        "roof_tile" => ("terracotta".to_string(), [0.6, 0.35, 0.25, 1.0]),
        "brick" => (base.clone(), wealth_color(wealth, 0.5)),
        _ => ("stone".to_string(), [0.5, 0.5, 0.5, 1.0]),
    }
}

/// Adjust color brightness based on wealth level.
fn wealth_color(wealth: f32, base_brightness: f32) -> [f32; 4] {
    let b = base_brightness * (0.7 + 0.3 * wealth);
    [b, b * 0.95, b * 0.9, 1.0]
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ir::style::WorldStyleProfile;

    #[test]
    fn compute_material_plan_for_keep() {
        let style = WorldStyleProfile::default();
        let plan = compute_material_plan("keep_1", "keep", &style);
        assert_eq!(plan.object_id, "keep_1");
        assert_eq!(plan.material_family, "limestone");
        assert!(plan.color_palette.primary[0] > 0.0);
    }

    #[test]
    fn compute_material_plan_with_weathering() {
        let style = WorldStyleProfile {
            decay_level: 0.8,
            ..Default::default()
        };
        let plan = compute_material_plan("wall_1", "curtain_wall", &style);
        assert!(plan
            .surface_modifiers
            .iter()
            .any(|m| m.modifier_type == "weathering"));
        assert!(plan
            .surface_modifiers
            .iter()
            .any(|m| m.modifier_type == "moss"));
    }

    #[test]
    fn desert_biome_uses_sandstone() {
        let style = WorldStyleProfile {
            biome: "desert".to_string(),
            ..Default::default()
        };
        let plan = compute_material_plan("keep_1", "keep", &style);
        assert_eq!(plan.material_family, "sandstone");
    }
}
