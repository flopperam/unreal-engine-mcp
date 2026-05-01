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
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))

def generate_hyper_surface(resolution=100):
    """
    10,000頂点の虹色の数理曲面を生成
    """
    positions = []
    normals = []
    uvs = []
    colors = []
    indices = []
    
    size = 1000.0
    half_res = resolution // 2
    
    # Generate vertices
    for y in range(resolution):
        for x in range(resolution):
            # Normalize coords to [-1, 1]
            nx = (x / (resolution - 1)) * 2.0 - 1.0
            ny = (y / (resolution - 1)) * 2.0 - 1.0
            
            # Mathematical surface: Wavy ripple
            dist = math.sqrt(nx*nx + ny*ny)
            z = math.sin(dist * 10.0) * 100.0
            
            positions.append([nx * size, ny * size, z])
            
            # Simple normal (pointing up-ish)
            normals.append([0.0, 0.0, 1.0])
            
            # UV: Tiling
            uvs.append([x / 10.0, y / 10.0])
            
            # Color: Rainbow based on position and height
            r = int((nx + 1.0) * 127)
            g = int((ny + 1.0) * 127)
            b = int((math.sin(z * 0.05) + 1.0) * 127)
            colors.append([r, g, b, 255])
            
    # Generate indices (triangles)
    for y in range(resolution - 1):
        for x in range(resolution - 1):
            i0 = y * resolution + x
            i1 = i0 + 1
            i2 = (y + 1) * resolution + x
            i3 = i2 + 1
            
            # Triangle 1
            indices.extend([i0, i1, i2])
            # Triangle 2
            indices.extend([i1, i3, i2])
            
    return positions, normals, uvs, colors, indices

def main():
    print("🚀 Generating Phase 0 Showpiece: Rainbow Hyper-Surface...")
    res = 100
    pos, norm, uv, col, idx = generate_hyper_surface(res)
    
    mcp_id = "phase0_perfect_showpiece"
    payload = {
        "mcp_id": mcp_id,
        "actor_name": "Perfect_Phase0_Showpiece",
        "vertex_count": len(pos),
        "index_count": len(idx),
        "positions": pos,
        "normals": norm,
        "uvs": uv,
        "colors": col,
        "indices": idx,
        "location": [0.0, 0.0, 300.0],
        "material_path": "/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial",
        "focus_viewport": True
    }
    
    print(f"📡 Sending {len(pos)} vertices and {len(idx)//3} triangles via binary pipeline...")
    start = time.time()
    
    try:
        response = post_json("/procedural/create-mesh", payload)
        elapsed = (time.time() - start) * 1000
        
        if response.get("success"):
            u_resp = response["data"]["unreal_response"]
            print("\n✨ SUCCESS! Look at your Unreal Engine Editor screen.")
            print(f"⏱️ Total E2E Latency: {elapsed:.2f} ms")
            print(f"📦 Data Size: {u_resp['bytes'] / 1024:.1f} KB")
            print(f"🏗️ UE Build Time: {u_resp['build_time_ms']:.2f} ms")
            print("\nこの複雑な形状、虹色のカラー、そしてテクスチャ座標が、")
            print("『一瞬』で転送・描画されました。これがPhase 0の真骨頂です。")
        else:
            print(f"\n❌ Error: {response.get('error')}")
            
    except Exception as e:
        print(f"\n❌ Request failed: {e}")

if __name__ == "__main__":
    main()
