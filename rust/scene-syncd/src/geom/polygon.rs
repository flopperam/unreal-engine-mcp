use geo::algorithm::bool_ops::BooleanOps;
use geo::algorithm::contains::Contains;
use geo::{Coord, LineString, Polygon};

/// A 2D polygon for exact geometric queries using the `geo` crate.
#[derive(Debug, Clone, PartialEq)]
pub struct Polygon2 {
    pub exterior: Vec<(f64, f64)>,
}

impl Polygon2 {
    pub fn from_footprint_rect(min_x: f64, min_y: f64, max_x: f64, max_y: f64) -> Self {
        Self {
            exterior: vec![
                (min_x, min_y),
                (max_x, min_y),
                (max_x, max_y),
                (min_x, max_y),
                (min_x, min_y),
            ],
        }
    }

    pub fn to_geo_polygon(&self) -> Polygon<f64> {
        let coords: Vec<Coord> = self
            .exterior
            .iter()
            .map(|(x, y)| Coord { x: *x, y: *y })
            .collect();
        Polygon::new(LineString::from(coords), vec![])
    }

    /// Exact point-in-polygon test.
    pub fn contains_point(&self, x: f64, y: f64) -> bool {
        let geo_poly = self.to_geo_polygon();
        geo_poly.contains(&Coord { x, y })
    }

    /// Exact 2D polygon overlap using geo boolean operations.
    pub fn overlaps(&self, other: &Polygon2) -> bool {
        let a = self.to_geo_polygon();
        let b = other.to_geo_polygon();
        // Two polygons overlap if their intersection area > 0.
        let intersection = a.intersection(&b);
        !intersection.0.is_empty()
    }

    /// Compute the axis-aligned bounding box.
    pub fn bbox(&self) -> Option<(f64, f64, f64, f64)> {
        if self.exterior.is_empty() {
            return None;
        }
        let mut min_x = f64::INFINITY;
        let mut max_x = f64::NEG_INFINITY;
        let mut min_y = f64::INFINITY;
        let mut max_y = f64::NEG_INFINITY;
        for (x, y) in &self.exterior {
            min_x = min_x.min(*x);
            max_x = max_x.max(*x);
            min_y = min_y.min(*y);
            max_y = max_y.max(*y);
        }
        Some((min_x, min_y, max_x, max_y))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn point_inside_rectangle() {
        let poly = Polygon2::from_footprint_rect(0.0, 0.0, 10.0, 10.0);
        assert!(poly.contains_point(5.0, 5.0));
    }

    #[test]
    fn point_outside_rectangle() {
        let poly = Polygon2::from_footprint_rect(0.0, 0.0, 10.0, 10.0);
        assert!(!poly.contains_point(15.0, 5.0));
    }

    #[test]
    fn two_rectangles_overlap() {
        let a = Polygon2::from_footprint_rect(0.0, 0.0, 10.0, 10.0);
        let b = Polygon2::from_footprint_rect(5.0, 5.0, 15.0, 15.0);
        assert!(a.overlaps(&b));
    }

    #[test]
    fn separated_rectangles_do_not_overlap() {
        let a = Polygon2::from_footprint_rect(0.0, 0.0, 10.0, 10.0);
        let b = Polygon2::from_footprint_rect(20.0, 20.0, 30.0, 30.0);
        assert!(!a.overlaps(&b));
    }

    use proptest::prelude::*;

    fn arb_polygon() -> impl Strategy<Value = Polygon2> {
        (any::<f64>(), any::<f64>(), any::<f64>(), any::<f64>())
            .prop_filter("positive size", |(_, _, w, h)| *w > 0.01 && *h > 0.01)
            .prop_map(|(min_x, min_y, w, h)| {
                Polygon2::from_footprint_rect(min_x, min_y, min_x + w, min_y + h)
            })
    }

    proptest! {
        #[test]
        fn overlaps_reflexive(a in arb_polygon(), b in arb_polygon()) {
            let ab = a.overlaps(&b);
            let ba = b.overlaps(&a);
            prop_assert_eq!(ab, ba, "overlaps must be reflexive");
        }
    }
}
