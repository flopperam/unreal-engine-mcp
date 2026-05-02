use crate::domain::SceneObject;
use crate::geom::footprint::Footprint2;
use crate::validation::diagnostic::Diagnostic;

pub trait ValidationRule: Send + Sync {
    fn code(&self) -> &'static str;
    fn validate(&self, objects: &[SceneObject], footprints: &[Footprint2]) -> Vec<Diagnostic>;
}

pub struct ValidationEngine {
    rules: Vec<Box<dyn ValidationRule>>,
}

impl Default for ValidationEngine {
    fn default() -> Self {
        Self::new()
    }
}

impl ValidationEngine {
    pub fn new() -> Self {
        Self { rules: Vec::new() }
    }

    pub fn add_rule(&mut self, rule: Box<dyn ValidationRule>) {
        self.rules.push(rule);
    }

    pub fn validate(&self, objects: &[SceneObject], footprints: &[Footprint2]) -> Vec<Diagnostic> {
        let mut all = Vec::new();
        for rule in &self.rules {
            let mut results = rule.validate(objects, footprints);
            all.append(&mut results);
        }
        all
    }

    pub fn has_errors(&self, diagnostics: &[Diagnostic]) -> bool {
        diagnostics
            .iter()
            .any(|d| matches!(d.severity, crate::validation::diagnostic::Severity::Error))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::SceneObject;
    use crate::validation::diagnostic::Diagnostic;

    struct DummyRule;

    impl ValidationRule for DummyRule {
        fn code(&self) -> &'static str {
            "DUMMY"
        }

        fn validate(
            &self,
            _objects: &[SceneObject],
            _footprints: &[crate::geom::footprint::Footprint2],
        ) -> Vec<Diagnostic> {
            vec![Diagnostic::warning("DUMMY", "always warns".to_string())]
        }
    }

    #[test]
    fn engine_collects_diagnostics() {
        let mut engine = ValidationEngine::new();
        engine.add_rule(Box::new(DummyRule));
        let d = engine.validate(&[], &[]);
        assert_eq!(d.len(), 1);
        assert_eq!(d[0].code, "DUMMY");
    }

    #[test]
    fn has_errors_detects_error() {
        let engine = ValidationEngine::new();
        let diags = vec![
            Diagnostic::error("E1", "err".to_string()),
            Diagnostic::warning("W1", "warn".to_string()),
        ];
        assert!(engine.has_errors(&diags));
    }

    #[test]
    fn no_errors_when_only_warnings() {
        let engine = ValidationEngine::new();
        let diags = vec![Diagnostic::warning("W1", "warn".to_string())];
        assert!(!engine.has_errors(&diags));
    }

    use crate::domain::{Rotator, Transform, Vec3};
    use proptest::prelude::*;
    use serde_json::json;

    fn arb_scene_object() -> impl Strategy<Value = SceneObject> {
        (
            any::<f64>(),
            any::<f64>(),
            any::<f64>(),
            any::<f64>(),
            any::<f64>(),
            any::<f64>(),
        )
            .prop_map(|(x, y, z, sx, sy, sz)| SceneObject {
                id: String::new(),
                scene: "scene:test".to_string(),
                group: None,
                mcp_id: "obj".to_string(),
                desired_name: "obj".to_string(),
                unreal_actor_name: None,
                actor_type: "StaticMeshActor".to_string(),
                asset_ref: json!({}),
                transform: Transform {
                    location: Vec3 { x, y, z },
                    rotation: Rotator {
                        pitch: 0.0,
                        yaw: 0.0,
                        roll: 0.0,
                    },
                    scale: Vec3 {
                        x: sx.abs().max(0.001),
                        y: sy.abs().max(0.001),
                        z: sz.abs().max(0.001),
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
            })
    }

    proptest! {
        #[test]
        fn no_panic_on_random_inputs(objs in prop::collection::vec(arb_scene_object(), 0..20)) {
            let engine = ValidationEngine::new();
            let footprints: Vec<crate::geom::footprint::Footprint2> =
                objs.iter().map(|o| crate::geom::footprint::Footprint2::from_scene_object(o, 0)).collect();
            let _ = engine.validate(&objs, &footprints);
        }
    }
}
