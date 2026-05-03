"""Integration tests for Level/Sublevel/World Partition/Advanced World tools.

These tests require a running Unreal Editor with the UnrealMCP plugin loaded.
Run with: python -m pytest Python/tests/integration/test_level_management.py -v
"""

import sys
import time
import unittest
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

from server.project_editor_tools import (
    level_tool,
    sublevel_tool,
    world_partition_tool,
    advanced_world_tool,
)
from server.core import get_unreal_connection


class TestLevelManagement(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal

    # ------------------------------------------------------------------
    # Phase 1: Basic Level CRUD
    # ------------------------------------------------------------------
    def test_01_create_level(self):
        result = level_tool(action="create", level_path="/Game/Maps/TestMCP_Integration")
        print(f"create_level: {result}")
        self.assertTrue(result.get("success"), f"create_level failed: {result.get('error')}")

    def test_02_save_level(self):
        result = level_tool(action="save", level_path="/Game/Maps/TestMCP_Integration")
        print(f"save_level: {result}")
        self.assertTrue(result.get("success"), f"save_level failed: {result.get('error')}")

    def test_03_load_level(self):
        result = level_tool(action="load", level_path="/Game/Maps/TestMCP_Integration")
        print(f"load_level: {result}")
        self.assertTrue(result.get("success"), f"load_level failed: {result.get('error')}")
        time.sleep(1.0)

    def test_04_duplicate_level(self):
        result = level_tool(
            action="duplicate",
            source_path="/Game/Maps/TestMCP_Integration",
            dest_path="/Game/Maps/TestMCP_Integration_Dup",
        )
        print(f"duplicate_level: {result}")
        self.assertTrue(result.get("success"), f"duplicate_level failed: {result.get('error')}")

    def test_05_rename_level(self):
        result = level_tool(
            action="rename",
            source_path="/Game/Maps/TestMCP_Integration_Dup",
            dest_path="/Game/Maps/TestMCP_Integration_Renamed",
        )
        print(f"rename_level: {result}")
        self.assertTrue(result.get("success"), f"rename_level failed: {result.get('error')}")

    def test_06_delete_level(self):
        result = level_tool(
            action="delete",
            level_path="/Game/Maps/TestMCP_Integration_Renamed",
        )
        print(f"delete_level: {result}")
        self.assertTrue(result.get("success"), f"delete_level failed: {result.get('error')}")

    # ------------------------------------------------------------------
    # Phase 2: Persistent Level / Sublevel
    # ------------------------------------------------------------------
    def test_10_get_persistent_level(self):
        result = sublevel_tool(action="get_persistent_level")
        print(f"get_persistent_level: {result}")
        self.assertTrue(result.get("success"), f"get_persistent_level failed: {result.get('error')}")
        self.assertIn("persistent_level", result)

    def test_11_add_sublevel(self):
        # Ensure the sublevel asset exists first
        level_tool(action="create", level_path="/Game/Maps/Sublevel_Test")
        level_tool(action="save", level_path="/Game/Maps/Sublevel_Test")
        result = sublevel_tool(
            action="add_sublevel",
            level_path="/Game/Maps/Sublevel_Test",
        )
        print(f"add_sublevel: {result}")
        self.assertTrue(result.get("success"), f"add_sublevel failed: {result.get('error')}")
        time.sleep(0.5)

    def test_12_set_sublevel_visible(self):
        result = sublevel_tool(
            action="set_sublevel_visible",
            level_path="/Game/Maps/Sublevel_Test",
            visible=True,
        )
        print(f"set_sublevel_visible: {result}")
        self.assertTrue(result.get("success"), f"set_sublevel_visible failed: {result.get('error')}")

    def test_13_set_sublevel_loaded(self):
        result = sublevel_tool(
            action="set_sublevel_loaded",
            level_path="/Game/Maps/Sublevel_Test",
            loaded=True,
        )
        print(f"set_sublevel_loaded: {result}")
        self.assertTrue(result.get("success"), f"set_sublevel_loaded failed: {result.get('error')}")

    def test_14_set_sublevel_streaming_settings(self):
        result = sublevel_tool(
            action="set_streaming_settings",
            level_path="/Game/Maps/Sublevel_Test",
            priority=10,
            streaming_distance=5000.0,
        )
        print(f"set_streaming_settings: {result}")
        self.assertTrue(result.get("success"), f"set_streaming_settings failed: {result.get('error')}")

    def test_15_remove_sublevel(self):
        result = sublevel_tool(
            action="remove_sublevel",
            level_path="/Game/Maps/Sublevel_Test",
        )
        print(f"remove_sublevel: {result}")
        self.assertTrue(result.get("success"), f"remove_sublevel failed: {result.get('error')}")

    # ------------------------------------------------------------------
    # Phase 3: World Partition (best-effort; may require WP-enabled map)
    # ------------------------------------------------------------------
    def test_20_get_world_partition_cells(self):
        result = world_partition_tool(action="get_cells")
        print(f"get_cells: {result}")
        # May fail if WP is not enabled; we just verify it doesn't crash
        self.assertIsNotNone(result)

    def test_21_set_world_partition_grid(self):
        result = world_partition_tool(
            action="set_grid",
            placement_grid_size=12800,
            foliage_grid_size=25600,
        )
        print(f"set_grid: {result}")
        self.assertTrue(result.get("success"), f"set_grid failed: {result.get('error')}")

    # ------------------------------------------------------------------
    # Phase 4: Advanced World (config-only / best-effort)
    # ------------------------------------------------------------------
    def test_30_set_one_file_per_actor(self):
        result = advanced_world_tool(
            action="set_one_file_per_actor",
            enabled=True,
        )
        print(f"set_one_file_per_actor: {result}")
        self.assertTrue(result.get("success"), f"set_one_file_per_actor failed: {result.get('error')}")

    def test_31_set_world_origin_rebasing(self):
        result = advanced_world_tool(
            action="set_world_origin_rebasing",
            enabled=True,
        )
        print(f"set_world_origin_rebasing: {result}")
        self.assertTrue(result.get("success"), f"set_world_origin_rebasing failed: {result.get('error')}")

    def test_32_create_data_layer(self):
        result = advanced_world_tool(action="create_data_layer", name="IntegrationLayer")
        print(f"create_data_layer: {result}")
        self.assertTrue(result.get("success"), f"create_data_layer failed: {result.get('error')}")

    def test_33_build_hlod(self):
        result = advanced_world_tool(action="build_hlod")
        print(f"build_hlod: {result}")
        # HLOD build launches a commandlet; verify it returns success
        self.assertTrue(result.get("success"), f"build_hlod failed: {result.get('error')}")


if __name__ == "__main__":
    unittest.main()
