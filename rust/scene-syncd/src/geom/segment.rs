use crate::geom::units::Cm;

/// A 2D line segment in the XY plane.
#[derive(Debug, Clone, PartialEq)]
pub struct Segment2 {
    pub x1: f64,
    pub y1: f64,
    pub x2: f64,
    pub y2: f64,
}

impl Segment2 {
    pub fn new(x1: f64, y1: f64, x2: f64, y2: f64) -> Self {
        Self { x1, y1, x2, y2 }
    }

    pub fn length(&self) -> f64 {
        let dx = self.x2 - self.x1;
        let dy = self.y2 - self.y1;
        (dx * dx + dy * dy).sqrt()
    }

    pub fn midpoint(&self) -> (f64, f64) {
        ((self.x1 + self.x2) / 2.0, (self.y1 + self.y2) / 2.0)
    }

    /// Returns true if the two segments intersect (including endpoints) within epsilon.
    pub fn intersects(&self, other: &Segment2, epsilon: Cm) -> bool {
        let e = epsilon.value();
        // Orientation tests using robust cross products.
        let cross1 =
            (self.x2 - self.x1) * (other.y1 - self.y1) - (self.y2 - self.y1) * (other.x1 - self.x1);
        let cross2 =
            (self.x2 - self.x1) * (other.y2 - self.y1) - (self.y2 - self.y1) * (other.x2 - self.x1);
        let cross3 = (other.x2 - other.x1) * (self.y1 - other.y1)
            - (other.y2 - other.y1) * (self.x1 - other.x1);
        let cross4 = (other.x2 - other.x1) * (self.y2 - other.y1)
            - (other.y2 - other.y1) * (self.x2 - other.x1);

        if cross1.abs() < e && cross2.abs() < e {
            // Collinear - check bounding box overlap.
            return self.bbox_overlaps(other, epsilon);
        }

        let d1 = cross1 * cross2;
        let d2 = cross3 * cross4;
        d1 <= e * e && d2 <= e * e
    }

    fn bbox_overlaps(&self, other: &Segment2, epsilon: Cm) -> bool {
        let e = epsilon.value();
        self.x1.max(other.x1) - e <= self.x2.min(other.x2) + e
            && self.y1.max(other.y1) - e <= self.y2.min(other.y2) + e
    }
}

/// A 3D line segment.
#[derive(Debug, Clone, PartialEq)]
pub struct Segment3 {
    pub x1: f64,
    pub y1: f64,
    pub z1: f64,
    pub x2: f64,
    pub y2: f64,
    pub z2: f64,
}

impl Segment3 {
    pub fn new(x1: f64, y1: f64, z1: f64, x2: f64, y2: f64, z2: f64) -> Self {
        Self {
            x1,
            y1,
            z1,
            x2,
            y2,
            z2,
        }
    }

    pub fn length(&self) -> f64 {
        let dx = self.x2 - self.x1;
        let dy = self.y2 - self.y1;
        let dz = self.z2 - self.z1;
        (dx * dx + dy * dy + dz * dz).sqrt()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn segment_length() {
        let s = Segment2::new(0.0, 0.0, 3.0, 4.0);
        assert!((s.length() - 5.0).abs() < 1e-9);
    }

    #[test]
    fn segments_intersect_at_cross() {
        let a = Segment2::new(0.0, 0.0, 10.0, 10.0);
        let b = Segment2::new(0.0, 10.0, 10.0, 0.0);
        assert!(a.intersects(&b, Cm(0.01)));
    }

    #[test]
    fn segments_do_not_intersect_when_parallel() {
        let a = Segment2::new(0.0, 0.0, 10.0, 0.0);
        let b = Segment2::new(0.0, 5.0, 10.0, 5.0);
        assert!(!a.intersects(&b, Cm(0.01)));
    }

    #[test]
    fn segment3_length() {
        let s = Segment3::new(0.0, 0.0, 0.0, 1.0, 2.0, 2.0);
        assert!((s.length() - 3.0).abs() < 1e-9);
    }
}
