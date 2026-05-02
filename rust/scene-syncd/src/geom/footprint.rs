use crate::domain::SceneObject;
use crate::geom::obb::Obb3;
use crate::geom::polygon::Polygon2;
use crate::geom::units::Cm;

/// A 2D axis-aligned footprint in the XY plane derived from a scene object.
///
/// This is a dual-representation structure:
/// - **AABB fields** (`min_x` … `max_y`) are always present for broad-phase queries.
/// - **`polygon`** is populated when the object has non-trivial rotation, giving exact
///   polygon overlap via `geo::Polygon` boolean operations.
#[derive(Debug, Clone, PartialEq)]
pub struct Footprint2 {
    pub min_x: f64,
    pub max_x: f64,
    pub min_y: f64,
    pub max_y: f64,
    pub z: f64,
    pub mcp_id: String,
    /// Semantic kind extracted from tags (e.g. "layout_kind:keep")
    pub kind: String,
    /// Occlusion layer extracted from the kind registry (higher = above).
    pub layer: i32,
    /// Exact polygon for rotated objects. `None` for axis-aligned objects.
    pub polygon: Option<Polygon2>,
}

impl Footprint2 {
    pub fn from_scene_object(obj: &SceneObject, layer: i32) -> Self {
        let t = &obj.transform;
        // Treat scale as full size in cm for BasicShapes cubes/planes.
        let half_x = t.scale.x.abs() * 50.0;
        let half_y = t.scale.y.abs() * 50.0;

        let min_x = t.location.x - half_x;
        let max_x = t.location.x + half_x;
        let min_y = t.location.y - half_y;
        let max_y = t.location.y + half_y;

        let polygon = if has_non_trivial_rotation(t.rotation.pitch, t.rotation.yaw, t.rotation.roll)
        {
            let obb = Obb3::from_scene_object(obj);
            Some(polygon_from_obb_xy(&obb))
        } else {
            None
        };

        Self {
            min_x,
            max_x,
            min_y,
            max_y,
            z: t.location.z,
            mcp_id: obj.mcp_id.clone(),
            kind: extract_kind(&obj.tags).unwrap_or_default(),
            layer,
            polygon,
        }
    }

    /// Build a footprint from an OBB, projecting its 3D volume to the XY plane
    /// and computing the convex-hull polygon.
    pub fn from_scene_object_obb(obj: &SceneObject, layer: i32) -> Self {
        let obb = Obb3::from_scene_object(obj);
        Self::from_obb(
            &obb,
            obj.mcp_id.clone(),
            extract_kind(&obj.tags).unwrap_or_default(),
            layer,
        )
    }

    /// Build a footprint directly from an OBB.
    pub fn from_obb(obb: &Obb3, mcp_id: String, kind: String, layer: i32) -> Self {
        let verts = obb.vertices();
        let mut min_x = f64::INFINITY;
        let mut max_x = f64::NEG_INFINITY;
        let mut min_y = f64::INFINITY;
        let mut max_y = f64::NEG_INFINITY;
        for v in &verts {
            min_x = min_x.min(v.x);
            max_x = max_x.max(v.x);
            min_y = min_y.min(v.y);
            max_y = max_y.max(v.y);
        }
        let polygon = Some(polygon_from_obb_xy(obb));
        Self {
            min_x,
            max_x,
            min_y,
            max_y,
            z: obb.center.z,
            mcp_id,
            kind,
            layer,
            polygon,
        }
    }

    pub fn width(&self) -> f64 {
        self.max_x - self.min_x
    }

    pub fn height(&self) -> f64 {
        self.max_y - self.min_y
    }

    /// 2D overlap test.
    ///
    /// - Broad-phase: AABB rejection (fast).
    /// - Exact phase: if **both** footprints carry a `polygon`, use `geo::Polygon`
    ///   boolean intersection.  Otherwise fall back to the AABB result.
    pub fn intersects_2d(&self, other: &Footprint2, epsilon: Cm) -> bool {
        let e = epsilon.value();

        // Broad-phase: AABB rejection
        let aabb_overlap = self.max_x + e > other.min_x
            && self.min_x - e < other.max_x
            && self.max_y + e > other.min_y
            && self.min_y - e < other.max_y;

        if !aabb_overlap {
            return false;
        }

        // Exact phase: polygon overlap when both sides have geometry
        if let (Some(ref p1), Some(ref p2)) = (&self.polygon, &other.polygon) {
            p1.overlaps(p2)
        } else {
            // One or both lack polygon precision — AABB overlap is the best we can do.
            true
        }
    }

    pub fn is_surface_like(&self) -> bool {
        // Thin objects like ground, moat, road are surface-like.
        matches!(
            self.kind.as_str(),
            "ground" | "moat" | "road" | "bridge" | "water"
        )
    }
}

fn extract_kind(tags: &[String]) -> Option<String> {
    tags.iter()
        .find_map(|t| t.strip_prefix("layout_kind:").map(|s| s.to_string()))
}

/// Threshold (degrees) below which rotation is considered axis-aligned.
const ROTATION_EPSILON_DEG: f64 = 0.01;

fn has_non_trivial_rotation(pitch: f64, yaw: f64, roll: f64) -> bool {
    pitch.abs() > ROTATION_EPSILON_DEG
        || yaw.abs() > ROTATION_EPSILON_DEG
        || roll.abs() > ROTATION_EPSILON_DEG
}

/// Project an OBB to the XY plane and compute the convex-hull polygon.
fn polygon_from_obb_xy(obb: &Obb3) -> Polygon2 {
    let verts = obb.vertices();
    let mut xy: Vec<(f64, f64)> = verts.iter().map(|v| (v.x, v.y)).collect();

    // Deduplicate near-identical vertices (floating-point tolerance)
    xy.sort_by(|a, b| {
        let cx = a.0.partial_cmp(&b.0).unwrap();
        if cx == std::cmp::Ordering::Equal {
            a.1.partial_cmp(&b.1).unwrap()
        } else {
            cx
        }
    });
    let mut deduped: Vec<(f64, f64)> = Vec::new();
    const DEDUP_EPS: f64 = 1e-6;
    for &p in &xy {
        if let Some(last) = deduped.last() {
            if (p.0 - last.0).abs() < DEDUP_EPS && (p.1 - last.1).abs() < DEDUP_EPS {
                continue;
            }
        }
        deduped.push(p);
    }

    let hull = convex_hull_2d(&mut deduped);
    Polygon2 { exterior: hull }
}

/// Monotone-chain convex hull (Andrew's algorithm).
fn convex_hull_2d(points: &mut [(f64, f64)]) -> Vec<(f64, f64)> {
    let n = points.len();
    if n <= 1 {
        return points.to_vec();
    }

    points.sort_by(|a, b| {
        let cx = a.0.partial_cmp(&b.0).unwrap();
        if cx == std::cmp::Ordering::Equal {
            a.1.partial_cmp(&b.1).unwrap()
        } else {
            cx
        }
    });

    let mut lower = Vec::new();
    for &p in points.iter() {
        while lower.len() >= 2 && cross(lower[lower.len() - 2], lower[lower.len() - 1], p) <= 0.0 {
            lower.pop();
        }
        lower.push(p);
    }

    let mut upper = Vec::new();
    for &p in points.iter().rev() {
        while upper.len() >= 2 && cross(upper[upper.len() - 2], upper[upper.len() - 1], p) <= 0.0 {
            upper.pop();
        }
        upper.push(p);
    }

    lower.pop();
    upper.pop();
    lower.extend(upper);
    lower
}

fn cross(o: (f64, f64), a: (f64, f64), b: (f64, f64)) -> f64 {
    (a.0 - o.0) * (b.1 - o.1) - (a.1 - o.1) * (b.0 - o.0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::domain::{Rotator, SceneObject, Transform, Vec3};
    use serde_json::json;

    fn make_object(x: f64, y: f64, sx: f64, sy: f64, tags: Vec<String>) -> SceneObject {
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
                location: Vec3 { x, y, z: 0.0 },
                rotation: Rotator {
                    pitch: 0.0,
                    yaw: 0.0,
                    roll: 0.0,
                },
                scale: Vec3 {
                    x: sx,
                    y: sy,
                    z: 1.0,
                },
            },
            visual: json!({}),
            physics: json!({}),
            tags,
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

    fn make_rotated_object(
        x: f64,
        y: f64,
        sx: f64,
        sy: f64,
        yaw: f64,
        tags: Vec<String>,
    ) -> SceneObject {
        let mut obj = make_object(x, y, sx, sy, tags);
        obj.transform.rotation.yaw = yaw;
        obj
    }

    #[test]
    fn footprint_from_object() {
        let obj = make_object(100.0, 200.0, 2.0, 4.0, vec!["layout_kind:keep".to_string()]);
        let fp = Footprint2::from_scene_object(&obj, 0);
        assert_eq!(fp.min_x, 0.0); // 100 - 2*50
        assert_eq!(fp.max_x, 200.0); // 100 + 2*50
        assert_eq!(fp.min_y, 0.0); // 200 - 4*50
        assert_eq!(fp.max_y, 400.0); // 200 + 4*50
        assert_eq!(fp.kind, "keep");
        assert!(fp.polygon.is_none()); // axis-aligned
    }

    #[test]
    fn rotated_object_gets_polygon() {
        let obj = make_rotated_object(
            0.0,
            0.0,
            2.0,
            2.0,
            45.0,
            vec!["layout_kind:wall".to_string()],
        );
        let fp = Footprint2::from_scene_object(&obj, 0);
        assert!(fp.polygon.is_some());
        let poly = fp.polygon.unwrap();
        assert!(poly.exterior.len() >= 4); // convex hull of projected rectangle
    }

    #[test]
    fn footprints_intersect() {
        let a = Footprint2 {
            min_x: 0.0,
            max_x: 10.0,
            min_y: 0.0,
            max_y: 10.0,
            z: 0.0,
            mcp_id: "a".to_string(),
            kind: "keep".to_string(),
            layer: 0,
            polygon: None,
        };
        let b = Footprint2 {
            min_x: 5.0,
            max_x: 15.0,
            min_y: 5.0,
            max_y: 15.0,
            z: 0.0,
            mcp_id: "b".to_string(),
            kind: "keep".to_string(),
            layer: 0,
            polygon: None,
        };
        assert!(a.intersects_2d(&b, Cm::ZERO));
    }

    #[test]
    fn footprints_do_not_intersect() {
        let a = Footprint2 {
            min_x: 0.0,
            max_x: 10.0,
            min_y: 0.0,
            max_y: 10.0,
            z: 0.0,
            mcp_id: "a".to_string(),
            kind: "keep".to_string(),
            layer: 0,
            polygon: None,
        };
        let b = Footprint2 {
            min_x: 20.0,
            max_x: 30.0,
            min_y: 20.0,
            max_y: 30.0,
            z: 0.0,
            mcp_id: "b".to_string(),
            kind: "keep".to_string(),
            layer: 0,
            polygon: None,
        };
        assert!(!a.intersects_2d(&b, Cm::ZERO));
    }

    #[test]
    fn rotated_walls_get_polygon_precision() {
        let obj_a = make_rotated_object(
            0.0,
            0.0,
            2.0,
            0.2,
            45.0,
            vec!["layout_kind:curtain_wall".to_string()],
        );
        let obj_b = make_rotated_object(
            0.0,
            0.0,
            2.0,
            0.2,
            90.0,
            vec!["layout_kind:curtain_wall".to_string()],
        );
        let a = Footprint2::from_scene_object(&obj_a, 0);
        let b = Footprint2::from_scene_object(&obj_b, 0);

        // Both rotated objects get a polygon.
        assert!(a.polygon.is_some());
        assert!(b.polygon.is_some());

        // Their AABBs certainly overlap (both centered at origin)
        assert!(a.intersects_2d(&b, Cm::ZERO));
    }

    #[test]
    fn convex_hull_basic() {
        let mut pts = vec![
            (0.0, 0.0),
            (10.0, 0.0),
            (10.0, 10.0),
            (0.0, 10.0),
            (5.0, 5.0), // interior point
        ];
        let hull = convex_hull_2d(&mut pts);
        assert_eq!(hull.len(), 4);
    }

    use proptest::prelude::*;

    fn arb_footprint() -> impl Strategy<Value = Footprint2> {
        (any::<f64>(), any::<f64>(), any::<f64>(), any::<f64>())
            .prop_filter("valid aabb", |(_, _, w, h)| *w > 0.01 && *h > 0.01)
            .prop_map(|(min_x, min_y, w, h)| Footprint2 {
                min_x,
                max_x: min_x + w,
                min_y,
                max_y: min_y + h,
                z: 0.0,
                mcp_id: "a".to_string(),
                kind: "keep".to_string(),
                layer: 0,
                polygon: None,
            })
    }

    proptest! {
        #[test]
        fn intersects_2d_symmetric(a in arb_footprint(), b in arb_footprint()) {
            let ab = a.intersects_2d(&b, crate::geom::units::Cm::ZERO);
            let ba = b.intersects_2d(&a, crate::geom::units::Cm::ZERO);
            prop_assert_eq!(ab, ba, "intersects_2d must be symmetric");
        }
    }
}
