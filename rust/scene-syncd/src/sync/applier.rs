use crate::db::SurrealSceneRepository;
use crate::error::AppError;
use crate::ir::instance_set::InstanceSet;
use crate::sync::instance_set_planner::{plan_instance_set_sync, InstanceSetOperation, InstanceSetState};
use crate::sync::{SyncAction, SyncOperation, SyncPlan};
use crate::unreal::client::UnrealClient;
use serde::{Deserialize, Serialize};
use serde_json::json;
use std::collections::{HashMap, HashSet};
use tokio::time::{sleep, Duration};

fn create_chunk_size() -> usize {
    std::env::var("SCENE_SYNCD_CREATE_CHUNK_SIZE")
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(50)
}

fn build_create_chunks(
    reduced_creates: &[serde_json::Value],
    chunk_size: usize,
    has_non_create_delta: bool,
) -> Vec<Vec<serde_json::Value>> {
    let mut chunks: Vec<Vec<serde_json::Value>> = reduced_creates
        .chunks(chunk_size)
        .map(|c| c.to_vec())
        .collect();
    if chunks.is_empty() && has_non_create_delta {
        chunks.push(Vec::new());
    }
    chunks
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct SyncApplyResult {
    pub run_id: String,
    pub scene_id: String,
    pub mode: String,
    pub summary: SyncApplySummary,
    pub operations: Vec<AppliedOperation>,
    pub warnings: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct SyncApplySummary {
    pub total: usize,
    pub succeeded: usize,
    pub failed: usize,
    pub skipped: usize,
    pub creates: usize,
    pub update_transforms: usize,
    pub update_visuals: usize,
    pub deletes: usize,
    pub noops: usize,
    pub instance_set_creates: usize,
    pub instance_set_updates: usize,
    pub instance_set_deletes: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppliedOperation {
    pub mcp_id: String,
    pub action: String,
    pub status: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub unreal_actor_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

/// Resolve an asset path from the `asset_ref` JSON object.
/// If `status` is `"missing"`, returns the `fallback` path and logs a warning.
fn resolve_asset_path(asset_ref: Option<&serde_json::Value>) -> String {
    let default = "/Engine/BasicShapes/Cube.Cube";
    let Some(aref) = asset_ref else {
        return default.to_string();
    };
    let status = aref
        .get("status")
        .and_then(|v| v.as_str())
        .unwrap_or("present");
    if status == "missing" {
        let fallback = aref
            .get("fallback")
            .and_then(|v| v.as_str())
            .unwrap_or(default);
        let primary = aref.get("path").and_then(|v| v.as_str()).unwrap_or("");
        tracing::warn!(
            primary = primary,
            fallback = fallback,
            "Asset is missing, using fallback"
        );
        return fallback.to_string();
    }
    aref.get("path")
        .and_then(|v| v.as_str())
        .unwrap_or(default)
        .to_string()
}

fn response_actor_names(response: &serde_json::Value, field: &str) -> HashSet<String> {
    response
        .get(field)
        .and_then(|v| v.as_array())
        .map(|items| {
            items
                .iter()
                .filter_map(|item| item.get("name").and_then(|v| v.as_str()))
                .map(|name| name.to_string())
                .collect()
        })
        .unwrap_or_default()
}

fn response_error_summary(response: &serde_json::Value) -> String {
    let errors = response
        .get("errors")
        .and_then(|v| v.as_array())
        .map(|items| {
            items
                .iter()
                .filter_map(|item| {
                    item.get("error")
                        .or_else(|| item.get("message"))
                        .and_then(|v| v.as_str())
                })
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();

    if errors.is_empty() {
        "apply_scene_delta reported partial failure".to_string()
    } else {
        errors.join("; ")
    }
}

/// Convert domain Transform to array-format JSON expected by Unreal ISM commands.
fn transforms_to_json(transforms: &[crate::domain::Transform]) -> Vec<serde_json::Value> {
    transforms
        .iter()
        .map(|t| {
            json!({
                "location": [t.location.x, t.location.y, t.location.z],
                "rotation": [t.rotation.pitch, t.rotation.yaw, t.rotation.roll],
                "scale": [t.scale.x, t.scale.y, t.scale.z],
            })
        })
        .collect()
}

/// Parse Unreal `list_instance_sets` response into `InstanceSetState` structs.
fn parse_instance_set_list(response: &serde_json::Value) -> Vec<InstanceSetState> {
    response
        .get("sets")
        .and_then(|v| v.as_array())
        .map(|arr| {
            arr.iter()
                .filter_map(|item| {
                    let set_id = item.get("set_id")?.as_str()?;
                    let mesh = item.get("mesh_path")?.as_str()?.to_string();
                    let material = item
                        .get("material_path")
                        .and_then(|v| v.as_str())
                        .map(|s| s.to_string());
                    let instance_count = item
                        .get("instance_count")
                        .and_then(|v| v.as_i64())
                        .unwrap_or(0) as usize;
                    Some(InstanceSetState {
                        set_id: set_id.to_string(),
                        mesh,
                        material,
                        instance_count,
                        cell_id: None,
                    })
                })
                .collect()
        })
        .unwrap_or_default()
}

pub async fn apply_sync(
    unreal: &UnrealClient,
    db: &SurrealSceneRepository,
    plan: &SyncPlan,
    mode: &str,
    allow_delete: bool,
    instance_sets: Option<&[InstanceSet]>,
) -> Result<SyncApplyResult, AppError> {
    if mode == "plan_only" {
        return Err(AppError::Validation(
            "plan_only is not valid for /sync/apply. Use /sync/plan instead.".to_string(),
        ));
    }

    let run_id = format!("{}_{}", plan.scene_id, ulid::Ulid::new());

    db.create_sync_run(&run_id, &plan.scene_id, mode, "running")
        .await?;

    let mut result = SyncApplyResult {
        run_id: run_id.clone(),
        scene_id: plan.scene_id.clone(),
        mode: mode.to_string(),
        ..Default::default()
    };

    for warning in &plan.warnings {
        result.warnings.push(warning.clone());
    }

    // --- Build lookup: mcp_id -> desired_hash from individual plan operations ---
    let mut mcp_id_to_hash: HashMap<String, String> = HashMap::new();
    for op in &plan.operations {
        if let Some(desired) = &op.desired {
            if let Some(hash) = desired.get("desired_hash").and_then(|v| v.as_str()) {
                mcp_id_to_hash.insert(op.mcp_id.clone(), hash.to_string());
            }
        }
    }

    // --- InstanceSet apply phase ----------------------------------------
    let mut instance_set_mcp_ids: HashSet<String> = HashSet::new();
    let mut instance_sync_marks: Vec<(String, String, Option<String>)> = Vec::new();
    let mut instance_op_records: Vec<(String, String, String, String)> = Vec::new();

    if let Some(desired_sets) = instance_sets {
        // Collect all mcp_ids that belong to instance sets
        for set in desired_sets {
            for mcp_id in &set.source_mcp_ids {
                instance_set_mcp_ids.insert(mcp_id.clone());
            }
        }

        // Query actual instance sets from Unreal
        let actual_sets = match unreal.list_instance_sets().await {
            Ok(resp) => parse_instance_set_list(&resp),
            Err(e) => {
                result.warnings.push(format!("Could not list instance sets from Unreal: {e}"));
                Vec::new()
            }
        };

        let ism_ops = plan_instance_set_sync(desired_sets, &actual_sets);

        for op in &ism_ops {
            match op {
                InstanceSetOperation::Create {
                    set_id,
                    mesh,
                    material,
                    transforms,
                    ..
                } => {
                    let json_transforms = transforms_to_json(transforms);
                    let mat_ref = material.as_deref();
                    match unreal.spawn_instance_set(set_id, mesh, mat_ref, json_transforms).await {
                        Ok(resp) => {
                            let success = resp
                                .get("success")
                                .and_then(|v| v.as_bool())
                                .unwrap_or(false);
                            let actor_name = resp
                                .get("actor_name")
                                .and_then(|v| v.as_str())
                                .map(|s| s.to_string());
                            if success {
                                result.summary.succeeded += 1;
                                result.summary.instance_set_creates += 1;
                                result.operations.push(AppliedOperation {
                                    mcp_id: set_id.clone(),
                                    action: "instance_set_create".to_string(),
                                    status: "success".to_string(),
                                    unreal_actor_name: actor_name.clone(),
                                    error: None,
                                });
                                // Mark all member mcp_ids as synced
                                if let Some(desired) = desired_sets.iter().find(|s| s.set_id == *set_id) {
                                    for mcp_id in &desired.source_mcp_ids {
                                        let hash = mcp_id_to_hash.get(mcp_id).cloned().unwrap_or_default();
                                        instance_sync_marks.push((mcp_id.clone(), hash, actor_name.clone()));
                                        instance_op_records.push((mcp_id.clone(), "create".to_string(), "success".to_string(), "applied via instance set".to_string()));
                                    }
                                }
                            } else {
                                result.summary.failed += 1;
                                let err_msg = resp
                                    .get("error")
                                    .and_then(|e| e.as_str())
                                    .unwrap_or("unknown spawn_instance_set error");
                                result.operations.push(AppliedOperation {
                                    mcp_id: set_id.clone(),
                                    action: "instance_set_create".to_string(),
                                    status: "error".to_string(),
                                    unreal_actor_name: None,
                                    error: Some(err_msg.to_string()),
                                });
                            }
                        }
                        Err(e) => {
                            result.summary.failed += 1;
                            result.operations.push(AppliedOperation {
                                mcp_id: set_id.clone(),
                                action: "instance_set_create".to_string(),
                                status: "error".to_string(),
                                unreal_actor_name: None,
                                error: Some(e.to_string()),
                            });
                        }
                    }
                }
                InstanceSetOperation::Update {
                    set_id,
                    mesh: _,
                    material: _,
                    transforms,
                    ..
                } => {
                    let json_transforms = transforms_to_json(transforms);
                    match unreal.update_instance_set(set_id, json_transforms).await {
                        Ok(resp) => {
                            let success = resp
                                .get("success")
                                .and_then(|v| v.as_bool())
                                .unwrap_or(false);
                            if success {
                                result.summary.succeeded += 1;
                                result.summary.instance_set_updates += 1;
                                result.operations.push(AppliedOperation {
                                    mcp_id: set_id.clone(),
                                    action: "instance_set_update".to_string(),
                                    status: "success".to_string(),
                                    unreal_actor_name: None,
                                    error: None,
                                });
                                if let Some(desired) = desired_sets.iter().find(|s| s.set_id == *set_id) {
                                    for mcp_id in &desired.source_mcp_ids {
                                        let hash = mcp_id_to_hash.get(mcp_id).cloned().unwrap_or_default();
                                        instance_sync_marks.push((mcp_id.clone(), hash, None));
                                        instance_op_records.push((mcp_id.clone(), "update_transform".to_string(), "success".to_string(), "applied via instance set".to_string()));
                                    }
                                }
                            } else {
                                result.summary.failed += 1;
                                let err_msg = resp
                                    .get("error")
                                    .and_then(|e| e.as_str())
                                    .unwrap_or("unknown update_instance_set error");
                                result.operations.push(AppliedOperation {
                                    mcp_id: set_id.clone(),
                                    action: "instance_set_update".to_string(),
                                    status: "error".to_string(),
                                    unreal_actor_name: None,
                                    error: Some(err_msg.to_string()),
                                });
                            }
                        }
                        Err(e) => {
                            result.summary.failed += 1;
                            result.operations.push(AppliedOperation {
                                mcp_id: set_id.clone(),
                                action: "instance_set_update".to_string(),
                                status: "error".to_string(),
                                unreal_actor_name: None,
                                error: Some(e.to_string()),
                            });
                        }
                    }
                }
                InstanceSetOperation::Delete { set_id } => {
                    match unreal.delete_instance_set(set_id).await {
                        Ok(resp) => {
                            let success = resp
                                .get("success")
                                .and_then(|v| v.as_bool())
                                .unwrap_or(false);
                            if success {
                                result.summary.succeeded += 1;
                                result.summary.instance_set_deletes += 1;
                                result.operations.push(AppliedOperation {
                                    mcp_id: set_id.clone(),
                                    action: "instance_set_delete".to_string(),
                                    status: "success".to_string(),
                                    unreal_actor_name: None,
                                    error: None,
                                });
                                if let Some(desired) = desired_sets.iter().find(|s| s.set_id == *set_id) {
                                    for mcp_id in &desired.source_mcp_ids {
                                        instance_sync_marks.push((mcp_id.clone(), "".to_string(), None));
                                        instance_op_records.push((mcp_id.clone(), "delete".to_string(), "success".to_string(), "applied via instance set".to_string()));
                                    }
                                }
                            } else {
                                result.summary.failed += 1;
                                let err_msg = resp
                                    .get("error")
                                    .and_then(|e| e.as_str())
                                    .unwrap_or("unknown delete_instance_set error");
                                result.operations.push(AppliedOperation {
                                    mcp_id: set_id.clone(),
                                    action: "instance_set_delete".to_string(),
                                    status: "error".to_string(),
                                    unreal_actor_name: None,
                                    error: Some(err_msg.to_string()),
                                });
                            }
                        }
                        Err(e) => {
                            result.summary.failed += 1;
                            result.operations.push(AppliedOperation {
                                mcp_id: set_id.clone(),
                                action: "instance_set_delete".to_string(),
                                status: "error".to_string(),
                                unreal_actor_name: None,
                                error: Some(e.to_string()),
                            });
                        }
                    }
                }
            }
        }
    }

    // --- Filter individual operations for mcp_ids already handled by instance sets ---
    let filtered_plan = if instance_set_mcp_ids.is_empty() {
        plan.clone()
    } else {
        let mut filtered = plan.clone();
        filtered.operations.retain(|op| !instance_set_mcp_ids.contains(&op.mcp_id));
        filtered
    };
    result.summary.total = filtered_plan.operations.len();

    // --- P4: Batch apply via apply_scene_delta ---------------------------
    let batch_result = apply_scene_delta_batch(
        db,
        unreal,
        &run_id,
        &plan.scene_id,
        &filtered_plan,
        mode,
        allow_delete,
    )
    .await;
    match batch_result {
        Ok(ops) => {
            for ao in ops {
                if ao.status == "success" {
                    result.summary.succeeded += 1;
                } else if ao.status == "skipped" {
                    result.summary.skipped += 1;
                } else {
                    result.summary.failed += 1;
                }
                match ao.action.as_str() {
                    "create" => result.summary.creates += 1,
                    "update_transform" => result.summary.update_transforms += 1,
                    "update_visual" => result.summary.update_visuals += 1,
                    "delete" => result.summary.deletes += 1,
                    "noop" => result.summary.noops += 1,
                    _ => {}
                }
                result.operations.push(ao);
            }
        }
        Err(e) => {
            result.summary.failed += filtered_plan.operations.len();
            result.warnings.push(format!("Batch apply failed: {e}"));
            for op in &filtered_plan.operations {
                result.operations.push(AppliedOperation {
                    mcp_id: op.mcp_id.clone(),
                    action: format!("{:?}", op.action).to_lowercase(),
                    status: "error".to_string(),
                    unreal_actor_name: None,
                    error: Some(e.to_string()),
                });
            }
        }
    }
    // ------------------------------------------------------------------

    // --- Persist instance set sync marks and operation records ------------
    if !instance_sync_marks.is_empty() {
        if let Err(e) = db.mark_objects_synced_batch(&plan.scene_id, &instance_sync_marks).await {
            result.warnings.push(format!("Failed to mark instance set objects as synced: {e}"));
        }
    }
    if !instance_op_records.is_empty() {
        if let Err(e) = db.record_operations_batch(&run_id, &plan.scene_id, &instance_op_records).await {
            result.warnings.push(format!("Failed to record instance set operations: {e}"));
        }
    }

    if let Err(e) = db.finish_sync_run(&run_id, &result.summary).await {
        result
            .warnings
            .push(format!("Failed to finalize sync run: {e}"));
    }

    Ok(result)
}

async fn apply_scene_delta_batch(
    db: &SurrealSceneRepository,
    unreal: &UnrealClient,
    run_id: &str,
    scene_id: &str,
    plan: &SyncPlan,
    mode: &str,
    allow_delete: bool,
) -> Result<Vec<AppliedOperation>, AppError> {
    let mut creates: Vec<serde_json::Value> = Vec::new();
    let mut updates: Vec<serde_json::Value> = Vec::new();
    let mut deletes: Vec<serde_json::Value> = Vec::new();
    let mut pre_results: Vec<AppliedOperation> = Vec::new();
    let mut sync_marks: Vec<(String, String, Option<String>)> = Vec::new(); // (mcp_id, desired_hash, actor_name)
    let mut op_records: Vec<(String, String, String, String)> = Vec::new(); // (mcp_id, action, status, reason)

    // --- Pre-fetch all entities and realizations for Create ops in batch ----
    let create_mcp_ids: Vec<&str> = plan
        .operations
        .iter()
        .filter(|op| op.action == SyncAction::Create)
        .filter_map(|op| op.desired.as_ref().map(|_| op.mcp_id.as_str()))
        .collect();

    let (entity_map, realization_map) = if create_mcp_ids.is_empty() {
        (HashMap::new(), HashMap::new())
    } else {
        let entity_map = db
            .find_entities_by_mcp_ids(scene_id, &create_mcp_ids)
            .await
            .unwrap_or_default();
        let entity_ids: Vec<&str> = entity_map.values().map(|e| e.entity_id.as_str()).collect();
        let realization_map = if entity_ids.is_empty() {
            HashMap::new()
        } else {
            db.find_realizations_for_entities(scene_id, &entity_ids)
                .await
                .unwrap_or_default()
        };
        (entity_map, realization_map)
    };

    for op in &plan.operations {
        match op.action {
            SyncAction::Create => {
                if let Some(desired) = &op.desired {
                    let mcp_id = op.mcp_id.as_str();
                    let desired_name = desired
                        .get("desired_name")
                        .and_then(|v| v.as_str())
                        .unwrap_or(mcp_id);
                    let transform = desired.get("transform");
                    let location = transform.and_then(|t| t.get("location"));
                    let rotation = transform.and_then(|t| t.get("rotation"));
                    let scale = transform.and_then(|t| t.get("scale"));
                    let loc: [f64; 3] = [
                        location
                            .and_then(|l| l.get("x"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        location
                            .and_then(|l| l.get("y"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        location
                            .and_then(|l| l.get("z"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                    ];
                    let rot: [f64; 3] = [
                        rotation
                            .and_then(|r| r.get("pitch"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        rotation
                            .and_then(|r| r.get("yaw"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        rotation
                            .and_then(|r| r.get("roll"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                    ];
                    let scl: [f64; 3] = [
                        scale
                            .and_then(|s| s.get("x"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(1.0),
                        scale
                            .and_then(|s| s.get("y"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(1.0),
                        scale
                            .and_then(|s| s.get("z"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(1.0),
                    ];

                    // Check for blueprint realization via SceneRealization (O(1) lookup)
                    let mut is_blueprint_create = false;
                    let mut blueprint_path = String::new();
                    if let Some(entity) = entity_map.get(mcp_id) {
                        if let Some(rz) = realization_map.get(&entity.entity_id) {
                            if rz.policy == "blueprint" {
                                if let Some(bp) =
                                    rz.metadata.get("blueprint_path").and_then(|v| v.as_str())
                                {
                                    if !bp.is_empty() {
                                        is_blueprint_create = true;
                                        blueprint_path = bp.to_string();
                                    }
                                }
                            }
                        }
                    }

                    if is_blueprint_create {
                        let result = unreal
                            .spawn_blueprint_actor(&blueprint_path, desired_name, loc, rot, scl)
                            .await;
                        match result {
                            Ok(response) => {
                                let success = response
                                    .get("success")
                                    .and_then(|v| v.as_bool())
                                    .unwrap_or(false);
                                if success {
                                    let actor_name = response
                                        .get("actor_name")
                                        .and_then(|v| v.as_str())
                                        .map(|s| s.to_string());
                                    let desired_hash = desired
                                        .get("desired_hash")
                                        .and_then(|v| v.as_str())
                                        .unwrap_or("");
                                    sync_marks.push((
                                        mcp_id.to_string(),
                                        desired_hash.to_string(),
                                        actor_name.clone(),
                                    ));
                                    op_records.push((
                                        mcp_id.to_string(),
                                        "create".to_string(),
                                        "success".to_string(),
                                        "actor created via blueprint spawn".to_string(),
                                    ));
                                    pre_results.push(AppliedOperation {
                                        mcp_id: mcp_id.to_string(),
                                        action: "create".to_string(),
                                        status: "success".to_string(),
                                        unreal_actor_name: actor_name,
                                        error: None,
                                    });
                                } else {
                                    let err_msg = response
                                        .get("error")
                                        .and_then(|e| e.as_str())
                                        .unwrap_or("unknown blueprint spawn error");
                                    op_records.push((
                                        mcp_id.to_string(),
                                        "create".to_string(),
                                        "error".to_string(),
                                        err_msg.to_string(),
                                    ));
                                    pre_results.push(AppliedOperation {
                                        mcp_id: mcp_id.to_string(),
                                        action: "create".to_string(),
                                        status: "error".to_string(),
                                        unreal_actor_name: None,
                                        error: Some(err_msg.to_string()),
                                    });
                                }
                            }
                            Err(e) => {
                                let err_msg = e.to_string();
                                op_records.push((
                                    mcp_id.to_string(),
                                    "create".to_string(),
                                    "error".to_string(),
                                    err_msg.clone(),
                                ));
                                pre_results.push(AppliedOperation {
                                    mcp_id: mcp_id.to_string(),
                                    action: "create".to_string(),
                                    status: "error".to_string(),
                                    unreal_actor_name: None,
                                    error: Some(err_msg),
                                });
                            }
                        }
                    } else {
                        let actor_type = desired
                            .get("actor_type")
                            .and_then(|v| v.as_str())
                            .unwrap_or("StaticMeshActor");
                        let asset_path = resolve_asset_path(desired.get("asset_ref"));
                        let mut tags =
                            vec!["managed_by_mcp".to_string(), format!("mcp_id:{}", mcp_id)];
                        if let Some(tags_val) = desired.get("tags").and_then(|v| v.as_array()) {
                            for tag in tags_val {
                                if let Some(tag_str) = tag.as_str() {
                                    let tag_string = tag_str.to_string();
                                    if !tags.contains(&tag_string) {
                                        tags.push(tag_string);
                                    }
                                }
                            }
                        }
                        creates.push(json!({
                            "name": desired_name,
                            "type": actor_type,
                            "mcp_id": mcp_id,
                            "location": loc,
                            "rotation": rot,
                            "scale": scl,
                            "static_mesh": asset_path,
                            "tags": tags,
                        }));
                    }
                }
            }
            SyncAction::UpdateTransform => {
                if mode == "plan_only" {
                    pre_results.push(AppliedOperation {
                        mcp_id: op.mcp_id.clone(),
                        action: "update_transform".to_string(),
                        status: "skipped".to_string(),
                        unreal_actor_name: None,
                        error: Some("plan_only mode".to_string()),
                    });
                    continue;
                }
                if let Some(desired) = &op.desired {
                    let mcp_id = op.mcp_id.as_str();
                    let transform = desired.get("transform");
                    let location = transform.and_then(|t| t.get("location"));
                    let rotation = transform.and_then(|t| t.get("rotation"));
                    let scale = transform.and_then(|t| t.get("scale"));
                    let loc: [f64; 3] = [
                        location
                            .and_then(|l| l.get("x"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        location
                            .and_then(|l| l.get("y"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        location
                            .and_then(|l| l.get("z"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                    ];
                    let rot: [f64; 3] = [
                        rotation
                            .and_then(|r| r.get("pitch"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        rotation
                            .and_then(|r| r.get("yaw"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                        rotation
                            .and_then(|r| r.get("roll"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(0.0),
                    ];
                    let scl: [f64; 3] = [
                        scale
                            .and_then(|s| s.get("x"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(1.0),
                        scale
                            .and_then(|s| s.get("y"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(1.0),
                        scale
                            .and_then(|s| s.get("z"))
                            .and_then(|v| v.as_f64())
                            .unwrap_or(1.0),
                    ];
                    updates.push(json!({
                        "mcp_id": mcp_id,
                        "location": loc,
                        "rotation": rot,
                        "scale": scl,
                    }));
                }
            }
            SyncAction::UpdateVisual => {
                if mode == "plan_only" {
                    pre_results.push(AppliedOperation {
                        mcp_id: op.mcp_id.clone(),
                        action: "update_visual".to_string(),
                        status: "skipped".to_string(),
                        unreal_actor_name: None,
                        error: Some("plan_only mode".to_string()),
                    });
                    continue;
                }
                // Visual updates still handled individually (bridge limitation)
                let applied = apply_visual_update(db, unreal, run_id, scene_id, op).await;
                pre_results.push(applied?);
            }
            SyncAction::Delete => {
                if mode == "plan_only" {
                    pre_results.push(AppliedOperation {
                        mcp_id: op.mcp_id.clone(),
                        action: "delete".to_string(),
                        status: "skipped".to_string(),
                        unreal_actor_name: None,
                        error: Some("plan_only mode".to_string()),
                    });
                    continue;
                }
                if !allow_delete {
                    op_records.push((
                        op.mcp_id.clone(),
                        "delete".to_string(),
                        "skipped".to_string(),
                        "allow_delete not enabled".to_string(),
                    ));
                    pre_results.push(AppliedOperation {
                        mcp_id: op.mcp_id.clone(),
                        action: "delete".to_string(),
                        status: "skipped".to_string(),
                        unreal_actor_name: None,
                        error: Some("allow_delete not enabled".to_string()),
                    });
                    continue;
                }
                deletes.push(json!({ "mcp_id": op.mcp_id }));
            }
            SyncAction::Noop => {
                op_records.push((
                    op.mcp_id.clone(),
                    "noop".to_string(),
                    "success".to_string(),
                    "no changes needed".to_string(),
                ));
                pre_results.push(AppliedOperation {
                    mcp_id: op.mcp_id.clone(),
                    action: "noop".to_string(),
                    status: "success".to_string(),
                    unreal_actor_name: None,
                    error: None,
                });
            }
            SyncAction::Conflict => {
                op_records.push((
                    op.mcp_id.clone(),
                    "conflict".to_string(),
                    "skipped".to_string(),
                    "conflict not auto-resolved".to_string(),
                ));
                pre_results.push(AppliedOperation {
                    mcp_id: op.mcp_id.clone(),
                    action: "conflict".to_string(),
                    status: "skipped".to_string(),
                    unreal_actor_name: None,
                    error: Some("conflict not auto-resolved".to_string()),
                });
            }
            SyncAction::Unsupported => {
                op_records.push((
                    op.mcp_id.clone(),
                    "unsupported".to_string(),
                    "skipped".to_string(),
                    "unsupported action".to_string(),
                ));
                pre_results.push(AppliedOperation {
                    mcp_id: op.mcp_id.clone(),
                    action: "unsupported".to_string(),
                    status: "skipped".to_string(),
                    unreal_actor_name: None,
                    error: Some("unsupported action".to_string()),
                });
            }
        }
    }

    // --- Build O(1) plan lookup to avoid repeated linear scans ---------
    let plan_lookup: HashMap<&str, &SyncOperation> = plan
        .operations
        .iter()
        .map(|op| (op.mcp_id.as_str(), op))
        .collect();

    // --- Send batch delta if any creates/updates/deletes exist ------------
    // Chunk size is configurable via SCENE_SYNCD_CREATE_CHUNK_SIZE env var.
    // --- Template clone grouping: same (type, mesh, tags) → group ------
    // C++ side already supports clone_actor. We group identical templates
    // so only the first in each group spawns; the rest are cloned.
    #[derive(Debug, Clone, Hash, Eq, PartialEq)]
    struct CloneGroupKey(String, String, Vec<String>); // actor_type, static_mesh, sorted_tags

    let mut groups: std::collections::HashMap<CloneGroupKey, Vec<usize>> =
        std::collections::HashMap::new();
    for (idx, create) in creates.iter().enumerate() {
        let actor_type = create
            .get("type")
            .and_then(|v| v.as_str())
            .unwrap_or("StaticMeshActor");
        let mesh = create
            .get("static_mesh")
            .and_then(|v| v.as_str())
            .unwrap_or("");
        let mut tags = create
            .get("tags")
            .and_then(|v| v.as_array())
            .map(|arr| {
                arr.iter()
                    .filter_map(|v| v.as_str().map(|s| s.to_string()))
                    .collect::<Vec<_>>()
            })
            .unwrap_or_default();
        tags.sort();
        let key = CloneGroupKey(actor_type.to_string(), mesh.to_string(), tags);
        groups.entry(key).or_default().push(idx);
    }

    // Build reduced creates (group leaders) and clone_ops
    let mut reduced_creates: Vec<serde_json::Value> = Vec::new();
    // (source_create_index_in_reduced, clone_params_json)
    let mut clone_ops: Vec<(usize, serde_json::Value)> = Vec::new();
    let mut reduced_index_to_group_first: std::collections::HashMap<usize, usize> =
        std::collections::HashMap::new();

    for (_key, indices) in groups {
        if indices.is_empty() {
            continue;
        }
        let leader_idx = reduced_creates.len();
        reduced_index_to_group_first.insert(leader_idx, indices[0]);
        reduced_creates.push(creates[indices[0]].clone());
        for &idx in &indices[1..] {
            clone_ops.push((leader_idx, creates[idx].clone()));
        }
    }

    // Re-chunk the reduced creates
    let chunk_sz = create_chunk_size();
    let create_chunks = build_create_chunks(
        &reduced_creates,
        chunk_sz,
        !updates.is_empty() || !deletes.is_empty(),
    );

    for (chunk_idx, create_chunk) in create_chunks.iter().enumerate() {
        let updates_chunk = if chunk_idx == 0 {
            updates.clone()
        } else {
            vec![]
        };
        let deletes_chunk = if chunk_idx == 0 {
            deletes.clone()
        } else {
            vec![]
        };

        let use_single_spawn =
            create_chunk.len() == 1 && updates_chunk.is_empty() && deletes_chunk.is_empty();
        let mut delta_result = if use_single_spawn {
            unreal
                .spawn_actor(create_chunk[0].clone())
                .await
                .map(|response| {
                    if response
                        .get("success")
                        .and_then(|v| v.as_bool())
                        .unwrap_or(false)
                    {
                        let name = response
                            .get("name")
                            .or_else(|| response.get("actor_name"))
                            .and_then(|v| v.as_str())
                            .unwrap_or_else(|| {
                                create_chunk[0]
                                    .get("name")
                                    .and_then(|v| v.as_str())
                                    .unwrap_or("")
                            });
                        json!({
                            "success": true,
                            "created_count": 1,
                            "updated_count": 0,
                            "deleted_count": 0,
                            "error_count": 0,
                            "created": [{"name": name}],
                            "updated": [],
                            "deleted": [],
                            "errors": [],
                        })
                    } else {
                        response
                    }
                })
        } else {
            unreal
                .apply_scene_delta(
                    &format!("{run_id}_batch_{chunk_idx}"),
                    create_chunk.clone(),
                    updates_chunk.clone(),
                    deletes_chunk.clone(),
                )
                .await
        };
        for attempt in 1..=2 {
            if delta_result.is_ok() {
                break;
            }
            sleep(Duration::from_millis(150 * attempt)).await;
            delta_result = if use_single_spawn {
                unreal
                    .spawn_actor(create_chunk[0].clone())
                    .await
                    .map(|response| {
                        if response
                            .get("success")
                            .and_then(|v| v.as_bool())
                            .unwrap_or(false)
                        {
                            let name = response
                                .get("name")
                                .or_else(|| response.get("actor_name"))
                                .and_then(|v| v.as_str())
                                .unwrap_or_else(|| {
                                    create_chunk[0]
                                        .get("name")
                                        .and_then(|v| v.as_str())
                                        .unwrap_or("")
                                });
                            json!({
                                "success": true,
                                "created_count": 1,
                                "updated_count": 0,
                                "deleted_count": 0,
                                "error_count": 0,
                                "created": [{"name": name}],
                                "updated": [],
                                "deleted": [],
                                "errors": [],
                            })
                        } else {
                            response
                        }
                    })
            } else {
                unreal
                    .apply_scene_delta(
                        &format!("{run_id}_batch_{chunk_idx}_retry_{attempt}"),
                        create_chunk.clone(),
                        updates_chunk.clone(),
                        deletes_chunk.clone(),
                    )
                    .await
            };
        }

        match delta_result {
            Ok(response) => {
                let success = response
                    .get("success")
                    .and_then(|v| v.as_bool())
                    .unwrap_or(false);
                if success {
                    let created_count = response
                        .get("created_count")
                        .and_then(|v| v.as_i64())
                        .unwrap_or(0) as usize;
                    let updated_count = response
                        .get("updated_count")
                        .and_then(|v| v.as_i64())
                        .unwrap_or(0) as usize;
                    let deleted_count = response
                        .get("deleted_count")
                        .and_then(|v| v.as_i64())
                        .unwrap_or(0) as usize;
                    let error_count = response
                        .get("error_count")
                        .and_then(|v| v.as_i64())
                        .unwrap_or(0) as usize;
                    let created_names = response_actor_names(&response, "created");
                    let updated_names = response_actor_names(&response, "updated");
                    let err_msg = response_error_summary(&response);

                    tracing::info!(
                        chunk = chunk_idx,
                        created = created_count,
                        updated = updated_count,
                        deleted = deleted_count,
                        errors = error_count,
                        "apply_scene_delta succeeded"
                    );

                    // Build source actor name map for clone operations
                    let mut source_names: std::collections::HashMap<usize, String> =
                        std::collections::HashMap::new();
                    if let Some(created_arr) = response.get("created").and_then(|v| v.as_array()) {
                        for (i, created) in created_arr.iter().enumerate() {
                            let name = created.get("name").and_then(|v| v.as_str()).unwrap_or("");
                            let reduced_idx = chunk_idx * chunk_sz + i;
                            if !name.is_empty() && reduced_idx < reduced_creates.len() {
                                source_names.insert(reduced_idx, name.to_string());
                            }
                        }
                    }

                    // Execute clone_ops for this chunk's spawned leaders
                    for (leader_idx, clone_params) in &clone_ops {
                        if let Some(source_name) = source_names.get(leader_idx) {
                            let clone_name = clone_params
                                .get("name")
                                .and_then(|v| v.as_str())
                                .unwrap_or("");
                            let clone_mcp_id = clone_params
                                .get("mcp_id")
                                .and_then(|v| v.as_str())
                                .unwrap_or("");
                            let loc = clone_params
                                .get("location")
                                .and_then(|v| v.as_array())
                                .map(|arr| {
                                    [
                                        arr.first().and_then(|v| v.as_f64()).unwrap_or(0.0),
                                        arr.get(1).and_then(|v| v.as_f64()).unwrap_or(0.0),
                                        arr.get(2).and_then(|v| v.as_f64()).unwrap_or(0.0),
                                    ]
                                })
                                .unwrap_or([0.0, 0.0, 0.0]);
                            let rot = clone_params
                                .get("rotation")
                                .and_then(|v| v.as_array())
                                .map(|arr| {
                                    [
                                        arr.first().and_then(|v| v.as_f64()).unwrap_or(0.0),
                                        arr.get(1).and_then(|v| v.as_f64()).unwrap_or(0.0),
                                        arr.get(2).and_then(|v| v.as_f64()).unwrap_or(0.0),
                                    ]
                                })
                                .unwrap_or([0.0, 0.0, 0.0]);
                            let scl = clone_params
                                .get("scale")
                                .and_then(|v| v.as_array())
                                .map(|arr| {
                                    [
                                        arr.first().and_then(|v| v.as_f64()).unwrap_or(1.0),
                                        arr.get(1).and_then(|v| v.as_f64()).unwrap_or(1.0),
                                        arr.get(2).and_then(|v| v.as_f64()).unwrap_or(1.0),
                                    ]
                                })
                                .unwrap_or([1.0, 1.0, 1.0]);
                            let clone_tags = clone_params
                                .get("tags")
                                .and_then(|v| v.as_array())
                                .map(|arr| {
                                    arr.iter()
                                        .filter_map(|v| v.as_str().map(|s| s.to_string()))
                                        .collect::<Vec<_>>()
                                })
                                .unwrap_or_default();
                            match unreal
                                .clone_actor(
                                    source_name,
                                    clone_name,
                                    loc,
                                    rot,
                                    scl,
                                    clone_mcp_id,
                                    &clone_tags,
                                )
                                .await
                            {
                                Ok(clone_resp) => {
                                    let clone_success = clone_resp
                                        .get("success")
                                        .and_then(|v| v.as_bool())
                                        .unwrap_or(false);
                                    if clone_success {
                                        let clone_actor_name = clone_resp
                                            .get("actor_name")
                                            .and_then(|v| v.as_str())
                                            .map(|s| s.to_string());
                                        let desired = plan_lookup
                                            .get(clone_mcp_id)
                                            .and_then(|op| op.desired.as_ref());
                                        let desired_hash = desired
                                            .and_then(|d| d.get("desired_hash"))
                                            .and_then(|v| v.as_str())
                                            .unwrap_or("");
                                        sync_marks.push((
                                            clone_mcp_id.to_string(),
                                            desired_hash.to_string(),
                                            clone_actor_name.clone(),
                                        ));
                                        op_records.push((
                                            clone_mcp_id.to_string(),
                                            "create".to_string(),
                                            "success".to_string(),
                                            "actor cloned from template".to_string(),
                                        ));
                                        pre_results.push(AppliedOperation {
                                            mcp_id: clone_mcp_id.to_string(),
                                            action: "create".to_string(),
                                            status: "success".to_string(),
                                            unreal_actor_name: clone_actor_name,
                                            error: None,
                                        });
                                    } else {
                                        let clone_err = clone_resp
                                            .get("error")
                                            .and_then(|e| e.as_str())
                                            .unwrap_or("clone failed");
                                        op_records.push((
                                            clone_mcp_id.to_string(),
                                            "create".to_string(),
                                            "error".to_string(),
                                            clone_err.to_string(),
                                        ));
                                        pre_results.push(AppliedOperation {
                                            mcp_id: clone_mcp_id.to_string(),
                                            action: "create".to_string(),
                                            status: "error".to_string(),
                                            unreal_actor_name: None,
                                            error: Some(clone_err.to_string()),
                                        });
                                    }
                                }
                                Err(e) => {
                                    let err_msg = e.to_string();
                                    op_records.push((
                                        clone_mcp_id.to_string(),
                                        "create".to_string(),
                                        "error".to_string(),
                                        err_msg.clone(),
                                    ));
                                    pre_results.push(AppliedOperation {
                                        mcp_id: clone_mcp_id.to_string(),
                                        action: "create".to_string(),
                                        status: "error".to_string(),
                                        unreal_actor_name: None,
                                        error: Some(err_msg),
                                    });
                                }
                            }
                        }
                    }

                    // Mark created objects in this chunk as synced
                    for create in create_chunk {
                        if let Some(mcp_id) = create.get("mcp_id").and_then(|v| v.as_str()) {
                            let actor_name = create
                                .get("name")
                                .and_then(|v| v.as_str())
                                .unwrap_or(mcp_id);
                            let actor_created = error_count == 0
                                && created_count == create_chunk.len()
                                || created_names.contains(actor_name);
                            if !actor_created {
                                op_records.push((
                                    mcp_id.to_string(),
                                    "create".to_string(),
                                    "error".to_string(),
                                    err_msg.clone(),
                                ));
                                pre_results.push(AppliedOperation {
                                    mcp_id: mcp_id.to_string(),
                                    action: "create".to_string(),
                                    status: "error".to_string(),
                                    unreal_actor_name: None,
                                    error: Some(err_msg.clone()),
                                });
                                continue;
                            }
                            let desired =
                                plan_lookup.get(mcp_id).and_then(|op| op.desired.as_ref());
                            let desired_hash = desired
                                .and_then(|d| d.get("desired_hash"))
                                .and_then(|v| v.as_str())
                                .unwrap_or("");
                            sync_marks.push((
                                mcp_id.to_string(),
                                desired_hash.to_string(),
                                Some(actor_name.to_string()),
                            ));
                            op_records.push((
                                mcp_id.to_string(),
                                "create".to_string(),
                                "success".to_string(),
                                "actor created in Unreal (batch)".to_string(),
                            ));
                            pre_results.push(AppliedOperation {
                                mcp_id: mcp_id.to_string(),
                                action: "create".to_string(),
                                status: "success".to_string(),
                                unreal_actor_name: Some(actor_name.to_string()),
                                error: None,
                            });
                        }
                    }
                    // Mark updated objects as synced (first chunk only)
                    if chunk_idx == 0 {
                        for update in &updates {
                            if let Some(mcp_id) = update.get("mcp_id").and_then(|v| v.as_str()) {
                                let desired_name = plan_lookup
                                    .get(mcp_id)
                                    .and_then(|op| op.desired.as_ref())
                                    .and_then(|d| d.get("desired_name"))
                                    .and_then(|v| v.as_str())
                                    .unwrap_or(mcp_id);
                                let actor_updated = error_count == 0
                                    && updated_count == updates.len()
                                    || updated_names.contains(desired_name);
                                if !actor_updated {
                                    op_records.push((
                                        mcp_id.to_string(),
                                        "update_transform".to_string(),
                                        "error".to_string(),
                                        err_msg.clone(),
                                    ));
                                    pre_results.push(AppliedOperation {
                                        mcp_id: mcp_id.to_string(),
                                        action: "update_transform".to_string(),
                                        status: "error".to_string(),
                                        unreal_actor_name: None,
                                        error: Some(err_msg.clone()),
                                    });
                                    continue;
                                }
                                let desired =
                                    plan_lookup.get(mcp_id).and_then(|op| op.desired.as_ref());
                                let desired_hash = desired
                                    .and_then(|d| d.get("desired_hash"))
                                    .and_then(|v| v.as_str())
                                    .unwrap_or("");
                                sync_marks.push((
                                    mcp_id.to_string(),
                                    desired_hash.to_string(),
                                    None,
                                ));
                                op_records.push((
                                    mcp_id.to_string(),
                                    "update_transform".to_string(),
                                    "success".to_string(),
                                    "transform updated (batch)".to_string(),
                                ));
                                pre_results.push(AppliedOperation {
                                    mcp_id: mcp_id.to_string(),
                                    action: "update_transform".to_string(),
                                    status: "success".to_string(),
                                    unreal_actor_name: None,
                                    error: None,
                                });
                            }
                        }
                        for delete in &deletes {
                            if let Some(mcp_id) = delete.get("mcp_id").and_then(|v| v.as_str()) {
                                db.mark_object_deleted_applied(scene_id, mcp_id).await?;
                                op_records.push((
                                    mcp_id.to_string(),
                                    "delete".to_string(),
                                    "success".to_string(),
                                    "actor deleted (batch)".to_string(),
                                ));
                                pre_results.push(AppliedOperation {
                                    mcp_id: mcp_id.to_string(),
                                    action: "delete".to_string(),
                                    status: "success".to_string(),
                                    unreal_actor_name: None,
                                    error: None,
                                });
                            }
                        }
                    }
                } else {
                    let err_msg = response
                        .get("error")
                        .and_then(|e| e.as_str())
                        .unwrap_or("unknown batch error");
                    tracing::warn!(
                        chunk = chunk_idx,
                        error = err_msg,
                        "apply_scene_delta failed"
                    );
                    for create in create_chunk {
                        if let Some(mcp_id) = create.get("mcp_id").and_then(|v| v.as_str()) {
                            op_records.push((
                                mcp_id.to_string(),
                                "create".to_string(),
                                "error".to_string(),
                                err_msg.to_string(),
                            ));
                            pre_results.push(AppliedOperation {
                                mcp_id: mcp_id.to_string(),
                                action: "create".to_string(),
                                status: "error".to_string(),
                                unreal_actor_name: None,
                                error: Some(err_msg.to_string()),
                            });
                        }
                    }
                }
            }
            Err(e) => {
                let err_msg = e.to_string();
                tracing::warn!(chunk = chunk_idx, error = %err_msg, "apply_scene_delta bridge error");
                for create in create_chunk {
                    if let Some(mcp_id) = create.get("mcp_id").and_then(|v| v.as_str()) {
                        op_records.push((
                            mcp_id.to_string(),
                            "create".to_string(),
                            "error".to_string(),
                            err_msg.clone(),
                        ));
                        pre_results.push(AppliedOperation {
                            mcp_id: mcp_id.to_string(),
                            action: "create".to_string(),
                            status: "error".to_string(),
                            unreal_actor_name: None,
                            error: Some(err_msg.clone()),
                        });
                    }
                }
            }
        }
    }

    // Batch apply all accumulated sync status updates
    if !sync_marks.is_empty() {
        db.mark_objects_synced_batch(scene_id, &sync_marks).await?;
    }

    // Batch insert all accumulated operation records
    if !op_records.is_empty() {
        db.record_operations_batch(run_id, scene_id, &op_records)
            .await?;
    }

    Ok(pre_results)
}

#[allow(dead_code)]
fn extract_verified_actor_name(
    verify_response: &serde_json::Value,
    mcp_id: &str,
) -> Result<Option<String>, String> {
    let verified = verify_response
        .get("success")
        .and_then(|v| v.as_bool())
        .unwrap_or(false);
    if !verified {
        let err_msg = verify_response
            .get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("created actor could not be verified by mcp_id");
        return Err(format!(
            "created actor was not verifiable by mcp_id: {err_msg}"
        ));
    }

    match verify_response.get("actor").and_then(|a| a.as_object()) {
        Some(actor_obj) => {
            Ok(actor_obj.get("name")
                .and_then(|n| n.as_str())
                .map(|s| s.to_string()))
        }
        None => {
            Err(format!(
                "created actor was not verifiable by mcp_id: find_actor_by_mcp_id returned success but no actor body (plugin may be missing mcp_id support for {})",
                mcp_id
            ))
        }
    }
}

async fn apply_visual_update(
    db: &SurrealSceneRepository,
    unreal: &UnrealClient,
    run_id: &str,
    scene_id: &str,
    op: &SyncOperation,
) -> Result<AppliedOperation, AppError> {
    let desired = op.desired.as_ref().ok_or_else(|| {
        AppError::Validation("update_visual operation missing desired data".to_string())
    })?;

    let mcp_id = op.mcp_id.as_str();
    let mut applied_count = 0usize;
    let mut skip_reasons: Vec<String> = Vec::new();

    // Resolve the Unreal actor name from the operation's actual data or via lookup
    let actor_name = op
        .actual
        .as_ref()
        .and_then(|a| a.get("name"))
        .and_then(|v| v.as_str())
        .map(|s| s.to_string());

    // Apply material color if visual contains color info
    if let Some(visual) = desired.get("visual").and_then(|v| v.as_object()) {
        if let (Some(r), Some(g), Some(b)) = (
            visual.get("color_r").and_then(|v| v.as_f64()),
            visual.get("color_g").and_then(|v| v.as_f64()),
            visual.get("color_b").and_then(|v| v.as_f64()),
        ) {
            if let Some(ref name) = actor_name {
                tracing::info!(mcp_id = mcp_id, "Setting material color in Unreal");
                let result = unreal.set_mesh_material_color(name, r, g, b).await;
                match result {
                    Ok(response) => {
                        let success = response
                            .get("success")
                            .and_then(|v| v.as_bool())
                            .unwrap_or(false);
                        if success {
                            applied_count += 1;
                        } else {
                            let err = response
                                .get("error")
                                .and_then(|e| e.as_str())
                                .unwrap_or("unknown material color error");
                            skip_reasons.push(format!("material color failed: {err}"));
                        }
                    }
                    Err(e) => {
                        skip_reasons.push(format!("material color bridge error: {e}"));
                    }
                }
            } else {
                skip_reasons.push("material color skipped: actor name not resolved".to_string());
            }
        }
    }

    // Asset/mesh changes require delete+create (bridge limitation)
    if desired.get("asset_ref").is_some()
        && op
            .actual
            .as_ref()
            .and_then(|a| a.get("static_mesh"))
            .is_some()
    {
        let desired_path = desired
            .get("asset_ref")
            .and_then(|v| v.get("path"))
            .and_then(|v| v.as_str())
            .unwrap_or("");
        let actual_path = op
            .actual
            .as_ref()
            .and_then(|a| a.get("static_mesh"))
            .and_then(|v| v.as_str())
            .unwrap_or("");
        if desired_path != actual_path && !desired_path.is_empty() {
            skip_reasons.push(
                "asset change requires delete+create (bridge has no mesh swap command)".to_string(),
            );
        }
    }

    // Tag changes are set at spawn time only (bridge limitation)
    if desired.get("tags").is_some() {
        // Tags are already applied on create; updating them requires re-spawn
    }

    let desired_hash = desired
        .get("desired_hash")
        .and_then(|v| v.as_str())
        .unwrap_or("");

    if applied_count > 0 {
        db.mark_object_synced(scene_id, mcp_id, desired_hash, None)
            .await?;
        db.record_operation(
            run_id,
            scene_id,
            mcp_id,
            "update_visual",
            "success",
            &format!("visual update applied ({applied_count} changes)"),
        )
        .await?;
        Ok(AppliedOperation {
            mcp_id: mcp_id.to_string(),
            action: "update_visual".to_string(),
            status: "success".to_string(),
            unreal_actor_name: None,
            error: None,
        })
    } else {
        let reason = if skip_reasons.is_empty() {
            "visual hash differs but no applicable bridge command exists"
        } else {
            &skip_reasons.join("; ")
        };
        db.record_operation(run_id, scene_id, mcp_id, "update_visual", "skipped", reason)
            .await?;
        Ok(AppliedOperation {
            mcp_id: mcp_id.to_string(),
            action: "update_visual".to_string(),
            status: "skipped".to_string(),
            unreal_actor_name: None,
            error: Some(reason.to_string()),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn build_create_chunks_keeps_delete_only_delta_executable() {
        let chunks = build_create_chunks(&[], 50, true);
        assert_eq!(chunks.len(), 1);
        assert!(chunks[0].is_empty());
    }

    #[test]
    fn build_create_chunks_empty_when_no_delta() {
        let chunks = build_create_chunks(&[], 50, false);
        assert!(chunks.is_empty());
    }

    #[test]
    fn build_create_chunks_splits_creates() {
        let creates = vec![
            json!({"mcp_id": "a"}),
            json!({"mcp_id": "b"}),
            json!({"mcp_id": "c"}),
        ];
        let chunks = build_create_chunks(&creates, 2, false);
        assert_eq!(chunks.len(), 2);
        assert_eq!(chunks[0].len(), 2);
        assert_eq!(chunks[1].len(), 1);
    }

    #[test]
    fn test_extract_verified_actor_name_success() {
        let response = json!({
            "success": true,
            "actor": {
                "name": "TestCube_01",
                "class": "StaticMeshActor",
                "location": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "tags": ["managed_by_mcp", "mcp_id:test_001"],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube"
            }
        });
        let result = extract_verified_actor_name(&response, "test_001");
        assert_eq!(result.unwrap(), Some("TestCube_01".to_string()));
    }

    #[test]
    fn test_extract_verified_actor_name_missing_actor() {
        let response = json!({
            "success": true,
            "message": "No actor found with the given mcp_id",
            "actor": null
        });
        let result = extract_verified_actor_name(&response, "test_002");
        assert!(result.is_err());
        let err = result.unwrap_err();
        assert!(err.contains("success but no actor body"));
        assert!(err.contains("test_002"));
    }

    #[test]
    fn test_extract_verified_actor_name_no_actor_field() {
        let response = json!({
            "success": false,
            "error": "Unknown command: find_actor_by_mcp_id"
        });
        let result = extract_verified_actor_name(&response, "test_003");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("not verifiable by mcp_id"));
    }

    #[test]
    fn test_extract_verified_actor_name_success_false() {
        let response = json!({
            "success": false,
            "error": "something went wrong"
        });
        let result = extract_verified_actor_name(&response, "test_004");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("not verifiable by mcp_id"));
    }
}
