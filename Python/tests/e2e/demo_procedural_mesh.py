"""Spawn a clearly visible procedural triangle for E2E demonstration."""

import json
import urllib.request


def generate_visible_triangle_mesh(size: float = 2400.0, height: float = 900.0, thickness: float = 180.0):
    """Generate a large triangular slab raised above the terrain."""
    positions = [
        [0.0, -thickness, height],
        [size, -thickness, height],
        [size * 0.5, -thickness, height + size * 0.75],
        [0.0, thickness, height],
        [size, thickness, height],
        [size * 0.5, thickness, height + size * 0.75],
    ]
    normals = [[0.0, 0.0, 1.0] for _ in positions]
    uvs = [[0.0, 0.0] for _ in positions]
    indices = [
        0, 1, 2,  # front
        5, 4, 3,  # back
        0, 3, 4, 0, 4, 1,  # bottom edge
        1, 4, 5, 1, 5, 2,  # right edge
        2, 5, 3, 2, 3, 0,  # left edge
    ]
    return positions, normals, uvs, indices


def api_post(path: str, payload: dict) -> dict:
    url = f"http://127.0.0.1:8787{path}"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=300) as resp:
        return json.loads(resp.read().decode("utf-8"))


if __name__ == "__main__":
    positions, normals, uvs, indices = generate_visible_triangle_mesh()

    print(f"Generated visible triangle slab: {len(positions)} vertices, {len(indices)} indices")

    result = api_post("/procedural/create-mesh", {
        "vertex_count": len(positions),
        "index_count": len(indices),
        "positions": positions,
        "normals": normals,
        "uvs": uvs,
        "indices": indices,
        "actor_name": "E2E_Visible_Triangle",
        "material_path": "",
        "flags": 1,
        "focus_viewport": True,
    })

    print(json.dumps(result, indent=2, ensure_ascii=False))
