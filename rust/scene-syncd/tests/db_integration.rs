use scene_syncd::db::SurrealSceneRepository;
use scene_syncd::domain::*;
use std::sync::atomic::{AtomicU64, Ordering};
use surrealdb::engine::any::Any;
use surrealdb::sql::Datetime;
use surrealdb::Surreal;

static DB_COUNTER: AtomicU64 = AtomicU64::new(0);

async fn setup_db() -> Surreal<Any> {
    let db: Surreal<Any> = Surreal::init();
    db.connect("memory").await.unwrap();
    let db_name = format!("test_{}", DB_COUNTER.fetch_add(1, Ordering::Relaxed));
    db.use_ns("test").use_db(db_name).await.unwrap();
    db
}

fn make_object(scene: &str, mcp_id: &str) -> SceneObject {
    let now = Datetime::from(chrono::Utc::now());
    SceneObject {
        id: String::new(),
        scene: format!("scene:{scene}"),
        group: None,
        mcp_id: mcp_id.to_string(),
        desired_name: mcp_id.to_string(),
        unreal_actor_name: None,
        actor_type: "StaticMeshActor".to_string(),
        asset_ref: serde_json::json!({"path": "/Engine/BasicShapes/Cube.Cube"}),
        transform: Transform::default(),
        visual: serde_json::json!({}),
        physics: serde_json::json!({}),
        tags: vec!["test".to_string()],
        metadata: serde_json::json!({}),
        desired_hash: "fake_hash".to_string(),
        last_applied_hash: None,
        sync_status: "pending".to_string(),
        deleted: false,
        revision: 1,
        created_at: now.clone(),
        updated_at: now,
    }
}

#[tokio::test]
async fn ensure_schema_and_default_scene() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    let scene = repo.upsert_scene("test_scene", "Test", None).await.unwrap();
    assert_eq!(scene.name, "Test");
}

#[tokio::test]
async fn upsert_and_list_objects() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    let obj = make_object("main", "cube_01");
    repo.upsert_object(&obj).await.unwrap();

    let objects = repo
        .list_desired_objects("main", false, None, None)
        .await
        .unwrap();
    assert_eq!(objects.len(), 1);
    assert_eq!(objects[0].mcp_id, "cube_01");
}

#[tokio::test]
async fn upsert_updates_existing_object() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    let mut obj = make_object("main", "cube_01");
    obj.transform.location.x = 100.0;
    obj.desired_hash = "hash_v1".to_string();
    repo.upsert_object(&obj).await.unwrap();

    let mut updated = make_object("main", "cube_01");
    updated.transform.location.x = 200.0;
    updated.desired_hash = "hash_v2".to_string();
    repo.upsert_object(&updated).await.unwrap();

    let objects = repo
        .list_desired_objects("main", false, None, None)
        .await
        .unwrap();
    assert_eq!(objects.len(), 1);
    assert_eq!(objects[0].transform.location.x, 200.0);
    assert_eq!(objects[0].desired_hash, "hash_v2");
}

#[tokio::test]
async fn list_objects_excludes_deleted() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_object(&make_object("main", "cube_01"))
        .await
        .unwrap();
    repo.upsert_object(&make_object("main", "cube_02"))
        .await
        .unwrap();

    repo.mark_object_deleted("main", "cube_01").await.unwrap();

    let active = repo
        .list_desired_objects("main", false, None, None)
        .await
        .unwrap();
    assert_eq!(active.len(), 1);
    assert_eq!(active[0].mcp_id, "cube_02");

    let all = repo
        .list_desired_objects("main", true, None, None)
        .await
        .unwrap();
    assert_eq!(all.len(), 2);
}

#[tokio::test]
async fn list_objects_group_id_filter() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    let mut obj_a = make_object("main", "cube_a");
    obj_a.group = Some("scene_group:wall_01".to_string());
    repo.upsert_object(&obj_a).await.unwrap();

    let mut obj_b = make_object("main", "cube_b");
    obj_b.group = Some("scene_group:pyramid_01".to_string());
    repo.upsert_object(&obj_b).await.unwrap();

    repo.upsert_object(&make_object("main", "cube_c"))
        .await
        .unwrap();

    let wall_objs = repo
        .list_desired_objects("main", false, Some("wall_01"), None)
        .await
        .unwrap();
    assert_eq!(wall_objs.len(), 1);
    assert_eq!(wall_objs[0].mcp_id, "cube_a");
}

#[tokio::test]
async fn list_objects_limit() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    for i in 0..10 {
        repo.upsert_object(&make_object("main", &format!("cube_{i:02}")))
            .await
            .unwrap();
    }

    let limited = repo
        .list_desired_objects("main", false, None, Some(3))
        .await
        .unwrap();
    assert_eq!(limited.len(), 3);
}

#[tokio::test]
async fn mark_object_synced() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_object(&make_object("main", "cube_01"))
        .await
        .unwrap();

    repo.mark_object_synced("main", "cube_01", "hash_v1", Some("SM_Cube_01"))
        .await
        .unwrap();

    let objects = repo
        .list_desired_objects("main", false, None, None)
        .await
        .unwrap();
    assert_eq!(objects[0].sync_status, "synced");
    assert_eq!(objects[0].last_applied_hash.as_deref(), Some("hash_v1"));
    assert_eq!(objects[0].unreal_actor_name.as_deref(), Some("SM_Cube_01"));
}

#[tokio::test]
async fn create_and_list_groups() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.create_group(
        "main",
        "wall",
        "my_wall",
        Some("create_wall".to_string()),
        serde_json::json!({"length": 5}),
        None,
    )
    .await
    .unwrap();

    let groups = repo.list_groups("main", false).await.unwrap();
    assert_eq!(groups.len(), 1);
    assert_eq!(groups[0].name, "my_wall");
    assert_eq!(groups[0].kind, "wall");
}

#[tokio::test]
async fn create_and_list_snapshots() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_object(&make_object("main", "cube_01"))
        .await
        .unwrap();
    repo.upsert_object(&make_object("main", "cube_02"))
        .await
        .unwrap();

    let snap = repo
        .create_snapshot("main", "initial", Some("first snapshot".to_string()))
        .await
        .unwrap();
    assert_eq!(snap.objects.len(), 2);

    let snapshots = repo.list_snapshots("main").await.unwrap();
    assert_eq!(snapshots.len(), 1);
}

#[tokio::test]
async fn restore_snapshot() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_object(&make_object("main", "cube_01"))
        .await
        .unwrap();
    repo.upsert_object(&make_object("main", "cube_02"))
        .await
        .unwrap();

    let snap = repo
        .create_snapshot("main", "pre_change", None)
        .await
        .unwrap();

    // Add a third object after snapshot
    repo.upsert_object(&make_object("main", "cube_03"))
        .await
        .unwrap();

    let summary = repo
        .restore_snapshot(&snap.id, "replace_desired")
        .await
        .unwrap();

    assert_eq!(summary["restored_objects"], 2);
    assert_eq!(summary["tombstoned_objects"], 1);

    // cube_03 should be deleted, cube_01 and cube_02 restored
    let active = repo
        .list_desired_objects("main", false, None, None)
        .await
        .unwrap();
    let active_ids: Vec<&str> = active.iter().map(|o| o.mcp_id.as_str()).collect();
    assert!(active_ids.contains(&"cube_01"));
    assert!(active_ids.contains(&"cube_02"));
    assert!(!active_ids.contains(&"cube_03"));
}

#[tokio::test]
async fn record_operation_and_sync_run() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.create_sync_run("run_001", "main", "apply_safe", "running")
        .await
        .unwrap();

    repo.record_operation("run_001", "main", "cube_01", "create", "ok", "new object")
        .await
        .unwrap();

    let summary = scene_syncd::sync::applier::SyncApplySummary {
        total: 1,
        succeeded: 1,
        failed: 0,
        skipped: 0,
        creates: 1,
        update_transforms: 0,
        update_visuals: 0,
        deletes: 0,
        noops: 0,
        instance_set_creates: 0,
        instance_set_updates: 0,
        instance_set_deletes: 0,
    };
    repo.finish_sync_run("run_001", &summary).await.unwrap();
}

#[tokio::test]
async fn ensure_schema_is_idempotent() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    let objects = repo
        .list_desired_objects("main", false, None, None)
        .await
        .unwrap();
    assert_eq!(objects.len(), 0);
}

// --- P6: Component CRUD tests ---

#[tokio::test]
async fn test_component_crud() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_scene("test_scene", "Test Scene", Some("For testing".to_string()))
        .await
        .expect("create scene");

    // Upsert component
    let component = repo
        .upsert_component(
            "test_scene",
            "entity_01",
            "navmesh",
            "nav_volume_main",
            serde_json::json!({
                "location": {"x": 0.0, "y": 0.0, "z": 0.0},
                "extent": {"x": 500.0, "y": 500.0, "z": 500.0}
            }),
            serde_json::json!({}),
        )
        .await
        .expect("upsert component");

    assert_eq!(component.scene, "scene:test_scene");
    assert_eq!(component.entity_id, "entity_01");
    assert_eq!(component.component_type, "navmesh");
    assert_eq!(component.name, "nav_volume_main");

    // List components
    let components = repo
        .list_components("test_scene", Some("entity_01"), None)
        .await
        .expect("list components");
    assert!(!components.is_empty());
    assert_eq!(components[0].component_type, "navmesh");

    // Delete component
    repo.delete_component("test_scene", "entity_01", "navmesh", "nav_volume_main")
        .await
        .expect("delete component");

    // Verify deletion
    let components_after = repo
        .list_components("test_scene", Some("entity_01"), None)
        .await
        .expect("list components after delete");
    assert!(components_after.is_empty());
}

// --- P6: Blueprint CRUD tests ---

#[tokio::test]
async fn test_blueprint_crud() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_scene("test_scene", "Test Scene", Some("For testing".to_string()))
        .await
        .expect("create scene");

    // Upsert blueprint
    let blueprint = repo
        .upsert_blueprint(
            "test_scene",
            "bp_guard_tower",
            "AStaticMeshActor",
            "AActor",
            vec![
                serde_json::json!({"type": "collision", "profile": "BlockAll"}),
                serde_json::json!({"type": "navmesh", "behavior": "blocked"}),
            ],
            vec![serde_json::json!({"name": "Health", "type": "float", "default": 100.0})],
            serde_json::json!({}),
        )
        .await
        .expect("upsert blueprint");

    assert_eq!(blueprint.scene, "scene:test_scene");
    assert_eq!(blueprint.blueprint_id, "bp_guard_tower");
    assert_eq!(blueprint.class_name, "AStaticMeshActor");

    // List blueprints
    let blueprints = repo
        .list_blueprints("test_scene", None)
        .await
        .expect("list blueprints");
    assert!(!blueprints.is_empty());

    // Delete blueprint
    repo.delete_blueprint("test_scene", "bp_guard_tower")
        .await
        .expect("delete blueprint");

    // Verify deletion
    let blueprints_after = repo
        .list_blueprints("test_scene", None)
        .await
        .expect("list blueprints after delete");
    assert!(blueprints_after.is_empty());
}

// --- P6: Realization CRUD tests ---

#[tokio::test]
async fn test_realization_crud() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_scene("test_scene", "Test Scene", Some("For testing".to_string()))
        .await
        .expect("create scene");

    // Upsert realization
    let realization = repo
        .upsert_realization(
            "test_scene",
            "entity_01",
            "blueprint",
            "pending",
            None,
            serde_json::json!({
                "blueprint_path": "/Game/Blueprints/BP_Tower.BP_Tower"
            }),
        )
        .await
        .expect("upsert realization");

    assert_eq!(realization.scene, "scene:test_scene");
    assert_eq!(realization.entity_id, "entity_01");
    assert_eq!(realization.policy, "blueprint");
    assert_eq!(realization.status, "pending");

    // List realizations
    let realizations = repo
        .list_realizations("test_scene", Some("entity_01"), None)
        .await
        .expect("list realizations");
    assert!(!realizations.is_empty());
    assert_eq!(realizations[0].policy, "blueprint");

    // Update realization status
    let updated = repo
        .update_realization_status("test_scene", "entity_01", "blueprint", "realized")
        .await
        .expect("update realization status");
    assert_eq!(updated.status, "realized");
}

// --- P6: Combined query tests ---

#[tokio::test]
async fn test_find_entity_and_realization_combined() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    repo.upsert_scene("test_scene", "Test Scene", Some("For testing".to_string()))
        .await
        .expect("create scene");

    // Create a semantic entity with an mcp_id
    let entity = repo
        .upsert_entity(
            "test_scene",
            "entity_01",
            "actor",
            "GuardTower",
            serde_json::json!({"health": 100}),
            vec!["e2e".to_string()],
            vec!["guard_01".to_string()],
            serde_json::json!({}),
        )
        .await
        .expect("upsert entity");

    assert_eq!(entity.entity_id, "entity_01");

    // Create a realization for the same entity
    let realization = repo
        .upsert_realization(
            "test_scene",
            "entity_01",
            "blueprint",
            "pending",
            None,
            serde_json::json!({"blueprint_path": "/Game/Blueprints/BP_Tower.BP_Tower"}),
        )
        .await
        .expect("upsert realization");

    assert_eq!(realization.policy, "blueprint");

    // Combined query: find entity by mcp_id, then find realization for that entity
    let found_entity = repo
        .find_entity_by_mcp_id("test_scene", "guard_01")
        .await
        .expect("find_entity_by_mcp_id")
        .expect("entity should exist");
    assert_eq!(found_entity.entity_id, "entity_01");

    let found_realization = repo
        .find_realization_for_entity("test_scene", &found_entity.entity_id)
        .await
        .expect("find_realization_for_entity")
        .expect("realization should exist");
    assert_eq!(found_realization.policy, "blueprint");
    assert_eq!(found_realization.entity_id, "entity_01");
    assert_eq!(found_realization.status, "pending");
}

#[tokio::test]
async fn test_find_entity_by_mcp_id_returns_none_for_missing() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    let result = repo
        .find_entity_by_mcp_id("test_scene", "nonexistent")
        .await
        .expect("query should not fail");
    assert!(result.is_none());
}

#[tokio::test]
async fn test_find_realization_for_entity_returns_none_for_missing() {
    let db = setup_db().await;
    let repo = SurrealSceneRepository::new(db);
    repo.ensure_schema().await.unwrap();
    repo.ensure_default_scene().await.unwrap();

    let result = repo
        .find_realization_for_entity("test_scene", "nonexistent")
        .await
        .expect("query should not fail");
    assert!(result.is_none());
}
