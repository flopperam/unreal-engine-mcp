use criterion::{black_box, criterion_group, criterion_main, Criterion};
use scene_syncd::domain::{Rotator, SceneObject, Transform, Vec3};
use scene_syncd::geom::footprint::Footprint2;
use scene_syncd::validation::engine::ValidationEngine;
use scene_syncd::validation::rules::no_overlap::NoSameLayerOverlap;
use scene_syncd::validation::rules::no_zero_scale::NoZeroOrNegativeScale;
use serde_json::json;

fn make_object(mcp_id: &str, x: f64, y: f64) -> SceneObject {
    SceneObject {
        id: String::new(),
        scene: "scene:bench".to_string(),
        group: None,
        mcp_id: mcp_id.to_string(),
        desired_name: mcp_id.to_string(),
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
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
        },
        visual: json!({}),
        physics: json!({}),
        tags: vec!["layout_kind:keep".to_string()],
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

fn bench_validate(c: &mut Criterion) {
    for size in [10, 50, 100, 500] {
        let objects: Vec<SceneObject> = (0..size)
            .map(|i| {
                make_object(
                    &format!("obj_{}", i),
                    (i % 100) as f64 * 500.0,
                    (i / 100) as f64 * 500.0,
                )
            })
            .collect();
        let footprints: Vec<Footprint2> = objects
            .iter()
            .map(|o| Footprint2::from_scene_object(o, 0))
            .collect();

        let mut engine = ValidationEngine::new();
        engine.add_rule(Box::new(NoSameLayerOverlap));
        engine.add_rule(Box::new(NoZeroOrNegativeScale));

        c.bench_function(&format!("validate_{}", size), |b| {
            b.iter(|| {
                black_box(engine.validate(&objects, &footprints));
            })
        });
    }
}

criterion_group!(benches, bench_validate);
criterion_main!(benches);
