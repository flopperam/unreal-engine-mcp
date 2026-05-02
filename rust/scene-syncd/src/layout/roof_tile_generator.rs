use crate::domain::{Rotator, SceneEntity, SceneObject, Transform, Vec3};
use crate::error::AppError;
use crate::layout::kind_registry::KindSpec;
use crate::layout::scene_object_builder::build_scene_object;
use crate::layout::span::Span;

/// Configuration for roof tile generation.
pub struct RoofTileConfig {
    pub tile_rows: usize,
    pub tile_columns: usize,
    pub tile_width: f64,
    pub tile_height: f64,
    pub overhang: f64,
}

impl RoofTileConfig {
    pub fn from_properties(properties: &serde_json::Value) -> Option<Self> {
        let roof = properties.get("roof")?;
        let enabled = roof
            .get("enabled")
            .and_then(|v| v.as_bool())
            .unwrap_or(true);
        if !enabled {
            return None;
        }
        Some(Self {
            tile_rows: roof.get("rows").and_then(|v| v.as_u64()).unwrap_or(4) as usize,
            tile_columns: roof.get("columns").and_then(|v| v.as_u64()).unwrap_or(6) as usize,
            tile_width: roof
                .get("tile_width")
                .and_then(|v| v.as_f64())
                .unwrap_or(60.0),
            tile_height: roof
                .get("tile_height")
                .and_then(|v| v.as_f64())
                .unwrap_or(40.0),
            overhang: roof
                .get("overhang")
                .and_then(|v| v.as_f64())
                .unwrap_or(50.0),
        })
    }
}

/// Generate roof tile SceneObjects for a building entity.
///
/// Tiles are placed on the top face of the building, covering the roof area
/// with a slight overhang. Each tile is a small plane-oriented rectangle.
pub fn generate_roof_tiles(
    scene_id: &str,
    entity: &SceneEntity,
    spec: &KindSpec,
    base_mcp_id: &str,
    span: &Span,
) -> Result<Vec<SceneObject>, AppError> {
    let config = match RoofTileConfig::from_properties(&entity.properties) {
        Some(c) => c,
        None => return Ok(vec![]),
    };

    let height = entity
        .properties
        .get("height")
        .and_then(|v| v.as_f64())
        .unwrap_or(400.0);

    let width = entity
        .properties
        .get("size")
        .and_then(|s| s.get("x"))
        .and_then(|v| v.as_f64())
        .unwrap_or(800.0);
    let depth = entity
        .properties
        .get("size")
        .and_then(|s| s.get("y"))
        .and_then(|v| v.as_f64())
        .unwrap_or(800.0);

    let yaw = span.yaw_degrees();
    let center = span.midpoint();
    let roof_z = center.z + height;

    let mut objects = Vec::with_capacity(config.tile_rows * config.tile_columns);

    for row in 0..config.tile_rows {
        for col in 0..config.tile_columns {
            let x_offset = ((col as f64 + 0.5) / config.tile_columns as f64 - 0.5) * width;
            let y_offset = ((row as f64 + 0.5) / config.tile_rows as f64 - 0.5) * depth;

            let transform = Transform {
                location: Vec3 {
                    x: center.x + x_offset,
                    y: center.y + y_offset,
                    z: roof_z + 10.0, // Slightly above the roof surface
                },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: config.tile_width / 100.0,
                    y: config.tile_height / 100.0,
                    z: 0.05,
                },
            };

            let mut object = build_scene_object(
                scene_id,
                entity,
                spec,
                format!("{base_mcp_id}_tile_{:02}_{:02}", row + 1, col + 1),
                format!("{} Tile {}-{}", entity.name, row + 1, col + 1),
                transform,
                "roof_tile",
            )?;

            for tag in ["roof_tile", "detail"] {
                if !object.tags.contains(&tag.to_string()) {
                    object.tags.push(tag.to_string());
                }
            }
            object.visual["draft"]["proxy_group"] = serde_json::json!("roof_tile");
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
    fn generate_roof_tiles_with_config() {
        let registry = KindRegistry::default();
        let spec = registry.get("keep").unwrap();
        let entity = make_entity(
            "keep",
            "Main Keep",
            json!({
                "height": 400.0,
                "size": {"x": 8.0, "y": 8.0},
                "roof": {"enabled": true, "rows": 2, "columns": 3}
            }),
        );
        let span = Span {
            from: Vec3 {
                x: -400.0,
                y: 0.0,
                z: 0.0,
            },
            to: Vec3 {
                x: 400.0,
                y: 0.0,
                z: 0.0,
            },
        };
        let tiles = generate_roof_tiles("test", &entity, spec, "keep_1", &span).unwrap();
        assert_eq!(tiles.len(), 6, "2 rows x 3 cols = 6 tiles");
        assert!(tiles[0].tags.contains(&"roof_tile".to_string()));
    }

    #[test]
    fn no_roof_tiles_when_disabled() {
        let registry = KindRegistry::default();
        let spec = registry.get("keep").unwrap();
        let entity = make_entity(
            "keep",
            "No Roof",
            json!({
                "height": 400.0,
                "roof": {"enabled": false}
            }),
        );
        let span = Span {
            from: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            to: Vec3 {
                x: 800.0,
                y: 0.0,
                z: 0.0,
            },
        };
        let tiles = generate_roof_tiles("test", &entity, spec, "keep_1", &span).unwrap();
        assert!(tiles.is_empty());
    }
}
