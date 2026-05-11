use glam::Vec3;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct SdfBounds {
    pub min: Vec3,
    pub max: Vec3,
}

impl SdfBounds {
    pub fn new(min: Vec3, max: Vec3) -> Self {
        Self { min, max }
    }

    pub fn merge(&self, other: &SdfBounds) -> SdfBounds {
        SdfBounds {
            min: self.min.min(other.min),
            max: self.max.max(other.max),
        }
    }

    pub fn expand(&self, padding: f32) -> SdfBounds {
        let p = Vec3::splat(padding);
        SdfBounds {
            min: self.min - p,
            max: self.max + p,
        }
    }

    pub fn size(&self) -> Vec3 {
        self.max - self.min
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum SdfPrimitive {
    Sphere {
        center: [f32; 3],
        radius: f32,
    },
    Box {
        min: [f32; 3],
        max: [f32; 3],
    },
    Torus {
        center: [f32; 3],
        major_radius: f32,
        minor_radius: f32,
    },
    Gyroid {
        frequency: f32,
        thickness: f32,
    },
    Scherk {
        frequency: f32,
    },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum SdfTree {
    Primitive(SdfPrimitive),
    Union(Box<SdfTree>, Box<SdfTree>, f32),
    Difference(Box<SdfTree>, Box<SdfTree>, f32),
    Intersection(Box<SdfTree>, Box<SdfTree>, f32),
    Transform(Box<SdfTree>, [f32; 16]),
}

impl SdfPrimitive {
    pub fn evaluate(&self, point: Vec3) -> f32 {
        match self {
            SdfPrimitive::Sphere { center, radius } => {
                let c = Vec3::from(*center);
                (point - c).length() - radius
            }
            SdfPrimitive::Box { min, max } => {
                let bmin = Vec3::from(*min);
                let bmax = Vec3::from(*max);
                let center = (bmin + bmax) * 0.5;
                let half = (bmax - bmin) * 0.5;
                let d = (point - center).abs() - half;
                d.max(Vec3::ZERO).length() + d.x.max(d.y).max(d.z).min(0.0)
            }
            SdfPrimitive::Torus {
                center,
                major_radius,
                minor_radius,
            } => {
                let c = Vec3::from(*center);
                let p = point - c;
                let q = glam::vec2(glam::vec2(p.x, p.y).length() - *major_radius, p.z);
                q.length() - minor_radius
            }
            SdfPrimitive::Gyroid {
                frequency,
                thickness,
            } => {
                let f = *frequency;
                let g = f32::sin(point.x * f) * f32::sin(point.y * f) * f32::cos(point.z * f)
                    + f32::sin(point.z * f) * f32::sin(point.x * f) * f32::cos(point.y * f)
                    + f32::sin(point.y * f) * f32::sin(point.z * f) * f32::cos(point.x * f);
                g.abs() - *thickness
            }
            SdfPrimitive::Scherk { frequency } => {
                let f = *frequency;
                let s = f32::sinh(point.x * f) * f32::sinh(point.y * f) - f32::sin(point.z * f);
                s.abs() - 0.1
            }
        }
    }

    pub fn estimate_bounds(&self) -> Option<SdfBounds> {
        match self {
            SdfPrimitive::Sphere { center, radius } => {
                let c = Vec3::from(*center);
                let r = *radius;
                Some(SdfBounds::new(c - Vec3::splat(r), c + Vec3::splat(r)))
            }
            SdfPrimitive::Box { min, max } => {
                Some(SdfBounds::new(Vec3::from(*min), Vec3::from(*max)))
            }
            SdfPrimitive::Torus {
                center,
                major_radius,
                minor_radius,
            } => {
                let c = Vec3::from(*center);
                let outer = major_radius + minor_radius;
                Some(SdfBounds::new(
                    c - Vec3::new(outer, outer, *minor_radius),
                    c + Vec3::new(outer, outer, *minor_radius),
                ))
            }
            SdfPrimitive::Gyroid { .. } | SdfPrimitive::Scherk { .. } => None,
        }
    }
}

impl SdfTree {
    pub fn evaluate(&self, point: Vec3) -> f32 {
        match self {
            SdfTree::Primitive(p) => p.evaluate(point),
            SdfTree::Union(a, b, smoothness) => {
                let da = a.evaluate(point);
                let db = b.evaluate(point);
                if *smoothness <= 0.0 {
                    da.min(db)
                } else {
                    smooth_min(da, db, *smoothness)
                }
            }
            SdfTree::Difference(a, b, smoothness) => {
                let da = a.evaluate(point);
                let db = b.evaluate(point);
                if *smoothness <= 0.0 {
                    (-db).max(da)
                } else {
                    smooth_min(-db, da, *smoothness)
                }
            }
            SdfTree::Intersection(a, b, smoothness) => {
                let da = a.evaluate(point);
                let db = b.evaluate(point);
                if *smoothness <= 0.0 {
                    da.max(db)
                } else {
                    smooth_min(da, db, *smoothness)
                }
            }
            SdfTree::Transform(child, mat) => {
                let m = glam::Mat4::from_cols_array(mat);
                let inv = m.inverse();
                let transformed = inv.transform_point3(point);
                child.evaluate(transformed)
            }
        }
    }

    pub fn normal(&self, point: Vec3, epsilon: f32) -> Vec3 {
        let ex = self.evaluate(Vec3::new(point.x + epsilon, point.y, point.z))
            - self.evaluate(Vec3::new(point.x - epsilon, point.y, point.z));
        let ey = self.evaluate(Vec3::new(point.x, point.y + epsilon, point.z))
            - self.evaluate(Vec3::new(point.x, point.y - epsilon, point.z));
        let ez = self.evaluate(Vec3::new(point.x, point.y, point.z + epsilon))
            - self.evaluate(Vec3::new(point.x, point.y, point.z - epsilon));
        let n = Vec3::new(ex, ey, ez);
        let len = n.length();
        if len > 0.0 {
            n / len
        } else {
            Vec3::Z
        }
    }

    pub fn estimate_bounds(&self) -> Option<SdfBounds> {
        match self {
            SdfTree::Primitive(p) => p.estimate_bounds(),
            SdfTree::Union(a, b, _) | SdfTree::Intersection(a, b, _) => {
                match (a.estimate_bounds(), b.estimate_bounds()) {
                    (Some(ba), Some(bb)) => Some(ba.merge(&bb)),
                    (Some(ba), None) => Some(ba),
                    (None, Some(bb)) => Some(bb),
                    (None, None) => None,
                }
            }
            SdfTree::Difference(a, b, _) => a.estimate_bounds().or_else(|| b.estimate_bounds()),
            SdfTree::Transform(child, mat) => child.estimate_bounds().map(|bounds| {
                let m = glam::Mat4::from_cols_array(mat);
                let corners = [
                    Vec3::new(bounds.min.x, bounds.min.y, bounds.min.z),
                    Vec3::new(bounds.max.x, bounds.min.y, bounds.min.z),
                    Vec3::new(bounds.min.x, bounds.max.y, bounds.min.z),
                    Vec3::new(bounds.min.x, bounds.min.y, bounds.max.z),
                    Vec3::new(bounds.max.x, bounds.max.y, bounds.min.z),
                    Vec3::new(bounds.max.x, bounds.min.y, bounds.max.z),
                    Vec3::new(bounds.min.x, bounds.max.y, bounds.max.z),
                    Vec3::new(bounds.max.x, bounds.max.y, bounds.max.z),
                ];
                let transformed: Vec<Vec3> =
                    corners.iter().map(|c| m.transform_point3(*c)).collect();
                let min = transformed.iter().fold(Vec3::MAX, |a, &b| a.min(b));
                let max = transformed.iter().fold(Vec3::MIN, |a, &b| a.max(b));
                SdfBounds::new(min, max)
            }),
        }
    }
}

fn smooth_min(a: f32, b: f32, k: f32) -> f32 {
    let h = (k - (a - b).abs()).max(0.0) / k;
    a.min(b) - h * h * k * 0.25
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sphere_sdf() {
        let sphere = SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        };
        assert!(sphere.evaluate(Vec3::ZERO) < 0.0);
        assert!(sphere.evaluate(Vec3::new(0.0, 0.0, 2.0)) > 0.0);
        let surface = sphere.evaluate(Vec3::new(1.0, 0.0, 0.0));
        assert!(surface.abs() < 1e-4);
    }

    #[test]
    fn test_box_sdf() {
        let box_ = SdfPrimitive::Box {
            min: [-1.0, -1.0, -1.0],
            max: [1.0, 1.0, 1.0],
        };
        assert!(box_.evaluate(Vec3::ZERO) < 0.0);
        assert!(box_.evaluate(Vec3::new(2.0, 0.0, 0.0)) > 0.0);
    }

    #[test]
    fn test_union_hard() {
        let a = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let b = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [3.0, 0.0, 0.0],
            radius: 1.0,
        });
        let union = SdfTree::Union(Box::new(a), Box::new(b), 0.0);
        assert!(union.evaluate(Vec3::ZERO) < 0.0);
        assert!(union.evaluate(Vec3::new(3.0, 0.0, 0.0)) < 0.0);
        assert!(union.evaluate(Vec3::new(1.5, 0.0, 0.0)) > 0.0);
    }

    #[test]
    fn test_union_smooth() {
        let a = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let b = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [3.0, 0.0, 0.0],
            radius: 1.0,
        });
        let union = SdfTree::Union(Box::new(a), Box::new(b), 0.5);
        let mid = union.evaluate(Vec3::new(1.5, 0.0, 0.0));
        assert!(mid < 1.0);
    }

    #[test]
    fn test_difference() {
        let a = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let b = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.5, 0.0, 0.0],
            radius: 0.8,
        });
        let diff = SdfTree::Difference(Box::new(a), Box::new(b), 0.0);
        assert!(diff.evaluate(Vec3::new(-0.9, 0.0, 0.0)) < 0.0);
        assert!(diff.evaluate(Vec3::new(0.5, 0.0, 0.0)) > 0.0);
    }

    #[test]
    fn test_intersection() {
        let a = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let b = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.5, 0.0, 0.0],
            radius: 1.0,
        });
        let isect = SdfTree::Intersection(Box::new(a), Box::new(b), 0.0);
        assert!(isect.evaluate(Vec3::new(0.25, 0.0, 0.0)) < 0.0);
        assert!(isect.evaluate(Vec3::new(2.0, 0.0, 0.0)) > 0.0);
    }

    #[test]
    fn test_normal_accuracy() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let n = sphere.normal(Vec3::new(1.0, 0.0, 0.0), 0.001);
        let expected = Vec3::X;
        let err = (n - expected).length();
        assert!(err < 0.01, "normal error = {err}, n = {n}");
    }

    #[test]
    fn test_estimate_bounds_sphere() {
        let sphere = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [1.0, 2.0, 3.0],
            radius: 2.0,
        });
        let bounds = sphere.estimate_bounds().unwrap();
        assert!((bounds.min.x - (-1.0)).abs() < 1e-4);
        assert!((bounds.max.x - 3.0).abs() < 1e-4);
    }

    #[test]
    fn test_gyroid_no_bounds() {
        let gyroid = SdfTree::Primitive(SdfPrimitive::Gyroid {
            frequency: 1.0,
            thickness: 0.1,
        });
        assert!(gyroid.estimate_bounds().is_none());
    }

    #[test]
    fn test_union_bounds_merge() {
        let a = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [0.0, 0.0, 0.0],
            radius: 1.0,
        });
        let b = SdfTree::Primitive(SdfPrimitive::Sphere {
            center: [5.0, 0.0, 0.0],
            radius: 1.0,
        });
        let union = SdfTree::Union(Box::new(a), Box::new(b), 0.0);
        let bounds = union.estimate_bounds().unwrap();
        assert!(bounds.max.x > 5.0);
    }
}
