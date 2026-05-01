use serde::{Deserialize, Serialize};

/// World-level style profile that drives material and color coherence decisions.
/// Defines the overall aesthetic of a scene (biome, faction, wealth, etc.).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct WorldStyleProfile {
    /// Biome determines base material palette (stone, wood, sand, etc.).
    pub biome: String,
    /// Faction influences heraldry colors and architectural style.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub faction: Option<String>,
    /// District further refines style within a scene (market, barracks, chapel).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub district: Option<String>,
    /// Historical period affects weathering and construction style.
    #[serde(default)]
    pub history: Vec<String>,
    /// Wealth level 0.0–1.0: affects material quality (rough stone vs polished marble).
    pub wealth_level: f32,
    /// Decay level 0.0–1.0: affects weathering (pristine vs crumbling).
    pub decay_level: f32,
    /// Magic level 0.0–1.0: affects emissive/arcane material properties.
    pub magic_level: f32,
}

impl Default for WorldStyleProfile {
    fn default() -> Self {
        Self {
            biome: "temperate".to_string(),
            faction: None,
            district: None,
            history: vec![],
            wealth_level: 0.5,
            decay_level: 0.1,
            magic_level: 0.0,
        }
    }
}

impl WorldStyleProfile {
    /// Derive a style profile from scene metadata, if present.
    pub fn from_metadata(metadata: &serde_json::Value) -> Option<Self> {
        let style = metadata.get("style")?;
        Some(Self {
            biome: style
                .get("biome")
                .and_then(|v| v.as_str())
                .unwrap_or("temperate")
                .to_string(),
            faction: style.get("faction").and_then(|v| v.as_str()).map(String::from),
            district: style.get("district").and_then(|v| v.as_str()).map(String::from),
            history: style
                .get("history")
                .and_then(|v| v.as_array())
                .map(|arr| arr.iter().filter_map(|v| v.as_str().map(String::from)).collect())
                .unwrap_or_default(),
            wealth_level: style
                .get("wealth_level")
                .and_then(|v| v.as_f64())
                .unwrap_or(0.5) as f32,
            decay_level: style
                .get("decay_level")
                .and_then(|v| v.as_f64())
                .unwrap_or(0.1) as f32,
            magic_level: style
                .get("magic_level")
                .and_then(|v| v.as_f64())
                .unwrap_or(0.0) as f32,
        })
    }

    /// Returns the base material family for this biome.
    pub fn material_family(&self) -> &str {
        match self.biome.as_str() {
            "desert" | "arid" => "sandstone",
            "arctic" | "tundra" => "ice_stone",
            "volcanic" => "basalt",
            "tropical" => "bamboo",
            "temperate" => "limestone",
            _ => "stone",
        }
    }

    /// Returns a weathering factor based on decay_level and history.
    pub fn weathering_factor(&self) -> f32 {
        self.decay_level * 0.7 + if self.history.contains(&"ancient".to_string()) { 0.3 } else { 0.0 }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn default_style_is_temperate() {
        let style = WorldStyleProfile::default();
        assert_eq!(style.biome, "temperate");
        assert_eq!(style.material_family(), "limestone");
    }

    #[test]
    fn from_metadata_extracts_fields() {
        let metadata = json!({
            "style": {
                "biome": "desert",
                "faction": "red_lions",
                "wealth_level": 0.8,
                "decay_level": 0.3,
                "magic_level": 0.2
            }
        });
        let style = WorldStyleProfile::from_metadata(&metadata).unwrap();
        assert_eq!(style.biome, "desert");
        assert_eq!(style.faction, Some("red_lions".to_string()));
        assert_eq!(style.material_family(), "sandstone");
    }

    #[test]
    fn weathering_factor_with_ancient_history() {
        let style = WorldStyleProfile {
            biome: "temperate".to_string(),
            decay_level: 0.5,
            history: vec!["ancient".to_string()],
            ..Default::default()
        };
        assert!(style.weathering_factor() > 0.5);
    }
}