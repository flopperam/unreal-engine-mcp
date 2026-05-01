import json
import urllib.request
import time

def api_post(path: str, payload: dict) -> dict:
    url = f"http://127.0.0.1:8787{path}"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=300) as resp:
        return json.loads(resp.read().decode("utf-8"))

def generate_mesh(z_offset: float):
    positions = [
        [0.0, 0.0, z_offset],
        [100.0, 0.0, z_offset],
        [0.0, 100.0, z_offset],
    ]
    normals = [[0.0, 0.0, 1.0] for _ in range(3)]
    indices = [0, 1, 2, 2, 1, 0]
    return positions, normals, indices

if __name__ == "__main__":
    ACTOR_NAME = "Idempotency_Test_Mesh"
    
    # 1. First upsert
    print(f"--- First upsert: {ACTOR_NAME} at Z=1000 ---")
    pos, norm, idx = generate_mesh(1000.0)
    res1 = api_post("/procedural/create-mesh", {
        "vertex_count": len(pos),
        "index_count": len(idx),
        "positions": pos,
        "normals": norm,
        "indices": idx,
        "actor_name": ACTOR_NAME,
        "focus_viewport": True,
    })
    print(f"Result 1: {json.dumps(res1, indent=2)}")
    
    time.sleep(2)
    
    # 2. Second upsert (same name, different geometry)
    print(f"--- Second upsert: {ACTOR_NAME} at Z=1500 ---")
    pos, norm, idx = generate_mesh(1500.0)
    res2 = api_post("/procedural/create-mesh", {
        "vertex_count": len(pos),
        "index_count": len(idx),
        "positions": pos,
        "normals": norm,
        "indices": idx,
        "actor_name": ACTOR_NAME,
        "focus_viewport": True,
    })
    print(f"Result 2: {json.dumps(res2, indent=2)}")
    
    print("\nTest finished. Check Unreal Editor for a single actor named 'Idempotency_Test_Mesh' with updated Z height.")
