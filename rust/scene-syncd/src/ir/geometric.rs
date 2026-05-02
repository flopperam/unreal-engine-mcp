use crate::domain::{Transform, Vec3};
use serde::{Deserialize, Serialize};

/// Geometric intermediate representation: volumes, footprints, connectors, and navigation hints.
/// Derived from SemanticScene during GeometryLoweringPass.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct GeometricIr {
    pub primitives: Vec<GeometricPrimitive>,
    pub connectors: Vec<Connector>,
    pub nav_surfaces: Vec<NavSurfaceHint>,
}

impl GeometricIr {
    pub fn new() -> Self {
        Self {
            primitives: Vec::new(),
            connectors: Vec::new(),
            nav_surfaces: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum GeometricPrimitive {
    Volume(VolumePrimitive),
    Footprint(FootprintPrimitive),
}

/// A 3D volume primitive with an optional extrusion height.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct VolumePrimitive {
    pub entity_id: String,
    pub kind: String,
    /// Base polygon in the XY plane (counter-clockwise exterior ring).
    pub base_polygon: Vec<(f64, f64)>,
    pub base_z: f64,
    pub height: f64,
    pub transform: Transform,
}

/// A 2D footprint primitive for surface-like objects (ground, road, moat).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FootprintPrimitive {
    pub entity_id: String,
    pub kind: String,
    pub polygon: Vec<(f64, f64)>,
    pub z: f64,
}

/// A connector between two entities (e.g. wall endpoint to tower corner).
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Connector {
    pub from_entity: String,
    pub to_entity: String,
    pub from_point: Vec3,
    pub to_point: Vec3,
    pub connector_type: ConnectorType,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum ConnectorType {
    WallToTower,
    BridgeEndpoint,
    GateOnWall,
    RoadToGatehouse,
}

/// Navigation surface hint for NavMesh generation.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct NavSurfaceHint {
    pub entity_id: String,
    pub kind: String,
    pub polygon: Vec<(f64, f64)>,
    pub z: f64,
    pub walkable: bool,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn geometric_ir_new_is_empty() {
        let ir = GeometricIr::new();
        assert!(ir.primitives.is_empty());
        assert!(ir.connectors.is_empty());
        assert!(ir.nav_surfaces.is_empty());
    }

    #[test]
    fn volume_primitive_serializes() {
        let vol = VolumePrimitive {
            entity_id: "keep_1".to_string(),
            kind: "keep".to_string(),
            base_polygon: vec![(0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0)],
            base_z: 0.0,
            height: 5.0,
            transform: Transform::default(),
        };
        let json = serde_json::to_string(&vol).unwrap();
        assert!(json.contains("keep_1"));
    }

    #[test]
    fn connector_types_roundtrip() {
        let conn = Connector {
            from_entity: "wall_1".to_string(),
            to_entity: "tower_1".to_string(),
            from_point: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            to_point: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            connector_type: ConnectorType::WallToTower,
        };
        let json = serde_json::to_string(&conn).unwrap();
        let back: Connector = serde_json::from_str(&json).unwrap();
        assert_eq!(back.connector_type, ConnectorType::WallToTower);
    }
}
