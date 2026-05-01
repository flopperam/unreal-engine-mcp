use crate::compiler::context::CompilerContext;
use crate::compiler::passes::Pass;
use crate::domain::SceneObject;
use crate::error::AppError;
use crate::ir::density::{DensityInput, DistanceBand};
use crate::ir::instance_set::group_into_instance_sets;
use crate::ir::lod::RenderCostEstimate;
use crate::ir::render_plan::{RenderDecision, RenderItem, RenderMode, RenderPlan};
use std::collections::HashMap;

/// Density / LOD planner pass.
///
/// Reads `context.objects`, computes per-object density inputs, decides
/// `RenderMode` for each object, builds a `RenderPlan`, and populates
/// `context.render_plan` + `context.instance_sets`.
pub struct DensityPlannerPass;

impl Pass for DensityPlannerPass {
    fn name(&self) -> &'static str {
        "density_lod"
    }

    fn run(&self,
        ctx: &mut CompilerContext,
    ) -> Result<(), AppError> {
        if ctx.objects.is_empty() {
            ctx.render_plan = Some(RenderPlan {
                scene_id: ctx.scene_id.clone(),
                items: Vec::new(),
                decisions: Vec::new(),
                estimated_cost: RenderCostEstimate {
                    actor_cost: 0.0,
                    instance_cost: 0.0,
                    total_draw_calls: 0,
                },
            });
            ctx.instance_sets = Vec::new();
            return Ok(());
        }

        // --- Step 1: Compute repetition counts per (mesh, material, kind) ---
        let mut repetition_counts: HashMap<(String, Option<String>, String), usize> =
            HashMap::new();
        for obj in &ctx.objects {
            if obj.deleted {
                continue;
            }
            let key = density_key(obj);
            *repetition_counts.entry(key).or_insert(0) += 1;
        }

        // --- Step 2: Build DensityInput and RenderDecision for each object ---
        let mut decisions: Vec<RenderDecision> = Vec::new();
        let mut items: Vec<RenderItem> = Vec::new();

        for obj in &ctx.objects {
            if obj.deleted {
                continue;
            }

            let density = build_density_input(obj, &repetition_counts);
            let decision = classify_object(obj.mcp_id.clone(), &density);

            let item = match decision.decision {
                RenderMode::Actor => RenderItem::Actor(obj.clone()),
                RenderMode::InstanceSet => RenderItem::Actor(obj.clone()), // placeholder: converted later
                RenderMode::HlodProxy => RenderItem::Actor(obj.clone()),   // Phase 5+
                RenderMode::Decal => RenderItem::Actor(obj.clone()),       // Phase 7+
                RenderMode::Omit => continue,                             // dropped
            };

            decisions.push(decision);
            items.push(item);
        }

        // --- Step 3: Convert InstanceSet-classified objects into InstanceSets ---
        let instance_set_mcp_ids: std::collections::HashSet<String> = decisions
            .iter()
            .filter(|d| matches!(d.decision, RenderMode::InstanceSet))
            .map(|d| d.source_mcp_id.clone())
            .collect();

        let instance_set_objects: Vec<SceneObject> = items
            .iter()
            .filter_map(|item| match item {
                RenderItem::Actor(obj) if instance_set_mcp_ids.contains(&obj.mcp_id) => {
                    Some(obj.clone())
                }
                _ => None,
            })
            .collect();

        let instance_sets =
            group_into_instance_sets(&instance_set_objects, Some(&decisions));

        // Replace Actor placeholders with actual InstanceSet items
        let mut final_items: Vec<RenderItem> = Vec::new();
        let mut used_in_set: std::collections::HashSet<String> =
            std::collections::HashSet::new();

        // Add InstanceSets first
        for set in &instance_sets {
            final_items.push(RenderItem::InstanceSet(set.clone()));
            for mcp_id in &set.source_mcp_ids {
                used_in_set.insert(mcp_id.clone());
            }
        }

        // Add remaining Actors
        for item in &items {
            if let RenderItem::Actor(obj) = item {
                if !used_in_set.contains(&obj.mcp_id) {
                    final_items.push(RenderItem::Actor(obj.clone()));
                }
            }
        }

        // --- Step 4: Estimate cost ---
        let actor_count = final_items
            .iter()
            .filter(|i| matches!(i, RenderItem::Actor(_)))
            .count();
        let instance_count: usize = instance_sets
            .iter()
            .map(|s| s.transforms.len())
            .sum();
        let estimated_cost = RenderCostEstimate {
            actor_cost: actor_count as f32,
            instance_cost: instance_count as f32 * 0.01,
            total_draw_calls: actor_count + instance_sets.len(),
        };

        let render_plan = RenderPlan {
            scene_id: ctx.scene_id.clone(),
            items: final_items,
            decisions,
            estimated_cost,
        };

        ctx.render_plan = Some(render_plan);
        ctx.instance_sets = instance_sets;

        Ok(())
    }
}

/// Build a (mesh, material, kind) key for density grouping.
fn density_key(obj: &SceneObject) -> (String, Option<String>, String) {
    let mesh = obj
        .asset_ref
        .get("mesh")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    let material = obj
        .asset_ref
        .get("material")
        .and_then(|v| v.as_str())
        .map(|s| s.to_string());
    let kind = obj
        .tags
        .iter()
        .find_map(|t| t.strip_prefix("layout_kind:"))
        .unwrap_or("unknown")
        .to_string();
    (mesh, material, kind)
}

/// Build DensityInput for a single object.
fn build_density_input(
    obj: &SceneObject,
    repetition_counts: &HashMap<(String, Option<String>, String), usize>,
) -> DensityInput {
    let key = density_key(obj);
    let repetition_count = *repetition_counts.get(&key).unwrap_or(&0);

    let semantic_kind = obj
        .tags
        .iter()
        .find_map(|t| t.strip_prefix("layout_kind:"))
        .unwrap_or("unknown")
        .to_string();

    let importance_score = obj
        .metadata
        .get("importance_score")
        .and_then(|v| v.as_f64())
        .unwrap_or(1.0) as f32;

    let visible_priority = obj
        .metadata
        .get("visible_priority")
        .and_then(|v| v.as_f64())
        .unwrap_or(1.0) as f32;

    let interaction_required = obj
        .tags
        .iter()
        .any(|t| t.starts_with("interaction:") || t == "gameplay:interactive");

    let collision_required = obj
        .tags
        .iter()
        .any(|t| t.starts_with("collision:") || t == "physics:solid")
        || obj
            .physics
            .get("collision_enabled")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);

    let distance_band = match obj
        .metadata
        .get("distance_band")
        .and_then(|v| v.as_str())
    {
        Some("near") => DistanceBand::Near,
        Some("mid") => DistanceBand::Mid,
        Some("far") => DistanceBand::Far,
        Some("distant") => DistanceBand::Distant,
        _ => DistanceBand::Near,
    };

    DensityInput {
        semantic_kind,
        importance_score,
        distance_band,
        repetition_count,
        visible_priority,
        interaction_required,
        collision_required,
    }
}

/// Classify a single object into a RenderMode based on its density input.
fn classify_object(source_mcp_id: String, density: &DensityInput) -> RenderDecision {
    let decision: RenderMode;
    let reason: String;

    if density.interaction_required || density.collision_required {
        decision = RenderMode::Actor;
        reason = "interaction or collision required".to_string();
    } else if density.repetition_count >= 50
        && !density.interaction_required
    {
        decision = RenderMode::InstanceSet;
        reason = format!(
            "repetition_count={} >= 50, no interaction",
            density.repetition_count
        );
    } else if matches!(density.distance_band, DistanceBand::Distant) {
        decision = RenderMode::Omit;
        reason = "distance_band=Distant".to_string();
    } else if density.visible_priority < 0.1 {
        decision = RenderMode::Omit;
        reason = format!("visible_priority={} < 0.1", density.visible_priority);
    } else {
        decision = RenderMode::Actor;
        reason = "default Actor fallback".to_string();
    }

    RenderDecision {
        source_mcp_id,
        decision,
        importance_score: density.importance_score,
        semantic_density: density.repetition_count as f32 / 100.0,
        visual_density: density.visible_priority,
        reason,
    }
}
