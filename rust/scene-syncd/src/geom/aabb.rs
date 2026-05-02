use crate::domain::{Transform, Vec3};
use crate::geom::units::Cm;
use glam::DVec3;

/// Axis-aligned bounding box in Unreal cm.
#[derive(Debug, Clone, PartialEq)]
pub struct Aabb3 {
    pub min: Vec3,
    pub max: Vec3,
}

impl Aabb3 {
    pub fn new(min: Vec3, max: Vec3) -> Self {
        Self { min, max }
    }

    pub fn from_center_extent(center: Vec3, extent: Vec3) -> Self {
        Self {
            min: Vec3 {
                x: center.x - extent.x,
                y: center.y - extent.y,
                z: center.z - extent.z,
            },
            max: Vec3 {
                x: center.x + extent.x,
                y: center.y + extent.y,
                z: center.z + extent.z,
            },
        }
    }

    pub fn from_domain_transform(t: &Transform) -> Self {
        // Treat scale as half-extents for basic-shape cubes/planes.
        let half = DVec3::new(
            t.scale.x.abs() * 50.0,
            t.scale.y.abs() * 50.0,
            t.scale.z.abs() * 50.0,
        );
        let center = DVec3::new(t.location.x, t.location.y, t.location.z);
        let min = center - half;
        let max = center + half;
        Self {
            min: Vec3 {
                x: min.x,
                y: min.y,
                z: min.z,
            },
            max: Vec3 {
                x: max.x,
                y: max.y,
                z: max.z,
            },
        }
    }

    pub fn intersects(&self, other: &Aabb3, epsilon: Cm) -> bool {
        let e = epsilon.value();
        self.max.x + e > other.min.x
            && self.min.x - e < other.max.x
            && self.max.y + e > other.min.y
            && self.min.y - e < other.max.y
            && self.max.z + e > other.min.z
            && self.min.z - e < other.max.z
    }

    pub fn contains_point(&self, point: &Vec3, epsilon: Cm) -> bool {
        let e = epsilon.value();
        point.x >= self.min.x - e
            && point.x <= self.max.x + e
            && point.y >= self.min.y - e
            && point.y <= self.max.y + e
            && point.z >= self.min.z - e
            && point.z <= self.max.z + e
    }

    pub fn volume(&self) -> f64 {
        let dx = self.max.x - self.min.x;
        let dy = self.max.y - self.min.y;
        let dz = self.max.z - self.min.z;
        dx * dy * dz
    }

    /// Expand the AABB to include another AABB.
    pub fn merge(&self, other: &Aabb3) -> Aabb3 {
        Aabb3 {
            min: Vec3 {
                x: self.min.x.min(other.min.x),
                y: self.min.y.min(other.min.y),
                z: self.min.z.min(other.min.z),
            },
            max: Vec3 {
                x: self.max.x.max(other.max.x),
                y: self.max.y.max(other.max.y),
                z: self.max.z.max(other.max.z),
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn aabb_intersects_overlapping() {
        let a = Aabb3::new(
            Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            Vec3 {
                x: 10.0,
                y: 10.0,
                z: 10.0,
            },
        );
        let b = Aabb3::new(
            Vec3 {
                x: 5.0,
                y: 5.0,
                z: 5.0,
            },
            Vec3 {
                x: 15.0,
                y: 15.0,
                z: 15.0,
            },
        );
        assert!(a.intersects(&b, Cm::ZERO));
    }

    #[test]
    fn aabb_does_not_intersect_separated() {
        let a = Aabb3::new(
            Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            Vec3 {
                x: 10.0,
                y: 10.0,
                z: 10.0,
            },
        );
        let b = Aabb3::new(
            Vec3 {
                x: 20.0,
                y: 20.0,
                z: 20.0,
            },
            Vec3 {
                x: 30.0,
                y: 30.0,
                z: 30.0,
            },
        );
        assert!(!a.intersects(&b, Cm::ZERO));
    }

    #[test]
    fn aabb_contains_point() {
        let a = Aabb3::new(
            Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            Vec3 {
                x: 10.0,
                y: 10.0,
                z: 10.0,
            },
        );
        assert!(a.contains_point(
            &Vec3 {
                x: 5.0,
                y: 5.0,
                z: 5.0,
            },
            Cm::ZERO,
        ));
        assert!(!a.contains_point(
            &Vec3 {
                x: 15.0,
                y: 5.0,
                z: 5.0,
            },
            Cm::ZERO,
        ));
    }

    #[test]
    fn aabb_from_domain_transform() {
        let t = Transform {
            location: Vec3 {
                x: 100.0,
                y: 200.0,
                z: 50.0,
            },
            rotation: crate::domain::Rotator {
                pitch: 0.0,
                yaw: 0.0,
                roll: 0.0,
            },
            scale: Vec3 {
                x: 2.0,
                y: 2.0,
                z: 2.0,
            },
        };
        let aabb = Aabb3::from_domain_transform(&t);
        assert_eq!(aabb.min.x, 0.0); // 100 - 2*50
        assert_eq!(aabb.max.x, 200.0); // 100 + 2*50
        assert_eq!(aabb.min.y, 100.0);
        assert_eq!(aabb.max.y, 300.0);
        assert_eq!(aabb.min.z, -50.0);
        assert_eq!(aabb.max.z, 150.0);
    }

    #[test]
    fn aabb_merge() {
        let a = Aabb3::new(
            Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            Vec3 {
                x: 10.0,
                y: 10.0,
                z: 10.0,
            },
        );
        let b = Aabb3::new(
            Vec3 {
                x: 5.0,
                y: -5.0,
                z: 20.0,
            },
            Vec3 {
                x: 15.0,
                y: 5.0,
                z: 30.0,
            },
        );
        let m = a.merge(&b);
        assert_eq!(m.min.x, 0.0);
        assert_eq!(m.min.y, -5.0);
        assert_eq!(m.min.z, 0.0);
        assert_eq!(m.max.x, 15.0);
        assert_eq!(m.max.y, 10.0);
        assert_eq!(m.max.z, 30.0);
    }
}
