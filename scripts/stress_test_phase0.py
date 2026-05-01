import math
import time
import json
import urllib.request

SCENE_SYNCD_URL = "http://127.0.0.1:8787"

def post_json(path, payload):
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        f"{SCENE_SYNCD_URL}{path}",
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=120) as response: # Longer timeout for huge payload
        return json.loads(response.read().decode("utf-8"))

def generate_giant_terrain(resolution=1000):
    """
    1,000,000頂点（2,000,000ポリゴン）の巨大地形を生成
    """
    positions = []
    normals = []
    uvs = []
    colors = []
    indices = []
    
    size = 5000.0
    
    print(f"  - Calculating {resolution*resolution} vertices...")
    
    for y in range(resolution):
        fy = y / (resolution - 1)
        ny = fy * 2.0 - 1.0
        for x in range(resolution):
            fx = x / (resolution - 1)
            nx = fx * 2.0 - 1.0
            
            # Complex Terrain: Layered Sines
            z = (math.sin(nx * 5.0) * math.cos(ny * 5.0) * 200.0 + 
                 math.sin(nx * 15.0) * 50.0 + 
                 math.cos(ny * 25.0) * 20.0)
            
            positions.append([nx * size, ny * size, z])
            normals.append([0.0, 0.0, 1.0])
            uvs.append([fx * 10.0, fy * 10.0]) # 10x tiling
            
            # Color based on height (Snowy peaks)
            brightness = int(max(0, min(255, (z + 200) / 400 * 255)))
            colors.append([brightness, brightness, 255, 255])
            
    print(f"  - Generating { (resolution-1)*(resolution-1)*2 } triangles...")
    for y in range(resolution - 1):
        for x in range(resolution - 1):
            i0 = y * resolution + x
            i1 = i0 + 1
            i2 = (y + 1) * resolution + x
            i3 = i2 + 1
            indices.extend([i0, i1, i2])
            indices.extend([i1, i3, i2])
            
    return positions, normals, uvs, colors, indices

def main():
    print("🏔️ Phase 0 Stress Test: Generating 1,000,000 Vertices (AAA Scale)...")
    res = 1000
    
    start_calc = time.time()
    pos, norm, uv, col, idx = generate_giant_terrain(res)
    print(f"✅ Calculation complete in {time.time() - start_calc:.2f} s")
    
    mcp_id = "phase0_stress_test_terrain"
    payload = {
        "mcp_id": mcp_id,
        "actor_name": "AAA_Scale_Terrain_1M",
        "vertex_count": len(pos),
        "index_count": len(idx),
        "positions": pos,
        "normals": norm,
        "uvs": uv,
        "colors": col,
        "indices": idx,
        "location": [0.0, 0.0, 0.0],
        "material_path": "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial",
        "focus_viewport": True
    }
    
    # Calculate approximate raw size
    # pos(12) + norm(12) + uv(8) + col(4) = 36 bytes per vertex
    # indices = 4 bytes per index
    total_raw_bytes = len(pos) * 36 + len(idx) * 4 + 104
    print(f"📡 Sending {total_raw_bytes / 1024 / 1024:.2f} MB via binary pipeline...")
    
    start_send = time.time()
    try:
        response = post_json("/procedural/create-mesh", payload)
        elapsed = (time.time() - start_send) * 1000
        
        if response.get("success"):
            u_resp = response["data"]["unreal_response"]
            print("\n👑 STRESS TEST SUCCESS!")
            print(f"⏱️ Total E2E Latency: {elapsed/1000:.2f} s")
            print(f"🏗️ UE Mesh Build Time: {u_resp['build_time_ms']/1000:.2f} s")
            print(f"📦 Transfer Speed: { (total_raw_bytes/1024/1024) / (u_resp['transfer_time_ms']/1000):.2f} MB/s")
            print("\nUnreal Engine 上に、広大な「100万頂点の地形」が出現しました。")
            print("これだけの物量を一瞬で扱えるのが、本システムの真の力です。")
        else:
            print(f"\n❌ Error: {response.get('error')}")
            
    except Exception as e:
        print(f"\n❌ Request failed: {e}")

if __name__ == "__main__":
    main()
