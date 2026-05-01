use crc32fast::Hasher;

pub const MCPM_MAGIC: u32 = 0x4D43504D; // 'MCPM'
pub const MCPM_VERSION: u32 = 1;
pub const MCPM_HEADER_SIZE: u32 = 104;

pub const MAX_VERTEX_COUNT: usize = 1_000_000;
pub const MAX_INDEX_COUNT: usize = 6_000_000;
pub const MAX_PAYLOAD_BYTES: usize = 256 * 1024 * 1024;
pub const MAX_ABS_COORDINATE: f32 = 10_000_000.0;

pub const FLAG_HAS_UV: u32 = 0x01;
pub const FLAG_HAS_TANGENT: u32 = 0x02;
pub const FLAG_HAS_COLOR: u32 = 0x04;
pub const FLAG_HAS_MATERIAL_ID: u32 = 0x08;
pub const SUPPORTED_FLAGS: u32 =
    FLAG_HAS_UV | FLAG_HAS_TANGENT | FLAG_HAS_COLOR | FLAG_HAS_MATERIAL_ID;

#[derive(Debug, Clone)]
pub struct ProceduralMeshHeader {
    pub magic: u32,
    pub version: u32,
    pub header_size: u32,
    pub flags: u32,
    pub vertex_count: u32,
    pub index_count: u32,
    pub payload_crc32: u32,
    pub reserved: u32,
    pub request_id: u64,
    pub mcp_id: [u8; 64],
}

impl ProceduralMeshHeader {
    pub fn new(
        flags: u32,
        vertex_count: u32,
        index_count: u32,
        payload_crc32: u32,
        request_id: u64,
        mcp_id_str: &str,
    ) -> Self {
        let mut mcp_id = [0u8; 64];
        let bytes = mcp_id_str.as_bytes();
        let len = bytes.len().min(63); // leave at least one null terminator if possible
        mcp_id[..len].copy_from_slice(&bytes[..len]);

        Self {
            magic: MCPM_MAGIC,
            version: MCPM_VERSION,
            header_size: MCPM_HEADER_SIZE,
            flags,
            vertex_count,
            index_count,
            payload_crc32,
            reserved: 0,
            request_id,
            mcp_id,
        }
    }

    pub fn to_bytes(&self) -> [u8; 104] {
        let mut buf = [0u8; MCPM_HEADER_SIZE as usize];
        buf[0..4].copy_from_slice(&self.magic.to_le_bytes());
        buf[4..8].copy_from_slice(&self.version.to_le_bytes());
        buf[8..12].copy_from_slice(&self.header_size.to_le_bytes());
        buf[12..16].copy_from_slice(&self.flags.to_le_bytes());
        buf[16..20].copy_from_slice(&self.vertex_count.to_le_bytes());
        buf[20..24].copy_from_slice(&self.index_count.to_le_bytes());
        buf[24..28].copy_from_slice(&self.payload_crc32.to_le_bytes());
        buf[28..32].copy_from_slice(&self.reserved.to_le_bytes());
        buf[32..40].copy_from_slice(&self.request_id.to_le_bytes());
        buf[40..104].copy_from_slice(&self.mcp_id);
        buf
    }

    pub fn from_bytes(buf: &[u8]) -> Option<Self> {
        if buf.len() < MCPM_HEADER_SIZE as usize {
            return None;
        }

        let mut mcp_id = [0u8; 64];
        mcp_id.copy_from_slice(&buf[40..104]);

        Some(Self {
            magic: u32::from_le_bytes(buf[0..4].try_into().unwrap()),
            version: u32::from_le_bytes(buf[4..8].try_into().unwrap()),
            header_size: u32::from_le_bytes(buf[8..12].try_into().unwrap()),
            flags: u32::from_le_bytes(buf[12..16].try_into().unwrap()),
            vertex_count: u32::from_le_bytes(buf[16..20].try_into().unwrap()),
            index_count: u32::from_le_bytes(buf[20..24].try_into().unwrap()),
            payload_crc32: u32::from_le_bytes(buf[24..28].try_into().unwrap()),
            reserved: u32::from_le_bytes(buf[28..32].try_into().unwrap()),
            request_id: u64::from_le_bytes(buf[32..40].try_into().unwrap()),
            mcp_id,
        })
    }
}

pub fn calculate_crc32(payload: &[u8]) -> u32 {
    let mut hasher = Hasher::new();
    hasher.update(payload);
    hasher.finalize()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_header_serialization() {
        let header = ProceduralMeshHeader::new(
            FLAG_HAS_UV,
            100,
            300,
            0xDEADBEEF,
            42,
            "test-mcp-id-12345",
        );

        let bytes = header.to_bytes();
        assert_eq!(bytes.len(), MCPM_HEADER_SIZE as usize);

        let decoded = ProceduralMeshHeader::from_bytes(&bytes).unwrap();
        assert_eq!(decoded.magic, MCPM_MAGIC);
        assert_eq!(decoded.version, MCPM_VERSION);
        assert_eq!(decoded.flags, FLAG_HAS_UV);
        assert_eq!(decoded.vertex_count, 100);
        assert_eq!(decoded.index_count, 300);
        assert_eq!(decoded.payload_crc32, 0xDEADBEEF);
        assert_eq!(decoded.request_id, 42);

        let mcp_str = std::ffi::CStr::from_bytes_until_nul(&decoded.mcp_id)
            .unwrap()
            .to_str()
            .unwrap();
        assert_eq!(mcp_str, "test-mcp-id-12345");
    }

    #[test]
    fn test_protocol_constants_match_cpp_phase0_contract() {
        assert_eq!(MCPM_MAGIC, 0x4D43504D);
        assert_eq!(MCPM_VERSION, 1);
        assert_eq!(MCPM_HEADER_SIZE, 104);
        assert_eq!(FLAG_HAS_UV, 0x01);
        assert_eq!(FLAG_HAS_TANGENT, 0x02);
        assert_eq!(FLAG_HAS_COLOR, 0x04);
        assert_eq!(FLAG_HAS_MATERIAL_ID, 0x08);
        assert_eq!(SUPPORTED_FLAGS, 0x0F);
        assert_eq!(MAX_VERTEX_COUNT, 1_000_000);
        assert_eq!(MAX_INDEX_COUNT, 6_000_000);
        assert_eq!(MAX_PAYLOAD_BYTES, 256 * 1024 * 1024);
        assert_eq!(MAX_ABS_COORDINATE, 10_000_000.0);
    }

    #[test]
    fn test_header_field_offsets() {
        let header = ProceduralMeshHeader::new(
            SUPPORTED_FLAGS,
            11,
            33,
            0xAABBCCDD,
            0x1122334455667788,
            "offset-test",
        );
        let bytes = header.to_bytes();

        assert_eq!(&bytes[0..4], &MCPM_MAGIC.to_le_bytes());
        assert_eq!(&bytes[4..8], &MCPM_VERSION.to_le_bytes());
        assert_eq!(&bytes[8..12], &MCPM_HEADER_SIZE.to_le_bytes());
        assert_eq!(&bytes[12..16], &SUPPORTED_FLAGS.to_le_bytes());
        assert_eq!(&bytes[16..20], &11u32.to_le_bytes());
        assert_eq!(&bytes[20..24], &33u32.to_le_bytes());
        assert_eq!(&bytes[24..28], &0xAABBCCDDu32.to_le_bytes());
        assert_eq!(&bytes[28..32], &0u32.to_le_bytes());
        assert_eq!(&bytes[32..40], &0x1122334455667788u64.to_le_bytes());
        assert_eq!(&bytes[40..51], b"offset-test");
    }

    #[test]
    fn test_crc32() {
        let data = b"hello world";
        let crc = calculate_crc32(data);
        assert_ne!(crc, 0);
    }
}
