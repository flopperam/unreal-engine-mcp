import json
import urllib.request
import time

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

def main():
    print("Testing Procedural Mesh E2E...")
    
    # 1. Create a simple triangle with UV and Color
    mcp_id = f"e2e_test_tri_{int(time.time())}"
    payload = {
        "mcp_id": mcp_id,
        "actor_name": "E2E_Test_Triangle",
        "vertex_count": 3,
        "index_count": 3,
        "positions": [
            [0.0, 0.0, 100.0],
            [100.0, 0.0, 100.0],
            [0.0, 100.0, 100.0]
        ],
        "normals": [
            [0.0, 0.0, 1.0],
            [0.0, 0.0, 1.0],
            [0.0, 0.0, 1.0]
        ],
        "uvs": [
            [0.0, 0.0],
            [1.0, 0.0],
            [0.0, 1.0]
        ],
        "indices": [0, 1, 2],
        "location": [500.0, 0.0, 200.0],
        "focus_viewport": True
    }
    
    print(f"Sending create-mesh request for {mcp_id}...")
    try:
        response = post_json("/procedural/create-mesh", payload)
        print("Response received:")
        print(json.dumps(response, indent=2))
        
        if response.get("success"):
            print("\nSUCCESS: Procedural mesh created successfully!")
            print(f"Actor Name: {response.get('actor_name')}")
            print(f"Triangle Count: {response.get('triangle_count')}")
            print(f"Bytes transferred: {response.get('bytes')}")
        else:
            print("\nFAILED: Server returned success=False")
            print(f"Error Code: {response.get('error_code')}")
            print(f"Error Message: {response.get('error')}")
            
    except Exception as e:
        print(f"\nERROR: Request failed: {e}")

if __name__ == "__main__":
    main()
