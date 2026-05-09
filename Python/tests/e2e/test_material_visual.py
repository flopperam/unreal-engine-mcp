"""Visual E2E test: spawn a cube, paint it red, screenshot, paint it blue, screenshot.

Requires a running Unreal Editor with MCP Bridge.
"""

import os
import time
from pathlib import Path

import pytest

from .conftest import unreal_command


@pytest.mark.requires_unreal
class TestMaterialVisualColorChange:
    """End-to-end visual test: actor color change via material instance."""

    def test_red_then_blue_cube_screenshots(self, tmp_path: Path):
        ts = time.strftime("%H%M%S")
        actor_name = f"VisualTestCube_{ts}"
        mic_name = f"E2E_VisualMIC_{ts}"
        mic_path = f"/Game/Materials/{mic_name}"

        # 1. Spawn cube
        result = unreal_command("spawn_actor", {
            "type": "StaticMeshActor",
            "name": actor_name,
            "location": [0.0, 0.0, 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube",
        })
        assert result.get("success") is True, f"spawn_actor failed: {result}"
        spawned_name = result.get("name", actor_name)

        # 2. Create a red material instance
        mic_result = unreal_command("create_material_instance", {
            "parent_material": "/Engine/BasicShapes/BasicShapeMaterial",
            "instance_name": mic_name,
            "package_path": "/Game/Materials/",
        })
        assert mic_result.get("success") is True, f"create_material_instance failed: {mic_result}"

        # 3. Set color to red
        red_result = unreal_command("batch_update_material_parameters", {
            "instance_path": mic_path,
            "parameters": [
                {"name": "Color", "type": "vector", "value": [1.0, 0.0, 0.0, 1.0]},
            ],
        })
        # Color parameter may not exist on BasicShapeMaterial; fallback to scalar
        if not red_result.get("success"):
            red_result = unreal_command("batch_update_material_parameters", {
                "instance_path": mic_path,
                "parameters": [
                    {"name": "BaseColor", "type": "vector", "value": [1.0, 0.0, 0.0, 1.0]},
                ],
            })
        assert red_result.get("success") is True, f"batch_update (red) failed: {red_result}"

        # 4. Apply material to actor
        apply_result = unreal_command("apply_material_to_actor", {
            "actor_name": spawned_name,
            "material_path": mic_path,
            "material_slot": 0,
        })
        assert apply_result.get("success") is True, f"apply_material_to_actor failed: {apply_result}"

        # 5. Screenshot (red)
        red_path = tmp_path / "screenshot_red.png"
        ss_red = unreal_command("take_screenshot", {
            "output_path": str(red_path),
        })
        assert ss_red.get("success") is True, f"take_screenshot (red) failed: {ss_red}"

        # 6. Change color to blue
        blue_result = unreal_command("batch_update_material_parameters", {
            "instance_path": mic_path,
            "parameters": [
                {"name": "Color", "type": "vector", "value": [0.0, 0.0, 1.0, 1.0]},
            ],
        })
        if not blue_result.get("success"):
            blue_result = unreal_command("batch_update_material_parameters", {
                "instance_path": mic_path,
                "parameters": [
                    {"name": "BaseColor", "type": "vector", "value": [0.0, 0.0, 1.0, 1.0]},
                ],
            })
        assert blue_result.get("success") is True, f"batch_update (blue) failed: {blue_result}"

        # Allow a frame for material update
        time.sleep(0.5)

        # 7. Screenshot (blue)
        blue_path = tmp_path / "screenshot_blue.png"
        ss_blue = unreal_command("take_screenshot", {
            "output_path": str(blue_path),
        })
        assert ss_blue.get("success") is True, f"take_screenshot (blue) failed: {ss_blue}"

        # 8. Assert files exist
        assert red_path.exists(), f"Red screenshot not found: {red_path}"
        assert blue_path.exists(), f"Blue screenshot not found: {blue_path}"

        # Cleanup
        unreal_command("delete_actor", {"actor_name": spawned_name})
        unreal_command("delete_asset", {"asset_path": mic_path})

        # Report paths for human inspection
        print(f"\n[VisualTest] Red screenshot:  {red_path}")
        print(f"[VisualTest] Blue screenshot: {blue_path}")
