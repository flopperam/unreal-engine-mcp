use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::error::AppError;
use petgraph::graph::{DiGraph, NodeIndex};
use std::collections::HashMap;

/// Node in the semantic entity/relation graph.
#[derive(Debug, Clone)]
pub struct GraphNode {
    pub entity_id: String,
    pub kind: String,
}

/// Build a directed graph of semantic entities and relations.
/// This enables future analysis such as cyclic dependency detection and orphan detection.
pub struct GraphBuildPass;

impl Pass for GraphBuildPass {
    fn name(&self) -> &'static str {
        "graph_build"
    }

    fn run(&self, ctx: &mut CompilerContext) -> Result<(), AppError> {
        // Extract entity IDs from scene object tags (layout_kind:... and layout_entity:...)
        let mut entity_ids: Vec<String> = Vec::new();
        let mut entity_kinds: HashMap<String, String> = HashMap::new();

        for obj in &ctx.objects {
            if let Some(entity_id) = obj
                .tags
                .iter()
                .find_map(|t| t.strip_prefix("layout_entity:"))
            {
                let id = entity_id.to_string();
                if !entity_ids.contains(&id) {
                    entity_ids.push(id.clone());
                }
                if let Some(kind) = obj.tags.iter().find_map(|t| t.strip_prefix("layout_kind:")) {
                    entity_kinds.insert(id, kind.to_string());
                }
            }
        }

        if entity_ids.len() >= 2 {
            let mut graph = DiGraph::<GraphNode, ()>::new();
            let mut index_by_id: HashMap<String, NodeIndex> = HashMap::new();

            for id in &entity_ids {
                let kind = entity_kinds.get(id).cloned().unwrap_or_default();
                let idx = graph.add_node(GraphNode {
                    entity_id: id.clone(),
                    kind,
                });
                index_by_id.insert(id.clone(), idx);
            }

            // Future: add edges from relation data stored in metadata or passed separately.
            // For now we just verify the graph builds successfully.

            // Orphan detection: nodes with no edges are flagged as info (not error)
            for node_idx in graph.node_indices() {
                let has_edges = graph.edges(node_idx).next().is_some()
                    || graph
                        .neighbors_directed(node_idx, petgraph::Direction::Incoming)
                        .next()
                        .is_some();
                if !has_edges && graph.node_count() > 1 {
                    let node = &graph[node_idx];
                    ctx.add_diagnostics(vec![crate::validation::diagnostic::Diagnostic::info(
                        "GRAPH_ORPHAN",
                        format!(
                            "Entity '{}' (kind={}) has no relations to other entities",
                            node.entity_id, node.kind
                        ),
                    )
                    .with_entity_id(node.entity_id.clone())]);
                }
            }
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::SceneObject;
    use serde_json::json;

    fn make_object(entity_id: &str, kind: &str) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: format!("{entity_id}_obj"),
            desired_name: entity_id.to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: crate::domain::Transform::default(),
            visual: json!({}),
            physics: json!({}),
            tags: vec![
                format!("layout_entity:{entity_id}"),
                format!("layout_kind:{kind}"),
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
    fn graph_build_detects_orphan() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![
            make_object("tower_a", "tower"),
            make_object("tower_b", "tower"),
        ];
        let pass = GraphBuildPass;
        pass.run(&mut ctx).unwrap();
        assert!(
            ctx.diagnostics.iter().any(|d| d.code == "GRAPH_ORPHAN"),
            "expected orphan info for disconnected entities"
        );
    }

    #[test]
    fn graph_build_no_warning_for_single_entity() {
        let mut ctx = CompilerContext::new("test".to_string());
        ctx.objects = vec![make_object("keep", "keep")];
        let pass = GraphBuildPass;
        pass.run(&mut ctx).unwrap();
        assert!(ctx.diagnostics.is_empty());
    }
}
