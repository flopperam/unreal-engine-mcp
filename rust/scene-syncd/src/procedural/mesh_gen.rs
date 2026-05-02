use glam::Vec3;

use crate::procedural::marching_cubes::{self, MeshData};
use crate::procedural::mesh_buffer::ProceduralMeshPayload;
use crate::procedural::sdf::{SdfBounds, SdfTree};

/// Convert Marching Cubes output into a ProceduralMeshPayload ready for TCP transfer.
/// SDF meshes do not generate UVs — use World-Aligned materials on the UE side.
pub fn sdf_to_mesh_payload(
    mcp_id: &str,
    request_id: u64,
    sdf: &SdfTree,
    bounds: SdfBounds,
    resolution: u32,
) -> Result<ProceduralMeshPayload<'static>, MeshGenError> {
    let mesh = marching_cubes::marching_cubes(sdf, bounds, resolution);
    mesh_data_to_payload(mcp_id, request_id, mesh)
}

/// Convert raw MeshData into a ProceduralMeshPayload.
pub fn mesh_data_to_payload(
    mcp_id: &str,
    request_id: u64,
    mesh: MeshData,
) -> Result<ProceduralMeshPayload<'static>, MeshGenError> {
    if mesh.positions.is_empty() {
        return Err(MeshGenError::EmptyMesh);
    }
    ProceduralMeshPayload::new(
        mcp_id,
        request_id,
        mesh.positions,
        mesh.normals,
        None, // no UVs for SDF meshes
        None, // no tangents for SDF meshes
        None, // no vertex colors
        None, // no material IDs
        mesh.indices,
    )
    .map_err(MeshGenError::Validation)
}

/// Auto-compute bounds for an SDF tree. Uses `estimate_bounds()` when available,
/// otherwise falls back to the provided default.
pub fn auto_bounds_for_sdf(sdf: &SdfTree, default_bounds: SdfBounds) -> SdfBounds {
    sdf.estimate_bounds().unwrap_or(default_bounds)
}

/// Auto-compute bounds with padding. Expands estimated bounds by `padding`
/// to ensure the isosurface is fully captured.
pub fn auto_bounds_with_padding(
    sdf: &SdfTree,
    default_bounds: SdfBounds,
    padding: f32,
) -> SdfBounds {
    let bounds = auto_bounds_for_sdf(sdf, default_bounds);
    bounds.expand(padding)
}

/// Compute the axis-aligned bounding box of generated mesh positions.
pub fn compute_mesh_aabb(positions: &[[f32; 3]]) -> Option<SdfBounds> {
    if positions.is_empty() {
        return None;
    }
    let mut min = Vec3::new(f32::MAX, f32::MAX, f32::MAX);
    let mut max = Vec3::new(f32::MIN, f32::MIN, f32::MIN);
    for p in positions {
        let v = Vec3::from(*p);
        min = min.min(v);
        max = max.max(v);
    }
    Some(SdfBounds::new(min, max))
}

#[derive(Debug, thiserror::Error)]
pub enum MeshGenError {
    #[error("Marching Cubes produced no vertices")]
    EmptyMesh,
    #[error("Mesh validation failed: {0}")]
    Validation(#[from] crate::procedural::mesh_buffer::MeshValidationError),
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::procedural::sdf::SdfPrimitive;

    #[test]
    fn test_sdf_to_mesh_payload_sphere() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let bounds = SdfBounds::new(Vec3::new(-1.5, -1.5, -1.5), Vec3::new(1.5, 1.5, 1.5));
        let payload = sdf_to_mesh_payload("sphere_test", 1, &sphere, bounds, 16).unwrap();
        assert!(payload.header.vertex_count > 0);
        assert!(payload.header.index_count > 0);
        assert_eq!(
            payload.header.flags & crate::procedural::protocol::FLAG_HAS_UV,
            0
        );
    }

    #[test]
    fn test_sdf_to_mesh_payload_empty() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let bounds = SdfBounds::new(Vec3::new(10.0, 10.0, 10.0), Vec3::new(12.0, 12.0, 12.0));
        let result = sdf_to_mesh_payload("empty_test", 2, &sphere, bounds, 8);
        assert!(matches!(result, Err(MeshGenError::EmptyMesh)));
    }

    #[test]
    fn test_auto_bounds_sphere() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [1.0, 2.0, 3.0],
            radius: 2.0,
        });
        let default = SdfBounds::new(Vec3::new(-10.0, -10.0, -10.0), Vec3::new(10.0, 10.0, 10.0));
        let bounds = auto_bounds_for_sdf(&sphere, default);
        assert!((bounds.min.x - (-1.0)).abs() < 1e-4);
        assert!((bounds.max.x - 3.0).abs() < 1e-4);
    }

    #[test]
    fn test_auto_bounds_gyroid_uses_default() {
        let gyroid = SdfTree::Primitive(SdfPrimitive::Gyroid {
            frequency: 1.0,
            thickness: 0.1,
        });
        let default = SdfBounds::new(Vec3::new(-5.0, -5.0, -5.0), Vec3::new(5.0, 5.0, 5.0));
        let bounds = auto_bounds_for_sdf(&gyroid, default);
        assert!((bounds.min.x - (-5.0)).abs() < 1e-4);
        assert!((bounds.max.x - 5.0).abs() < 1e-4);
    }

    #[test]
    fn test_auto_bounds_with_padding() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let default = SdfBounds::new(Vec3::new(-10.0, -10.0, -10.0), Vec3::new(10.0, 10.0, 10.0));
        let bounds = auto_bounds_with_padding(&sphere, default, 0.5);
        assert!(bounds.min.x < -1.0, "should be padded beyond sphere bounds");
        assert!(bounds.max.x > 1.0);
    }

    #[test]
    fn test_compute_mesh_aabb() {
        let positions = vec![[1.0, 2.0, 3.0], [-1.0, -2.0, -3.0], [0.0, 0.0, 0.0]];
        let aabb = compute_mesh_aabb(&positions).unwrap();
        assert!((aabb.min.x - (-1.0)).abs() < 1e-4);
        assert!((aabb.max.x - 1.0).abs() < 1e-4);
        assert!((aabb.min.z - (-3.0)).abs() < 1e-4);
        assert!((aabb.max.z - 3.0).abs() < 1e-4);
    }

    #[test]
    fn test_compute_mesh_aabb_empty() {
        assert!(compute_mesh_aabb(&[]).is_none());
    }

    #[test]
    fn test_payload_byte_roundtrip() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let bounds = SdfBounds::new(Vec3::new(-1.5, -1.5, -1.5), Vec3::new(1.5, 1.5, 1.5));
        let mut payload = sdf_to_mesh_payload("rt_test", 99, &sphere, bounds, 16).unwrap();
        let bytes = payload.to_bytes();
        assert_eq!(bytes.len(), payload.total_bytes());

        let restored = ProceduralMeshPayload::from_bytes(&bytes).unwrap();
        assert_eq!(restored.header.vertex_count, payload.header.vertex_count);
        assert_eq!(restored.header.index_count, payload.header.index_count);
        assert_eq!(restored.header.request_id, 99);
    }
}
