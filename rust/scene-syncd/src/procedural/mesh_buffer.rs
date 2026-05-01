use std::borrow::Cow;
use crate::procedural::protocol::*;

#[derive(Debug, Clone)]
pub struct ProceduralMeshPayload<'a> {
    pub header: ProceduralMeshHeader,
    pub positions: Cow<'a, [[f32; 3]]>,
    pub normals: Cow<'a, [[f32; 3]]>,
    pub uvs: Option<Cow<'a, [[f32; 2]]>>,
    pub colors: Option<Cow<'a, [[u8; 4]]>>,
    pub indices: Cow<'a, [u32]>,
    pub warnings: Vec<String>,
}

#[derive(Debug, thiserror::Error)]
pub enum MeshValidationError {
    #[error("Index {0} is out of bounds (max: {1})")]
    IndexOutOfBounds(u32, u32),
    #[error("Index count ({0}) must be a multiple of 3")]
    IndexCountNotMultipleOf3(u32),
    #[error("Position contains NaN or Infinity")]
    InvalidPosition,
    #[error("Normal contains NaN or Infinity")]
    InvalidNormal,
    #[error("UV array size ({0}) does not match vertex count ({1})")]
    UVMismatch(usize, usize),
    #[error("Normals array size ({0}) does not match vertex count ({1})")]
    NormalMismatch(usize, usize),
    #[error("Color array size ({0}) does not match vertex count ({1})")]
    ColorMismatch(usize, usize),
    #[error("Vertex count ({0}) exceeds maximum ({1})")]
    VertexCountTooLarge(usize, usize),
    #[error("Index count ({0}) exceeds maximum ({1})")]
    IndexCountTooLarge(usize, usize),
    #[error("Payload size ({0}) exceeds maximum ({1})")]
    PayloadTooLarge(usize, usize),
    #[error("Position exceeds maximum coordinate extent")]
    BoundsTooLarge,
    #[error("UV contains NaN or Infinity")]
    InvalidUv,
}

impl<'a> ProceduralMeshPayload<'a> {
    pub fn new(
        mcp_id: &str,
        request_id: u64,
        positions: Vec<[f32; 3]>,
        normals: Vec<[f32; 3]>,
        uvs: Option<Vec<[f32; 2]>>,
        colors: Option<Vec<[u8; 4]>>,
        indices: Vec<u32>,
    ) -> Result<Self, MeshValidationError> {
        let vertex_count = positions.len() as u32;
        let index_count = indices.len() as u32;

        if positions.len() > MAX_VERTEX_COUNT {
            return Err(MeshValidationError::VertexCountTooLarge(
                positions.len(),
                MAX_VERTEX_COUNT,
            ));
        }

        if indices.len() > MAX_INDEX_COUNT {
            return Err(MeshValidationError::IndexCountTooLarge(
                indices.len(),
                MAX_INDEX_COUNT,
            ));
        }

        if index_count % 3 != 0 {
            return Err(MeshValidationError::IndexCountNotMultipleOf3(index_count));
        }

        if normals.len() != positions.len() {
            return Err(MeshValidationError::NormalMismatch(normals.len(), positions.len()));
        }

        let mut flags = 0;
        if let Some(uv) = &uvs {
            if uv.len() != positions.len() {
                return Err(MeshValidationError::UVMismatch(uv.len(), positions.len()));
            }
            for uv in uv {
                if !uv[0].is_finite() || !uv[1].is_finite() {
                    return Err(MeshValidationError::InvalidUv);
                }
            }
            flags |= FLAG_HAS_UV;
        }

        if let Some(color) = &colors {
            if color.len() != positions.len() {
                return Err(MeshValidationError::ColorMismatch(
                    color.len(),
                    positions.len(),
                ));
            }
            flags |= FLAG_HAS_COLOR;
        }

        // Validate indices
        for &idx in &indices {
            if idx >= vertex_count {
                return Err(MeshValidationError::IndexOutOfBounds(idx, vertex_count));
            }
        }

        // Validate floats (no NaN or Inf)
        for p in &positions {
            if !p[0].is_finite() || !p[1].is_finite() || !p[2].is_finite() {
                return Err(MeshValidationError::InvalidPosition);
            }
            if p[0].abs() > MAX_ABS_COORDINATE
                || p[1].abs() > MAX_ABS_COORDINATE
                || p[2].abs() > MAX_ABS_COORDINATE
            {
                return Err(MeshValidationError::BoundsTooLarge);
            }
        }
        for n in &normals {
            if !n[0].is_finite() || !n[1].is_finite() || !n[2].is_finite() {
                return Err(MeshValidationError::InvalidNormal);
            }
        }

        let warnings = mesh_warnings(&indices);
        let header = ProceduralMeshHeader::new(
            flags,
            vertex_count,
            index_count,
            0, // crc will be calculated in to_bytes
            request_id,
            mcp_id,
        );

        Ok(Self {
            header,
            positions: Cow::Owned(positions),
            normals: Cow::Owned(normals),
            uvs: uvs.map(Cow::Owned),
            colors: colors.map(Cow::Owned),
            indices: Cow::Owned(indices),
            warnings,
        })
    }

    pub fn to_bytes(&mut self) -> Vec<u8> {
        let mut payload = Vec::new();

        // Write positions
        for p in self.positions.iter() {
            payload.extend_from_slice(&p[0].to_le_bytes());
            payload.extend_from_slice(&p[1].to_le_bytes());
            payload.extend_from_slice(&p[2].to_le_bytes());
        }

        // Write normals
        for n in self.normals.iter() {
            payload.extend_from_slice(&n[0].to_le_bytes());
            payload.extend_from_slice(&n[1].to_le_bytes());
            payload.extend_from_slice(&n[2].to_le_bytes());
        }

        // Write uvs
        if let Some(uvs) = &self.uvs {
            for uv in uvs.iter() {
                payload.extend_from_slice(&uv[0].to_le_bytes());
                payload.extend_from_slice(&uv[1].to_le_bytes());
            }
        }

        // Write vertex colors
        if let Some(colors) = &self.colors {
            for color in colors.iter() {
                payload.extend_from_slice(color);
            }
        }

        // Write indices
        for i in self.indices.iter() {
            payload.extend_from_slice(&i.to_le_bytes());
        }

        // Calculate CRC32 of payload
        self.header.payload_crc32 = calculate_crc32(&payload);

        let mut final_buffer = Vec::with_capacity(104 + payload.len());
        final_buffer.extend_from_slice(&self.header.to_bytes());
        final_buffer.extend_from_slice(&payload);

        final_buffer
    }

    pub fn total_bytes(&self) -> usize {
        let v = self.header.vertex_count as usize;
        let i = self.header.index_count as usize;
        
        let mut size = 104
            + v * std::mem::size_of::<[f32; 3]>()   // positions
            + v * std::mem::size_of::<[f32; 3]>()   // normals
            + i * std::mem::size_of::<u32>(); // indices

        if self.header.flags & FLAG_HAS_UV != 0 {
            size += v * std::mem::size_of::<[f32; 2]>();
        }
        if self.header.flags & FLAG_HAS_COLOR != 0 {
            size += v * std::mem::size_of::<[u8; 4]>();
        }

        size
    }

    pub fn validate_size(&self) -> Result<(), MeshValidationError> {
        let size = self.total_bytes();
        if size > MAX_PAYLOAD_BYTES {
            return Err(MeshValidationError::PayloadTooLarge(
                size,
                MAX_PAYLOAD_BYTES,
            ));
        }
        Ok(())
    }
}

fn mesh_warnings(indices: &[u32]) -> Vec<String> {
    let mut warnings = Vec::new();
    let mut seen = std::collections::HashSet::new();
    for (tri_index, tri) in indices.chunks_exact(3).enumerate() {
        let [a, b, c] = [tri[0], tri[1], tri[2]];
        if a == b || b == c || a == c {
            warnings.push(format!("DEGENERATE_TRIANGLE:{tri_index}"));
            continue;
        }
        let mut sorted = [a, b, c];
        sorted.sort_unstable();
        if !seen.insert(sorted) {
            warnings.push(format!("DUPLICATE_TRIANGLE:{tri_index}"));
        }
    }
    warnings
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_valid_mesh() {
        let mut payload = ProceduralMeshPayload::new(
            "test1",
            1,
            vec![[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
            vec![[0.0, 0.0, 1.0], [0.0, 0.0, 1.0], [0.0, 0.0, 1.0]],
            None,
            None,
            vec![0, 1, 2],
        ).unwrap();

        assert_eq!(payload.header.vertex_count, 3);
        assert_eq!(payload.header.index_count, 3);
        payload.validate_size().unwrap();
        let bytes = payload.to_bytes();
        assert_eq!(bytes.len(), payload.total_bytes());
        assert_eq!(bytes[0..4], MCPM_MAGIC.to_le_bytes());
        assert_eq!(bytes[4..8], MCPM_VERSION.to_le_bytes());
        assert_eq!(bytes[8..12], MCPM_HEADER_SIZE.to_le_bytes());
    }

    #[test]
    fn test_invalid_index() {
        let result = ProceduralMeshPayload::new(
            "test2",
            1,
            vec![[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
            vec![[0.0, 0.0, 1.0], [0.0, 0.0, 1.0], [0.0, 0.0, 1.0]],
            None,
            None,
            vec![0, 1, 3], // 3 is out of bounds
        );
        assert!(matches!(result, Err(MeshValidationError::IndexOutOfBounds(3, 3))));
    }

    #[test]
    fn test_nan_position() {
        let result = ProceduralMeshPayload::new(
            "test3",
            1,
            vec![[0.0, f32::NAN, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
            vec![[0.0, 0.0, 1.0], [0.0, 0.0, 1.0], [0.0, 0.0, 1.0]],
            None,
            None,
            vec![0, 1, 2],
        );
        assert!(matches!(result, Err(MeshValidationError::InvalidPosition)));
    }

    #[test]
    fn test_vertex_colors_are_serialized_and_flagged() {
        let mut payload = ProceduralMeshPayload::new(
            "test4",
            99,
            vec![[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
            vec![[0.0, 0.0, 1.0]; 3],
            Some(vec![[0.0, 0.0], [1.0, 0.0], [0.0, 1.0]]),
            Some(vec![[255, 0, 0, 255], [0, 255, 0, 255], [0, 0, 255, 255]]),
            vec![0, 1, 2],
        )
        .unwrap();

        assert_eq!(payload.header.flags & FLAG_HAS_UV, FLAG_HAS_UV);
        assert_eq!(payload.header.flags & FLAG_HAS_COLOR, FLAG_HAS_COLOR);
        let bytes = payload.to_bytes();
        assert_eq!(bytes.len(), payload.total_bytes());
        let payload_only = &bytes[MCPM_HEADER_SIZE as usize..];
        assert_eq!(
            calculate_crc32(payload_only),
            u32::from_le_bytes(bytes[24..28].try_into().unwrap())
        );
    }

    #[test]
    fn test_degenerate_and_duplicate_triangles_are_warnings() {
        let payload = ProceduralMeshPayload::new(
            "test5",
            1,
            vec![[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
            vec![[0.0, 0.0, 1.0]; 3],
            None,
            None,
            vec![0, 0, 1, 0, 1, 2, 2, 1, 0],
        )
        .unwrap();

        assert!(payload.warnings.iter().any(|w| w == "DEGENERATE_TRIANGLE:0"));
        assert!(payload.warnings.iter().any(|w| w == "DUPLICATE_TRIANGLE:2"));
    }
}
