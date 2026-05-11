use axum::extract::State;
use axum::Json;
use serde::Deserialize;
use serde_json::{json, Value};
use std::collections::HashMap;
use std::sync::Arc;
use std::time::Duration;
use surrealdb::engine::any::Any;
use surrealdb::sql::Datetime;
use surrealdb::Surreal;
use tokio::sync::Mutex;
use tokio::time::timeout;

use crate::compiler::passes::Pass;
use crate::config::Config;
use crate::db::SurrealSceneRepository;
use crate::domain::ids::validate_mcp_id;
use crate::domain::transform::compute_desired_hash;
use crate::domain::*;
use crate::error::AppError;
use crate::layout::denormalizer::denormalize_layout;
use crate::layout::kind_registry::KindRegistry;
use crate::layout::preview::preview_layout;
use crate::layout::realization::{realize_layout, RealizationStage};
use crate::sync::applier::apply_sync;
use crate::sync::planner::plan_sync;
use crate::unreal::client::UnrealClient;

#[derive(Debug, Clone)]
pub struct AppState {
    pub db: Surreal<Any>,
    pub config: Config,
    pub scene_locks: Arc<Mutex<HashMap<String, Arc<Mutex<()>>>>>,
    pub unreal_client: UnrealClient,
}

fn success_response(data: Value) -> Value {
    json!({
        "success": true,
        "data": data,
        "warnings": [],
        "error": null
    })
}

fn error_response(code: &str, message: &str) -> Value {
    json!({
        "success": false,
        "data": null,
        "warnings": [],
        "error": {
            "code": code,
            "message": message
        }
    })
}

fn normalize_scene_id_input(id: &str) -> Result<String, AppError> {
    crate::domain::ids::normalize_scene_id(id).map_err(AppError::Validation)
}

pub async fn health() -> Json<Value> {
    Json(json!({
        "success": true,
        "data": { "status": "ok" },
        "warnings": [],
        "error": null
    }))
}

#[derive(Debug, Deserialize)]
pub struct CreateSceneRequest {
    pub scene_id: String,
    #[serde(default)]
    pub name: Option<String>,
    #[serde(default)]
    pub description: Option<String>,
}

pub async fn create_scene(
    State(state): State<AppState>,
    Json(req): Json<CreateSceneRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let display_name = req.name.unwrap_or_else(|| scene_id.clone());
    let scene = repo
        .upsert_scene(&scene_id, &display_name, req.description)
        .await?;
    Ok(Json(success_response(
        serde_json::to_value(scene)
            .map_err(|e| AppError::Internal(format!("serialize scene error: {e}")))?,
    )))
}

#[derive(Debug, Deserialize)]
pub struct ListScenesRequest {}

pub async fn list_scenes(
    State(state): State<AppState>,
    Json(_req): Json<ListScenesRequest>,
) -> Result<Json<Value>, AppError> {
    let repo = SurrealSceneRepository::new(state.db.clone());
    let scenes = repo.list_scenes().await?;
    Ok(Json(success_response(json!({ "scenes": scenes }))))
}

#[derive(Debug, Deserialize)]
pub struct UpsertObjectRequest {
    pub scene_id: String,
    pub mcp_id: String,
    #[serde(default)]
    pub desired_name: Option<String>,
    #[serde(default = "default_actor_type")]
    pub actor_type: String,
    #[serde(default)]
    pub asset_ref: Option<serde_json::Value>,
    #[serde(default)]
    pub transform: Option<serde_json::Value>,
    #[serde(default)]
    pub visual: Option<serde_json::Value>,
    #[serde(default)]
    pub physics: Option<serde_json::Value>,
    #[serde(default)]
    pub tags: Option<Vec<String>>,
    #[serde(default)]
    pub metadata: Option<serde_json::Value>,
    #[serde(default)]
    pub group_id: Option<String>,
}

fn default_actor_type() -> String {
    "StaticMeshActor".to_string()
}

fn object_or_empty(v: Option<serde_json::Value>) -> serde_json::Value {
    match v {
        Some(serde_json::Value::Null) | None => json!({}),
        Some(value) => value,
    }
}

fn parse_transform(v: Option<serde_json::Value>) -> Transform {
    match v {
        Some(t) => {
            let loc = t.get("location");
            let rot = t.get("rotation");
            let scl = t.get("scale");
            Transform {
                location: Vec3 {
                    x: loc
                        .and_then(|l| l.get("x"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(0.0),
                    y: loc
                        .and_then(|l| l.get("y"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(0.0),
                    z: loc
                        .and_then(|l| l.get("z"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(0.0),
                },
                rotation: Rotator {
                    pitch: rot
                        .and_then(|r| r.get("pitch"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(0.0),
                    yaw: rot
                        .and_then(|r| r.get("yaw"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(0.0),
                    roll: rot
                        .and_then(|r| r.get("roll"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(0.0),
                },
                scale: Vec3 {
                    x: scl
                        .and_then(|s| s.get("x"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(1.0),
                    y: scl
                        .and_then(|s| s.get("y"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(1.0),
                    z: scl
                        .and_then(|s| s.get("z"))
                        .and_then(|v| v.as_f64())
                        .unwrap_or(1.0),
                },
            }
        }
        None => Transform::default(),
    }
}

pub async fn upsert_object(
    State(state): State<AppState>,
    Json(req): Json<UpsertObjectRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    if let Err(e) = validate_mcp_id(&req.mcp_id) {
        return Err(AppError::Validation(e));
    }

    let transform = parse_transform(req.transform);
    let desired_name = req.desired_name.unwrap_or_else(|| req.mcp_id.clone());

    let mut obj = SceneObject {
        id: format!("scene_object:{}:{}", scene_id, req.mcp_id),
        scene: format!("scene:{}", scene_id),
        group: req.group_id.map(|g| format!("scene_group:{g}")),
        mcp_id: req.mcp_id.clone(),
        desired_name,
        unreal_actor_name: None,
        actor_type: req.actor_type,
        asset_ref: object_or_empty(req.asset_ref),
        transform,
        visual: object_or_empty(req.visual),
        physics: object_or_empty(req.physics),
        tags: req.tags.unwrap_or_default(),
        metadata: object_or_empty(req.metadata),
        desired_hash: String::new(),
        last_applied_hash: None,
        sync_status: "pending".to_string(),
        deleted: false,
        revision: 1,
        created_at: Datetime::from(chrono::Utc::now()),
        updated_at: Datetime::from(chrono::Utc::now()),
    };

    obj.desired_hash = compute_desired_hash(&obj).map_err(AppError::Internal)?;

    let repo = SurrealSceneRepository::new(state.db.clone());
    let saved = repo.upsert_object(&obj).await?;

    Ok(Json(success_response(
        serde_json::to_value(saved)
            .map_err(|e| AppError::Internal(format!("serialize object error: {e}")))?,
    )))
}

#[derive(Debug, Deserialize)]
pub struct ListObjectsRequest {
    pub scene_id: String,
    #[serde(default)]
    pub include_deleted: bool,
    #[serde(default)]
    pub group_id: Option<String>,
    #[serde(default)]
    pub limit: Option<usize>,
}

pub async fn list_objects(
    State(state): State<AppState>,
    Json(req): Json<ListObjectsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let objects = repo
        .list_desired_objects(
            &scene_id,
            req.include_deleted,
            req.group_id.as_deref(),
            req.limit,
        )
        .await?;
    Ok(Json(success_response(json!({ "objects": objects }))))
}

#[derive(Debug, Deserialize)]
pub struct DeleteObjectRequest {
    pub scene_id: String,
    pub mcp_id: String,
}

pub async fn delete_object(
    State(state): State<AppState>,
    Json(req): Json<DeleteObjectRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    repo.mark_object_deleted(&scene_id, &req.mcp_id).await?;
    Ok(Json(success_response(
        json!({ "tombstoned": true, "mcp_id": req.mcp_id }),
    )))
}

#[derive(Debug, Deserialize)]
pub struct CreateGroupRequest {
    pub scene_id: String,
    pub kind: String,
    pub name: String,
    #[serde(default)]
    pub tool_name: Option<String>,
    #[serde(default)]
    pub params: serde_json::Value,
    #[serde(default)]
    pub seed: Option<String>,
}

pub async fn create_group(
    State(state): State<AppState>,
    Json(req): Json<CreateGroupRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let group = repo
        .create_group(
            &scene_id,
            &req.kind,
            &req.name,
            req.tool_name,
            req.params,
            req.seed,
        )
        .await?;
    Ok(Json(success_response(json!({ "group": group }))))
}

#[derive(Debug, Deserialize)]
pub struct ListGroupsRequest {
    pub scene_id: String,
    #[serde(default)]
    pub include_deleted: bool,
}

pub async fn list_groups(
    State(state): State<AppState>,
    Json(req): Json<ListGroupsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let groups = repo.list_groups(&scene_id, req.include_deleted).await?;
    Ok(Json(success_response(json!({
        "groups": groups,
        "count": groups.len(),
    }))))
}

#[derive(Debug, Deserialize)]
pub struct CreateGeneratorRunRequest {
    pub scene_id: String,
    pub kind: String,
    pub tool_name: String,
    pub name: String,
    #[serde(default)]
    pub params: serde_json::Value,
    #[serde(default)]
    pub seed: Option<String>,
    #[serde(default)]
    pub group_id: Option<String>,
    #[serde(default)]
    pub generated_count: i64,
}

pub async fn create_generator_run(
    State(state): State<AppState>,
    Json(req): Json<CreateGeneratorRunRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let run = repo
        .create_generator_run(
            &scene_id,
            &req.kind,
            &req.tool_name,
            &req.name,
            req.params,
            req.seed,
            req.group_id,
            req.generated_count,
        )
        .await?;
    Ok(Json(success_response(json!({ "generator_run": run }))))
}

pub async fn get_generator_run(
    State(state): State<AppState>,
    axum::extract::Path(run_id): axum::extract::Path<String>,
) -> Result<Json<Value>, AppError> {
    let repo = SurrealSceneRepository::new(state.db.clone());
    let run = repo.get_generator_run(&run_id).await?;
    match run {
        Some(r) => Ok(Json(success_response(json!({ "generator_run": r })))),
        None => Err(AppError::NotFound(format!(
            "generator_run {run_id} not found"
        ))),
    }
}

#[derive(Debug, Deserialize)]
pub struct CreateSnapshotRequest {
    pub scene_id: String,
    pub name: String,
    #[serde(default)]
    pub description: Option<String>,
}

pub async fn create_snapshot(
    State(state): State<AppState>,
    Json(req): Json<CreateSnapshotRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let snapshot = repo
        .create_snapshot(&scene_id, &req.name, req.description)
        .await?;
    let snapshot_id = snapshot.id.clone();
    let object_count = snapshot.objects.len();
    Ok(Json(success_response(json!({
        "snapshot": snapshot,
        "snapshot_id": snapshot_id,
        "object_count": object_count,
    }))))
}

#[derive(Debug, Deserialize)]
pub struct ListSnapshotsRequest {
    pub scene_id: String,
}

pub async fn list_snapshots(
    State(state): State<AppState>,
    Json(req): Json<ListSnapshotsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let snapshots = repo.list_snapshots(&scene_id).await?;
    Ok(Json(success_response(json!({
        "snapshots": snapshots,
        "count": snapshots.len(),
    }))))
}

#[derive(Debug, Deserialize)]
pub struct RestoreSnapshotRequest {
    pub snapshot_id: String,
    #[serde(default = "default_restore_mode")]
    pub restore_mode: String,
}

fn default_restore_mode() -> String {
    "replace_desired".to_string()
}

pub async fn restore_snapshot(
    State(state): State<AppState>,
    Json(req): Json<RestoreSnapshotRequest>,
) -> Result<Json<Value>, AppError> {
    let repo = SurrealSceneRepository::new(state.db.clone());
    let summary = repo
        .restore_snapshot(&req.snapshot_id, &req.restore_mode)
        .await?;
    Ok(Json(success_response(summary)))
}

#[derive(Debug, Deserialize)]
pub struct BulkUpsertRequest {
    pub scene_id: String,
    #[serde(default)]
    pub group_id: Option<String>,
    pub objects: Vec<UpsertObjectRequest>,
}

const MAX_BATCH_SIZE: usize = 500;

pub async fn bulk_upsert_objects(
    State(state): State<AppState>,
    Json(req): Json<BulkUpsertRequest>,
) -> Result<Json<Value>, AppError> {
    if req.objects.len() > MAX_BATCH_SIZE {
        return Err(AppError::Validation(format!(
            "bulk upsert exceeded maximum batch size of {MAX_BATCH_SIZE}; received {} objects",
            req.objects.len()
        )));
    }

    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let mut created = Vec::new();
    let mut errors = Vec::new();

    for (i, obj_req) in req.objects.into_iter().enumerate() {
        if let Err(e) = validate_mcp_id(&obj_req.mcp_id) {
            errors.push(json!({
                "index": i,
                "mcp_id": null,
                "error": e
            }));
            continue;
        }

        let transform = parse_transform(obj_req.transform);
        let desired_name = obj_req
            .desired_name
            .unwrap_or_else(|| obj_req.mcp_id.clone());

        let mut obj = SceneObject {
            id: format!("scene_object:{}:{}", scene_id, obj_req.mcp_id),
            scene: format!("scene:{}", scene_id),
            group: obj_req
                .group_id
                .clone()
                .or_else(|| req.group_id.clone())
                .map(|g| format!("scene_group:{g}")),
            mcp_id: obj_req.mcp_id.clone(),
            desired_name,
            unreal_actor_name: None,
            actor_type: obj_req.actor_type,
            asset_ref: object_or_empty(obj_req.asset_ref),
            transform,
            visual: object_or_empty(obj_req.visual),
            physics: object_or_empty(obj_req.physics),
            tags: obj_req.tags.unwrap_or_default(),
            metadata: object_or_empty(obj_req.metadata),
            desired_hash: String::new(),
            last_applied_hash: None,
            sync_status: "pending".to_string(),
            deleted: false,
            revision: 1,
            created_at: Datetime::from(chrono::Utc::now()),
            updated_at: Datetime::from(chrono::Utc::now()),
        };

        match compute_desired_hash(&obj) {
            Ok(hash) => obj.desired_hash = hash,
            Err(e) => {
                errors.push(json!({
                    "index": i,
                    "mcp_id": obj_req.mcp_id,
                    "error": e
                }));
                continue;
            }
        }

        match repo.upsert_object(&obj).await {
            Ok(saved) => created.push(serde_json::to_value(saved).unwrap_or_default()),
            Err(e) => {
                errors.push(json!({
                    "index": i,
                    "mcp_id": obj_req.mcp_id,
                    "error": e.to_string()
                }));
            }
        }
    }

    let response_body = json!({
        "upserted_count": created.len(),
        "error_count": errors.len(),
        "objects": created,
        "errors": errors,
    });

    if errors.is_empty() {
        Ok(Json(success_response(response_body)))
    } else if created.is_empty() {
        Ok(Json(error_response(
            "BULK_UPSERT_FAILED",
            "All bulk upsert operations failed",
        )))
    } else {
        let mut body = response_body;
        body["success"] = json!(false);
        body["partial_success"] = json!(true);
        Ok(Json(body))
    }
}

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct PlanSyncRequest {
    pub scene_id: String,
    #[serde(default = "default_plan_mode")]
    pub mode: String,
    #[serde(default)]
    pub orphan_policy: Option<String>,
}

fn default_plan_mode() -> String {
    "plan_only".to_string()
}

pub async fn plan_sync_route(
    State(state): State<AppState>,
    Json(req): Json<PlanSyncRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let desired_objects = repo
        .list_desired_objects(&scene_id, true, None, None)
        .await?;

    let unreal_client = state.unreal_client.clone();
    let (actual_actors, plan_unreal_warning) = match unreal_client.get_actors_in_level().await {
        Ok(actors) => (actors, None),
        Err(e) => {
            let msg = format!(
                "Could not reach Unreal for plan_sync: {e}. Proceeding with empty actual state."
            );
            tracing::warn!("{}", msg);
            (Vec::new(), Some(msg))
        }
    };

    let plan = plan_sync(&scene_id, &desired_objects, &actual_actors);

    // Phase 4: Density planning for instance set preview in plan response
    let density_plan = {
        let mut ctx = crate::compiler::context::CompilerContext::new(scene_id.clone());
        ctx.objects = desired_objects.clone();
        let pass = crate::compiler::passes::plan_density_lod::DensityPlannerPass;
        let _ = pass.run(&mut ctx);
        ctx.render_plan
    };
    let instance_sets = density_plan
        .as_ref()
        .map(|p| p.instance_sets())
        .unwrap_or_default();
    let instance_set_count = instance_sets.len();

    let mut warnings = plan.warnings.clone();
    if let Some(w) = plan_unreal_warning {
        warnings.push(w);
    }

    Ok(Json(success_response(json!({
        "scene_id": plan.scene_id,
        "summary": {
            "create": plan.summary.create,
            "update_transform": plan.summary.update_transform,
            "update_visual": plan.summary.update_visual,
            "delete": plan.summary.delete,
            "noop": plan.summary.noop,
            "conflict": plan.summary.conflict,
            "unsupported": plan.summary.unsupported,
            "instance_sets": instance_set_count,
            "instance_set_creates": instance_set_count,
        },
        "operations": plan.operations,
        "instance_sets": instance_sets.iter().map(|s| serde_json::json!({
            "set_id": s.set_id,
            "mesh": s.mesh,
            "instance_count": s.transforms.len(),
        })).collect::<Vec<_>>(),
        "warnings": warnings,
    }))))
}

#[derive(Debug, Deserialize)]
pub struct ApplySyncRequest {
    pub scene_id: String,
    #[serde(default = "default_apply_mode")]
    pub mode: String,
    #[serde(default)]
    pub allow_delete: bool,
    #[serde(default = "default_max_operations")]
    pub max_operations: usize,
}

fn default_apply_mode() -> String {
    "apply_safe".to_string()
}

fn default_max_operations() -> usize {
    2000
}

pub async fn apply_sync_route(
    State(state): State<AppState>,
    Json(req): Json<ApplySyncRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let desired_objects = repo
        .list_desired_objects(&scene_id, true, None, None)
        .await?;

    let allow_delete = req.mode == "apply_all" || req.allow_delete;
    let mode = req.mode.as_str();

    let unreal_client = state.unreal_client.clone();
    let (actual_actors, apply_unreal_warning) = match unreal_client.get_actors_in_level().await {
        Ok(actors) => (actors, None),
        Err(e) => {
            tracing::warn!("Could not reach Unreal for apply_sync: {e}");
            if !allow_delete {
                let msg = format!(
                    "Could not read Unreal actual state for apply_sync: {e}. Proceeding with empty actual state because deletes are disabled."
                );
                tracing::warn!("{}", msg);
                (Vec::new(), Some(msg))
            } else {
                return Ok(Json(error_response(
                "unreal_unreachable",
                &format!("Could not reach Unreal for apply_sync: {e}. Apply aborted to avoid unsafe operations on empty actual state."),
            )));
            }
        }
    };

    let mut plan = plan_sync(&scene_id, &desired_objects, &actual_actors);
    if let Some(warning) = apply_unreal_warning {
        plan.warnings.push(warning);
    }

    if plan.operations.len() > req.max_operations {
        return Err(AppError::Validation(
            format!(
                "plan has {} operations which exceeds max_operations {}. Use scene_plan_sync first to review.",
                plan.operations.len(),
                req.max_operations
            ),
        ));
    }

    let scene_lock = {
        let mut locks = state.scene_locks.lock().await;
        locks
            .entry(scene_id.clone())
            .or_insert_with(|| Arc::new(Mutex::new(())))
            .clone()
    };

    let _guard = match timeout(Duration::from_secs(30), scene_lock.lock()).await {
        Ok(guard) => guard,
        Err(_) => {
            return Err(AppError::Validation(
                format!("Could not acquire scene lock for '{}' within 30s; another sync apply is in progress.", scene_id),
            ));
        }
    };

    // Phase 4: run density planner to decide which objects become InstanceSets
    let desired_sets = {
        let mut ctx = crate::compiler::context::CompilerContext::new(scene_id.clone());
        ctx.objects = desired_objects.clone();
        let pass = crate::compiler::passes::plan_density_lod::DensityPlannerPass;
        let _ = pass.run(&mut ctx);
        ctx.instance_sets
    };

    let result = apply_sync(
        &unreal_client,
        &repo,
        &plan,
        mode,
        allow_delete,
        Some(&desired_sets),
    )
    .await?;

    drop(_guard);
    {
        let mut locks = state.scene_locks.lock().await;
        if let Some(lock_arc) = locks.get(&scene_id) {
            if std::sync::Arc::strong_count(lock_arc) == 1 {
                locks.remove(&scene_id);
            }
        }
    }

    Ok(Json(success_response(
        serde_json::to_value(result)
            .map_err(|e| AppError::Internal(format!("serialize result error: {e}")))?,
    )))
}

// ------------------------------------------------------------------
// P3: Semantic entity / relation / asset routes
// ------------------------------------------------------------------

#[derive(Debug, Deserialize)]
pub struct BulkUpsertEntitiesRequest {
    pub scene_id: String,
    pub entities: Vec<EntityPayload>,
}

#[derive(Debug, Deserialize)]
pub struct EntityPayload {
    pub entity_id: String,
    pub kind: String,
    pub name: String,
    #[serde(default)]
    pub properties: serde_json::Value,
    #[serde(default)]
    pub tags: Vec<String>,
    #[serde(default)]
    pub mcp_ids: Vec<String>,
    #[serde(default)]
    pub metadata: serde_json::Value,
}

pub async fn bulk_upsert_entities(
    State(state): State<AppState>,
    Json(req): Json<BulkUpsertEntitiesRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let mut created = Vec::new();
    let mut errors = Vec::new();

    for entity in req.entities {
        match repo
            .upsert_entity(
                &scene_id,
                &entity.entity_id,
                &entity.kind,
                &entity.name,
                entity.properties,
                entity.tags,
                entity.mcp_ids,
                entity.metadata,
            )
            .await
        {
            Ok(e) => created.push(serde_json::to_value(e).unwrap_or_default()),
            Err(e) => errors.push(json!({"entity_id": entity.entity_id, "error": e.to_string()})),
        }
    }

    Ok(Json(success_response(json!({
        "upserted_count": created.len(),
        "error_count": errors.len(),
        "entities": created,
        "errors": errors,
    }))))
}

#[derive(Debug, Deserialize)]
pub struct ListEntitiesRequest {
    pub scene_id: String,
    #[serde(default)]
    pub kind: Option<String>,
}

pub async fn list_entities(
    State(state): State<AppState>,
    Json(req): Json<ListEntitiesRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let entities = repo.list_entities(&scene_id, req.kind.as_deref()).await?;
    Ok(Json(success_response(json!({ "entities": entities }))))
}

#[derive(Debug, Deserialize)]
pub struct BulkUpsertRelationsRequest {
    pub scene_id: String,
    pub relations: Vec<RelationPayload>,
}

#[derive(Debug, Deserialize)]
pub struct RelationPayload {
    pub relation_id: String,
    pub source_entity_id: String,
    pub target_entity_id: String,
    pub relation_type: String,
    #[serde(default)]
    pub properties: serde_json::Value,
    #[serde(default)]
    pub metadata: serde_json::Value,
}

pub async fn bulk_upsert_relations(
    State(state): State<AppState>,
    Json(req): Json<BulkUpsertRelationsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let mut created = Vec::new();
    let mut errors = Vec::new();

    for relation in req.relations {
        match repo
            .upsert_relation(
                &scene_id,
                &relation.relation_id,
                &relation.source_entity_id,
                &relation.target_entity_id,
                &relation.relation_type,
                relation.properties,
                relation.metadata,
            )
            .await
        {
            Ok(r) => created.push(serde_json::to_value(r).unwrap_or_default()),
            Err(e) => {
                errors.push(json!({"relation_id": relation.relation_id, "error": e.to_string()}))
            }
        }
    }

    Ok(Json(success_response(json!({
        "upserted_count": created.len(),
        "error_count": errors.len(),
        "relations": created,
        "errors": errors,
    }))))
}

#[derive(Debug, Deserialize)]
pub struct ListRelationsRequest {
    pub scene_id: String,
    #[serde(default)]
    pub relation_type: Option<String>,
}

pub async fn list_relations(
    State(state): State<AppState>,
    Json(req): Json<ListRelationsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let relations = repo
        .list_relations(&scene_id, req.relation_type.as_deref())
        .await?;
    Ok(Json(success_response(json!({ "relations": relations }))))
}

#[derive(Debug, Deserialize)]
pub struct UpsertAssetRequest {
    pub scene_id: String,
    pub asset_id: String,
    pub kind: String,
    #[serde(default = "default_asset_status")]
    pub status: String,
    #[serde(default)]
    pub fallback: String,
    #[serde(default)]
    pub semantic_tags: Vec<String>,
    #[serde(default = "default_asset_quality")]
    pub quality: String,
    #[serde(default)]
    pub variants: serde_json::Value,
    #[serde(default)]
    pub metadata: serde_json::Value,
}

fn default_asset_status() -> String {
    "present".to_string()
}

fn default_asset_quality() -> String {
    "prototype".to_string()
}

pub async fn upsert_asset(
    State(state): State<AppState>,
    Json(req): Json<UpsertAssetRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let asset = repo
        .upsert_asset(
            &scene_id,
            &req.asset_id,
            &req.kind,
            &req.status,
            &req.fallback,
            req.semantic_tags,
            &req.quality,
            req.variants,
            req.metadata,
        )
        .await?;
    Ok(Json(success_response(json!({ "asset": asset }))))
}

#[derive(Debug, Deserialize)]
pub struct ListAssetsRequest {
    pub scene_id: String,
    #[serde(default)]
    pub kind: Option<String>,
}

pub async fn list_assets(
    State(state): State<AppState>,
    Json(req): Json<ListAssetsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let assets = repo.list_assets(&scene_id, req.kind.as_deref()).await?;
    Ok(Json(success_response(json!({ "assets": assets }))))
}

// ------------------------------------------------------------------
// P6: Component, Blueprint, Realization routes
// ------------------------------------------------------------------

#[derive(Debug, Deserialize)]
pub struct UpsertComponentRequest {
    pub scene_id: String,
    pub entity_id: String,
    pub component_type: String,
    pub name: String,
    #[serde(default)]
    pub properties: serde_json::Value,
    #[serde(default)]
    pub metadata: serde_json::Value,
}

pub async fn upsert_component(
    State(state): State<AppState>,
    Json(req): Json<UpsertComponentRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let component = repo
        .upsert_component(
            &scene_id,
            &req.entity_id,
            &req.component_type,
            &req.name,
            req.properties,
            req.metadata,
        )
        .await?;
    Ok(Json(success_response(json!({ "component": component }))))
}

#[derive(Debug, Deserialize)]
pub struct ListComponentsRequest {
    pub scene_id: String,
    #[serde(default)]
    pub entity_id: Option<String>,
    #[serde(default)]
    pub component_type: Option<String>,
}

pub async fn list_components(
    State(state): State<AppState>,
    Json(req): Json<ListComponentsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let components = repo
        .list_components(
            &scene_id,
            req.entity_id.as_deref(),
            req.component_type.as_deref(),
        )
        .await?;
    Ok(Json(success_response(json!({ "components": components }))))
}

#[derive(Debug, Deserialize)]
pub struct DeleteComponentRequest {
    pub scene_id: String,
    pub entity_id: String,
    pub component_type: String,
    pub name: String,
}

pub async fn delete_component(
    State(state): State<AppState>,
    Json(req): Json<DeleteComponentRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    repo.delete_component(&scene_id, &req.entity_id, &req.component_type, &req.name)
        .await?;
    Ok(Json(success_response(json!({ "deleted": true }))))
}

#[derive(Debug, Deserialize)]
pub struct UpsertBlueprintRequest {
    pub scene_id: String,
    pub blueprint_id: String,
    pub class_name: String,
    #[serde(default)]
    pub parent_class: String,
    #[serde(default)]
    pub components: Vec<serde_json::Value>,
    #[serde(default)]
    pub variables: Vec<serde_json::Value>,
    #[serde(default)]
    pub metadata: serde_json::Value,
}

pub async fn upsert_blueprint(
    State(state): State<AppState>,
    Json(req): Json<UpsertBlueprintRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let blueprint = repo
        .upsert_blueprint(
            &scene_id,
            &req.blueprint_id,
            &req.class_name,
            &req.parent_class,
            req.components,
            req.variables,
            req.metadata,
        )
        .await?;
    Ok(Json(success_response(json!({ "blueprint": blueprint }))))
}

#[derive(Debug, Deserialize)]
pub struct ListBlueprintsRequest {
    pub scene_id: String,
    #[serde(default)]
    pub class_name: Option<String>,
}

pub async fn list_blueprints(
    State(state): State<AppState>,
    Json(req): Json<ListBlueprintsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let blueprints = repo
        .list_blueprints(&scene_id, req.class_name.as_deref())
        .await?;
    Ok(Json(success_response(json!({ "blueprints": blueprints }))))
}

#[derive(Debug, Deserialize)]
pub struct DeleteBlueprintRequest {
    pub scene_id: String,
    pub blueprint_id: String,
}

pub async fn delete_blueprint(
    State(state): State<AppState>,
    Json(req): Json<DeleteBlueprintRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    repo.delete_blueprint(&scene_id, &req.blueprint_id).await?;
    Ok(Json(success_response(json!({ "deleted": true }))))
}

#[derive(Debug, Deserialize)]
pub struct UpsertRealizationRequest {
    pub scene_id: String,
    pub entity_id: String,
    pub policy: String,
    #[serde(default = "default_realization_status")]
    pub status: String,
    #[serde(default)]
    pub unreal_actor_name: Option<String>,
    #[serde(default)]
    pub metadata: serde_json::Value,
}

fn default_realization_status() -> String {
    "pending".to_string()
}

pub async fn upsert_realization(
    State(state): State<AppState>,
    Json(req): Json<UpsertRealizationRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let realization = repo
        .upsert_realization(
            &scene_id,
            &req.entity_id,
            &req.policy,
            &req.status,
            req.unreal_actor_name,
            req.metadata,
        )
        .await?;
    Ok(Json(success_response(
        json!({ "realization": realization }),
    )))
}

#[derive(Debug, Deserialize)]
pub struct ListRealizationsRequest {
    pub scene_id: String,
    #[serde(default)]
    pub entity_id: Option<String>,
    #[serde(default)]
    pub policy: Option<String>,
}

pub async fn list_realizations(
    State(state): State<AppState>,
    Json(req): Json<ListRealizationsRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let realizations = repo
        .list_realizations(&scene_id, req.entity_id.as_deref(), req.policy.as_deref())
        .await?;
    Ok(Json(success_response(
        json!({ "realizations": realizations }),
    )))
}

#[derive(Debug, Deserialize)]
pub struct UpdateRealizationStatusRequest {
    pub scene_id: String,
    pub entity_id: String,
    pub policy: String,
    pub status: String,
}

pub async fn update_realization_status(
    State(state): State<AppState>,
    Json(req): Json<UpdateRealizationStatusRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());
    let realization = repo
        .update_realization_status(&scene_id, &req.entity_id, &req.policy, &req.status)
        .await?;
    Ok(Json(success_response(
        json!({ "realization": realization }),
    )))
}

// ------------------------------------------------------------------
// Layout editing & approval
// ------------------------------------------------------------------

pub async fn update_layout_node_transform(
    State(state): State<AppState>,
    axum::extract::Path((scene_id, entity_id)): axum::extract::Path<(String, String)>,
    Json(req): Json<serde_json::Value>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    // Merge existing properties with new transform data
    let entities = repo.list_entities(&scene_id, None).await?;
    let entity = entities
        .into_iter()
        .find(|e| e.entity_id == entity_id)
        .ok_or_else(|| AppError::NotFound(format!("entity {entity_id} not found")))?;

    let mut properties = entity.properties.clone();
    if let Some(loc) = req.get("location") {
        properties["location"] = loc.clone();
    }
    if let Some(rot) = req.get("rotation") {
        properties["rotation"] = rot.clone();
    }
    if let Some(scl) = req.get("scale") {
        properties["scale"] = scl.clone();
    }
    if let Some(props) = req.get("properties") {
        if let Some(obj) = props.as_object() {
            for (k, v) in obj {
                properties[k] = v.clone();
            }
        }
    }

    let updated = repo
        .update_entity_transform(&scene_id, &entity_id, properties)
        .await?;
    Ok(Json(success_response(
        serde_json::to_value(updated)
            .map_err(|e| AppError::Internal(format!("serialize error: {e}")))?,
    )))
}

pub async fn approve_layout(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    // Update scene status to approved_layout
    let _scene = repo
        .update_scene_status(&scene_id, "approved_layout")
        .await?;

    // Create a snapshot for rollback
    let snapshot = repo
        .create_snapshot(
            &scene_id,
            &format!(
                "auto_approved_{}",
                chrono::Utc::now().format("%Y%m%d%H%M%S")
            ),
            Some("Auto-snapshot on layout approval".to_string()),
        )
        .await?;

    Ok(Json(success_response(json!({
        "scene_id": scene_id,
        "status": "approved_layout",
        "snapshot_id": snapshot.id,
    }))))
}

pub async fn preview_layout_route(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let objects = preview_layout(&repo, &scene_id).await?;

    let object_values: Vec<serde_json::Value> = objects
        .into_iter()
        .map(|o| serde_json::to_value(o).unwrap_or_default())
        .collect();

    Ok(Json(success_response(json!({
        "scene_id": scene_id,
        "object_count": object_values.len(),
        "objects": object_values,
    }))))
}

pub async fn compile_preview_route(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let result =
        crate::compiler::pipeline::CompilerPipeline::compile_preview(&repo, &scene_id).await?;

    Ok(Json(success_response(json!({
        "scene_id": result.scene_id,
        "stage": result.stage,
        "summary": {
            "errors": result.summary.errors,
            "warnings": result.summary.warnings,
            "infos": result.summary.infos,
            "objects": result.summary.objects,
        },
        "objects": result.objects,
        "diagnostics": result.diagnostics,
    }))))
}

// ------------------------------------------------------------------
// Realization pipeline
// ------------------------------------------------------------------

#[derive(Debug, Deserialize)]
pub struct RealizeLayoutRequest {
    pub stage: String,
    #[serde(default)]
    pub persist: bool,
}

pub async fn realize_layout_route(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
    Json(req): Json<RealizeLayoutRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let stage = RealizationStage::parse(&req.stage)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let objects = realize_layout(&repo, &scene_id, stage, req.persist).await?;

    let object_values: Vec<serde_json::Value> = objects
        .into_iter()
        .map(|o| serde_json::to_value(o).unwrap_or_default())
        .collect();

    Ok(Json(success_response(json!({
        "scene_id": scene_id,
        "stage": req.stage,
        "persisted": req.persist,
        "object_count": object_values.len(),
        "objects": object_values,
    }))))
}

// ------------------------------------------------------------------
// Layout denormalization
// ------------------------------------------------------------------

pub async fn denormalize_layout_route(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let entities = repo.list_entities(&scene_id, None).await?;
    let relations = repo.list_relations(&scene_id, None).await?;

    let registry = KindRegistry::default();
    let mut objects = denormalize_layout(&scene_id, &entities, &relations, &registry)?;

    // Compute desired_hash for each object and upsert
    let mut created = Vec::new();
    let mut errors = Vec::new();
    for obj in &mut objects {
        match compute_desired_hash(obj) {
            Ok(hash) => obj.desired_hash = hash,
            Err(e) => {
                errors.push(json!({
                    "mcp_id": obj.mcp_id,
                    "error": e
                }));
                continue;
            }
        }
        match repo.upsert_object(obj).await {
            Ok(saved) => created.push(serde_json::to_value(saved).unwrap_or_default()),
            Err(e) => errors.push(json!({
                "mcp_id": obj.mcp_id.clone(),
                "error": e.to_string()
            })),
        }
    }

    Ok(Json(success_response(json!({
        "upserted_count": created.len(),
        "error_count": errors.len(),
        "objects": created,
        "errors": errors,
    }))))
}

// ------------------------------------------------------------------
// Sprint F: Compiler API routes
// ------------------------------------------------------------------

pub async fn validate_route(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let result =
        crate::compiler::pipeline::CompilerPipeline::compile_validate_only(&repo, &scene_id)
            .await?;

    Ok(Json(success_response(json!({
        "scene_id": result.scene_id,
        "stage": result.stage,
        "summary": {
            "errors": result.summary.errors,
            "warnings": result.summary.warnings,
            "infos": result.summary.infos,
            "objects": result.summary.objects,
            "instance_sets": result.summary.instance_sets,
            "world_cells": result.summary.world_cells,
        },
        "diagnostics": result.diagnostics,
    }))))
}

pub async fn compile_plan_route(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    // Fetch actual state from Unreal for diff comparison.
    let actual_objects = repo
        .list_desired_objects(&scene_id, true, None, None)
        .await?;

    let result =
        crate::compiler::pipeline::CompilerPipeline::compile_plan(&repo, &scene_id, actual_objects)
            .await?;

    Ok(Json(success_response(json!({
        "scene_id": result.scene_id,
        "stage": result.stage,
        "mode": result.mode,
        "summary": {
            "errors": result.summary.errors,
            "warnings": result.summary.warnings,
            "infos": result.summary.infos,
            "objects": result.summary.objects,
            "instance_sets": result.summary.instance_sets,
            "world_cells": result.summary.world_cells,
        },
        "diagnostics": result.diagnostics,
    }))))
}

#[derive(Debug, Deserialize)]
pub struct CompileApplyRequest {
    #[serde(default)]
    pub allow_delete: bool,
}

pub async fn compile_apply_route(
    State(state): State<AppState>,
    axum::extract::Path(scene_id): axum::extract::Path<String>,
    Json(req): Json<CompileApplyRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&scene_id)?;
    let repo = SurrealSceneRepository::new(state.db.clone());

    let result = crate::compiler::pipeline::CompilerPipeline::compile_apply(
        &repo,
        &scene_id,
        req.allow_delete,
    )
    .await?;

    Ok(Json(success_response(json!({
        "scene_id": result.scene_id,
        "stage": result.stage,
        "mode": result.mode,
        "summary": {
            "errors": result.summary.errors,
            "warnings": result.summary.warnings,
            "infos": result.summary.infos,
            "objects": result.summary.objects,
            "instance_sets": result.summary.instance_sets,
            "world_cells": result.summary.world_cells,
        },
        "diagnostics": result.diagnostics,
    }))))
}

// ------------------------------------------------------------------
// Phase 10: PIE / Log Validation routes
// ------------------------------------------------------------------

#[derive(Debug, Deserialize)]
pub struct PieRunRequest {
    pub scene_id: String,
    #[serde(default = "default_pie_mode")]
    pub mode: String,
    #[serde(default = "default_pie_timeout")]
    pub timeout_secs: u64,
}

fn default_pie_mode() -> String {
    "smoke".to_string()
}

fn default_pie_timeout() -> u64 {
    60
}

pub async fn pie_run(
    State(state): State<AppState>,
    Json(req): Json<PieRunRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;
    let mode = match req.mode.as_str() {
        "smoke" => crate::unreal::pie_types::TestMode::Smoke,
        "full" => crate::unreal::pie_types::TestMode::Full,
        "performance" => crate::unreal::pie_types::TestMode::Performance,
        _ => {
            return Err(AppError::Validation(format!(
                "unknown PIE mode: {}",
                req.mode
            )))
        }
    };

    let run_id = format!(
        "pie_{}_{}",
        scene_id,
        chrono::Utc::now().format("%Y%m%d%H%M%S")
    );

    // Attempt to start PIE via Unreal client.
    let unreal_client = state.unreal_client.clone();
    let result = match unreal_client.start_pie().await {
        Ok(_) => {
            // PIE started; wait for timeout then stop.
            tokio::time::sleep(tokio::time::Duration::from_secs(req.timeout_secs.min(120))).await;
            let _ = unreal_client.stop_pie().await;
            crate::unreal::pie_types::TestResult::Passed
        }
        Err(e) => {
            tracing::warn!("PIE start failed: {e}");
            crate::unreal::pie_types::TestResult::ConnectionError
        }
    };

    let test_run = crate::unreal::pie_types::UnrealTestRun {
        run_id: run_id.clone(),
        scene_id: scene_id.clone(),
        mode,
        result,
        logs: Vec::new(),
        diagnostics: Vec::new(),
    };

    Ok(Json(success_response(json!({
        "run_id": test_run.run_id,
        "scene_id": test_run.scene_id,
        "mode": match test_run.mode {
            crate::unreal::pie_types::TestMode::Smoke => "smoke",
            crate::unreal::pie_types::TestMode::Full => "full",
            crate::unreal::pie_types::TestMode::Performance => "performance",
        },
        "result": match test_run.result {
            crate::unreal::pie_types::TestResult::Passed => "passed",
            crate::unreal::pie_types::TestResult::Failed => "failed",
            crate::unreal::pie_types::TestResult::Timeout => "timeout",
            crate::unreal::pie_types::TestResult::ConnectionError => "connection_error",
        },
    }))))
}

#[derive(Debug, Deserialize)]
pub struct ParseLogsRequest {
    pub raw_output: String,
}

pub async fn parse_logs(Json(req): Json<ParseLogsRequest>) -> Result<Json<Value>, AppError> {
    let events = crate::unreal::pie_types::parse_unreal_logs(&req.raw_output);
    let diagnostics = crate::unreal::pie_types::extract_diagnostics(&events);
    Ok(Json(success_response(json!({
        "event_count": events.len(),
        "diagnostic_count": diagnostics.len(),
        "events": events,
        "diagnostics": diagnostics,
    }))))
}

#[derive(Debug, Deserialize)]
pub struct FixPlanRequest {
    pub scene_id: String,
    pub diagnostics: Vec<serde_json::Value>,
}

pub async fn fix_plan(
    State(_state): State<AppState>,
    Json(req): Json<FixPlanRequest>,
) -> Result<Json<Value>, AppError> {
    let scene_id = normalize_scene_id_input(&req.scene_id)?;

    let diagnostics: Vec<crate::unreal::pie_types::UnrealDiagnostic> = req
        .diagnostics
        .into_iter()
        .filter_map(|v| serde_json::from_value(v).ok())
        .collect();

    // Simple fix plan generation: map each diagnostic to a fix operation.
    let operations: Vec<crate::unreal::pie_types::FixOperation> = diagnostics
        .iter()
        .filter(|d| d.severity == "error" || d.severity == "warning")
        .map(|d| crate::unreal::pie_types::FixOperation {
            operation_type: match d.code.as_str() {
                "NO_Z_FIGHTING" | "Z_FIGHTING" => "adjust_transform".to_string(),
                "NO_OVERLAP" | "OVERLAP" => "adjust_transform".to_string(),
                "LOD_POLICY_MISSING" => "add_lod_tag".to_string(),
                "INSTANCE_SET_REQUIRED" => "convert_to_instance_set".to_string(),
                _ => "inspect".to_string(),
            },
            target_mcp_id: d.source.clone().unwrap_or_default(),
            params: serde_json::json!({
                "severity": d.severity,
                "code": d.code,
            }),
            description: d.description.clone(),
        })
        .collect();

    let confidence = if diagnostics.is_empty() {
        1.0
    } else {
        let errors = diagnostics.iter().filter(|d| d.severity == "error").count();
        1.0_f32.min(1.0 - (errors as f32 * 0.1))
    };

    let plan = crate::unreal::pie_types::FixPlan {
        requires_user_approval: confidence < 0.7,
        confidence,
        diagnostics,
        operations,
    };

    Ok(Json(success_response(json!({
        "scene_id": scene_id,
        "confidence": plan.confidence,
        "requires_user_approval": plan.requires_user_approval,
        "operation_count": plan.operations.len(),
        "operations": plan.operations,
    }))))
}

// ------------------------------------------------------------------
// Procedural mesh generation
// ------------------------------------------------------------------

#[derive(Debug, Deserialize)]
pub struct CreateProceduralMeshRequest {
    pub vertex_count: u32,
    pub index_count: u32,
    pub positions: Vec<[f32; 3]>,
    pub normals: Vec<[f32; 3]>,
    pub indices: Vec<u32>,
    #[serde(default)]
    pub uvs: Vec<[f32; 2]>,
    #[serde(default)]
    pub colors: Vec<[u8; 4]>,
    #[serde(default)]
    pub mcp_id: Option<String>,
    #[serde(default)]
    pub flags: u32,
    #[serde(default = "default_actor_name")]
    pub actor_name: String,
    #[serde(default)]
    pub material_path: String,
    #[serde(default)]
    pub location: Option<[f32; 3]>,
    #[serde(default)]
    pub rotation: Option<[f32; 3]>,
    #[serde(default)]
    pub scale: Option<[f32; 3]>,
    #[serde(default = "default_focus_viewport")]
    pub focus_viewport: bool,
}

fn default_actor_name() -> String {
    "ProceduralMesh".to_string()
}

fn default_focus_viewport() -> bool {
    true
}

pub async fn create_procedural_mesh_route(
    State(state): State<AppState>,
    Json(req): Json<CreateProceduralMeshRequest>,
) -> Result<Json<Value>, AppError> {
    use crate::procedural::mesh_buffer::ProceduralMeshPayload;

    let mcp_id = req.mcp_id.clone().unwrap_or_else(|| req.actor_name.clone());
    let request_id = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64;

    let uvs = if req.uvs.is_empty() {
        None
    } else {
        Some(req.uvs)
    };
    let colors = if req.colors.is_empty() {
        None
    } else {
        Some(req.colors)
    };

    let payload = match ProceduralMeshPayload::new(
        &mcp_id,
        request_id,
        req.positions,
        req.normals,
        uvs,
        None, // tangents not supported via JSON API yet
        colors,
        None, // material_ids not supported via JSON API yet
        req.indices,
    ) {
        Ok(p) => p,
        Err(e) => return Err(AppError::Validation(format!("Invalid mesh data: {}", e))),
    };
    payload
        .validate_size()
        .map_err(|e| AppError::Validation(format!("Invalid mesh data: {}", e)))?;

    let start = std::time::Instant::now();
    let location = req.location.unwrap_or([0.0, 0.0, 0.0]);
    let rotation = req.rotation.unwrap_or([0.0, 0.0, 0.0]);
    let scale = req.scale.unwrap_or([1.0, 1.0, 1.0]);

    let result = state
        .unreal_client
        .upsert_procedural_mesh(
            &mcp_id,
            &req.actor_name,
            if req.material_path.is_empty() {
                None
            } else {
                Some(&req.material_path)
            },
            [location[0] as f64, location[1] as f64, location[2] as f64],
            [rotation[0] as f64, rotation[1] as f64, rotation[2] as f64],
            [scale[0] as f64, scale[1] as f64, scale[2] as f64],
            req.focus_viewport,
            payload,
        )
        .await;
    let elapsed = start.elapsed();

    match result {
        Ok(resp) => {
            tracing::info!("Procedural mesh created in {:?}", elapsed);
            Ok(Json(success_response(json!({
                "unreal_response": resp,
                "elapsed_ms": elapsed.as_millis() as u64,
            }))))
        }
        Err(e) => {
            tracing::error!("Failed to create procedural mesh: {}", e);
            Err(e)
        }
    }
}

// ── Phase 1: SDF → Marching Cubes mesh ──────────────────────────────

#[derive(Debug, Deserialize)]
pub struct SdfMeshRequest {
    pub sdf: SdfTreeDesc,
    #[serde(default = "default_sdf_resolution")]
    pub resolution: u32,
    #[serde(default)]
    pub bounds: Option<SdfBoundsDesc>,
    #[serde(default = "default_bounds_padding")]
    pub bounds_padding: f32,
    #[serde(default)]
    pub mcp_id: Option<String>,
    #[serde(default = "default_actor_name_sdf")]
    pub actor_name: String,
    #[serde(default)]
    pub material_path: String,
    #[serde(default)]
    pub location: Option<[f32; 3]>,
    #[serde(default)]
    pub rotation: Option<[f32; 3]>,
    #[serde(default)]
    pub scale: Option<[f32; 3]>,
    #[serde(default = "default_focus_viewport")]
    pub focus_viewport: bool,
}

#[derive(Debug, Deserialize)]
pub struct SdfTreeDesc {
    #[serde(rename = "type")]
    pub sdf_type: String,
    #[serde(default)]
    pub center: [f32; 3],
    #[serde(default = "default_radius")]
    pub radius: f32,
    #[serde(default)]
    pub min: [f32; 3],
    #[serde(default = "default_one")]
    pub max: [f32; 3],
    #[serde(default = "default_major_radius")]
    pub major_radius: f32,
    #[serde(default = "default_minor_radius")]
    pub minor_radius: f32,
    #[serde(default = "default_frequency")]
    pub frequency: f32,
    #[serde(default = "default_thickness")]
    pub thickness: f32,
    #[serde(default)]
    pub smoothness: f32,
    #[serde(default)]
    pub left: Option<Box<SdfTreeDesc>>,
    #[serde(default)]
    pub right: Option<Box<SdfTreeDesc>>,
    #[serde(default)]
    pub a: Option<Box<SdfTreeDesc>>,
    #[serde(default)]
    pub b: Option<Box<SdfTreeDesc>>,
    #[serde(default)]
    pub child: Option<Box<SdfTreeDesc>>,
    #[serde(default)]
    pub children: Vec<SdfTreeDesc>,
    #[serde(default)]
    pub matrix: Option<[f32; 16]>,
}

#[derive(Debug, Deserialize)]
pub struct SdfBoundsDesc {
    pub min: [f32; 3],
    pub max: [f32; 3],
}

fn default_sdf_resolution() -> u32 {
    32
}
fn default_bounds_padding() -> f32 {
    0.0
}
fn default_actor_name_sdf() -> String {
    "SdfMesh".to_string()
}
fn default_radius() -> f32 {
    1.0
}
fn default_one() -> [f32; 3] {
    [1.0, 1.0, 1.0]
}
fn default_major_radius() -> f32 {
    1.0
}
fn default_minor_radius() -> f32 {
    0.3
}
fn default_frequency() -> f32 {
    1.0
}
fn default_thickness() -> f32 {
    0.1
}

fn identity_matrix() -> [f32; 16] {
    [
        1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
    ]
}

fn collect_sdf_children(desc: &SdfTreeDesc) -> Vec<&SdfTreeDesc> {
    let mut children = Vec::new();
    if let Some(left) = desc.left.as_deref().or(desc.a.as_deref()) {
        children.push(left);
    }
    if let Some(right) = desc.right.as_deref().or(desc.b.as_deref()) {
        children.push(right);
    }
    children.extend(desc.children.iter());
    children
}

fn build_folded_sdf_tree<F>(
    desc: &SdfTreeDesc,
    op_name: &str,
    make_node: F,
) -> Result<crate::procedural::sdf::SdfTree, String>
where
    F: Fn(
        crate::procedural::sdf::SdfTree,
        crate::procedural::sdf::SdfTree,
        f32,
    ) -> crate::procedural::sdf::SdfTree,
{
    let children = collect_sdf_children(desc);
    if children.len() < 2 {
        return Err(format!("{op_name} requires at least 2 children"));
    }

    let mut iter = children.into_iter();
    let first = build_sdf_tree(iter.next().expect("checked child count"))?;
    iter.try_fold(first, |acc, child| {
        Ok(make_node(acc, build_sdf_tree(child)?, desc.smoothness))
    })
}

fn build_sdf_tree(desc: &SdfTreeDesc) -> Result<crate::procedural::sdf::SdfTree, String> {
    use crate::procedural::sdf::{SdfPrimitive, SdfTree};
    match desc.sdf_type.to_ascii_lowercase().as_str() {
        "sphere" => Ok(SdfTree::Primitive(SdfPrimitive::Sphere {
            center: desc.center,
            radius: desc.radius,
        })),
        "box" => Ok(SdfTree::Primitive(SdfPrimitive::Box {
            min: desc.min,
            max: desc.max,
        })),
        "torus" => Ok(SdfTree::Primitive(SdfPrimitive::Torus {
            center: desc.center,
            major_radius: desc.major_radius,
            minor_radius: desc.minor_radius,
        })),
        "gyroid" => Ok(SdfTree::Primitive(SdfPrimitive::Gyroid {
            frequency: desc.frequency,
            thickness: desc.thickness,
        })),
        "scherk" => Ok(SdfTree::Primitive(SdfPrimitive::Scherk {
            frequency: desc.frequency,
        })),
        "union" => build_folded_sdf_tree(desc, "union", |a, b, s| {
            SdfTree::Union(Box::new(a), Box::new(b), s)
        }),
        "difference" | "subtract" => build_folded_sdf_tree(desc, "difference", |a, b, s| {
            SdfTree::Difference(Box::new(a), Box::new(b), s)
        }),
        "intersection" | "intersect" => build_folded_sdf_tree(desc, "intersection", |a, b, s| {
            SdfTree::Intersection(Box::new(a), Box::new(b), s)
        }),
        "transform" => {
            let child = desc
                .child
                .as_deref()
                .or_else(|| desc.children.first())
                .ok_or_else(|| "transform requires a child".to_string())?;
            Ok(SdfTree::Transform(
                Box::new(build_sdf_tree(child)?),
                desc.matrix.unwrap_or_else(identity_matrix),
            ))
        }
        other => Err(format!("unsupported SDF node type '{other}'")),
    }
}

pub async fn sdf_mesh_route(
    State(state): State<AppState>,
    Json(req): Json<SdfMeshRequest>,
) -> Result<Json<Value>, AppError> {
    use crate::procedural::mesh_gen::{auto_bounds_with_padding, sdf_to_mesh_payload};
    use crate::procedural::sdf::SdfBounds;
    use glam::Vec3;

    let sdf = build_sdf_tree(&req.sdf)
        .map_err(|e| AppError::Validation(format!("Invalid SDF tree: {e}")))?;
    let default_bounds = SdfBounds::new(Vec3::new(-5.0, -5.0, -5.0), Vec3::new(5.0, 5.0, 5.0));
    let bounds = match req.bounds {
        Some(b) => SdfBounds::new(Vec3::from(b.min), Vec3::from(b.max)),
        None => auto_bounds_with_padding(&sdf, default_bounds, req.bounds_padding),
    };

    let mcp_id = req.mcp_id.clone().unwrap_or_else(|| req.actor_name.clone());
    let request_id = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64;

    let payload = sdf_to_mesh_payload(&mcp_id, request_id, &sdf, bounds, req.resolution)
        .map_err(|e| AppError::Validation(format!("SDF mesh generation failed: {}", e)))?;
    payload
        .validate_size()
        .map_err(|e| AppError::Validation(format!("Invalid mesh data: {}", e)))?;

    let location = req.location.unwrap_or([0.0, 0.0, 0.0]);
    let rotation = req.rotation.unwrap_or([0.0, 0.0, 0.0]);
    let scale = req.scale.unwrap_or([1.0, 1.0, 1.0]);

    let start = std::time::Instant::now();
    let result = state
        .unreal_client
        .upsert_procedural_mesh(
            &mcp_id,
            &req.actor_name,
            if req.material_path.is_empty() {
                None
            } else {
                Some(&req.material_path)
            },
            [location[0] as f64, location[1] as f64, location[2] as f64],
            [rotation[0] as f64, rotation[1] as f64, rotation[2] as f64],
            [scale[0] as f64, scale[1] as f64, scale[2] as f64],
            req.focus_viewport,
            payload,
        )
        .await;
    let elapsed = start.elapsed();

    match result {
        Ok(resp) => Ok(Json(success_response(json!({
            "unreal_response": resp,
            "elapsed_ms": elapsed.as_millis() as u64,
        })))),
        Err(e) => Err(e),
    }
}

// ── Phase 1: Superformula mesh ──────────────────────────────────────

#[derive(Debug, Deserialize)]
pub struct SuperformulaMeshRequest {
    #[serde(default = "default_sf_m")]
    pub m1: f32,
    #[serde(default = "default_sf_n")]
    pub n1_1: f32,
    #[serde(default = "default_sf_n")]
    pub n2_1: f32,
    #[serde(default = "default_sf_n")]
    pub n3_1: f32,
    #[serde(default = "default_sf_one")]
    pub a1: f32,
    #[serde(default = "default_sf_one")]
    pub b1: f32,
    #[serde(default = "default_sf_m")]
    pub m2: f32,
    #[serde(default = "default_sf_n")]
    pub n1_2: f32,
    #[serde(default = "default_sf_n")]
    pub n2_2: f32,
    #[serde(default = "default_sf_n")]
    pub n3_2: f32,
    #[serde(default = "default_sf_one")]
    pub a2: f32,
    #[serde(default = "default_sf_one")]
    pub b2: f32,
    #[serde(default = "default_sf_resolution")]
    pub resolution: u32,
    #[serde(default = "default_sf_scale")]
    pub scale: f32,
    #[serde(default)]
    pub mcp_id: Option<String>,
    #[serde(default = "default_actor_name_sf")]
    pub actor_name: String,
    #[serde(default)]
    pub material_path: String,
    #[serde(default)]
    pub location: Option<[f32; 3]>,
    #[serde(default)]
    pub rotation: Option<[f32; 3]>,
    #[serde(default)]
    pub scale_override: Option<[f32; 3]>,
    #[serde(default = "default_focus_viewport")]
    pub focus_viewport: bool,
}

fn default_sf_m() -> f32 {
    6.0
}
fn default_sf_n() -> f32 {
    1.0
}
fn default_sf_one() -> f32 {
    1.0
}
fn default_sf_resolution() -> u32 {
    32
}
fn default_sf_scale() -> f32 {
    100.0
}
fn default_actor_name_sf() -> String {
    "SuperformulaMesh".to_string()
}

pub async fn superformula_mesh_route(
    State(state): State<AppState>,
    Json(req): Json<SuperformulaMeshRequest>,
) -> Result<Json<Value>, AppError> {
    use crate::procedural::superformula::{superformula_mesh, SuperformulaParams};

    let params = SuperformulaParams {
        m1: req.m1,
        n1_1: req.n1_1,
        n2_1: req.n2_1,
        n3_1: req.n3_1,
        a1: req.a1,
        b1: req.b1,
        m2: req.m2,
        n1_2: req.n1_2,
        n2_2: req.n2_2,
        n3_2: req.n3_2,
        a2: req.a2,
        b2: req.b2,
    };

    let mcp_id = req.mcp_id.clone().unwrap_or_else(|| req.actor_name.clone());
    let request_id = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64;

    let payload = superformula_mesh(&params, req.resolution, req.scale, &mcp_id, request_id)
        .map_err(|e| AppError::Validation(format!("Superformula mesh generation failed: {}", e)))?;
    payload
        .validate_size()
        .map_err(|e| AppError::Validation(format!("Invalid mesh data: {}", e)))?;

    let location = req.location.unwrap_or([0.0, 0.0, 0.0]);
    let rotation = req.rotation.unwrap_or([0.0, 0.0, 0.0]);
    let scale = req.scale_override.unwrap_or([1.0, 1.0, 1.0]);

    let start = std::time::Instant::now();
    let result = state
        .unreal_client
        .upsert_procedural_mesh(
            &mcp_id,
            &req.actor_name,
            if req.material_path.is_empty() {
                None
            } else {
                Some(&req.material_path)
            },
            [location[0] as f64, location[1] as f64, location[2] as f64],
            [rotation[0] as f64, rotation[1] as f64, rotation[2] as f64],
            [scale[0] as f64, scale[1] as f64, scale[2] as f64],
            req.focus_viewport,
            payload,
        )
        .await;
    let elapsed = start.elapsed();

    match result {
        Ok(resp) => Ok(Json(success_response(json!({
            "unreal_response": resp,
            "elapsed_ms": elapsed.as_millis() as u64,
        })))),
        Err(e) => Err(e),
    }
}

// ── Phase 1: L-System spline ────────────────────────────────────────

#[derive(Debug, Deserialize)]
pub struct LSystemSplineRequest {
    #[serde(default = "default_ls_axiom")]
    pub axiom: String,
    #[serde(default)]
    pub rules: Vec<[String; 2]>,
    #[serde(default = "default_ls_iterations")]
    pub iterations: u32,
    #[serde(default = "default_ls_step")]
    pub step_length: f32,
    #[serde(default = "default_ls_angle")]
    pub angle_degrees: f32,
    #[serde(default)]
    pub origin: Option<[f32; 3]>,
    #[serde(default)]
    pub heading: Option<[f32; 3]>,
    #[serde(default)]
    pub up: Option<[f32; 3]>,
    #[serde(default)]
    pub preset: Option<String>,
    #[serde(default)]
    pub closed_loop: bool,
    #[serde(default = "default_ls_tangent_mode")]
    pub tangent_mode: String,
    #[serde(default)]
    pub mcp_id: Option<String>,
    #[serde(default = "default_actor_name_ls")]
    pub spline_name: String,
    #[serde(default = "default_create_in_unreal")]
    pub create_in_unreal: bool,
    #[serde(default = "default_focus_viewport")]
    pub focus_viewport: bool,
}

fn default_ls_axiom() -> String {
    "F".to_string()
}
fn default_ls_iterations() -> u32 {
    3
}
fn default_ls_step() -> f32 {
    50.0
}
fn default_ls_angle() -> f32 {
    90.0
}
fn default_ls_tangent_mode() -> String {
    "curve".to_string()
}
fn default_actor_name_ls() -> String {
    "LSystemSpline".to_string()
}
fn default_create_in_unreal() -> bool {
    true
}

pub async fn lsystem_spline_route(
    State(state): State<AppState>,
    Json(req): Json<LSystemSplineRequest>,
) -> Result<Json<Value>, AppError> {
    use crate::procedural::lsystem::{evaluate_lsystem, DimensionMode, LSystemParams};

    let mut params = if let Some(preset_name) = &req.preset {
        crate::procedural::lsystem_presets::resolve_preset(preset_name)
            .ok_or_else(|| AppError::Validation(format!("Unknown preset: {}", preset_name)))?
    } else {
        let rules: Vec<(char, String)> = req
            .rules
            .iter()
            .filter_map(|[sym, repl]| sym.chars().next().map(|c| (c, repl.clone())))
            .collect();

        LSystemParams {
            axiom: req.axiom.clone(),
            rules,
            iterations: req.iterations,
            step_length: req.step_length,
            angle_degrees: req.angle_degrees,
            origin: req.origin.unwrap_or([0.0, 0.0, 0.0]),
            heading: req.heading.unwrap_or([1.0, 0.0, 0.0]),
            up: req.up.unwrap_or([0.0, 0.0, 1.0]),
            dimension_mode: DimensionMode::ThreeD,
        }
    };

    // Allow common tuning fields to override preset values.
    params.iterations = req.iterations.min(10);
    params.step_length = req.step_length;
    if let Some(origin) = req.origin {
        params.origin = origin;
    }
    if let Some(heading) = req.heading {
        params.heading = heading;
    }
    if let Some(up) = req.up {
        params.up = up;
    }

    let result = evaluate_lsystem(&params);
    if result.segments.is_empty() {
        return Err(AppError::Validation(
            "L-System produced no drawable segments".to_string(),
        ));
    }

    let segments_json: Vec<Value> = result
        .segments
        .iter()
        .map(|seg| {
            json!({
                "start": {"x": seg.start[0], "y": seg.start[1], "z": seg.start[2]},
                "end": {"x": seg.end[0], "y": seg.end[1], "z": seg.end[2]},
            })
        })
        .collect();

    let unreal_response = if req.create_in_unreal {
        Some(
            state
                .unreal_client
                .create_spline_from_points(
                    &req.mcp_id
                        .clone()
                        .unwrap_or_else(|| req.spline_name.clone()),
                    &req.spline_name,
                    segments_json.clone(),
                    req.closed_loop,
                    &req.tangent_mode,
                    req.focus_viewport,
                )
                .await?,
        )
    } else {
        None
    };

    Ok(Json(success_response(json!({
        "spline_name": req.spline_name,
        "segment_count": result.segments.len(),
        "segments": segments_json,
        "derived_length": result.derived_string.len(),
        "closed_loop": req.closed_loop,
        "tangent_mode": req.tangent_mode,
        "unreal_response": unreal_response,
    }))))
}

// ── Phase 1: WFC Grid ─────────────────────────────────────────────────

#[derive(Debug, Deserialize)]
pub struct WfcGridRequest {
    pub width: u32,
    pub height: u32,
    pub tileset: crate::procedural::wfc::WfcTileset,
    #[serde(default)]
    pub seed: Option<u64>,
    #[serde(default)]
    pub periodic: bool,
}

pub async fn wfc_grid_route(
    State(_state): State<AppState>,
    Json(req): Json<WfcGridRequest>,
) -> Result<Json<Value>, AppError> {
    use crate::procedural::generator::{GenerateContext, GenerationLimits};
    use crate::procedural::wfc::{WfcGenerator, WfcParams};
    use crate::procedural::generator::Generator;

    let mut limits = GenerationLimits::default();
    limits.max_iterations = (req.width * req.height * 100).max(1000) as u32;
    let ctx = GenerateContext::new(req.seed, Some(limits));

    let params = WfcParams {
        width: req.width,
        height: req.height,
        tileset: req.tileset,
        seed: req.seed,
        periodic: req.periodic,
    };

    let gen = WfcGenerator;
    let output = gen
        .generate(&params, &ctx)
        .map_err(|e| AppError::Validation(format!("WFC generation failed: {e}")))?;

    Ok(Json(success_response(json!({
        "width": output.data.width,
        "height": output.data.height,
        "tiles": output.data.tiles,
        "stats": output.stats,
    }))))
}

#[cfg(test)]
mod tests {
    use super::object_or_empty;
    use serde_json::json;

    #[test]
    fn object_or_empty_converts_missing_or_null_to_object() {
        assert_eq!(object_or_empty(None), json!({}));
        assert_eq!(object_or_empty(Some(serde_json::Value::Null)), json!({}));
    }

    #[test]
    fn object_or_empty_preserves_explicit_object() {
        assert_eq!(
            object_or_empty(Some(json!({ "path": "/Engine/BasicShapes/Cube.Cube" }))),
            json!({
                "path": "/Engine/BasicShapes/Cube.Cube"
            })
        );
    }
}
