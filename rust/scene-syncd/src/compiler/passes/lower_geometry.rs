use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::error::AppError;
use crate::geom::footprint::Footprint2;
use crate::ir::geometric::{
    Connector, ConnectorType, FootprintPrimitive, GeometricIr, GeometricPrimitive, VolumePrimitive,
};
use crate::layout::kind_registry::KindRegistry;

pub struct GeometryLoweringPass;

impl Pass for GeometryLoweringPass {
    fn name(&self) -> &'static str {
        "lower_geometry"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        let registry = KindRegistry::default();
        ctx.footprints.clear();
        let mut geo_ir = GeometricIr::new();

        for obj in &ctx.objects {
            if obj.deleted {
                continue;
            }
            let kind = obj.tags.iter().find_map(|t| t.strip_prefix("layout_kind:"));
            let layer = kind
                .and_then(|k| registry.get(k))
                .map(|s| s.layer)
                .unwrap_or(0);
            let fp = Footprint2::from_scene_object(obj, layer);
            ctx.footprints.push(fp.clone());

            let entity_id = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_entity:"))
                .unwrap_or(&obj.mcp_id)
                .to_string();
            let kind_str = kind.unwrap_or("").to_string();

            // Build geometric primitives based on kind.
            if is_surface_like(&kind_str) {
                let polygon = if let Some(ref poly) = fp.polygon {
                    poly.exterior.clone()
                } else {
                    vec![
                        (fp.min_x, fp.min_y),
                        (fp.max_x, fp.min_y),
                        (fp.max_x, fp.max_y),
                        (fp.min_x, fp.max_y),
                    ]
                };
                geo_ir
                    .primitives
                    .push(GeometricPrimitive::Footprint(FootprintPrimitive {
                        entity_id: entity_id.clone(),
                        kind: kind_str.clone(),
                        polygon,
                        z: fp.z,
                    }));
            } else {
                let base_polygon = if let Some(ref poly) = fp.polygon {
                    poly.exterior.clone()
                } else {
                    vec![
                        (fp.min_x, fp.min_y),
                        (fp.max_x, fp.min_y),
                        (fp.max_x, fp.max_y),
                        (fp.min_x, fp.max_y),
                    ]
                };
                let height = obj
                    .metadata
                    .get("height")
                    .and_then(|v| v.as_f64())
                    .unwrap_or(10.0);
                geo_ir
                    .primitives
                    .push(GeometricPrimitive::Volume(VolumePrimitive {
                        entity_id: entity_id.clone(),
                        kind: kind_str.clone(),
                        base_polygon,
                        base_z: fp.z,
                        height,
                        transform: obj.transform.clone(),
                    }));
            }
        }

        ctx.geometric_ir = Some(geo_ir);
        Ok(())
    }
}

fn is_surface_like(kind: &str) -> bool {
    matches!(kind, "ground" | "moat" | "road" | "bridge" | "water")
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_object(kind: &str, x: f64, y: f64, sx: f64, sy: f64) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: format!("{kind}_obj"),
            desired_name: kind.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: Transform {
                location: Vec3 { x, y, z: 0.0 },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw: 0.0,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: sx,
                    y: sy,
                    z: 1.0,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec![format!("layout_kind:{}", kind)],
            metadata: json!({}),
            desired_hash: String::new(),
            last_applied_hash: None,
            sync_status: "pending".to_string(),
            deleted: false,
            revision: 1,
            created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
        }
    }

    #[test]
    fn lowers_footprints_for_objects() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![
            make_object("keep", 0.0, 0.0, 2.0, 2.0),
            make_object("tower", 100.0, 0.0, 1.0, 1.0),
        ];
        let pass = GeometryLoweringPass;
        pass.run(&mut ctx).unwrap();
        assert_eq!(ctx.footprints.len(), 2);
        assert_eq!(ctx.footprints[0].mcp_id, "keep_obj");
        assert_eq!(ctx.footprints[1].mcp_id, "tower_obj");
        assert!(ctx.geometric_ir.is_some());
    }

    #[test]
    fn skips_deleted_objects() {
        let mut ctx = CompilerContext::new("test".to_string());
        let mut obj = make_object("keep", 0.0, 0.0, 2.0, 2.0);
        obj.deleted = true;
        ctx.objects = vec![obj];
        let pass = GeometryLoweringPass;
        pass.run(&mut ctx).unwrap();
        assert!(ctx.footprints.is_empty());
        let geo = ctx.geometric_ir.unwrap();
        assert!(geo.primitives.is_empty());
    }

    #[test]
    fn surface_kind_gets_footprint_primitive() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![make_object("ground", 0.0, 0.0, 10.0, 10.0)];
        let pass = GeometryLoweringPass;
        pass.run(&mut ctx).unwrap();
        let geo = ctx.geometric_ir.unwrap();
        assert_eq!(geo.primitives.len(), 1);
        assert!(matches!(
            geo.primitives[0],
            GeometricPrimitive::Footprint(..)
        ));
    }

    #[test]
    fn volume_kind_gets_volume_primitive() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![make_object("keep", 0.0, 0.0, 2.0, 2.0)];
        let pass = GeometryLoweringPass;
        pass.run(&mut ctx).unwrap();
        let geo = ctx.geometric_ir.unwrap();
        assert_eq!(geo.primitives.len(), 1);
        assert!(matches!(geo.primitives[0], GeometricPrimitive::Volume(..)));
    }
}
