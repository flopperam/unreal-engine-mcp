use crate::domain::{Rotator, SceneEntity, SceneObject, Transform, Vec3};
use crate::error::AppError;
use crate::layout::kind_registry::KindSpec;
use crate::layout::scene_object_builder::build_scene_object;
use crate::layout::span::Span;

/// Configuration for brick generation on wall surfaces.
pub struct BrickConfig {
    pub brick_rows: usize,
    pub brick_columns: usize,
    pub brick_width: f64,
    pub brick_height: f64,
    pub mortar_gap: f64,
}

impl BrickConfig {
    pub fn from_properties(properties: &serde_json::Value) -> Option<Self> {
        let bricks = properties.get("bricks")?;
        let enabled = bricks
            .get("enabled")
            .and_then(|v| v.as_bool())
            .unwrap_or(true);
        if !enabled {
            return None;
        }
        Some(Self {
            brick_rows: bricks.get("rows").and_then(|v| v.as_u64()).unwrap_or(8) as usize,
            brick_columns: bricks.get("columns").and_then(|v| v.as_u64()).unwrap_or(12) as usize,
            brick_width: bricks.get("width").and_then(|v| v.as_f64()).unwrap_or(40.0),
            brick_height: bricks
                .get("height")
                .and_then(|v| v.as_f64())
                .unwrap_or(20.0),
            mortar_gap: bricks
                .get("mortar_gap")
                .and_then(|v| v.as_f64())
                .unwrap_or(5.0),
        })
    }
}

/// Generate brick SceneObjects for a curtain wall surface.
///
/// Bricks are placed on the face of the wall, arranged in a grid pattern
/// with staggered rows (offset every other row by half a brick width).
pub fn generate_bricks(
    scene_id: &str,
    entity: &SceneEntity,
    spec: &KindSpec,
    base_mcp_id: &str,
    span: &Span,
) -> Result<Vec<SceneObject>, AppError> {
    let config = match BrickConfig::from_properties(&entity.properties) {
        Some(c) => c,
        None => return Ok(vec![]),
    };

    let height = entity
        .properties
        .get("height")
        .and_then(|v| v.as_f64())
        .unwrap_or(400.0);
    let thickness = entity
        .properties
        .get("thickness")
        .and_then(|v| v.as_f64())
        .unwrap_or(50.0);

    let wall_length = span.length();
    let yaw = span.yaw_degrees();

    // Compute row spacing to distribute bricks evenly across the wall height
    let row_spacing = height / (config.brick_rows + 1) as f64;
    let col_spacing = wall_length / (config.brick_columns + 1) as f64;

    let mut objects = Vec::with_capacity(config.brick_rows * config.brick_columns);

    for row in 0..config.brick_rows {
        // Offset odd rows by half a column spacing for staggered brick pattern
        let row_offset = if row % 2 == 1 { col_spacing * 0.5 } else { 0.0 };

        for col in 0..config.brick_columns {
            let t = ((col + 1) as f64 * col_spacing + row_offset) / wall_length;
            // Clamp t to [0.05, 0.95] to avoid bricks at exact wall endpoints
            let t = t.clamp(0.05, 0.95);
            let mut location = span.point_at(t);

            // Height from bottom, with row spacing
            let z_offset = (row + 1) as f64 * row_spacing;
            location.z += z_offset;

            // Push bricks slightly in front of the wall (half thickness)
            let offset_x = -thickness.sin() * 0.5 * thickness / 100.0;
            let offset_y = thickness.cos() * 0.5 * thickness / 100.0;

            let transform = Transform {
                location: Vec3 {
                    x: location.x + offset_x,
                    y: location.y + offset_y,
                    z: location.z,
                },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: config.brick_width / 100.0,
                    y: 0.1, // thin depth (surface decoration)
                    z: config.brick_height / 100.0,
                },
            };

            let mut object = build_scene_object(
                scene_id,
                entity,
                spec,
                format!("{base_mcp_id}_brick_{:02}_{:02}", row + 1, col + 1),
                format!("{} Brick {}-{}", entity.name, row + 1, col + 1),
                transform,
                "brick",
            )?;

            for tag in ["brick", "detail"] {
                if !object.tags.contains(&tag.to_string()) {
                    object.tags.push(tag.to_string());
                }
            }
            object.visual["draft"]["proxy_group"] = serde_json::json!("brick");
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
    fn generate_bricks_with_config() {
        let registry = KindRegistry::default();
        let spec = registry.get("curtain_wall").unwrap();
        let entity = make_entity(
            "curtain_wall",
            "North Wall",
            json!({
                "height": 400.0,
                "thickness": 50.0,
                "from": {"x": -500.0, "y": 0.0, "z": 0.0},
                "to": {"x": 500.0, "y": 0.0, "z": 0.0},
                "bricks": {"enabled": true, "rows": 3, "columns": 4}
            }),
        );
        let span = Span {
            from: Vec3 {
                x: -500.0,
                y: 0.0,
                z: 0.0,
            },
            to: Vec3 {
                x: 500.0,
                y: 0.0,
                z: 0.0,
            },
        };
        let bricks = generate_bricks("test", &entity, spec, "wall_1", &span).unwrap();
        assert_eq!(bricks.len(), 12, "3 rows x 4 cols = 12 bricks");
        assert!(bricks[0].tags.contains(&"brick".to_string()));
    }

    #[test]
    fn no_bricks_when_disabled() {
        let registry = KindRegistry::default();
        let spec = registry.get("curtain_wall").unwrap();
        let entity = make_entity(
            "curtain_wall",
            "No Bricks",
            json!({
                "height": 400.0,
                "bricks": {"enabled": false}
            }),
        );
        let span = Span {
            from: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            to: Vec3 {
                x: 1000.0,
                y: 0.0,
                z: 0.0,
            },
        };
        let bricks = generate_bricks("test", &entity, spec, "wall_1", &span).unwrap();
        assert!(bricks.is_empty());
    }
}
