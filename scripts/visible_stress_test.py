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
    with urllib.request.urlopen(request, timeout=120) as response:
        return json.loads(response.read().decode("utf-8"))

def generate_visible_terrain(resolution=500):
    """
    250,000頂点の「確実に見える」巨大地形を生成
    """
    positions = []
    normals = []
    uvs = []
    colors = []
    indices = []
    
    size = 3000.0
    
    print(f"  - Calculating {resolution*resolution} vertices...")
    
    for y in range(resolution):
        fy = y / (resolution - 1)
        ny = fy * 2.0 - 1.0
        for x in range(resolution):
            fx = x / (resolution - 1)
            nx = fx * 2.0 - 1.0
            
            # Complex Terrain: Layered Sines (Centered at Z=300 to avoid floor)
            z = (math.sin(nx * 4.0) * math.cos(ny * 4.0) * 300.0 + 
                 math.sin(nx * 10.0) * 100.0) + 500.0 # Lift it up!
            
            positions.append([nx * size, ny * size, z])
            
            # Approximate Normal calculation for better lighting
            dz_dx = 4.0 * math.cos(nx * 4.0) * math.cos(ny * 4.0) * 300.0 / size
            dz_dy = -4.0 * math.sin(nx * 4.0) * math.sin(ny * 4.0) * 300.0 / size
            vec_n = [-dz_dx, -dz_dy, 1.0]
            mag = math.sqrt(sum(c*c for c in vec_n))
            normals.append([c/mag for c in vec_n])
            
            uvs.append([fx * 5.0, fy * 5.0])
            
            # Dramatic Colors (Red to Blue based on height)
            r = int(max(0, min(255, (z - 200) / 600 * 255)))
            b = 255 - r
            colors.append([r, 120, b, 255])
            
    print(f"  - Generating indices...")
    for y in range(resolution - 1):
        for x in range(resolution - 1):
            i0 = y * resolution + x
            i1 = i0 + 1
            i2 = (y + 1) * resolution + x
            i3 = i2 + 1
            # Maintain CCW winding
            indices.extend([i0, i1, i2])
            indices.extend([i1, i3, i2])
            
    return positions, normals, uvs, colors, indices

def main():
    print("🏔️ Phase 0 Final Demo: Generating Opaque AAA Terrain...")
    res = 500
    
    start_calc = time.time()
    pos, norm, uv, col, idx = generate_visible_terrain(res)
    
    mcp_id = "phase0_visible_terrain"
    payload = {
        "mcp_id": mcp_id,
        "actor_name": "Visible_AAA_Terrain",
        "vertex_count": len(pos),
        "index_count": len(idx),
        "positions": pos,
        "normals": norm,
        "uvs": uv,
        "colors": col,
        "indices": idx,
        "location": [0.0, 0.0, 0.0],
        "material_path": "/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial", # Proven visible
        "focus_viewport": True
    }
    
    print(f"📡 Sending {len(pos)} vertices...")
    try:
        response = post_json("/procedural/create-mesh", payload)
        if response.get("success"):
            print("\n✨ STRESS TEST SUCCESS!")
            print("Unreal Editor を確認してください。")
            print("もし見えない場合は、Outlinerで 'Visible_AAA_Terrain' を選択して 'F' キーを押してください。")
        else:
            print(f"\n❌ Error: {response.get('error')}")
    except Exception as e:
        print(f"\n❌ Request failed: {e}")

if __name__ == "__main__":
    main()
