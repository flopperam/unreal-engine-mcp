use crate::geom::footprint::Footprint2;
use crate::geom::units::Cm;
use rstar::{RTree, RTreeObject, AABB};

/// An indexed 2D envelope for use in an R-tree.
#[derive(Debug, Clone)]
struct IndexedFootprint {
    pub min_x: f64,
    pub max_x: f64,
    pub min_y: f64,
    pub max_y: f64,
    pub index: usize,
}

impl RTreeObject for IndexedFootprint {
    type Envelope = AABB<[f64; 2]>;

    fn envelope(&self) -> Self::Envelope {
        AABB::from_corners([self.min_x, self.min_y], [self.max_x, self.max_y])
    }
}

/// Spatial index for 2D footprints to avoid all-pairs overlap checks.
pub struct SpatialSceneIndex {
    tree: RTree<IndexedFootprint>,
    footprints: Vec<Footprint2>,
}

impl SpatialSceneIndex {
    pub fn from_footprints(footprints: Vec<Footprint2>) -> Self {
        let indexed: Vec<IndexedFootprint> = footprints
            .iter()
            .enumerate()
            .map(|(i, fp)| IndexedFootprint {
                min_x: fp.min_x,
                max_x: fp.max_x,
                min_y: fp.min_y,
                max_y: fp.max_y,
                index: i,
            })
            .collect();
        let tree = RTree::bulk_load(indexed);
        Self { tree, footprints }
    }

    /// Return all pairs of footprints whose 2D envelopes overlap.
    pub fn overlapping_pairs(&self, epsilon: Cm) -> Vec<(&Footprint2, &Footprint2)> {
        let e = epsilon.value();
        let mut pairs = Vec::new();
        let mut seen = std::collections::HashSet::new();

        for (i, fp) in self.footprints.iter().enumerate() {
            let envelope =
                AABB::from_corners([fp.min_x - e, fp.min_y - e], [fp.max_x + e, fp.max_y + e]);
            for candidate in self.tree.locate_in_envelope_intersecting(&envelope) {
                let j = candidate.index;
                if j <= i {
                    continue;
                }
                let key = if i < j { (i, j) } else { (j, i) };
                if seen.contains(&key) {
                    continue;
                }
                let other = &self.footprints[j];
                if fp.intersects_2d(other, epsilon) {
                    seen.insert(key);
                    pairs.push((fp, other));
                }
            }
        }
        pairs
    }

    pub fn footprints(&self) -> &[Footprint2] {
        &self.footprints
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_fp(min_x: f64, max_x: f64, min_y: f64, max_y: f64, z: f64, mcp_id: &str) -> Footprint2 {
        Footprint2 {
            min_x,
            max_x,
            min_y,
            max_y,
            z,
            mcp_id: mcp_id.to_string(),
            kind: "keep".to_string(),
            layer: 0,
            polygon: None,
        }
    }

    #[test]
    fn finds_overlapping_pair() {
        let fps = vec![
            make_fp(0.0, 10.0, 0.0, 10.0, 0.0, "a"),
            make_fp(5.0, 15.0, 5.0, 15.0, 0.0, "b"),
            make_fp(100.0, 110.0, 100.0, 110.0, 0.0, "c"),
        ];
        let index = SpatialSceneIndex::from_footprints(fps);
        let pairs = index.overlapping_pairs(Cm::ZERO);
        assert_eq!(pairs.len(), 1);
        assert_eq!(pairs[0].0.mcp_id, "a");
        assert_eq!(pairs[0].1.mcp_id, "b");
    }

    #[test]
    fn no_pairs_when_separated() {
        let fps = vec![
            make_fp(0.0, 10.0, 0.0, 10.0, 0.0, "a"),
            make_fp(20.0, 30.0, 20.0, 30.0, 0.0, "b"),
        ];
        let index = SpatialSceneIndex::from_footprints(fps);
        let pairs = index.overlapping_pairs(Cm::ZERO);
        assert!(pairs.is_empty());
    }
}
