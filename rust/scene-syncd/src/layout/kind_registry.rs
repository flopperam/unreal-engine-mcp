use std::collections::HashMap;

/// Default actor specification for a semantic layout entity kind.
#[derive(Debug, Clone)]
pub struct KindSpec {
    pub actor_type: &'static str,
    pub asset_path: &'static str,
    pub default_tags: Vec<&'static str>,
    pub draft_color: [f64; 4],
    /// Z-layer for occlusion ordering. Higher = above. LAYER_GAP cm between layers.
    pub layer: i32,
}

/// Vertical spacing between layers in Unreal cm.
pub const LAYER_GAP: f64 = 100.0;

/// Registry mapping entity kinds to their default realization specifications.
pub struct KindRegistry {
    map: HashMap<String, KindSpec>,
}

impl Default for KindRegistry {
    fn default() -> Self {
        let mut map = HashMap::new();
        map.insert(
            "keep".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["castle", "keep"],
                draft_color: [0.65, 0.78, 1.0, 0.35],
                layer: 0,
            },
        );
        map.insert(
            "curtain_wall".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["castle", "wall"],
                draft_color: [0.55, 0.72, 1.0, 0.3],
                layer: 0,
            },
        );
        map.insert(
            "tower".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["castle", "tower"],
                draft_color: [0.8, 0.68, 1.0, 0.34],
                layer: 0,
            },
        );
        map.insert(
            "gatehouse".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["castle", "gate"],
                draft_color: [1.0, 0.78, 0.45, 0.36],
                layer: 0,
            },
        );
        map.insert(
            "ground".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Plane.Plane",
                default_tags: vec!["castle", "ground"],
                draft_color: [0.42, 0.7, 0.5, 0.22],
                layer: -1,
            },
        );
        map.insert(
            "bridge".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["castle", "bridge"],
                draft_color: [0.74, 0.84, 0.92, 0.32],
                layer: 1,
            },
        );
        map.insert(
            "moat".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Plane.Plane",
                default_tags: vec!["castle", "moat", "water"],
                draft_color: [0.2, 0.45, 0.75, 0.35],
                layer: -2,
            },
        );
        map.insert(
            "building".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["building"],
                draft_color: [0.7, 0.7, 0.7, 0.3],
                layer: 0,
            },
        );
        map.insert(
            "decoration".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["decoration"],
                draft_color: [0.9, 0.8, 0.3, 0.3],
                layer: 0,
            },
        );
        map.insert(
            "crenellation".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["crenellation", "detail"],
                draft_color: [0.8, 0.75, 0.65, 0.35],
                layer: 1,
            },
        );
        map.insert(
            "window".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["window", "detail"],
                draft_color: [0.4, 0.6, 0.9, 0.5],
                layer: 1,
            },
        );
        map.insert(
            "roof_tile".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["roof_tile", "detail"],
                draft_color: [0.6, 0.35, 0.25, 0.4],
                layer: 1,
            },
        );
        map.insert(
            "brick".to_string(),
            KindSpec {
                actor_type: "StaticMeshActor",
                asset_path: "/Engine/BasicShapes/Cube.Cube",
                default_tags: vec!["brick", "detail"],
                draft_color: [0.7, 0.5, 0.35, 0.35],
                layer: 1,
            },
        );
        Self { map }
    }
}

impl KindRegistry {
    pub fn get(&self, kind: &str) -> Option<&KindSpec> {
        self.map.get(kind)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn kind_registry_has_all_castle_kinds() {
        let registry = KindRegistry::default();
        for kind in [
            "keep",
            "curtain_wall",
            "tower",
            "gatehouse",
            "ground",
            "bridge",
            "moat",
            "building",
            "decoration",
            "crenellation",
            "window",
            "roof_tile",
            "brick",
        ] {
            assert!(registry.get(kind).is_some(), "missing kind: {kind}");
        }
    }

    #[test]
    fn moat_uses_plane_and_water_color() {
        let registry = KindRegistry::default();
        let spec = registry.get("moat").unwrap();
        assert_eq!(spec.asset_path, "/Engine/BasicShapes/Plane.Plane");
        assert!(spec.draft_color[2] > 0.5); // blue-ish
    }
}
