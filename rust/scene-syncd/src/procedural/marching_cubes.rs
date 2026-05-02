use glam::Vec3;
use rayon::prelude::*;

use crate::procedural::sdf::{SdfBounds, SdfTree};

pub struct MeshData {
    pub positions: Vec<[f32; 3]>,
    pub normals: Vec<[f32; 3]>,
    pub indices: Vec<u32>,
}

pub fn marching_cubes(sdf: &SdfTree, bounds: SdfBounds, resolution: u32) -> MeshData {
    let res = resolution.max(1).min(256) as usize;
    let size = bounds.size();
    let cell_size = size / res as f32;

    // Sample SDF values on a (res+1)^3 grid
    let nx = res + 1;
    let ny = res + 1;
    let nz = res + 1;
    let total = nx * ny * nz;

    let mut values = vec![0.0f32; total];
    values
        .par_chunks_mut(nx * ny)
        .enumerate()
        .for_each(|(z, chunk)| {
            let zc = z as f32;
            for y in 0..ny {
                for x in 0..nx {
                    let px = bounds.min.x + x as f32 * cell_size.x;
                    let py = bounds.min.y + y as f32 * cell_size.y;
                    let pz = bounds.min.z + zc * cell_size.z;
                    let point = Vec3::new(px, py, pz);
                    chunk[y * nx + x] = sdf.evaluate(point);
                }
            }
        });

    // Extract triangles using Marching Cubes LUT
    let slices: Vec<(Vec<[f32; 3]>, Vec<[f32; 3]>, Vec<u32>)> = (0..res)
        .into_par_iter()
        .map(|z| {
            let mut positions = Vec::new();
            let mut normals = Vec::new();
            let mut indices = Vec::new();

            for y in 0..res {
                for x in 0..res {
                    let idx =
                        |xi: usize, yi: usize, zi: usize| -> usize { zi * ny * nx + yi * nx + xi };

                    // 8 corner values of the cell
                    let v = [
                        values[idx(x, y, z)],
                        values[idx(x + 1, y, z)],
                        values[idx(x + 1, y + 1, z)],
                        values[idx(x, y + 1, z)],
                        values[idx(x, y, z + 1)],
                        values[idx(x + 1, y, z + 1)],
                        values[idx(x + 1, y + 1, z + 1)],
                        values[idx(x, y + 1, z + 1)],
                    ];

                    let case_index = (v[0] < 0.0) as u8
                        | ((v[1] < 0.0) as u8) << 1
                        | ((v[2] < 0.0) as u8) << 2
                        | ((v[3] < 0.0) as u8) << 3
                        | ((v[4] < 0.0) as u8) << 4
                        | ((v[5] < 0.0) as u8) << 5
                        | ((v[6] < 0.0) as u8) << 6
                        | ((v[7] < 0.0) as u8) << 7;

                    if case_index == 0 || case_index == 255 {
                        continue;
                    }

                    // Corner positions
                    let corners = [
                        Vec3::new(
                            bounds.min.x + x as f32 * cell_size.x,
                            bounds.min.y + y as f32 * cell_size.y,
                            bounds.min.z + z as f32 * cell_size.z,
                        ),
                        Vec3::new(
                            bounds.min.x + (x + 1) as f32 * cell_size.x,
                            bounds.min.y + y as f32 * cell_size.y,
                            bounds.min.z + z as f32 * cell_size.z,
                        ),
                        Vec3::new(
                            bounds.min.x + (x + 1) as f32 * cell_size.x,
                            bounds.min.y + (y + 1) as f32 * cell_size.y,
                            bounds.min.z + z as f32 * cell_size.z,
                        ),
                        Vec3::new(
                            bounds.min.x + x as f32 * cell_size.x,
                            bounds.min.y + (y + 1) as f32 * cell_size.y,
                            bounds.min.z + z as f32 * cell_size.z,
                        ),
                        Vec3::new(
                            bounds.min.x + x as f32 * cell_size.x,
                            bounds.min.y + y as f32 * cell_size.y,
                            bounds.min.z + (z + 1) as f32 * cell_size.z,
                        ),
                        Vec3::new(
                            bounds.min.x + (x + 1) as f32 * cell_size.x,
                            bounds.min.y + y as f32 * cell_size.y,
                            bounds.min.z + (z + 1) as f32 * cell_size.z,
                        ),
                        Vec3::new(
                            bounds.min.x + (x + 1) as f32 * cell_size.x,
                            bounds.min.y + (y + 1) as f32 * cell_size.y,
                            bounds.min.z + (z + 1) as f32 * cell_size.z,
                        ),
                        Vec3::new(
                            bounds.min.x + x as f32 * cell_size.x,
                            bounds.min.y + (y + 1) as f32 * cell_size.y,
                            bounds.min.z + (z + 1) as f32 * cell_size.z,
                        ),
                    ];

                    // Edge table lookup
                    let edge_flags = EDGE_TABLE[case_index as usize];
                    if edge_flags == 0 {
                        continue;
                    }

                    // Interpolate vertex positions on active edges
                    let mut vertex_list: [Option<[f32; 3]>; 12] = [None; 12];
                    for edge in 0..12 {
                        if edge_flags & (1 << edge) == 0 {
                            continue;
                        }
                        let (a, b) = EDGE_ENDPOINTS[edge];
                        let t = if (v[a] - v[b]).abs() < 1e-10 {
                            0.5
                        } else {
                            v[a] / (v[a] - v[b])
                        };
                        let p = corners[a] + (corners[b] - corners[a]) * t;
                        vertex_list[edge] = Some(p.to_array());
                    }

                    // Generate triangles from tri table
                    let tri_row = &TRI_TABLE[case_index as usize];
                    let mut tri_idx = 0;
                    while tri_idx < 16 && tri_row[tri_idx] >= 0 {
                        let e0 = tri_row[tri_idx] as usize;
                        let e1 = tri_row[tri_idx + 1] as usize;
                        let e2 = tri_row[tri_idx + 2] as usize;
                        tri_idx += 3;

                        let base = positions.len() as u32;
                        for &ei in &[e0, e1, e2] {
                            if let Some(pos) = vertex_list[ei] {
                                let point = Vec3::from(pos);
                                let n = sdf.normal(point, cell_size.x * 0.1);
                                positions.push(pos);
                                normals.push(n.to_array());
                            }
                        }
                        if positions.len() as u32 > base + 2 {
                            indices.push(base);
                            indices.push(base + 1);
                            indices.push(base + 2);
                        }
                    }
                }
            }

            (positions, normals, indices)
        })
        .collect();

    // Merge slices
    let mut all_positions = Vec::new();
    let mut all_normals = Vec::new();
    let mut all_indices = Vec::new();

    for (positions, normals, indices) in slices {
        let offset = all_positions.len() as u32;
        all_positions.extend(positions);
        all_normals.extend(normals);
        all_indices.extend(indices.iter().map(|&i| i + offset));
    }

    MeshData {
        positions: all_positions,
        normals: all_normals,
        indices: all_indices,
    }
}

// Marching Cubes edge table: which edges are intersected for each case (0-255)
const EDGE_TABLE: [u16; 256] = [
    0x0, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c, 0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03,
    0xe09, 0xf00, 0x190, 0x99, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c, 0x99c, 0x895, 0xb9f,
    0xa96, 0xd9a, 0xc93, 0xf99, 0xe90, 0x230, 0x339, 0x33, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30, 0x3a0, 0x2a9, 0x1a3, 0xaa, 0x7a6,
    0x6af, 0x5a5, 0x4ac, 0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0, 0x460, 0x569,
    0x663, 0x76a, 0x66, 0x16f, 0x265, 0x36c, 0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69,
    0xb60, 0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff, 0x3f5, 0x2fc, 0xdfc, 0xcf5, 0xfff, 0xef6,
    0x9fa, 0x8f3, 0xbf9, 0xaf0, 0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55, 0x15c, 0xe5c,
    0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950, 0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf,
    0x1c5, 0xcc, 0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0, 0x8c0, 0x9c9, 0xac3,
    0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc, 0xcc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c, 0x15c, 0x55, 0x35f, 0x256, 0x55a,
    0x453, 0x759, 0x650, 0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc, 0x2fc, 0x3f5,
    0xff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0, 0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65,
    0xc6c, 0x36c, 0x265, 0x16f, 0x66, 0x76a, 0x663, 0x569, 0x460, 0xca0, 0xda9, 0xea3, 0xfaa,
    0x8a6, 0x9af, 0xaa5, 0xbac, 0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa, 0x1a3, 0x2a9, 0x3a0, 0xd30,
    0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c, 0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33,
    0x339, 0x230, 0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c, 0x69c, 0x795, 0x49f,
    0x596, 0x29a, 0x393, 0x99, 0x190, 0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0,
];

// For each edge (0-11), the two corner indices it connects
const EDGE_ENDPOINTS: [(usize, usize); 12] = [
    (0, 1),
    (1, 2),
    (2, 3),
    (3, 0),
    (4, 5),
    (5, 6),
    (6, 7),
    (7, 4),
    (0, 4),
    (1, 5),
    (2, 6),
    (3, 7),
];

// Marching Cubes triangle table: for each case, list of edge indices forming triangles (-1 = end)
const TRI_TABLE: [[i8; 16]; 256] = include!("marching_cubes_table.in");

#[cfg(test)]
mod tests {
    use super::*;
    use crate::procedural::sdf::{SdfPrimitive, SdfTree};

    #[test]
    fn test_sphere_mesh() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let bounds = SdfBounds::new(Vec3::new(-1.5, -1.5, -1.5), Vec3::new(1.5, 1.5, 1.5));
        let mesh = marching_cubes(&sphere, bounds, 16);
        assert!(mesh.positions.len() > 0, "should have vertices");
        assert!(mesh.indices.len() > 0, "should have indices");
        assert!(
            mesh.indices.len() % 3 == 0,
            "indices should be multiple of 3"
        );
        // All indices should be valid
        let vcount = mesh.positions.len() as u32;
        for &idx in &mesh.indices {
            assert!(idx < vcount, "index {idx} out of bounds {vcount}");
        }
    }

    #[test]
    fn test_empty_outside() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        // Bounds far away from the sphere
        let bounds = SdfBounds::new(Vec3::new(10.0, 10.0, 10.0), Vec3::new(12.0, 12.0, 12.0));
        let mesh = marching_cubes(&sphere, bounds, 8);
        assert_eq!(mesh.positions.len(), 0, "no vertices for empty region");
        assert_eq!(mesh.indices.len(), 0);
    }

    #[test]
    fn test_gyroid_mesh() {
        let gyroid = SdfTree::Primitive(SdfPrimitive::Gyroid {
            frequency: 1.0,
            thickness: 0.1,
        });
        let bounds = SdfBounds::new(Vec3::new(-3.0, -3.0, -3.0), Vec3::new(3.0, 3.0, 3.0));
        let mesh = marching_cubes(&gyroid, bounds, 16);
        assert!(mesh.positions.len() > 0, "gyroid should produce vertices");
        assert!(mesh.indices.len() > 0);
    }

    #[test]
    fn test_resolution_clamp() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let bounds = SdfBounds::new(Vec3::new(-1.5, -1.5, -1.5), Vec3::new(1.5, 1.5, 1.5));
        // resolution=0 is clamped to 1 — grid is too coarse to capture the surface,
        // but the function should not panic
        let mesh = marching_cubes(&sphere, bounds, 0);
        assert_eq!(mesh.indices.len() % 3, 0, "indices should be multiple of 3");
        // resolution=2 is coarse but enough to capture the sphere
        let mesh2 = marching_cubes(&sphere, bounds, 2);
        assert!(
            mesh2.positions.len() > 0,
            "resolution 2 should produce vertices"
        );
    }

    #[test]
    fn test_parallel_produces_valid_mesh() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let bounds = SdfBounds::new(Vec3::new(-1.5, -1.5, -1.5), Vec3::new(1.5, 1.5, 1.5));
        let mesh = marching_cubes(&sphere, bounds, 32);
        let vcount = mesh.positions.len() as u32;
        for &idx in &mesh.indices {
            assert!(idx < vcount);
        }
        // Verify normals are normalized (length close to 1)
        for n in &mesh.normals {
            let len = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]).sqrt();
            assert!(len > 0.9 && len < 1.1, "normal not unit: len={len}");
        }
    }
}
