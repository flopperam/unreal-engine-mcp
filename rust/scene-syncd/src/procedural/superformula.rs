use glam::Vec3;

use crate::procedural::mesh_buffer::ProceduralMeshPayload;
use crate::procedural::mesh_gen::MeshGenError;

/// Parameters for the 3D Superformula (two independent 2D superformulas).
#[derive(Debug, Clone)]
pub struct SuperformulaParams {
    /// m parameter for the latitude superformula (controls symmetry order)
    pub m1: f32,
    /// n1 parameter for the latitude superformula
    pub n1_1: f32,
    /// n2 parameter for the latitude superformula
    pub n2_1: f32,
    /// n3 parameter for the latitude superformula
    pub n3_1: f32,
    /// a parameter for the latitude superformula
    pub a1: f32,
    /// b parameter for the latitude superformula
    pub b1: f32,
    /// m parameter for the longitude superformula
    pub m2: f32,
    /// n1 parameter for the longitude superformula
    pub n1_2: f32,
    /// n2 parameter for the longitude superformula
    pub n2_2: f32,
    /// n3 parameter for the longitude superformula
    pub n3_2: f32,
    /// a parameter for the longitude superformula
    pub a2: f32,
    /// b parameter for the longitude superformula
    pub b2: f32,
}

impl Default for SuperformulaParams {
    fn default() -> Self {
        // Produces a rounded shape close to a sphere
        Self {
            m1: 6.0,
            n1_1: 1.0,
            n2_1: 1.0,
            n3_1: 1.0,
            a1: 1.0,
            b1: 1.0,
            m2: 6.0,
            n1_2: 1.0,
            n2_2: 1.0,
            n3_2: 1.0,
            a2: 1.0,
            b2: 1.0,
        }
    }
}

/// Evaluate the 2D superformula: r(θ) = (|cos(mθ/4)/a|^n2 + |sin(mθ/4)/b|^n3)^(-1/n1)
fn sf2d(theta: f32, m: f32, n1: f32, n2: f32, n3: f32, a: f32, b: f32) -> f32 {
    let t1 = (f32::cos(m * theta / 4.0) / a).abs().powf(n2);
    let t2 = (f32::sin(m * theta / 4.0) / b).abs().powf(n3);
    let sum = t1 + t2;
    if sum < 1e-10 {
        return 0.0;
    }
    sum.powf(-1.0 / n1)
}

/// Generate a 3D Superformula mesh with UV mapping from spherical coordinates.
pub fn superformula_mesh(
    params: &SuperformulaParams,
    resolution: u32,
    scale: f32,
    mcp_id: &str,
    request_id: u64,
) -> Result<ProceduralMeshPayload<'static>, MeshGenError> {
    let res = resolution.max(4).min(256) as usize;
    let lat_steps = res;
    let lon_steps = res * 2; // longitude needs double resolution for proper wrapping

    let mut positions = Vec::new();
    let mut normals = Vec::new();
    let mut uvs = Vec::new();
    let mut indices = Vec::new();

    // Generate vertices on a (lat_steps+1) x (lon_steps+1) grid
    // Latitude: -π/2 to π/2 (theta from 0 to π)
    // Longitude: 0 to 2π (phi from 0 to 2π)
    let lat_count = lat_steps + 1;
    let lon_count = lon_steps + 1;

    // Precompute radii for each latitude and longitude
    let mut r_lat = vec![0.0f32; lat_count];
    for i in 0..lat_count {
        let theta =
            -std::f32::consts::FRAC_PI_2 + std::f32::consts::PI * i as f32 / lat_steps as f32;
        // Map theta ∈ [-π/2, π/2] to superformula angle ∈ [-π/2, π/2]
        r_lat[i] = sf2d(
            theta,
            params.m1,
            params.n1_1,
            params.n2_1,
            params.n3_1,
            params.a1,
            params.b1,
        );
    }

    let mut r_lon = vec![0.0f32; lon_count];
    for j in 0..lon_count {
        let phi = 2.0 * std::f32::consts::PI * j as f32 / lon_steps as f32;
        r_lon[j] = sf2d(
            phi,
            params.m2,
            params.n1_2,
            params.n2_2,
            params.n3_2,
            params.a2,
            params.b2,
        );
    }

    // Generate vertices
    for i in 0..lat_count {
        let theta =
            -std::f32::consts::FRAC_PI_2 + std::f32::consts::PI * i as f32 / lat_steps as f32;
        let cos_theta = f32::cos(theta);
        let sin_theta = f32::sin(theta);

        for j in 0..lon_count {
            let phi = 2.0 * std::f32::consts::PI * j as f32 / lon_steps as f32;
            let cos_phi = f32::cos(phi);
            let sin_phi = f32::sin(phi);

            let r1 = r_lat[i];
            let r2 = r_lon[j];

            let x = r1 * cos_theta * r2 * cos_phi * scale;
            let y = r1 * cos_theta * r2 * sin_phi * scale;
            let z = r1 * sin_theta * scale;

            positions.push([x, y, z]);

            // Approximate normal via central differences on the parametric surface
            let eps_theta = std::f32::consts::PI / lat_steps as f32;
            let eps_phi = 2.0 * std::f32::consts::PI / lon_steps as f32;

            // Tangent in theta direction
            let ct_plus = f32::cos(theta + eps_theta);
            let st_plus = f32::sin(theta + eps_theta);
            let r1_p = sf2d(
                theta + eps_theta,
                params.m1,
                params.n1_1,
                params.n2_1,
                params.n3_1,
                params.a1,
                params.b1,
            );
            let dx_dt =
                (r1_p * ct_plus * r2 * cos_phi - r1 * cos_theta * r2 * cos_phi) * scale / eps_theta;
            let dy_dt =
                (r1_p * ct_plus * r2 * sin_phi - r1 * cos_theta * r2 * sin_phi) * scale / eps_theta;
            let dz_dt = (r1_p * st_plus - r1 * sin_theta) * scale / eps_theta;

            // Tangent in phi direction
            let phi_p = phi + eps_phi;
            let r2_p = sf2d(
                phi_p,
                params.m2,
                params.n1_2,
                params.n2_2,
                params.n3_2,
                params.a2,
                params.b2,
            );
            let dx_dp = (r1 * cos_theta * r2_p * f32::cos(phi_p) - r1 * cos_theta * r2 * cos_phi)
                * scale
                / eps_phi;
            let dy_dp = (r1 * cos_theta * r2_p * f32::sin(phi_p) - r1 * cos_theta * r2 * sin_phi)
                * scale
                / eps_phi;
            let dz_dp = 0.0; // z doesn't depend on phi

            // Normal = cross product of tangent vectors
            let n = Vec3::new(dx_dt, dy_dt, dz_dt).cross(Vec3::new(dx_dp, dy_dp, dz_dp));
            let len = n.length();
            let n = if len > 1e-10 { n / len } else { Vec3::Z };
            normals.push(n.to_array());

            // UV from spherical coordinates
            let u = j as f32 / lon_steps as f32;
            let v = i as f32 / lat_steps as f32;
            uvs.push([u, v]);
        }
    }

    // Generate triangle indices
    for i in 0..lat_steps {
        for j in 0..lon_steps {
            let v00 = (i * lon_count + j) as u32;
            let v10 = ((i + 1) * lon_count + j) as u32;
            let v01 = (i * lon_count + j + 1) as u32;
            let v11 = ((i + 1) * lon_count + j + 1) as u32;

            indices.push(v00);
            indices.push(v10);
            indices.push(v01);

            indices.push(v01);
            indices.push(v10);
            indices.push(v11);
        }
    }

    if positions.is_empty() {
        return Err(MeshGenError::EmptyMesh);
    }

    ProceduralMeshPayload::new(
        mcp_id,
        request_id,
        positions,
        normals,
        Some(uvs),
        None,
        None,
        None,
        indices,
    )
    .map_err(MeshGenError::Validation)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_produces_valid_mesh() {
        let params = SuperformulaParams::default();
        let payload = superformula_mesh(&params, 16, 1.0, "sf_default", 1).unwrap();
        assert!(payload.header.vertex_count > 0);
        assert!(payload.header.index_count > 0);
        assert!(payload.header.flags & crate::procedural::protocol::FLAG_HAS_UV != 0);
    }

    #[test]
    fn test_sphere_like_shape() {
        let params = SuperformulaParams {
            m1: 0.0,
            n1_1: 1.0,
            n2_1: 1.0,
            n3_1: 1.0,
            a1: 1.0,
            b1: 1.0,
            m2: 0.0,
            n1_2: 1.0,
            n2_2: 1.0,
            n3_2: 1.0,
            a2: 1.0,
            b2: 1.0,
        };
        let payload = superformula_mesh(&params, 16, 1.0, "sf_sphere", 2).unwrap();
        assert!(payload.header.vertex_count > 0);
    }

    #[test]
    fn test_high_symmetry() {
        let params = SuperformulaParams {
            m1: 8.0,
            n1_1: 0.5,
            n2_1: 0.5,
            n3_1: 0.5,
            a1: 1.0,
            b1: 1.0,
            m2: 8.0,
            n1_2: 0.5,
            n2_2: 0.5,
            n3_2: 0.5,
            a2: 1.0,
            b2: 1.0,
        };
        let payload = superformula_mesh(&params, 32, 1.0, "sf_star", 3).unwrap();
        assert!(payload.header.vertex_count > 0);
        assert!(payload.header.index_count % 3 == 0);
    }

    #[test]
    fn test_uv_range() {
        let params = SuperformulaParams::default();
        let payload = superformula_mesh(&params, 8, 1.0, "sf_uv", 4).unwrap();
        let uvs = payload.uvs.as_ref().unwrap();
        for uv in uvs.iter() {
            assert!(uv[0] >= 0.0 && uv[0] <= 1.0, "u out of range: {}", uv[0]);
            assert!(uv[1] >= 0.0 && uv[1] <= 1.0, "v out of range: {}", uv[1]);
        }
    }

    #[test]
    fn test_indices_valid() {
        let params = SuperformulaParams::default();
        let payload = superformula_mesh(&params, 16, 1.0, "sf_idx", 5).unwrap();
        let vcount = payload.header.vertex_count;
        for &idx in payload.indices.iter() {
            assert!(idx < vcount, "index {idx} out of bounds {vcount}");
        }
    }

    #[test]
    fn test_normals_are_unit() {
        let params = SuperformulaParams::default();
        let payload = superformula_mesh(&params, 16, 1.0, "sf_norm", 6).unwrap();
        for n in payload.normals.iter() {
            let len = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]).sqrt();
            assert!(len > 0.9 && len < 1.1, "normal not unit: len={len}");
        }
    }

    #[test]
    fn test_resolution_clamped() {
        let params = SuperformulaParams::default();
        let payload = superformula_mesh(&params, 3, 1.0, "sf_low", 7).unwrap();
        // resolution=3 clamped to 4
        assert!(payload.header.vertex_count > 0);
    }
}
