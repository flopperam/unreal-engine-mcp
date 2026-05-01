use crate::domain::{Rotator, SceneEntity, SceneObject, Transform, Vec3};
use crate::error::AppError;
use crate::layout::scene_object_builder::build_scene_object;
use crate::layout::span::Span;

/// Configuration for window generation from a facade rule.
pub struct WindowConfig {
    pub rows: usize,
    pub columns: usize,
    pub column_spacing: Option<f64>,
    pub row_spacing: Option<f64>,
    pub window_width: f64,
    pub window_height: f64,
    pub arch_allowed: bool,
}

impl WindowConfig {
    /// Derive window configuration from entity properties.
    pub fn from_properties(properties: &serde_json::Value) -> Option<Self> {
        let windows = properties.get("windows")?;
        let enabled = windows
            .get("enabled")
            .and_then(|v| v.as_bool())
            .unwrap_or(true);
        if !enabled {
            return None;
        }
        Some(Self {
            rows: windows
                .get("rows")
                .and_then(|v| v.as_u64())
                .unwrap_or(3) as usize,
            columns: windows
                .get("columns")
                .and_then(|v| v.as_u64())
                .unwrap_or(2) as usize,
            column_spacing: windows.get("column_spacing").and_then(|v| v.as_f64()),
            row_spacing: windows.get("row_spacing").and_then(|v| v.as_f64()),
            window_width: windows
                .get("width")
                .and_then(|v| v.as_f64())
                .unwrap_or(100.0),
            window_height: windows
                .get("height")
                .and_then(|v| v.as_f64())
                .unwrap_or(150.0),
            arch_allowed: windows
                .get("arch_allowed")
                .and_then(|v| v.as_bool())
                .unwrap_or(true),
        })
    }
}

/// Generate window SceneObjects for a building entity.
///
/// Windows are placed on the facade of the entity using the span to determine
/// orientation and the entity properties to configure rows/columns/spacing.
pub fn generate_windows(
    scene_id: &str,
    entity: &SceneEntity,
    spec: &crate::layout::kind_registry::KindSpec,
    base_mcp_id: &str,
    span: &Span,
) -> Result<Vec<SceneObject>, AppError> {
    let config = match WindowConfig::from_properties(&entity.properties) {
        Some(c) => c,
        None => return Ok(vec![]),
    };

    let height = entity
        .properties
        .get("height")
        .and_then(|v| v.as_f64())
        .unwrap_or(400.0);

    let wall_length = span.length();
    let yaw = span.yaw_degrees();

    // Calculate spacing defaults based on wall length
    let col_spacing = config
        .column_spacing
        .unwrap_or_else(|| wall_length / (config.columns + 1) as f64);
    let row_spacing = config
        .row_spacing
        .unwrap_or_else(|| height / (config.rows + 1) as f64);

    let mut objects = Vec::new();

    for row in 0..config.rows {
        for col in 0..config.columns {
            // Position along the wall using col_spacing from the start
            let offset_along = (col + 1) as f64 * col_spacing;
            let t = (offset_along / wall_length).clamp(0.05, 0.95);
            let mut location = span.point_at(t);

            // Height offset: bottom row is row 0, top row is (rows-1)
            let z_offset = (row + 1) as f64 * row_spacing;
            location.z += z_offset;

            let transform = Transform {
                location,
                rotation: Rotator {
                    pitch: 0.0,
                    yaw,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: config.window_width / 100.0,
                    y: 0.1,
                    z: config.window_height / 100.0,
                },
            };

            let mut object = build_scene_object(
                scene_id,
                entity,
                spec,
                format!("{base_mcp_id}_win_{:02}_{:02}", row + 1, col + 1),
                format!("{} Window {}-{}", entity.name, row + 1, col + 1),
                transform,
                "window",
            )?;

            for tag in ["window", "detail"] {
                if !object.tags.contains(&tag.to_string()) {
                    object.tags.push(tag.to_string());
                }
            }
            object.visual["draft"]["proxy_group"] = serde_json::json!("window");
            object.metadata["render_mode"] = serde_json::json!("instance_set");

            objects.push(object);
        }
    }

    Ok(objects)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::layout::kind_registry::KindRegistry;
    use serde_json::json;

    fn make_entity(kind: &str, name: &str, props: serde_json::Value) -> SceneEntity {
        SceneEntity {
            id: String::new(),
            scene: "scene:test".to_string(),
            entity_id: format!("ent_{}", name.to_lowercase().replace(' ', "_")),
            kind: kind.to_string(),
            name: name.to_string(),
            properties: props,
            tags: vec![],
            mcp_ids: vec![],
            metadata: json!({}),
            deleted: false,
            revision: 1,
            created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
        }
    }

    #[test]
    fn generate_windows_with_config() {
        let registry = KindRegistry::default();
        let spec = registry.get("keep").unwrap();
        let entity = make_entity(
            "keep",
            "Main Keep",
            json!({
                "height": 600.0,
                "size": {"x": 8.0, "y": 8.0, "z": 20.0},
                "windows": {"enabled": true, "rows": 2, "columns": 3}
            }),
        );
        let span = Span {
            from: Vec3 { x: -400.0, y: 0.0, z: 0.0 },
            to: Vec3 { x: 400.0, y: 0.0, z: 0.0 },
        };
        let windows = generate_windows("test", &entity, spec, "keep_1", &span).unwrap();
        assert_eq!(windows.len(), 6, "2 rows x 3 cols = 6 windows");
        assert!(windows[0].tags.contains(&"window".to_string()));
        assert!(windows[0].tags.contains(&"detail".to_string()));
    }

    #[test]
    fn no_windows_when_disabled() {
        let registry = KindRegistry::default();
        let spec = registry.get("keep").unwrap();
        let entity = make_entity(
            "keep",
            "No Windows",
            json!({
                "height": 600.0,
                "windows": {"enabled": false}
            }),
        );
        let span = Span {
            from: Vec3 { x: 0.0, y: 0.0, z: 0.0 },
            to: Vec3 { x: 800.0, y: 0.0, z: 0.0 },
        };
        let windows = generate_windows("test", &entity, spec, "keep_1", &span).unwrap();
        assert!(windows.is_empty());
    }

    #[test]
    fn no_windows_without_config() {
        let registry = KindRegistry::default();
        let spec = registry.get("keep").unwrap();
        let entity = make_entity("keep", "Plain Keep", json!({"height": 400.0}));
        let span = Span {
            from: Vec3 { x: 0.0, y: 0.0, z: 0.0 },
            to: Vec3 { x: 800.0, y: 0.0, z: 0.0 },
        };
        let windows = generate_windows("test", &entity, spec, "keep_1", &span).unwrap();
        assert!(windows.is_empty());
    }
}