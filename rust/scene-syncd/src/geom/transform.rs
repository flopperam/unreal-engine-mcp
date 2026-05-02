use crate::domain::{Rotator, Transform, Vec3};
use glam::{DQuat, DVec3};

/// Convert domain Vec3 to glam DVec3 for internal computation.
pub fn to_glam_vec3(v: &Vec3) -> DVec3 {
    DVec3::new(v.x, v.y, v.z)
}

/// Convert glam DVec3 back to domain Vec3.
pub fn from_glam_vec3(v: DVec3) -> Vec3 {
    Vec3 {
        x: v.x,
        y: v.y,
        z: v.z,
    }
}

/// Convert domain Rotator (pitch/yaw/roll in degrees) to glam quaternion.
pub fn to_glam_quat(r: &Rotator) -> DQuat {
    let yaw = r.yaw.to_radians();
    let pitch = r.pitch.to_radians();
    let roll = r.roll.to_radians();
    DQuat::from_euler(glam::EulerRot::YXZ, yaw, pitch, roll)
}

/// Convert glam quaternion back to domain Rotator (pitch/yaw/roll in degrees).
pub fn from_glam_quat(q: DQuat) -> Rotator {
    let (yaw, pitch, roll) = q.to_euler(glam::EulerRot::YXZ);
    Rotator {
        pitch: (pitch as f64).to_degrees(),
        yaw: (yaw as f64).to_degrees(),
        roll: (roll as f64).to_degrees(),
    }
}

/// Transform a point by domain Transform (first scale, then rotate, then translate).
pub fn transform_point(t: &Transform, p: &Vec3) -> Vec3 {
    let scaled = DVec3::new(p.x * t.scale.x, p.y * t.scale.y, p.z * t.scale.z);
    let rotated = to_glam_quat(&t.rotation).mul_vec3(scaled);
    let translated = rotated + to_glam_vec3(&t.location);
    from_glam_vec3(translated)
}

/// Compute the world-space AABB corners of a unit cube transformed by `t`.
pub fn transform_aabb_corners(t: &Transform) -> [Vec3; 8] {
    let q = to_glam_quat(&t.rotation);
    let s = to_glam_vec3(&t.scale);
    let loc = to_glam_vec3(&t.location);

    let mut corners: [Vec3; 8] = core::array::from_fn(|_| Vec3 {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    });
    for i in 0..8usize {
        let dx = if i & 1 != 0 { 0.5 } else { -0.5 };
        let dy = if i & 2 != 0 { 0.5 } else { -0.5 };
        let dz = if i & 4 != 0 { 0.5 } else { -0.5 };
        let local = DVec3::new(dx * s.x, dy * s.y, dz * s.z);
        let world = q.mul_vec3(local) + loc;
        corners[i] = from_glam_vec3(world);
    }
    corners
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip_vec3() {
        let v = Vec3 {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        };
        let g = to_glam_vec3(&v);
        let back = from_glam_vec3(g);
        assert_eq!(v, back);
    }

    #[test]
    fn identity_transform_point() {
        let t = Transform::default();
        let p = Vec3 {
            x: 10.0,
            y: 0.0,
            z: 0.0,
        };
        let out = transform_point(&t, &p);
        assert!((out.x - 10.0).abs() < 1e-9);
        assert!((out.y).abs() < 1e-9);
        assert!((out.z).abs() < 1e-9);
    }

    #[test]
    fn scale_then_translate() {
        let t = Transform {
            location: Vec3 {
                x: 100.0,
                y: 0.0,
                z: 0.0,
            },
            rotation: Rotator {
                pitch: 0.0,
                yaw: 0.0,
                roll: 0.0,
            },
            scale: Vec3 {
                x: 2.0,
                y: 1.0,
                z: 1.0,
            },
        };
        let p = Vec3 {
            x: 10.0,
            y: 0.0,
            z: 0.0,
        };
        let out = transform_point(&t, &p);
        // scaled to 20,0,0 then translated by 100,0,0 = 120,0,0
        assert!((out.x - 120.0).abs() < 1e-9);
    }

    #[test]
    fn yaw_rotates_around_y() {
        let t = Transform {
            location: Vec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            rotation: Rotator {
                pitch: 0.0,
                yaw: 90.0,
                roll: 0.0,
            },
            scale: Vec3 {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
        };
        let p = Vec3 {
            x: 1.0,
            y: 0.0,
            z: 0.0,
        };
        let out = transform_point(&t, &p);
        // glam uses Y-up right-handed: 90 deg yaw around Y sends +X toward -Z
        assert!((out.x).abs() < 1e-9);
        assert!((out.z + 1.0).abs() < 1e-9);
    }
}
