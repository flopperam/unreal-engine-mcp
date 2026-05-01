use crate::domain::{SceneEntity, SceneObject};
use crate::error::AppError;
use crate::layout::brick_generator::generate_bricks;
use crate::layout::crenellations::generate_crenellations;
use crate::layout::kind_registry::KindRegistry;
use crate::layout::roof_tile_generator::generate_roof_tiles;
use crate::layout::span::Span;
use crate::layout::window_generator::generate_windows;

/// Detail realizer: generates procedural details (crenellations, windows, roof tiles,
/// bricks) for layout entities and marks them for InstanceSet rendering when repeated.
pub struct DetailRealizer;

impl DetailRealizer {
    /// Realize crenellations for all curtain-wall entities in the scene.
    pub fn realize_crenellations(
        scene_id: &str,
        entities: &[SceneEntity],
        spans: &[(String, Span)],
    ) -> Result<Vec<SceneObject>, AppError> {
        let registry = KindRegistry::default();
        let mut objects = Vec::new();

        for entity in entities {
            if entity.deleted {
                continue;
            }
            if entity.kind != "curtain_wall" {
                continue;
            }
            let Some(spec) = registry.get(&entity.kind) else {
                continue;
            };
            let Some((_, span)) = spans.iter().find(|(id, _)| id == &entity.entity_id) else {
                continue;
            };
            let base_mcp_id = format!("{}_{}", scene_id, entity.entity_id);
            let crenels = generate_crenellations(scene_id, entity, spec, &base_mcp_id, span)?;
            objects.extend(crenels);
        }

        Ok(objects)
    }

    /// Realize windows for all building/keep/tower entities in the scene.
    pub fn realize_windows(
        &self,
        scene_id: &str,
        entities: &[SceneEntity],
        spans: &[(String, Span)],
    ) -> Result<Vec<SceneObject>, AppError> {
        let registry = KindRegistry::default();
        let mut objects = Vec::new();

        for entity in entities {
            if entity.deleted {
                continue;
            }
            if !matches!(entity.kind.as_str(), "keep" | "building" | "tower" | "gatehouse") {
                continue;
            }
            let Some(spec) = registry.get(&entity.kind) else {
                continue;
            };
            let Some((_, span)) = spans.iter().find(|(id, _)| id == &entity.entity_id) else {
                // Fall back to entity properties for span if no explicit span
                continue;
            };
            let base_mcp_id = format!("{}_{}", scene_id, entity.entity_id);
            let wins = generate_windows(scene_id, entity, spec, &base_mcp_id, span)?;
            objects.extend(wins);
        }

        Ok(objects)
    }

    /// Realize roof tiles for all building/keep/tower entities in the scene.
    pub fn realize_roof_tiles(
        &self,
        scene_id: &str,
        entities: &[SceneEntity],
        spans: &[(String, Span)],
    ) -> Result<Vec<SceneObject>, AppError> {
        let registry = KindRegistry::default();
        let mut objects = Vec::new();

        for entity in entities {
            if entity.deleted {
                continue;
            }
            if !matches!(entity.kind.as_str(), "keep" | "building" | "tower" | "gatehouse") {
                continue;
            }
            let Some(spec) = registry.get(&entity.kind) else {
                continue;
            };
            let Some((_, span)) = spans.iter().find(|(id, _)| id == &entity.entity_id) else {
                continue;
            };
            let base_mcp_id = format!("{}_{}", scene_id, entity.entity_id);
            let tiles = generate_roof_tiles(scene_id, entity, spec, &base_mcp_id, span)?;
            objects.extend(tiles);
        }

        Ok(objects)
    }

    /// Realize bricks for all curtain-wall entities in the scene.
    pub fn realize_bricks(
        &self,
        scene_id: &str,
        entities: &[SceneEntity],
        spans: &[(String, Span)],
    ) -> Result<Vec<SceneObject>, AppError> {
        let registry = KindRegistry::default();
        let mut objects = Vec::new();

        for entity in entities {
            if entity.deleted {
                continue;
            }
            if entity.kind != "curtain_wall" {
                continue;
            }
            let Some(spec) = registry.get(&entity.kind) else {
                continue;
            };
            let Some((_, span)) = spans.iter().find(|(id, _)| id == &entity.entity_id) else {
                continue;
            };
            let base_mcp_id = format!("{}_{}", scene_id, entity.entity_id);
            let bricks = generate_bricks(scene_id, entity, spec, &base_mcp_id, span)?;
            objects.extend(bricks);
        }

        Ok(objects)
    }
}