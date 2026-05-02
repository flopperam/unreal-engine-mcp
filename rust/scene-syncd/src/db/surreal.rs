use crate::domain::*;
use crate::error::AppError;
use surrealdb::engine::any::Any;
use surrealdb::sql::Datetime;
use surrealdb::Surreal;

/// Build a scene_object record key, stripping any `scene:` prefix from the scene id.
pub fn scene_object_record_key(scene_id: &str, mcp_id: &str) -> String {
    let id = scene_id.strip_prefix("scene:").unwrap_or(scene_id);
    format!("{}:{}", id, mcp_id)
}

#[derive(Debug, Clone)]
pub struct SurrealSceneRepository {
    db: Surreal<Any>,
}

impl SurrealSceneRepository {
    pub fn new(db: Surreal<Any>) -> Self {
        Self { db }
    }

    pub async fn ensure_schema(&self) -> Result<(), AppError> {
        let queries = [
            "DEFINE TABLE scene SCHEMAFULL;",
            "DEFINE FIELD name ON TABLE scene TYPE string;",
            "DEFINE FIELD description ON TABLE scene TYPE option<string>;",
            "DEFINE FIELD status ON TABLE scene TYPE string DEFAULT \"active\";",
            "DEFINE FIELD active_revision ON TABLE scene TYPE int DEFAULT 1;",
            "DEFINE FIELD unreal_project_path ON TABLE scene TYPE option<string>;",
            "DEFINE FIELD unreal_level_name ON TABLE scene TYPE option<string>;",
            "DEFINE FIELD created_at ON TABLE scene TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene TYPE datetime DEFAULT time::now();",

            "DEFINE TABLE scene_group SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_group TYPE string;",
            "DEFINE FIELD kind ON TABLE scene_group TYPE string;",
            "DEFINE FIELD tool_name ON TABLE scene_group TYPE option<string>;",
            "DEFINE FIELD name ON TABLE scene_group TYPE string;",
            "DEFINE FIELD params ON TABLE scene_group TYPE object DEFAULT {};",
            "DEFINE FIELD seed ON TABLE scene_group TYPE option<string>;",
            "DEFINE FIELD revision ON TABLE scene_group TYPE int DEFAULT 1;",
            "DEFINE FIELD deleted ON TABLE scene_group TYPE bool DEFAULT false;",
            "DEFINE FIELD created_at ON TABLE scene_group TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_group TYPE datetime DEFAULT time::now();",

            "DEFINE TABLE scene_object SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_object TYPE string;",
            "DEFINE FIELD OVERWRITE group ON TABLE scene_object TYPE option<string>;",
            "DEFINE FIELD mcp_id ON TABLE scene_object TYPE string;",
            "DEFINE FIELD desired_name ON TABLE scene_object TYPE string;",
            "DEFINE FIELD unreal_actor_name ON TABLE scene_object TYPE option<string>;",
            "DEFINE FIELD actor_type ON TABLE scene_object TYPE string;",
            "DEFINE FIELD OVERWRITE asset_ref ON TABLE scene_object FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD asset_ref.path ON TABLE scene_object TYPE option<string>;",
            "DEFINE FIELD transform ON TABLE scene_object TYPE object;",
            "DEFINE FIELD transform.location ON TABLE scene_object TYPE object;",
            "DEFINE FIELD transform.location.x ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.location.y ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.location.z ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.rotation ON TABLE scene_object TYPE object;",
            "DEFINE FIELD transform.rotation.pitch ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.rotation.yaw ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.rotation.roll ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.scale ON TABLE scene_object TYPE object;",
            "DEFINE FIELD transform.scale.x ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.scale.y ON TABLE scene_object TYPE number;",
            "DEFINE FIELD transform.scale.z ON TABLE scene_object TYPE number;",
            "DEFINE FIELD OVERWRITE visual ON TABLE scene_object FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD OVERWRITE physics ON TABLE scene_object FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD tags ON TABLE scene_object TYPE array DEFAULT [];",
            "DEFINE FIELD tags.* ON TABLE scene_object TYPE string;",
            "DEFINE FIELD OVERWRITE metadata ON TABLE scene_object FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD desired_hash ON TABLE scene_object TYPE string;",
            "DEFINE FIELD last_applied_hash ON TABLE scene_object TYPE option<string>;",
            "DEFINE FIELD sync_status ON TABLE scene_object TYPE string DEFAULT \"pending\";",
            "DEFINE FIELD deleted ON TABLE scene_object TYPE bool DEFAULT false;",
            "DEFINE FIELD revision ON TABLE scene_object TYPE int DEFAULT 1;",
            "DEFINE FIELD created_at ON TABLE scene_object TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_object TYPE datetime DEFAULT time::now();",

            "DEFINE INDEX scene_object_scene_mcp_id ON TABLE scene_object COLUMNS scene, mcp_id UNIQUE;",
            "DEFINE INDEX scene_object_scene_group ON TABLE scene_object COLUMNS scene, group;",
            "DEFINE INDEX scene_object_sync_status ON TABLE scene_object COLUMNS scene, sync_status;",

            "DEFINE TABLE scene_snapshot SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_snapshot TYPE string;",
            "DEFINE FIELD name ON TABLE scene_snapshot TYPE string;",
            "DEFINE FIELD description ON TABLE scene_snapshot TYPE option<string>;",
            "DEFINE FIELD revision ON TABLE scene_snapshot TYPE int;",
            "DEFINE FIELD groups ON TABLE scene_snapshot TYPE array;",
            "DEFINE FIELD groups.* ON TABLE scene_snapshot FLEXIBLE TYPE object;",
            "DEFINE FIELD objects ON TABLE scene_snapshot TYPE array;",
            "DEFINE FIELD objects.* ON TABLE scene_snapshot FLEXIBLE TYPE object;",
            "DEFINE FIELD created_at ON TABLE scene_snapshot TYPE datetime DEFAULT time::now();",

            "DEFINE TABLE sync_run SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE sync_run TYPE string;",
            "DEFINE FIELD mode ON TABLE sync_run TYPE string;",
            "DEFINE FIELD status ON TABLE sync_run TYPE string;",
            "DEFINE FIELD summary ON TABLE sync_run TYPE object DEFAULT {};",
            "DEFINE FIELD started_at ON TABLE sync_run TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD ended_at ON TABLE sync_run TYPE option<datetime>;",
            "DEFINE FIELD error ON TABLE sync_run TYPE option<string>;",

            "DEFINE TABLE scene_operation SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_operation TYPE string;",
            "DEFINE FIELD OVERWRITE sync_run ON TABLE scene_operation TYPE string;",
            "DEFINE FIELD OVERWRITE object ON TABLE scene_operation TYPE option<string>;",
            "DEFINE FIELD mcp_id ON TABLE scene_operation TYPE option<string>;",
            "DEFINE FIELD action ON TABLE scene_operation TYPE string;",
            "DEFINE FIELD reason ON TABLE scene_operation TYPE string;",
            "DEFINE FIELD desired ON TABLE scene_operation TYPE option<object>;",
            "DEFINE FIELD actual ON TABLE scene_operation TYPE option<object>;",
            "DEFINE FIELD status ON TABLE scene_operation TYPE string;",
            "DEFINE FIELD attempts ON TABLE scene_operation TYPE int DEFAULT 1;",
            "DEFINE FIELD error ON TABLE scene_operation TYPE option<string>;",
            "DEFINE FIELD created_at ON TABLE scene_operation TYPE datetime DEFAULT time::now();",

            "DEFINE TABLE actor_observation SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE actor_observation TYPE string;",
            "DEFINE FIELD OVERWRITE sync_run ON TABLE actor_observation TYPE option<string>;",
            "DEFINE FIELD mcp_id ON TABLE actor_observation TYPE option<string>;",
            "DEFINE FIELD unreal_actor_name ON TABLE actor_observation TYPE string;",
            "DEFINE FIELD class_name ON TABLE actor_observation TYPE string;",
            "DEFINE FIELD transform ON TABLE actor_observation TYPE object;",
            "DEFINE FIELD tags ON TABLE actor_observation TYPE array;",
            "DEFINE FIELD raw ON TABLE actor_observation TYPE option<object>;",
            "DEFINE FIELD observed_at ON TABLE actor_observation TYPE datetime DEFAULT time::now();",

            "DEFINE TABLE generator_run SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE generator_run TYPE string;",
            "DEFINE FIELD kind ON TABLE generator_run TYPE string;",
            "DEFINE FIELD tool_name ON TABLE generator_run TYPE string;",
            "DEFINE FIELD name ON TABLE generator_run TYPE string;",
            "DEFINE FIELD params ON TABLE generator_run TYPE object DEFAULT {};",
            "DEFINE FIELD seed ON TABLE generator_run TYPE option<string>;",
            "DEFINE FIELD group_id ON TABLE generator_run TYPE option<string>;",
            "DEFINE FIELD generated_count ON TABLE generator_run TYPE int DEFAULT 0;",
            "DEFINE FIELD status ON TABLE generator_run TYPE string DEFAULT 'completed';",
            "DEFINE FIELD created_at ON TABLE generator_run TYPE datetime DEFAULT time::now();",

            // P3: Semantic tables
            "DEFINE TABLE scene_entity SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_entity TYPE string;",
            "DEFINE FIELD entity_id ON TABLE scene_entity TYPE string;",
            "DEFINE FIELD kind ON TABLE scene_entity TYPE string;",
            "DEFINE FIELD name ON TABLE scene_entity TYPE string;",
            "DEFINE FIELD OVERWRITE properties ON TABLE scene_entity FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD tags ON TABLE scene_entity TYPE array DEFAULT [];",
            "DEFINE FIELD tags.* ON TABLE scene_entity TYPE string;",
            "DEFINE FIELD mcp_ids ON TABLE scene_entity TYPE array DEFAULT [];",
            "DEFINE FIELD mcp_ids.* ON TABLE scene_entity TYPE string;",
            "DEFINE FIELD OVERWRITE metadata ON TABLE scene_entity FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD deleted ON TABLE scene_entity TYPE bool DEFAULT false;",
            "DEFINE FIELD revision ON TABLE scene_entity TYPE int DEFAULT 1;",
            "DEFINE FIELD created_at ON TABLE scene_entity TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_entity TYPE datetime DEFAULT time::now();",
            "DEFINE INDEX scene_entity_scene_entity_id ON TABLE scene_entity COLUMNS scene, entity_id UNIQUE;",

            "DEFINE TABLE scene_relation SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_relation TYPE string;",
            "DEFINE FIELD relation_id ON TABLE scene_relation TYPE string;",
            "DEFINE FIELD source_entity_id ON TABLE scene_relation TYPE string;",
            "DEFINE FIELD target_entity_id ON TABLE scene_relation TYPE string;",
            "DEFINE FIELD relation_type ON TABLE scene_relation TYPE string;",
            "DEFINE FIELD OVERWRITE properties ON TABLE scene_relation FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD OVERWRITE metadata ON TABLE scene_relation FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD created_at ON TABLE scene_relation TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_relation TYPE datetime DEFAULT time::now();",
            "DEFINE INDEX scene_relation_scene_id ON TABLE scene_relation COLUMNS scene, relation_id UNIQUE;",

            // Reserved for P6 (NavMesh/Collision/AI) and future implementation.
            "DEFINE TABLE scene_component SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_component TYPE string;",
            "DEFINE FIELD entity_id ON TABLE scene_component TYPE string;",
            "DEFINE FIELD component_type ON TABLE scene_component TYPE string;",
            "DEFINE FIELD name ON TABLE scene_component TYPE string;",
            "DEFINE FIELD OVERWRITE properties ON TABLE scene_component FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD OVERWRITE metadata ON TABLE scene_component FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD created_at ON TABLE scene_component TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_component TYPE datetime DEFAULT time::now();",
            "DEFINE INDEX scene_component_entity ON TABLE scene_component COLUMNS scene, entity_id, component_type, name UNIQUE;",

            "DEFINE TABLE scene_asset SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_asset TYPE string;",
            "DEFINE FIELD asset_id ON TABLE scene_asset TYPE string;",
            "DEFINE FIELD kind ON TABLE scene_asset TYPE string;",
            "DEFINE FIELD status ON TABLE scene_asset TYPE string DEFAULT 'present';",
            "DEFINE FIELD fallback ON TABLE scene_asset TYPE string DEFAULT '';",
            "DEFINE FIELD semantic_tags ON TABLE scene_asset TYPE array DEFAULT [];",
            "DEFINE FIELD semantic_tags.* ON TABLE scene_asset TYPE string;",
            "DEFINE FIELD quality ON TABLE scene_asset TYPE string DEFAULT 'prototype';",
            "DEFINE FIELD OVERWRITE variants ON TABLE scene_asset FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD OVERWRITE metadata ON TABLE scene_asset FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD created_at ON TABLE scene_asset TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_asset TYPE datetime DEFAULT time::now();",
            "DEFINE INDEX scene_asset_scene_id ON TABLE scene_asset COLUMNS scene, asset_id UNIQUE;",

            // Reserved for P6 (NavMesh/Collision/AI) and future implementation.
            "DEFINE TABLE scene_blueprint SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_blueprint TYPE string;",
            "DEFINE FIELD blueprint_id ON TABLE scene_blueprint TYPE string;",
            "DEFINE FIELD class_name ON TABLE scene_blueprint TYPE string;",
            "DEFINE FIELD parent_class ON TABLE scene_blueprint TYPE string DEFAULT '';",
            "DEFINE FIELD components ON TABLE scene_blueprint TYPE array DEFAULT [];",
            "DEFINE FIELD components.* ON TABLE scene_blueprint TYPE object;",
            "DEFINE FIELD variables ON TABLE scene_blueprint TYPE array DEFAULT [];",
            "DEFINE FIELD variables.* ON TABLE scene_blueprint TYPE object;",
            "DEFINE FIELD OVERWRITE metadata ON TABLE scene_blueprint FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE FIELD created_at ON TABLE scene_blueprint TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_blueprint TYPE datetime DEFAULT time::now();",
            "DEFINE INDEX scene_blueprint_scene_id ON TABLE scene_blueprint COLUMNS scene, blueprint_id UNIQUE;",

            // Reserved for P6 (NavMesh/Collision/AI) and future implementation.
            "DEFINE TABLE scene_realization SCHEMAFULL;",
            "DEFINE FIELD OVERWRITE scene ON TABLE scene_realization TYPE string;",
            "DEFINE FIELD entity_id ON TABLE scene_realization TYPE string;",
            "DEFINE FIELD policy ON TABLE scene_realization TYPE string;",
            "DEFINE FIELD status ON TABLE scene_realization TYPE string DEFAULT 'pending';",
            "DEFINE FIELD unreal_actor_name ON TABLE scene_realization TYPE option<string>;",
            "DEFINE FIELD OVERWRITE metadata ON TABLE scene_realization FLEXIBLE TYPE object DEFAULT {};",
            "DEFINE INDEX scene_realization_entity_policy ON TABLE scene_realization COLUMNS scene, entity_id, policy UNIQUE;",
            "DEFINE FIELD created_at ON TABLE scene_realization TYPE datetime DEFAULT time::now();",
            "DEFINE FIELD updated_at ON TABLE scene_realization TYPE datetime DEFAULT time::now();",
        ];

        for query in &queries {
            self.db
                .query(*query)
                .await
                .map_err(|e| AppError::Database(format!("schema migration error: {e}")))?;
        }

        tracing::info!("Schema migrations applied");
        Ok(())
    }

    pub async fn ensure_default_scene(&self) -> Result<(), AppError> {
        let now = Datetime::from(chrono::Utc::now());

        let existing: Option<Scene> = self
            .db
            .select(("scene", "main"))
            .await
            .map_err(|e| AppError::Database(format!("select scene:main error: {e}")))?;

        if existing.is_none() {
            let scene = Scene {
                id: "scene:main".to_string(),
                name: "Main Scene".to_string(),
                description: Some("Default managed scene".to_string()),
                status: "active".to_string(),
                active_revision: 1,
                unreal_project_path: None,
                unreal_level_name: None,
                created_at: now.clone(),
                updated_at: now,
            };

            let _: Option<Scene> = self
                .db
                .create(("scene", "main"))
                .content(scene)
                .await
                .map_err(|e| AppError::Database(format!("create scene:main error: {e}")))?;

            tracing::info!("Created default scene:main");
        }

        Ok(())
    }

    pub async fn create_scene(
        &self,
        scene_id: &str,
        name: &str,
        description: Option<String>,
    ) -> Result<Scene, AppError> {
        let now = Datetime::from(chrono::Utc::now());
        let scene = Scene {
            id: format!("scene:{scene_id}"),
            name: name.to_string(),
            description,
            status: "active".to_string(),
            active_revision: 1,
            unreal_project_path: None,
            unreal_level_name: None,
            created_at: now.clone(),
            updated_at: now,
        };

        let created: Option<Scene> = self
            .db
            .create(("scene", scene_id))
            .content(scene)
            .await
            .map_err(|e| AppError::Database(format!("create scene error: {e}")))?;

        created.ok_or_else(|| AppError::Internal("failed to create scene".to_string()))
    }

    pub async fn upsert_scene(
        &self,
        scene_id: &str,
        name: &str,
        description: Option<String>,
    ) -> Result<Scene, AppError> {
        let now = Datetime::from(chrono::Utc::now());

        let existing: Option<Scene> = self
            .db
            .select(("scene", scene_id))
            .await
            .map_err(|e| AppError::Database(format!("select scene error: {e}")))?;

        if let Some(mut scene) = existing {
            scene.name = name.to_string();
            if description.is_some() {
                scene.description = description;
            }
            scene.updated_at = now;
            let updated: Option<Scene> =
                self.db
                    .update(("scene", scene_id))
                    .content(scene)
                    .await
                    .map_err(|e| AppError::Database(format!("update scene error: {e}")))?;
            updated.ok_or_else(|| AppError::Internal("failed to update scene".to_string()))
        } else {
            self.create_scene(scene_id, name, description).await
        }
    }

    pub async fn list_scenes(&self) -> Result<Vec<Scene>, AppError> {
        let scenes: Vec<Scene> = self
            .db
            .select("scene")
            .await
            .map_err(|e| AppError::Database(format!("list scenes error: {e}")))?;
        Ok(scenes)
    }

    pub async fn update_scene_status(
        &self,
        scene_id: &str,
        status: &str,
    ) -> Result<Scene, AppError> {
        let existing: Option<Scene> = self
            .db
            .select(("scene", scene_id))
            .await
            .map_err(|e| AppError::Database(format!("select scene error: {e}")))?;
        let mut scene =
            existing.ok_or_else(|| AppError::NotFound(format!("scene {scene_id} not found")))?;
        scene.status = status.to_string();
        scene.updated_at = Datetime::from(chrono::Utc::now());
        let updated: Option<Scene> = self
            .db
            .update(("scene", scene_id))
            .content(scene.clone())
            .await
            .map_err(|e| AppError::Database(format!("update scene status error: {e}")))?;
        updated.ok_or_else(|| AppError::Internal("failed to update scene status".to_string()))
    }

    pub async fn upsert_object(&self, obj: &SceneObject) -> Result<SceneObject, AppError> {
        let record_key = scene_object_record_key(&obj.scene, &obj.mcp_id);
        let key_owned = record_key.clone();
        let scene_id = obj.scene.trim_start_matches("scene:");

        let updated: Option<SceneObject> = self
            .db
            .query(
                "UPSERT type::thing($table, $key) MERGE { \
                    scene: $scene, \
                    group: $group, \
                    mcp_id: $mcp_id, \
                    desired_name: $desired_name, \
                    actor_type: $actor_type, \
                    asset_ref: $asset_ref, \
                    transform: $transform, \
                    visual: $visual, \
                    physics: $physics, \
                    tags: $tags, \
                    metadata: $metadata, \
                    desired_hash: $desired_hash, \
                    deleted: $deleted, \
                    sync_status: 'pending', \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_object"))
            .bind(("key", key_owned))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("group", obj.group.clone()))
            .bind(("mcp_id", obj.mcp_id.clone()))
            .bind(("desired_name", obj.desired_name.clone()))
            .bind(("actor_type", obj.actor_type.clone()))
            .bind(("asset_ref", obj.asset_ref.clone()))
            .bind(("transform", obj.transform.clone()))
            .bind(("visual", obj.visual.clone()))
            .bind(("physics", obj.physics.clone()))
            .bind(("tags", obj.tags.clone()))
            .bind(("metadata", obj.metadata.clone()))
            .bind(("desired_hash", obj.desired_hash.clone()))
            .bind(("deleted", obj.deleted))
            .await
            .map_err(|e| AppError::Database(format!("upsert object error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("upsert object parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to upsert object".to_string()))
    }

    pub async fn list_desired_objects(
        &self,
        scene: &str,
        include_deleted: bool,
        group_id: Option<&str>,
        limit: Option<usize>,
    ) -> Result<Vec<SceneObject>, AppError> {
        let mut conditions = vec![format!("scene = $scene")];
        if !include_deleted {
            conditions.push("deleted = false".to_string());
        }
        if group_id.is_some() {
            conditions.push("group = $group".to_string());
        }
        let where_clause = conditions.join(" AND ");
        let mut query = format!("SELECT * FROM scene_object WHERE {where_clause}");
        if limit.is_some() {
            query.push_str(" LIMIT $limit");
        }

        let mut q = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene}")));
        if let Some(n) = limit {
            q = q.bind(("limit", n));
        }
        if let Some(gid) = group_id {
            q = q.bind(("group", format!("scene_group:{gid}")));
        }

        let objects: Vec<SceneObject> = q
            .await
            .map_err(|e| AppError::Database(format!("list objects error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list objects parse error: {e}")))?;

        Ok(objects)
    }

    pub async fn mark_object_deleted(&self, scene: &str, mcp_id: &str) -> Result<(), AppError> {
        let record_key = scene_object_record_key(scene, mcp_id);

        let updated: Option<SceneObject> = self
            .db
            .update(("scene_object", record_key.as_str()))
            .merge(serde_json::json!({
                "deleted": true,
                "sync_status": "pending",
            }))
            .await
            .map_err(|e| AppError::Database(format!("mark deleted error: {e}")))?;

        if updated.is_none() {
            return Err(AppError::NotFound(format!(
                "object {mcp_id} not found in scene {scene}"
            )));
        }

        Ok(())
    }

    pub async fn create_group(
        &self,
        scene_id: &str,
        kind: &str,
        name: &str,
        tool_name: Option<String>,
        params: serde_json::Value,
        seed: Option<String>,
    ) -> Result<SceneGroup, AppError> {
        let now = Datetime::from(chrono::Utc::now());
        let record_key = format!("{}:{}", scene_id, name);
        let group = SceneGroup {
            id: format!("scene_group:{record_key}"),
            scene: format!("scene:{scene_id}"),
            kind: kind.to_string(),
            tool_name,
            name: name.to_string(),
            params,
            seed,
            revision: 1,
            deleted: false,
            created_at: now.clone(),
            updated_at: now,
        };

        let created: Option<SceneGroup> = self
            .db
            .create(("scene_group", record_key.as_str()))
            .content(group)
            .await
            .map_err(|e| AppError::Database(format!("create group error: {e}")))?;

        created.ok_or_else(|| AppError::Internal("failed to create group".to_string()))
    }

    pub async fn list_groups(
        &self,
        scene_id: &str,
        include_deleted: bool,
    ) -> Result<Vec<SceneGroup>, AppError> {
        let query = if include_deleted {
            "SELECT * FROM scene_group WHERE scene = $scene"
        } else {
            "SELECT * FROM scene_group WHERE scene = $scene AND deleted = false"
        };

        let groups: Vec<SceneGroup> = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene_id}")))
            .await
            .map_err(|e| AppError::Database(format!("list groups error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list groups parse error: {e}")))?;

        Ok(groups)
    }

    pub async fn create_generator_run(
        &self,
        scene_id: &str,
        kind: &str,
        tool_name: &str,
        name: &str,
        params: serde_json::Value,
        seed: Option<String>,
        group_id: Option<String>,
        generated_count: i64,
    ) -> Result<GeneratorRun, AppError> {
        let now = Datetime::from(chrono::Utc::now());
        let run_id = format!("{scene_id}:{tool_name}:{}", ulid::Ulid::new());
        let run = GeneratorRun {
            id: format!("generator_run:{run_id}"),
            scene: format!("scene:{scene_id}"),
            kind: kind.to_string(),
            tool_name: tool_name.to_string(),
            name: name.to_string(),
            params,
            seed,
            group_id,
            generated_count,
            status: "completed".to_string(),
            created_at: now,
        };

        let created: Option<GeneratorRun> = self
            .db
            .create(("generator_run", run_id.as_str()))
            .content(run)
            .await
            .map_err(|e| AppError::Database(format!("create generator_run error: {e}")))?;

        created.ok_or_else(|| AppError::Internal("failed to create generator_run".to_string()))
    }

    pub async fn get_generator_run(&self, run_id: &str) -> Result<Option<GeneratorRun>, AppError> {
        let run: Option<GeneratorRun> = self
            .db
            .select(("generator_run", run_id))
            .await
            .map_err(|e| AppError::Database(format!("get generator_run error: {e}")))?;
        Ok(run)
    }

    pub async fn create_snapshot(
        &self,
        scene_id: &str,
        name: &str,
        description: Option<String>,
    ) -> Result<SceneSnapshot, AppError> {
        let now = Datetime::from(chrono::Utc::now());
        let snapshot_key = format!("{}_{}", scene_id, ulid::Ulid::new());
        let objects = self
            .list_desired_objects(scene_id, false, None, None)
            .await?;
        let snapshot = SceneSnapshot {
            id: format!("scene_snapshot:{snapshot_key}"),
            scene: format!("scene:{scene_id}"),
            name: name.to_string(),
            description,
            revision: 1,
            groups: Vec::new(),
            objects,
            created_at: now,
        };

        let created: Option<SceneSnapshot> = self
            .db
            .create(("scene_snapshot", snapshot_key.as_str()))
            .content(snapshot.clone())
            .await
            .map_err(|e| AppError::Database(format!("create snapshot error: {e}")))?;

        created
            .map(|_| snapshot)
            .ok_or_else(|| AppError::Internal("failed to create snapshot".to_string()))
    }

    pub async fn list_snapshots(&self, scene_id: &str) -> Result<Vec<SceneSnapshot>, AppError> {
        let snapshots: Vec<SceneSnapshot> = self
            .db
            .query("SELECT * FROM scene_snapshot WHERE scene = $scene")
            .bind(("scene", format!("scene:{scene_id}")))
            .await
            .map_err(|e| AppError::Database(format!("list snapshots error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list snapshots parse error: {e}")))?;

        Ok(snapshots)
    }

    pub async fn restore_snapshot(
        &self,
        snapshot_id: &str,
        restore_mode: &str,
    ) -> Result<serde_json::Value, AppError> {
        if restore_mode != "replace_desired" {
            return Err(AppError::Validation(format!(
                "unsupported restore_mode {restore_mode}; expected replace_desired"
            )));
        }

        let snapshot_key = snapshot_id
            .strip_prefix("scene_snapshot:")
            .unwrap_or(snapshot_id);
        let snapshot: Option<SceneSnapshot> = self
            .db
            .select(("scene_snapshot", snapshot_key))
            .await
            .map_err(|e| AppError::Database(format!("select snapshot error: {e}")))?;
        let snapshot = snapshot
            .ok_or_else(|| AppError::NotFound(format!("snapshot {snapshot_id} not found")))?;

        let scene_id = snapshot
            .scene
            .strip_prefix("scene:")
            .unwrap_or(&snapshot.scene);
        let snapshot_ids: std::collections::HashSet<String> = snapshot
            .objects
            .iter()
            .map(|obj| obj.mcp_id.clone())
            .collect();

        let existing = self
            .list_desired_objects(scene_id, false, None, None)
            .await?;
        let mut tombstoned = 0usize;
        for obj in existing {
            if !snapshot_ids.contains(&obj.mcp_id) {
                self.mark_object_deleted(scene_id, &obj.mcp_id).await?;
                tombstoned += 1;
            }
        }

        let mut restored = 0usize;
        for mut obj in snapshot.objects {
            obj.scene = snapshot.scene.clone();
            obj.deleted = false;
            obj.sync_status = "pending".to_string();
            obj.last_applied_hash = None;
            self.upsert_object(&obj).await?;
            restored += 1;
        }

        Ok(serde_json::json!({
            "snapshot_id": format!("scene_snapshot:{snapshot_key}"),
            "scene_id": scene_id,
            "restore_mode": restore_mode,
            "restored_objects": restored,
            "tombstoned_objects": tombstoned,
            "summary": {
                "restored_objects": restored,
                "tombstoned_objects": tombstoned,
            },
        }))
    }

    pub async fn create_sync_run(
        &self,
        run_id: &str,
        scene_id: &str,
        mode: &str,
        status: &str,
    ) -> Result<(), AppError> {
        self.db
            .query(
                "CREATE type::thing($id) SET scene = $scene, mode = $mode, status = $status, summary = {}, started_at = time::now(), ended_at = NULL, error = NULL"
            )
            .bind(("id", format!("sync_run:{run_id}")))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("mode", mode.to_string()))
            .bind(("status", status.to_string()))
            .await
            .map_err(|e| AppError::Database(format!("create sync_run error: {e}")))?;

        Ok(())
    }

    pub async fn finish_sync_run(
        &self,
        run_id: &str,
        summary: &crate::sync::applier::SyncApplySummary,
    ) -> Result<(), AppError> {
        let now = Datetime::from(chrono::Utc::now());

        let summary_json = serde_json::to_value(summary)
            .map_err(|e| AppError::Internal(format!("serialize summary error: {e}")))?;

        let status = if summary.failed > 0 {
            "completed_with_errors"
        } else {
            "completed"
        };

        self.db
            .query(
                "UPDATE type::thing($id) SET status = $status, summary = $summary, ended_at = $now",
            )
            .bind(("id", format!("sync_run:{run_id}")))
            .bind(("status", status.to_string()))
            .bind(("summary", summary_json))
            .bind(("now", now))
            .await
            .map_err(|e| AppError::Database(format!("finish sync_run error: {e}")))?;

        Ok(())
    }

    pub async fn mark_object_synced(
        &self,
        scene_id: &str,
        mcp_id: &str,
        desired_hash: &str,
        unreal_actor_name: Option<&str>,
    ) -> Result<(), AppError> {
        let record_key = scene_object_record_key(scene_id, mcp_id);

        let updated: Option<SceneObject> = if let Some(name) = unreal_actor_name {
            self.db
                .update(("scene_object", record_key.as_str()))
                .merge(serde_json::json!({
                    "sync_status": "synced",
                    "last_applied_hash": desired_hash,
                    "unreal_actor_name": name,
                }))
                .await
                .map_err(|e| AppError::Database(format!("mark synced error: {e}")))?
        } else {
            self.db
                .update(("scene_object", record_key.as_str()))
                .merge(serde_json::json!({
                    "sync_status": "synced",
                    "last_applied_hash": desired_hash,
                }))
                .await
                .map_err(|e| AppError::Database(format!("mark synced error: {e}")))?
        };

        if updated.is_none() {
            return Err(AppError::NotFound(format!(
                "object {mcp_id} not found in scene {scene_id}"
            )));
        }

        Ok(())
    }

    /// Batch version: update sync_status for multiple objects in one query.
    pub async fn mark_objects_synced_batch(
        &self,
        scene_id: &str,
        items: &[(String, String, Option<String>)], // (mcp_id, desired_hash, unreal_actor_name)
    ) -> Result<(), AppError> {
        if items.is_empty() {
            return Ok(());
        }
        // Build a single query with multiple UPDATE statements.
        // Use type::thing() so record keys containing colons are parsed correctly.
        let mut query = String::new();
        for (i, (mcp_id, _, _)) in items.iter().enumerate() {
            let _record_key = scene_object_record_key(scene_id, mcp_id);
            query.push_str(&format!(
                "UPDATE type::thing('scene_object', $rk{i}) MERGE {{ sync_status: $status{i}, last_applied_hash: $hash{i} ",
            ));
            if items[i].2.is_some() {
                query.push_str(&format!(", unreal_actor_name: $name{i} "));
            }
            query.push_str("}; ");
        }
        let mut q = self.db.query(&query);
        for (i, (mcp_id, hash, actor_name)) in items.iter().enumerate() {
            let record_key = scene_object_record_key(scene_id, mcp_id);
            q = q.bind((format!("rk{i}"), record_key));
            q = q.bind((format!("status{i}"), "synced"));
            q = q.bind((format!("hash{i}"), hash.clone()));
            if let Some(name) = actor_name {
                q = q.bind((format!("name{i}"), name.clone()));
            }
        }
        q.await
            .map_err(|e| AppError::Database(format!("mark synced batch error: {e}")))?;
        Ok(())
    }

    pub async fn mark_object_deleted_applied(
        &self,
        scene_id: &str,
        mcp_id: &str,
    ) -> Result<(), AppError> {
        let record_key = scene_object_record_key(scene_id, mcp_id);

        let updated: Option<SceneObject> = self
            .db
            .update(("scene_object", record_key.as_str()))
            .merge(serde_json::json!({
                "sync_status": "synced",
            }))
            .await
            .map_err(|e| AppError::Database(format!("mark deleted applied error: {e}")))?;

        if updated.is_none() {
            return Err(AppError::NotFound(format!(
                "object {mcp_id} not found in scene {scene_id}"
            )));
        }

        Ok(())
    }

    pub async fn record_operation(
        &self,
        run_id: &str,
        scene_id: &str,
        mcp_id: &str,
        action: &str,
        status: &str,
        reason: &str,
    ) -> Result<(), AppError> {
        self.db
            .query(
                "CREATE scene_operation SET scene = $scene, sync_run = $sync_run, mcp_id = $mcp_id, action = $action, reason = $reason, status = $status, attempts = 1, created_at = time::now()"
            )
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("sync_run", format!("sync_run:{run_id}")))
            .bind(("mcp_id", mcp_id.to_string()))
            .bind(("action", action.to_string()))
            .bind(("reason", reason.to_string()))
            .bind(("status", status.to_string()))
            .await
            .map_err(|e| AppError::Database(format!("record operation error: {e}")))?;

        Ok(())
    }

    /// Batch version: insert multiple scene_operation records in one query.
    pub async fn record_operations_batch(
        &self,
        run_id: &str,
        scene_id: &str,
        ops: &[(String, String, String, String)], // (mcp_id, action, status, reason)
    ) -> Result<(), AppError> {
        if ops.is_empty() {
            return Ok(());
        }
        let records: Vec<serde_json::Value> = ops
            .iter()
            .map(|(mcp_id, action, status, reason)| {
                serde_json::json!({
                    "scene": format!("scene:{scene_id}"),
                    "sync_run": format!("sync_run:{run_id}"),
                    "mcp_id": mcp_id,
                    "action": action,
                    "reason": reason,
                    "status": status,
                    "attempts": 1,
                    "created_at": "time::now()"
                })
            })
            .collect();
        self.db
            .query("INSERT INTO scene_operation $records")
            .bind(("records", records))
            .await
            .map_err(|e| AppError::Database(format!("record operations batch error: {e}")))?;
        Ok(())
    }

    // ------------------------------------------------------------------
    // P3: Semantic repository methods
    // ------------------------------------------------------------------

    pub async fn upsert_entity(
        &self,
        scene_id: &str,
        entity_id: &str,
        kind: &str,
        name: &str,
        properties: serde_json::Value,
        tags: Vec<String>,
        mcp_ids: Vec<String>,
        metadata: serde_json::Value,
    ) -> Result<SceneEntity, AppError> {
        let record_key = format!("{scene_id}:{entity_id}");
        let updated: Option<SceneEntity> = self
            .db
            .query(
                "UPSERT type::thing($table, $key) MERGE { \
                    scene: $scene, \
                    entity_id: $entity_id, \
                    kind: $kind, \
                    name: $name, \
                    properties: $properties, \
                    tags: $tags, \
                    mcp_ids: $mcp_ids, \
                    metadata: $metadata, \
                    deleted: $deleted, \
                    revision: $revision, \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_entity"))
            .bind(("key", record_key.clone()))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("entity_id", entity_id.to_string()))
            .bind(("kind", kind.to_string()))
            .bind(("name", name.to_string()))
            .bind(("properties", properties))
            .bind(("tags", tags))
            .bind(("mcp_ids", mcp_ids))
            .bind(("metadata", metadata))
            .bind(("deleted", false))
            .bind(("revision", 1))
            .await
            .map_err(|e| AppError::Database(format!("upsert entity error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("upsert entity parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to upsert entity".to_string()))
    }

    pub async fn list_entities(
        &self,
        scene_id: &str,
        kind: Option<&str>,
    ) -> Result<Vec<SceneEntity>, AppError> {
        let mut query =
            "SELECT * FROM scene_entity WHERE scene = $scene AND deleted = false".to_string();
        if kind.is_some() {
            query.push_str(" AND kind = $kind");
        }
        let mut q = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene_id}")));
        if let Some(k) = kind {
            q = q.bind(("kind", k.to_string()));
        }
        let entities: Vec<SceneEntity> = q
            .await
            .map_err(|e| AppError::Database(format!("list entities error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list entities parse error: {e}")))?;
        Ok(entities)
    }

    pub async fn update_entity_transform(
        &self,
        scene_id: &str,
        entity_id: &str,
        properties: serde_json::Value,
    ) -> Result<SceneEntity, AppError> {
        let record_key = format!("{scene_id}:{entity_id}");
        let updated: Option<SceneEntity> = self
            .db
            .query(
                "UPDATE type::thing($table, $key) MERGE { \
                    properties: $properties, \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_entity"))
            .bind(("key", record_key))
            .bind(("properties", properties))
            .await
            .map_err(|e| AppError::Database(format!("update entity transform error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("update entity transform parse error: {e}")))?;

        updated.ok_or_else(|| {
            AppError::NotFound(format!("entity {entity_id} not found in scene {scene_id}"))
        })
    }

    pub async fn upsert_relation(
        &self,
        scene_id: &str,
        relation_id: &str,
        source_entity_id: &str,
        target_entity_id: &str,
        relation_type: &str,
        properties: serde_json::Value,
        metadata: serde_json::Value,
    ) -> Result<SceneRelation, AppError> {
        let record_key = format!("{scene_id}:{relation_id}");
        let updated: Option<SceneRelation> = self
            .db
            .query(
                "UPSERT type::thing($table, $key) MERGE { \
                    scene: $scene, \
                    relation_id: $relation_id, \
                    source_entity_id: $source_entity_id, \
                    target_entity_id: $target_entity_id, \
                    relation_type: $relation_type, \
                    properties: $properties, \
                    metadata: $metadata, \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_relation"))
            .bind(("key", record_key.clone()))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("relation_id", relation_id.to_string()))
            .bind(("source_entity_id", source_entity_id.to_string()))
            .bind(("target_entity_id", target_entity_id.to_string()))
            .bind(("relation_type", relation_type.to_string()))
            .bind(("properties", properties))
            .bind(("metadata", metadata))
            .await
            .map_err(|e| AppError::Database(format!("upsert relation error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("upsert relation parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to upsert relation".to_string()))
    }

    pub async fn list_relations(
        &self,
        scene_id: &str,
        relation_type: Option<&str>,
    ) -> Result<Vec<SceneRelation>, AppError> {
        let mut query = "SELECT * FROM scene_relation WHERE scene = $scene".to_string();
        if relation_type.is_some() {
            query.push_str(" AND relation_type = $relation_type");
        }
        let mut q = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene_id}")));
        if let Some(rt) = relation_type {
            q = q.bind(("relation_type", rt.to_string()));
        }
        let relations: Vec<SceneRelation> = q
            .await
            .map_err(|e| AppError::Database(format!("list relations error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list relations parse error: {e}")))?;
        Ok(relations)
    }

    pub async fn upsert_asset(
        &self,
        scene_id: &str,
        asset_id: &str,
        kind: &str,
        status: &str,
        fallback: &str,
        semantic_tags: Vec<String>,
        quality: &str,
        variants: serde_json::Value,
        metadata: serde_json::Value,
    ) -> Result<SceneAsset, AppError> {
        let record_key = format!("{scene_id}:{asset_id}");
        let updated: Option<SceneAsset> = self
            .db
            .query(
                "UPSERT type::thing($table, $key) MERGE { \
                    scene: $scene, \
                    asset_id: $asset_id, \
                    kind: $kind, \
                    status: $status, \
                    fallback: $fallback, \
                    semantic_tags: $semantic_tags, \
                    quality: $quality, \
                    variants: $variants, \
                    metadata: $metadata, \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_asset"))
            .bind(("key", record_key.clone()))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("asset_id", asset_id.to_string()))
            .bind(("kind", kind.to_string()))
            .bind(("status", status.to_string()))
            .bind(("fallback", fallback.to_string()))
            .bind(("semantic_tags", semantic_tags))
            .bind(("quality", quality.to_string()))
            .bind(("variants", variants))
            .bind(("metadata", metadata))
            .await
            .map_err(|e| AppError::Database(format!("upsert asset error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("upsert asset parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to upsert asset".to_string()))
    }

    pub async fn list_assets(
        &self,
        scene_id: &str,
        kind: Option<&str>,
    ) -> Result<Vec<SceneAsset>, AppError> {
        let mut query = "SELECT * FROM scene_asset WHERE scene = $scene".to_string();
        if kind.is_some() {
            query.push_str(" AND kind = $kind");
        }
        let mut q = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene_id}")));
        if let Some(k) = kind {
            q = q.bind(("kind", k.to_string()));
        }
        let assets: Vec<SceneAsset> = q
            .await
            .map_err(|e| AppError::Database(format!("list assets error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list assets parse error: {e}")))?;
        Ok(assets)
    }

    // ------------------------------------------------------------------
    // P6: Component, Blueprint, Realization CRUD
    // ------------------------------------------------------------------

    pub async fn upsert_component(
        &self,
        scene_id: &str,
        entity_id: &str,
        component_type: &str,
        name: &str,
        properties: serde_json::Value,
        metadata: serde_json::Value,
    ) -> Result<SceneComponent, AppError> {
        let record_key = format!("{scene_id}:{entity_id}:{component_type}:{name}");
        let updated: Option<SceneComponent> = self
            .db
            .query(
                "UPSERT type::thing($table, $key) MERGE { \
                    scene: $scene, \
                    entity_id: $entity_id, \
                    component_type: $component_type, \
                    name: $name, \
                    properties: $properties, \
                    metadata: $metadata, \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_component"))
            .bind(("key", record_key.clone()))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("entity_id", entity_id.to_string()))
            .bind(("component_type", component_type.to_string()))
            .bind(("name", name.to_string()))
            .bind(("properties", properties))
            .bind(("metadata", metadata))
            .await
            .map_err(|e| AppError::Database(format!("upsert component error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("upsert component parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to upsert component".to_string()))
    }

    pub async fn list_components(
        &self,
        scene_id: &str,
        entity_id: Option<&str>,
        component_type: Option<&str>,
    ) -> Result<Vec<SceneComponent>, AppError> {
        let mut query =
            "SELECT * FROM scene_component WITH NOINDEX WHERE scene = $scene".to_string();
        if entity_id.is_some() {
            query.push_str(" AND entity_id = $entity_id");
        }
        if component_type.is_some() {
            query.push_str(" AND component_type = $component_type");
        }
        let mut q = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene_id}")));
        if let Some(eid) = entity_id {
            q = q.bind(("entity_id", eid.to_string()));
        }
        if let Some(ct) = component_type {
            q = q.bind(("component_type", ct.to_string()));
        }
        let components: Vec<SceneComponent> = q
            .await
            .map_err(|e| AppError::Database(format!("list components error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list components parse error: {e}")))?;
        Ok(components)
    }

    pub async fn delete_component(
        &self,
        scene_id: &str,
        entity_id: &str,
        component_type: &str,
        name: &str,
    ) -> Result<(), AppError> {
        let record_key = format!("{scene_id}:{entity_id}:{component_type}:{name}");
        self.db
            .query("DELETE type::thing($table, $key) WHERE scene = $scene")
            .bind(("table", "scene_component"))
            .bind(("key", record_key))
            .bind(("scene", format!("scene:{scene_id}")))
            .await
            .map_err(|e| AppError::Database(format!("delete component error: {e}")))?;
        Ok(())
    }

    pub async fn upsert_blueprint(
        &self,
        scene_id: &str,
        blueprint_id: &str,
        class_name: &str,
        parent_class: &str,
        components: Vec<serde_json::Value>,
        variables: Vec<serde_json::Value>,
        metadata: serde_json::Value,
    ) -> Result<SceneBlueprint, AppError> {
        let record_key = format!("{scene_id}:{blueprint_id}");
        let updated: Option<SceneBlueprint> = self
            .db
            .query(
                "UPSERT type::thing($table, $key) MERGE { \
                    scene: $scene, \
                    blueprint_id: $blueprint_id, \
                    class_name: $class_name, \
                    parent_class: $parent_class, \
                    components: $components, \
                    variables: $variables, \
                    metadata: $metadata, \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_blueprint"))
            .bind(("key", record_key.clone()))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("blueprint_id", blueprint_id.to_string()))
            .bind(("class_name", class_name.to_string()))
            .bind(("parent_class", parent_class.to_string()))
            .bind(("components", components))
            .bind(("variables", variables))
            .bind(("metadata", metadata))
            .await
            .map_err(|e| AppError::Database(format!("upsert blueprint error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("upsert blueprint parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to upsert blueprint".to_string()))
    }

    pub async fn list_blueprints(
        &self,
        scene_id: &str,
        class_name: Option<&str>,
    ) -> Result<Vec<SceneBlueprint>, AppError> {
        let mut query = "SELECT * FROM scene_blueprint WHERE scene = $scene".to_string();
        if class_name.is_some() {
            query.push_str(" AND class_name = $class_name");
        }
        let mut q = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene_id}")));
        if let Some(cn) = class_name {
            q = q.bind(("class_name", cn.to_string()));
        }
        let blueprints: Vec<SceneBlueprint> = q
            .await
            .map_err(|e| AppError::Database(format!("list blueprints error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list blueprints parse error: {e}")))?;
        Ok(blueprints)
    }

    pub async fn delete_blueprint(
        &self,
        scene_id: &str,
        blueprint_id: &str,
    ) -> Result<(), AppError> {
        let record_key = format!("{scene_id}:{blueprint_id}");
        self.db
            .query("DELETE type::thing($table, $key) WHERE scene = $scene")
            .bind(("table", "scene_blueprint"))
            .bind(("key", record_key))
            .bind(("scene", format!("scene:{scene_id}")))
            .await
            .map_err(|e| AppError::Database(format!("delete blueprint error: {e}")))?;
        Ok(())
    }

    pub async fn upsert_realization(
        &self,
        scene_id: &str,
        entity_id: &str,
        policy: &str,
        status: &str,
        unreal_actor_name: Option<String>,
        metadata: serde_json::Value,
    ) -> Result<SceneRealization, AppError> {
        let record_key = format!("{scene_id}:{entity_id}:{policy}");
        let updated: Option<SceneRealization> = self
            .db
            .query(
                "UPSERT type::thing($table, $key) MERGE { \
                    scene: $scene, \
                    entity_id: $entity_id, \
                    policy: $policy, \
                    status: $status, \
                    unreal_actor_name: $unreal_actor_name, \
                    metadata: $metadata, \
                    updated_at: time::now() \
                 }",
            )
            .bind(("table", "scene_realization"))
            .bind(("key", record_key.clone()))
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("entity_id", entity_id.to_string()))
            .bind(("policy", policy.to_string()))
            .bind(("status", status.to_string()))
            .bind(("unreal_actor_name", unreal_actor_name))
            .bind(("metadata", metadata))
            .await
            .map_err(|e| AppError::Database(format!("upsert realization error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("upsert realization parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to upsert realization".to_string()))
    }

    pub async fn list_realizations(
        &self,
        scene_id: &str,
        entity_id: Option<&str>,
        policy: Option<&str>,
    ) -> Result<Vec<SceneRealization>, AppError> {
        let mut query =
            "SELECT * FROM scene_realization WITH NOINDEX WHERE scene = $scene".to_string();
        if entity_id.is_some() {
            query.push_str(" AND entity_id = $entity_id");
        }
        if policy.is_some() {
            query.push_str(" AND policy = $policy");
        }
        let mut q = self
            .db
            .query(query)
            .bind(("scene", format!("scene:{scene_id}")));
        if let Some(eid) = entity_id {
            q = q.bind(("entity_id", eid.to_string()));
        }
        if let Some(p) = policy {
            q = q.bind(("policy", p.to_string()));
        }
        let realizations: Vec<SceneRealization> = q
            .await
            .map_err(|e| AppError::Database(format!("list realizations error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("list realizations parse error: {e}")))?;
        Ok(realizations)
    }

    pub async fn find_realization_for_entity(
        &self,
        scene_id: &str,
        entity_id: &str,
    ) -> Result<Option<SceneRealization>, AppError> {
        let realizations: Vec<SceneRealization> = self
            .db
            .query("SELECT * FROM scene_realization WITH NOINDEX WHERE scene = $scene AND entity_id = $entity_id LIMIT 1")
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("entity_id", entity_id.to_string()))
            .await
            .map_err(|e| AppError::Database(format!("find realization for entity error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("find realization for entity parse error: {e}")))?;
        Ok(realizations.into_iter().next())
    }

    /// Batch version: fetch all realizations for the given entity IDs.
    pub async fn find_realizations_for_entities(
        &self,
        scene_id: &str,
        entity_ids: &[&str],
    ) -> Result<std::collections::HashMap<String, SceneRealization>, AppError> {
        if entity_ids.is_empty() {
            return Ok(std::collections::HashMap::new());
        }
        let ids: Vec<String> = entity_ids.iter().map(|s| s.to_string()).collect();
        let realizations: Vec<SceneRealization> = self
            .db
            .query(
                "SELECT * FROM scene_realization WITH NOINDEX WHERE scene = $scene AND entity_id IN $entity_ids",
            )
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("entity_ids", ids))
            .await
            .map_err(|e| AppError::Database(format!("find realizations for entities error: {e}")))?
            .take(0)
            .map_err(|e| {
                AppError::Database(format!("find realizations for entities parse error: {e}"))
            })?;
        let mut map = std::collections::HashMap::new();
        for rz in realizations {
            map.insert(rz.entity_id.clone(), rz);
        }
        Ok(map)
    }

    pub async fn find_entity_by_mcp_id(
        &self,
        scene_id: &str,
        mcp_id: &str,
    ) -> Result<Option<SceneEntity>, AppError> {
        let entities: Vec<SceneEntity> = self
            .db
            .query("SELECT * FROM scene_entity WHERE scene = $scene AND deleted = false AND mcp_ids CONTAINS $mcp_id LIMIT 1")
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("mcp_id", mcp_id.to_string()))
            .await
            .map_err(|e| AppError::Database(format!("find entity by mcp_id error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("find entity by mcp_id parse error: {e}")))?;
        Ok(entities.into_iter().next())
    }

    /// Batch version: fetch all entities whose mcp_ids overlap with the given set.
    pub async fn find_entities_by_mcp_ids(
        &self,
        scene_id: &str,
        mcp_ids: &[&str],
    ) -> Result<std::collections::HashMap<String, SceneEntity>, AppError> {
        if mcp_ids.is_empty() {
            return Ok(std::collections::HashMap::new());
        }
        let ids: Vec<String> = mcp_ids.iter().map(|s| s.to_string()).collect();
        let entities: Vec<SceneEntity> = self
            .db
            .query("SELECT * FROM scene_entity WHERE scene = $scene AND deleted = false AND mcp_ids CONTAINSANY $mcp_ids")
            .bind(("scene", format!("scene:{scene_id}")))
            .bind(("mcp_ids", ids))
            .await
            .map_err(|e| AppError::Database(format!("find entities by mcp_ids error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("find entities by mcp_ids parse error: {e}")))?;
        let mut map = std::collections::HashMap::new();
        for e in entities {
            for mid in &e.mcp_ids {
                map.insert(mid.clone(), e.clone());
            }
        }
        Ok(map)
    }

    pub async fn update_realization_status(
        &self,
        scene_id: &str,
        entity_id: &str,
        policy: &str,
        status: &str,
    ) -> Result<SceneRealization, AppError> {
        let record_key = format!("{scene_id}:{entity_id}:{policy}");
        let updated: Option<SceneRealization> = self
            .db
            .query(
                "UPDATE type::thing($table, $key) MERGE { status: $status, updated_at: time::now() }",
            )
            .bind(("table", "scene_realization"))
            .bind(("key", record_key))
            .bind(("status", status.to_string()))
            .await
            .map_err(|e| AppError::Database(format!("update realization status error: {e}")))?
            .take(0)
            .map_err(|e| AppError::Database(format!("update realization status parse error: {e}")))?;

        updated.ok_or_else(|| AppError::Internal("failed to update realization status".to_string()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn record_key_strips_scene_prefix() {
        assert_eq!(scene_object_record_key("scene:main", "obj_1"), "main:obj_1");
    }

    #[test]
    fn record_key_bare_scene_id_unchanged() {
        assert_eq!(scene_object_record_key("main", "obj_1"), "main:obj_1");
    }

    #[test]
    fn record_key_consistent_across_prefix_variants() {
        let with_prefix = scene_object_record_key("scene:my_scene", "actor_42");
        let without_prefix = scene_object_record_key("my_scene", "actor_42");
        assert_eq!(with_prefix, without_prefix);
    }

    #[test]
    fn record_key_strips_only_first_prefix() {
        // strip_prefix removes only the first occurrence
        assert_eq!(
            scene_object_record_key("scene:scene:x", "obj"),
            "scene:x:obj"
        );
    }
}
