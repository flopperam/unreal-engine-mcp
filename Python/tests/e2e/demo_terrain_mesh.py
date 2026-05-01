"""Generate a large terrain-like mesh for E2E demonstration."""

import math
import random
import json
import urllib.request


def generate_terrain_mesh(size: float = 6000.0, resolution: int = 100):
    """Generate a grid terrain with ~10K vertices."""
    positions = []
    normals = []
    uvs = []
    indices = []

    random.seed(42)

    # Generate height map
    heights = {}
    for y in range(resolution + 1):
        for x in range(resolution + 1):
            fx = x / resolution
            fy = y / resolution
            # Multiple octaves of noise
            h = (
                math.sin(fx * 8) * math.cos(fy * 8) * 180 +
                math.sin(fx * 16 + 1) * math.cos(fy * 16 + 2) * 90 +
                math.sin(fx * 32 + 3) * math.cos(fy * 32 + 4) * 45 +
                random.uniform(-8, 8)
            )
            heights[(x, y)] = h

    # Generate vertices
    for y in range(resolution + 1):
        for x in range(resolution + 1):
            fx = x / resolution
            fy = y / resolution
            px = (fx - 0.5) * size
            py = (fy - 0.5) * size
            pz = heights[(x, y)]
            positions.append([px, py, pz])
            uvs.append([fx, fy])

    # Compute normals from heights
    for y in range(resolution + 1):
        for x in range(resolution + 1):
            # Sample neighbors for normal
            hL = heights.get((x - 1, y), heights[(x, y)])
            hR = heights.get((x + 1, y), heights[(x, y)])
            hD = heights.get((x, y - 1), heights[(x, y)])
            hU = heights.get((x, y + 1), heights[(x, y)])

            dx = (hR - hL) / (size / resolution)
            dy = (hU - hD) / (size / resolution)

            nx = -dx
            ny = -dy
            nz = 2.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            normals.append([nx / length, ny / length, nz / length])

    # Generate indices (two triangles per quad)
    for y in range(resolution):
        for x in range(resolution):
            a = y * (resolution + 1) + x
            b = a + 1
            c = (y + 1) * (resolution + 1) + x
            d = c + 1
            indices.extend([a, c, b])
            indices.extend([b, c, d])

    return positions, normals, uvs, indices


def api_post(path: str, payload: dict) -> dict:
    url = f"http://127.0.0.1:8787{path}"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=300) as resp:
        return json.loads(resp.read().decode("utf-8"))


if __name__ == "__main__":
    positions, normals, uvs, indices = generate_terrain_mesh(size=6000.0, resolution=100)

    print(f"Generated terrain: {len(positions)} vertices, {len(indices)} indices")
    print(f"Payload size: ~{24 + len(positions)*36 + len(indices)*4} bytes")

    result = api_post("/procedural/create-mesh", {
        "vertex_count": len(positions),
        "index_count": len(indices),
        "positions": positions,
        "normals": normals,
        "uvs": uvs,
        "indices": indices,
        "actor_name": "E2E_Terrain_10K",
        "material_path": "",
        "flags": 1,
        "location": [0.0, 0.0, 150.0],
        "scale": [1.0, 1.0, 1.0],
        "focus_viewport": True,
    })

    print(json.dumps(result, indent=2, ensure_ascii=False))
