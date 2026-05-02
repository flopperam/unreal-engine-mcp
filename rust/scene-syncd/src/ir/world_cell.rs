use serde::{Deserialize, Serialize};

/// A world partition cell for large-scene streaming.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct WorldCell {
    pub cell_id: String,
    pub min_x: f64,
    pub max_x: f64,
    pub min_y: f64,
    pub max_y: f64,
    pub object_ids: Vec<String>,
    /// Dirty hash for incremental sync tracking.
    pub dirty_hash: String,
}

impl WorldCell {
    pub fn contains(&self, x: f64, y: f64) -> bool {
        x >= self.min_x && x <= self.max_x && y >= self.min_y && y <= self.max_y
    }
}

/// Compute a deterministic dirty hash for a cell from its objects.
/// SHA256 of sorted desired_hashes (order-independent).
pub fn compute_dirty_hash(objects: &[crate::domain::SceneObject]) -> String {
    use sha2::{Digest, Sha256};
    let mut hashes: Vec<&str> = objects
        .iter()
        .filter(|o| !o.deleted)
        .map(|o| o.desired_hash.as_str())
        .collect();
    hashes.sort_unstable();
    let mut hasher = Sha256::new();
    for h in hashes {
        hasher.update(h.as_bytes());
    }
    format!("{:x}", hasher.finalize())
}

/// Simple fixed grid partitioning for now.
pub fn partition_into_cells(
    object_positions: &[(String, f64, f64)],
    cell_size: f64,
) -> Vec<WorldCell> {
    use std::collections::HashMap;

    let mut groups: HashMap<(i64, i64), Vec<String>> = HashMap::new();

    for (id, x, y) in object_positions {
        let cx = (*x / cell_size).floor() as i64;
        let cy = (*y / cell_size).floor() as i64;
        groups.entry((cx, cy)).or_default().push(id.clone());
    }

    let mut cells = Vec::new();
    for ((cx, cy), ids) in groups {
        let min_x = cx as f64 * cell_size;
        let max_x = min_x + cell_size;
        let min_y = cy as f64 * cell_size;
        let max_y = min_y + cell_size;
        cells.push(WorldCell {
            cell_id: format!("cell_{}_{}", cx, cy),
            min_x,
            max_x,
            min_y,
            max_y,
            object_ids: ids,
            dirty_hash: String::new(),
        });
    }

    cells
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cell_contains_point() {
        let cell = WorldCell {
            cell_id: "c0".to_string(),
            min_x: 0.0,
            max_x: 1000.0,
            min_y: 0.0,
            max_y: 1000.0,
            object_ids: vec![],
            dirty_hash: String::new(),
        };
        assert!(cell.contains(500.0, 500.0));
        assert!(!cell.contains(1500.0, 500.0));
    }

    #[test]
    fn partition_groups_objects() {
        let positions = vec![
            ("a".to_string(), 500.0, 500.0),
            ("b".to_string(), 1500.0, 500.0),
            ("c".to_string(), 500.0, 1500.0),
        ];
        let cells = partition_into_cells(&positions, 1000.0);
        assert_eq!(cells.len(), 3);
        // Each object should be in its own cell.
        let total_objects: usize = cells.iter().map(|c| c.object_ids.len()).sum();
        assert_eq!(total_objects, 3);
    }
}
