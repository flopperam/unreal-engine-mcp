"""One-shot cleanup script to fix the current scene."""
import sys
sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parents[2]))

from server.core import get_unreal_connection
from server.actor_tools import delete_actor, spawn_actor
from server.project_editor_tools import viewport_tool, level_tool
import time

unreal = get_unreal_connection()
unreal.connect()

# Delete duplicate/old actors that accumulated from previous test runs
duplicates = [
    "MCP_SunLight", "MCP_PointLight1", "MCP_Floor",
    "MCP_Sun_0", "MCP_SkyLight_0", "MCP_Ground_0",
    "MCP_SkyAtmosphere_0", "MCP_HeightFog_0",
]
for name in duplicates:
    try:
        r = delete_actor(name=name)
        print(f"Deleted {name}: {r.get('success', False)}")
    except Exception as e:
        print(f"Skip {name}: {e}")
    time.sleep(0.2)

# Set camera to look at ground from a good vantage point
r = viewport_tool(
    action="set_camera_position",
    location=[0.0, -3000.0, 2000.0],
    rotation=[-25.0, 0.0, 0.0],
)
print(f"Camera set: {r.get('success', False)}")

# Save
r = level_tool(action="save", asset_path="/Game/Maps/E2E_Advanced_Main")
print(f"Saved: {r.get('success', False)}")
