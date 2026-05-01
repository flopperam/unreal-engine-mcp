"""Verify the large terrain mesh actually exists in Unreal."""

import json
import socket


def unreal_command(command: str, params: dict = None) -> dict:
    payload = json.dumps({"command": command, "params": params or {}}).encode("utf-8") + b"\n"
    with socket.create_connection(("127.0.0.1", 55557), timeout=10) as s:
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.sendall(payload)
        data = bytearray()
        while b"\n" not in data:
            chunk = s.recv(262144)
            if not chunk:
                raise ConnectionError("Peer closed")
            data.extend(chunk)
        return json.loads(bytes(data).split(b"\n", 1)[0].decode("utf-8"))


if __name__ == "__main__":
    # First, spawn the large terrain
    import urllib.request

    def api_post(path: str, payload: dict) -> dict:
        url = f"http://127.0.0.1:8787{path}"
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(url, data=data, method="POST")
        req.add_header("Content-Type", "application/json")
        with urllib.request.urlopen(req, timeout=300) as resp:
            return json.loads(resp.read().decode("utf-8"))

    print("Spawning 10K terrain mesh...")

    import math, random

    random.seed(42)
    resolution = 100
    size = 6000.0
    heights = {}
    for y in range(resolution + 1):
        for x in range(resolution + 1):
            fx, fy = x / resolution, y / resolution
            heights[(x, y)] = (
                math.sin(fx * 8) * math.cos(fy * 8) * 180 +
                math.sin(fx * 16 + 1) * math.cos(fy * 16 + 2) * 90 +
                math.sin(fx * 32 + 3) * math.cos(fy * 32 + 4) * 45 +
                random.uniform(-8, 8)
            )

    positions, normals, uvs, indices = [], [], [], []
    for y in range(resolution + 1):
        for x in range(resolution + 1):
            fx, fy = x / resolution, y / resolution
            px, py = (fx - 0.5) * size, (fy - 0.5) * size
            pz = heights[(x, y)]
            positions.append([px, py, pz])
            uvs.append([fx, fy])

    for y in range(resolution + 1):
        for x in range(resolution + 1):
            hL = heights.get((x - 1, y), heights[(x, y)])
            hR = heights.get((x + 1, y), heights[(x, y)])
            hD = heights.get((x, y - 1), heights[(x, y)])
            hU = heights.get((x, y + 1), heights[(x, y)])
            dx = (hR - hL) / (size / resolution)
            dy = (hU - hD) / (size / resolution)
            nx, ny, nz = -dx, -dy, 2.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            normals.append([nx / length, ny / length, nz / length])

    for y in range(resolution):
        for x in range(resolution):
            a = y * (resolution + 1) + x
            b = a + 1
            c = (y + 1) * (resolution + 1) + x
            d = c + 1
            indices.extend([a, c, b])
            indices.extend([b, c, d])

    result = api_post("/procedural/create-mesh", {
        "vertex_count": len(positions),
        "index_count": len(indices),
        "positions": positions,
        "normals": normals,
        "uvs": uvs,
        "indices": indices,
        "actor_name": "Terrain_10K_Live",
        "material_path": "",
        "flags": 1,
        "location": [0.0, 0.0, 150.0],
        "scale": [1.0, 1.0, 1.0],
        "focus_viewport": True,
    })

    print(json.dumps(result, indent=2, ensure_ascii=False))

    # Verify actor exists in Unreal
    print("\nVerifying actor exists in Unreal...")
    actors_resp = unreal_command("get_actors_in_level", {})
    actors = actors_resp.get("result", {}).get("actors", [])

    found = [a for a in actors if a.get("name") == "Terrain_10K_Live"]
    if found:
        print(f"FOUND: {found[0]}")
    else:
        names = [a.get("name") for a in actors]
        print(f"Actor not found. Level contains {len(names)} actors:")
        for n in names:
            print(f"  - {n}")
