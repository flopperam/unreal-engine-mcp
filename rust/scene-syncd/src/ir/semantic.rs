use crate::ir::material::MaterialPlan;
use crate::ir::style::WorldStyleProfile;
use serde::{Deserialize, Serialize};

/// Typed semantic IR derived from denormalized SceneObjects.
/// Used by graph-build, anchor inference, and constraint extraction passes.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SemanticScene {
    pub scene_id: String,
    pub entities: Vec<SemanticEntity>,
    pub metadata: serde_json::Value,
    /// World style profile driving material coherence decisions.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub style_profile: Option<WorldStyleProfile>,
    /// Material plans for each object, computed by MaterialCoherencePass.
    #[serde(default)]
    pub material_plans: Vec<MaterialPlan>,
}

impl SemanticScene {
    pub fn new(scene_id: String) -> Self {
        Self {
            scene_id,
            entities: Vec::new(),
            metadata: serde_json::json!({}),
            style_profile: None,
            material_plans: Vec::new(),
        }
    }

    /// Build a SemanticScene from already-denormalized SceneObjects.
    /// Each object is expected to carry `layout_entity:{id}` and `layout_kind:{kind}` tags.
    pub fn from_objects(scene_id: String, objects: &[crate::domain::SceneObject]) -> Self {
        use std::collections::HashMap;

        let mut entity_map: HashMap<String, SemanticEntity> = HashMap::new();

        for obj in objects {
            if obj.deleted {
                continue;
            }
            let entity_id = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_entity:"))
                .unwrap_or(&obj.mcp_id)
                .to_string();

            let kind_str = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_kind:"))
                .unwrap_or("");

            let kind = SemanticKind::from_str(kind_str);

            let entry = entity_map
                .entry(entity_id.clone())
                .or_insert_with(|| SemanticEntity {
                    entity_id: entity_id.clone(),
                    kind: kind.clone(),
                    name: obj.desired_name.clone(),
                    properties: obj.metadata.clone(),
                    tags: obj.tags.clone(),
                    metadata: serde_json::json!({
                        "mcp_id": obj.mcp_id,
                        "actor_type": obj.actor_type,
                    }),
                });

            // If this object refines the kind, update it.
            if kind != SemanticKind::Unknown(String::new()) {
                entry.kind = kind;
            }
        }

        Self {
            scene_id,
            entities: entity_map.into_values().collect(),
            metadata: serde_json::json!({}),
            style_profile: None,
            material_plans: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SemanticEntity {
    pub entity_id: String,
    pub kind: SemanticKind,
    pub name: String,
    pub properties: serde_json::Value,
    pub tags: Vec<String>,
    pub metadata: serde_json::Value,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum SemanticKind {
    Keep,
    Tower,
    CurtainWall,
    Gatehouse,
    Bridge,
    Moat,
    Ground,
    Road,
    District,
    PatrolRoute,
    Unknown(String),
}

impl SemanticKind {
    pub fn from_str(s: &str) -> Self {
        match s {
            "keep" => Self::Keep,
            "tower" => Self::Tower,
            "curtain_wall" => Self::CurtainWall,
            "gatehouse" => Self::Gatehouse,
            "bridge" => Self::Bridge,
            "moat" => Self::Moat,
            "ground" => Self::Ground,
            "road" => Self::Road,
            "district" => Self::District,
            "patrol_route" => Self::PatrolRoute,
            other => Self::Unknown(other.to_string()),
        }
    }

    pub fn as_str(&self) -> &str {
        match self {
            Self::Keep => "keep",
            Self::Tower => "tower",
            Self::CurtainWall => "curtain_wall",
            Self::Gatehouse => "gatehouse",
            Self::Bridge => "bridge",
            Self::Moat => "moat",
            Self::Ground => "ground",
            Self::Road => "road",
            Self::District => "district",
            Self::PatrolRoute => "patrol_route",
            Self::Unknown(s) => s.as_str(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_obj(mcp_id: &str, entity_tag: &str, kind_tag: &str) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: mcp_id.to_string(),
            desired_name: mcp_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: Transform {
                location: Vec3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                },
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
            tags: vec![
                format!("layout_entity:{}", entity_tag),
                format!("layout_kind:{}", kind_tag),
            ],
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
    fn semantic_scene_from_objects() {
        let objs = vec![
            make_obj("t1", "tower_1", "tower"),
            make_obj("w1", "wall_1", "curtain_wall"),
        ];
        let scene = SemanticScene::from_objects("test".to_string(), &objs);
        assert_eq!(scene.entities.len(), 2);
        assert!(scene.entities.iter().any(|e| e.kind == SemanticKind::Tower));
        assert!(scene
            .entities
            .iter()
            .any(|e| e.kind == SemanticKind::CurtainWall));
    }

    #[test]
    fn skips_deleted_objects() {
        let mut obj = make_obj("t1", "tower_1", "tower");
        obj.deleted = true;
        let scene = SemanticScene::from_objects("test".to_string(), &[obj]);
        assert!(scene.entities.is_empty());
    }

    #[test]
    fn unknown_kind_preserved() {
        let objs = vec![make_obj("x1", "x_1", "alien_spaceship")];
        let scene = SemanticScene::from_objects("test".to_string(), &objs);
        assert_eq!(
            scene.entities[0].kind,
            SemanticKind::Unknown("alien_spaceship".to_string())
        );
    }
}
