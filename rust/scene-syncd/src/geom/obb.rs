use crate::domain::{SceneObject, Vec3};
use crate::geom::aabb::Aabb3;
use crate::geom::units::Cm;
use glam::{DQuat, DVec3};

/// Oriented bounding box for rotated objects (walls, gates).
/// Uses `glam::DQuat` for rotation to match Unreal's coordinate system.
#[derive(Debug, Clone, PartialEq)]
pub struct Obb3 {
    pub center: Vec3,
    pub half_extents: Vec3,
    pub rotation: DQuat,
}

impl Obb3 {
    /// Build an OBB from a scene object's transform.
    /// Scale is treated as full size in cm for basic-shape cubes/planes.
    pub fn from_scene_object(obj: &SceneObject) -> Self {
        let t = &obj.transform;
        let half = DVec3::new(
            t.scale.x.abs() * 50.0,
            t.scale.y.abs() * 50.0,
            t.scale.z.abs() * 50.0,
        );

        // glam is Y-up right-handed.  yaw rotates +X towards -Z, matching Unreal.
        let rot = DQuat::from_euler(
            glam::EulerRot::YXZ,
            t.rotation.yaw.to_radians(),
            t.rotation.pitch.to_radians(),
            t.rotation.roll.to_radians(),
        );

        Self {
            center: Vec3 {
                x: t.location.x,
                y: t.location.y,
                z: t.location.z,
            },
            half_extents: Vec3 {
                x: half.x,
                y: half.y,
                z: half.z,
            },
            rotation: rot,
        }
    }

    /// Compute the 8 world-space vertices of this OBB.
    pub fn vertices(&self) -> [Vec3; 8] {
        let c = DVec3::new(self.center.x, self.center.y, self.center.z);
        let h = DVec3::new(
            self.half_extents.x,
            self.half_extents.y,
            self.half_extents.z,
        );
        let r = self.rotation;

        let signs: [(f64, f64, f64); 8] = [
            (-1.0, -1.0, -1.0),
            (1.0, -1.0, -1.0),
            (1.0, 1.0, -1.0),
            (-1.0, 1.0, -1.0),
            (-1.0, -1.0, 1.0),
            (1.0, -1.0, 1.0),
            (1.0, 1.0, 1.0),
            (-1.0, 1.0, 1.0),
        ];

        core::array::from_fn(|i| {
            let s = signs[i];
            let local = DVec3::new(s.0 * h.x, s.1 * h.y, s.2 * h.z);
            let rotated = rotate_vec_by_quat(&r, local);
            let world = c + rotated;
            Vec3 {
                x: world.x,
                y: world.y,
                z: world.z,
            }
        })
    }

    /// Convert to an axis-aligned bounding box (conservative).
    pub fn to_aabb(&self) -> Aabb3 {
        let verts = self.vertices();
        let mut min_x = f64::INFINITY;
        let mut max_x = f64::NEG_INFINITY;
        let mut min_y = f64::INFINITY;
        let mut max_y = f64::NEG_INFINITY;
        let mut min_z = f64::INFINITY;
        let mut max_z = f64::NEG_INFINITY;
        for v in &verts {
            min_x = min_x.min(v.x);
            max_x = max_x.max(v.x);
            min_y = min_y.min(v.y);
            max_y = max_y.max(v.y);
            min_z = min_z.min(v.z);
            max_z = max_z.max(v.z);
        }
        Aabb3::new(
            Vec3 {
                x: min_x,
                y: min_y,
                z: min_z,
            },
            Vec3 {
                x: max_x,
                y: max_y,
                z: max_z,
            },
        )
    }

    /// Exact OBB-vs-OBB intersection using the Separating Axis Theorem.
    pub fn intersects_obb(&self, other: &Obb3, epsilon: Cm) -> bool {
        let e = epsilon.value();

        let c1 = DVec3::new(self.center.x, self.center.y, self.center.z);
        let c2 = DVec3::new(other.center.x, other.center.y, other.center.z);
        let t = c2 - c1;

        // Column vectors of each rotation matrix (local axes in world space)
        let a = self.rotation_matrix();
        let b = other.rotation_matrix();

        // Half-extents as glam vectors for dot products
        let h1 = DVec3::new(
            self.half_extents.x,
            self.half_extents.y,
            self.half_extents.z,
        );
        let h2 = DVec3::new(
            other.half_extents.x,
            other.half_extents.y,
            other.half_extents.z,
        );

        // Build the rotation matrix R = A^T * B expressed as dot products
        let mut r_mat = [[0.0; 3]; 3];
        for i in 0..3 {
            for j in 0..3 {
                r_mat[i][j] = a[i].dot(b[j]);
            }
        }

        // Build absolute version (with epsilon to avoid near-zero axis issues)
        let mut abs_r = [[0.0; 3]; 3];
        for i in 0..3 {
            for j in 0..3 {
                abs_r[i][j] = r_mat[i][j].abs() + f64::EPSILON;
            }
        }

        // --- Test axes of A ---
        for i in 0..3 {
            let ra = h1[i];
            let rb = h2[0] * abs_r[i][0] + h2[1] * abs_r[i][1] + h2[2] * abs_r[i][2];
            let t_proj = t.dot(a[i]).abs();
            if t_proj > ra + rb + e {
                return false;
            }
        }

        // --- Test axes of B ---
        for i in 0..3 {
            let ra = h1[0] * abs_r[0][i] + h1[1] * abs_r[1][i] + h1[2] * abs_r[2][i];
            let rb = h2[i];
            let t_proj = t.dot(b[i]).abs();
            if t_proj > ra + rb + e {
                return false;
            }
        }

        // --- Test cross-product axes (9 combinations) ---
        for i in 0..3 {
            for j in 0..3 {
                let axis = a[i].cross(b[j]);
                let len_sq = axis.length_squared();
                if len_sq < 1e-12 {
                    continue; // parallel axes, already tested
                }
                let axis_norm = axis / len_sq.sqrt();

                let ra = h1[(i + 1) % 3] * abs_r[(i + 2) % 3][j]
                    + h1[(i + 2) % 3] * abs_r[(i + 1) % 3][j];
                let rb = h2[(j + 1) % 3] * abs_r[i][(j + 2) % 3]
                    + h2[(j + 2) % 3] * abs_r[i][(j + 1) % 3];
                let t_proj = t.dot(axis_norm).abs();
                if t_proj > ra + rb + e {
                    return false;
                }
            }
        }

        true
    }

    /// Check whether a point lies inside this OBB.
    pub fn contains_point(&self, point: &Vec3, epsilon: Cm) -> bool {
        let e = epsilon.value();
        let local = self.world_to_local(DVec3::new(point.x, point.y, point.z));
        local.x.abs() <= self.half_extents.x + e
            && local.y.abs() <= self.half_extents.y + e
            && local.z.abs() <= self.half_extents.z + e
    }

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    fn rotation_matrix(&self) -> [DVec3; 3] {
        let r = self.rotation;
        [
            rotate_vec_by_quat(&r, DVec3::new(1.0, 0.0, 0.0)),
            rotate_vec_by_quat(&r, DVec3::new(0.0, 1.0, 0.0)),
            rotate_vec_by_quat(&r, DVec3::new(0.0, 0.0, 1.0)),
        ]
    }

    fn world_to_local(&self, world: DVec3) -> DVec3 {
        let offset = world - DVec3::new(self.center.x, self.center.y, self.center.z);
        // Inverse rotation = conjugate for unit quaternion
        let r_inv = self.rotation.conjugate();
        rotate_vec_by_quat(&r_inv, offset)
    }
}

/// Rotate a vector by a unit quaternion.
/// `glam` does not expose `Mul<DVec3>` for `DQuat`, so we do it manually.
fn rotate_vec_by_quat(q: &DQuat, v: DVec3) -> DVec3 {
    let qv = DVec3::new(q.x, q.y, q.z);
    let t = 2.0 * qv.cross(v);
    v + q.w * t + qv.cross(t)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform};
    use serde_json::json;

    fn make_obj(x: f64, y: f64, z: f64, sx: f64, sy: f64, sz: f64, yaw: f64) -> SceneObject {
        SceneObject {
            id: String::new(),
            scene: "scene:test".to_string(),
            group: None,
            mcp_id: "obj".to_string(),
            desired_name: "obj".to_string(),
            unreal_actor_name: None,
            actor_type: "StaticMeshActor".to_string(),
            asset_ref: json!({}),
            transform: Transform {
                location: Vec3 { x, y, z },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: sx,
                    y: sy,
                    z: sz,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags: vec![],
            metadata: json!({}),
            desired_hash: String::new(),
            last_applied_hash: None,
            sync_status: "pending".to_string(),
            deleted: false,
            revision: 1,
            created_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
            updated_at: surrealdb::sql::Datetime::from(chrono::Utc::now()),
        }
    }

    #[test]
    fn obb_from_scene_object() {
        let obj = make_obj(100.0, 200.0, 50.0, 2.0, 4.0, 1.0, 0.0);
        let obb = Obb3::from_scene_object(&obj);
        assert_eq!(obb.center.x, 100.0);
        assert_eq!(obb.half_extents.x, 100.0); // 2*50
        assert_eq!(obb.half_extents.y, 200.0); // 4*50
    }

    #[test]
    fn obb_vertices_count() {
        let obb = Obb3 {
            center: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 50.0,
                y: 50.0,
                z: 50.0,
            },
            rotation: DQuat::IDENTITY,
        };
        let verts = obb.vertices();
        assert_eq!(verts.len(), 8);
    }

    #[test]
    fn obb_to_aabb_matches_for_identity() {
        let obb = Obb3 {
            center: Vec3 {
                x: 100.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 50.0,
                y: 50.0,
                z: 50.0,
            },
            rotation: DQuat::IDENTITY,
        };
        let aabb = obb.to_aabb();
        assert_eq!(aabb.min.x, 50.0);
        assert_eq!(aabb.max.x, 150.0);
    }

    #[test]
    fn obb_intersects_overlapping() {
        let a = Obb3 {
            center: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 50.0,
                y: 50.0,
                z: 50.0,
            },
            rotation: DQuat::IDENTITY,
        };
        let b = Obb3 {
            center: Vec3 {
                x: 50.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 50.0,
                y: 50.0,
                z: 50.0,
            },
            rotation: DQuat::IDENTITY,
        };
        assert!(a.intersects_obb(&b, Cm::ZERO));
    }

    #[test]
    fn obb_does_not_interact_separated() {
        let a = Obb3 {
            center: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 10.0,
                y: 10.0,
                z: 10.0,
            },
            rotation: DQuat::IDENTITY,
        };
        let b = Obb3 {
            center: Vec3 {
                x: 100.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 10.0,
                y: 10.0,
                z: 10.0,
            },
            rotation: DQuat::IDENTITY,
        };
        assert!(!a.intersects_obb(&b, Cm::ZERO));
    }

    #[test]
    fn obb_contains_point_center() {
        let obb = Obb3 {
            center: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 50.0,
                y: 50.0,
                z: 50.0,
            },
            rotation: DQuat::IDENTITY,
        };
        assert!(obb.contains_point(
            &Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0
            },
            Cm::ZERO
        ));
    }

    #[test]
    fn obb_contains_point_outside() {
        let obb = Obb3 {
            center: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 10.0,
                y: 10.0,
                z: 10.0,
            },
            rotation: DQuat::IDENTITY,
        };
        assert!(!obb.contains_point(
            &Vec3 {
                x: 100.0,
                y: 0.0,
                z: 0.0,
            },
            Cm::ZERO,
        ));
    }

    #[test]
    fn rotated_obbs_intersect_when_expected() {
        // Two identical cubes at same center with 45° yaw should overlap
        let a = Obb3 {
            center: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 50.0,
                y: 10.0,
                z: 50.0,
            },
            rotation: DQuat::IDENTITY,
        };
        let b = Obb3 {
            center: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            half_extents: Vec3 {
                x: 50.0,
                y: 10.0,
                z: 50.0,
            },
            rotation: DQuat::from_euler(glam::EulerRot::YXZ, 45f64.to_radians(), 0.0, 0.0),
        };
        assert!(a.intersects_obb(&b, Cm::ZERO));
    }
}
