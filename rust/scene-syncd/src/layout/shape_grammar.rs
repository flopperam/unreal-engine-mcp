use serde::{Deserialize, Serialize};

/// Facade rule describing how windows should be arranged on a building face.
/// Phase 6: minimal implementation. Full shape grammar planned for Phase 7+.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FacadeRule {
    /// Number of window rows on the facade.
    pub window_rows: usize,
    /// Number of window columns on the facade.
    pub window_columns: usize,
    /// Horizontal spacing between window columns in Unreal cm. None = auto.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub column_spacing: Option<f64>,
    /// Whether arch-top windows are allowed.
    #[serde(default)]
    pub arch_allowed: bool,
}

impl FacadeRule {
    /// Derive a FacadeRule from entity properties, if a `facade` key exists.
    pub fn from_properties(properties: &serde_json::Value) -> Option<Self> {
        let facade = properties.get("facade")?;
        Some(Self {
            window_rows: facade
                .get("window_rows")
                .and_then(|v| v.as_u64())
                .unwrap_or(3) as usize,
            window_columns: facade
                .get("window_columns")
                .and_then(|v| v.as_u64())
                .unwrap_or(2) as usize,
            column_spacing: facade.get("column_spacing").and_then(|v| v.as_f64()),
            arch_allowed: facade
                .get("arch_allowed")
                .and_then(|v| v.as_bool())
                .unwrap_or(true),
        })
    }

    /// Default facade rule for a building with the given dimensions.
    pub fn default_for(height: f64, width: f64) -> Self {
        let rows = ((height / 200.0).floor() as usize).clamp(1, 8);
        let columns = ((width / 300.0).floor() as usize).clamp(1, 10);
        Self {
            window_rows: rows,
            window_columns: columns,
            column_spacing: None,
            arch_allowed: true,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn facade_rule_from_properties() {
        let props = json!({
            "facade": {
                "window_rows": 4,
                "window_columns": 3,
                "arch_allowed": false
            }
        });
        let rule = FacadeRule::from_properties(&props).unwrap();
        assert_eq!(rule.window_rows, 4);
        assert_eq!(rule.window_columns, 3);
        assert!(!rule.arch_allowed);
    }

    #[test]
    fn facade_rule_default_for() {
        let rule = FacadeRule::default_for(600.0, 900.0);
        assert_eq!(rule.window_rows, 3); // 600/200 = 3
        assert_eq!(rule.window_columns, 3); // 900/300 = 3
        assert!(rule.arch_allowed);
    }

    #[test]
    fn facade_rule_missing_facade_key() {
        let props = json!({"height": 400.0});
        assert!(FacadeRule::from_properties(&props).is_none());
    }
}
