use crate::domain::{Rotator, SceneEntity, SceneObject, Transform, Vec3};
use crate::error::AppError;
use crate::layout::kind_registry::KindSpec;
use crate::layout::scene_object_builder::build_scene_object;
use crate::layout::span::Span;

pub fn should_generate_crenellations(entity: &SceneEntity) -> bool {
    entity
        .properties
        .get("crenellations")
        .and_then(|v| {
            if let Some(enabled) = v.as_bool() {
                Some(enabled)
            } else {
                v.get("enabled").and_then(|enabled| enabled.as_bool())
            }
        })
        .unwrap_or(false)
}

pub fn crenellation_count(properties: &serde_json::Value, span: &Span) -> usize {
    if let Some(count) = properties
        .get("crenellations")
        .and_then(|v| v.get("count"))
        .and_then(|v| v.as_u64())
    {
        return count.clamp(1, 512) as usize;
    }
    let spacing = properties
        .get("crenellations")
        .and_then(|v| v.get("spacing"))
        .and_then(|v| v.as_f64())
        .unwrap_or(300.0);
    if spacing <= 0.0 {
        return 1;
    }
    (span.length() / spacing).floor().max(1.0).min(512.0) as usize
}

fn crenellation_scale(properties: &serde_json::Value) -> Vec3 {
    if let Some(size) = properties
        .get("crenellations")
        .and_then(|v| v.get("size"))
        .and_then(|v| {
            Some(Vec3 {
                x: v.get("x")?.as_f64()?,
                y: v.get("y")?.as_f64()?,
                z: v.get("z")?.as_f64()?,
            })
        })
    {
        return size;
    }
    Vec3 {
        x: 0.6,
        y: properties
            .get("thickness")
            .and_then(|v| v.as_f64())
            .unwrap_or(50.0)
            / 100.0,
        z: 0.8,
    }
}

pub fn generate_crenellations(
    scene_id: &str,
    entity: &SceneEntity,
    spec: &KindSpec,
    base_mcp_id: &str,
    span: &Span,
) -> Result<Vec<SceneObject>, AppError> {
    if entity.kind != "curtain_wall" || !should_generate_crenellations(entity) {
        return Ok(vec![]);
    }

    let count = crenellation_count(&entity.properties, span);
    let height = entity
        .properties
        .get("height")
        .and_then(|v| v.as_f64())
        .unwrap_or(400.0);
    let scale = crenellation_scale(&entity.properties);
    let mut objects = Vec::with_capacity(count);

    for i in 0..count {
        let mut location = span.point_at((i as f64 + 0.5) / count as f64);
        location.z += height + (scale.z * 50.0);
        let transform = Transform {
            location,
            rotation: Rotator {
                pitch: 0.0,
                yaw: span.yaw_degrees(),
                roll: 0.0,
            },
            scale: scale.clone(),
        };
        let mut object = build_scene_object(
            scene_id,
            entity,
            spec,
            format!("{base_mcp_id}_crenel_{:03}", i + 1),
            format!("{} Crenellation {:03}", entity.name, i + 1),
            transform,
            "crenellation",
        )?;
        for tag in ["crenellation", "detail"] {
            if !object.tags.contains(&tag.to_string()) {
                object.tags.push(tag.to_string());
            }
        }
        object.visual["draft"]["proxy_group"] = serde_json::json!("crenellation");
        object.metadata["render_mode"] = serde_json::json!("instance_set");
        objects.push(object);
    }

    Ok(objects)
}
