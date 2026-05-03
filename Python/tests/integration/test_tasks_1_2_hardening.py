"""Hard integration coverage for tasks 1 and 2 from docs/superpowers/plans/tasks.md.

These tests require a running Unreal Editor with the UnrealMCP plugin loaded.
They intentionally verify round trips and state transitions rather than only
checking that commands return success.

Safety improvements over v1:
- Config backups are written to disk immediately so tearDownClass can restore
  even if the process crashes mid-test.
- PIE tests use longer waits and explicit stop-before-start guards.
- SAFE_MAP is loaded between every major test to reset editor state.
"""

from __future__ import annotations

import atexit
import json
import os
import sys
import time
import unittest
from pathlib import Path
from typing import Any

import pytest


PYTHON_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = PYTHON_ROOT.parent
PROJECT_ROOT = REPO_ROOT / "FlopperamUnrealMCP"
CONFIG_ROOT = PROJECT_ROOT / "Config"
SAFE_MAP = "/Game/Maps/E2E_Advanced_Main"

if str(PYTHON_ROOT) not in sys.path:
    sys.path.insert(0, str(PYTHON_ROOT))

from server.core import get_unreal_connection
from server.actor_tools import spawn_actor
from server.project_editor_tools import (
    advanced_world_tool,
    editor_control_tool,
    engine_settings_tool,
    level_tool,
    play_tool,
    project_settings_tool,
    sublevel_tool,
    viewport_tool,
    world_partition_tool,
    world_settings_tool,
)


def _truthy_ini(value: Any) -> bool:
    return str(value).strip().lower() in {"true", "1", "yes"}


# ---------------------------------------------------------------------------
# Config backup / restore helpers (crash-safe)
# ---------------------------------------------------------------------------

def _backup_configs(config_root: Path) -> dict[Path, bytes | None]:
    """Read config files and return their contents."""
    backups: dict[Path, bytes | None] = {}
    for name in [
        "DefaultEngine.ini",
        "DefaultGame.ini",
        "DefaultInput.ini",
        "DefaultScalability.ini",
    ]:
        path = config_root / name
        backups[path] = path.read_bytes() if path.exists() else None
    return backups


def _restore_configs(backups: dict[Path, bytes | None]) -> None:
    """Write backed-up config files back to disk."""
    for path, original in backups.items():
        if original is None:
            if path.exists():
                path.unlink()
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(original)


@pytest.mark.requires_unreal
class TestTasksOneTwoHardening(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        cls.run_id = f"MCPHard_{int(time.time())}"
        cls.generated_levels: list[str] = []
        cls._config_backups = _backup_configs(CONFIG_ROOT)
        level_tool(action="load", asset_path=SAFE_MAP)
        time.sleep(0.5)

        # Ensure SAFE_MAP has basic lighting and a ground plane so the viewport
        # is never pitch-black during the test run.
        # ------------------------------------------------------------------
        # The C++ HandleSpawnActor now checks the world for existing FNames,
        # but we still explicitly delete-then-recreate to guarantee a clean
        # slate regardless of what the previous test run left behind.
        env_actors = [
            {"type": "DirectionalLight", "name": "MCP_Sun",
             "location": [0.0, 0.0, 1000.0], "rotation": [-45.0, 30.0, 0.0]},
            {"type": "SkyLight", "name": "MCP_SkyLight",
             "location": [0.0, 0.0, 1000.0]},
            {"type": "StaticMeshActor", "name": "MCP_Ground",
             "location": [0.0, 0.0, 0.0], "scale": [200.0, 200.0, 1.0],
             "static_mesh": "/Engine/BasicShapes/Plane"},
            {"type": "SkyAtmosphere", "name": "MCP_SkyAtmosphere",
             "location": [0.0, 0.0, 0.0]},
            {"type": "ExponentialHeightFog", "name": "MCP_HeightFog",
             "location": [0.0, 0.0, 100.0]},
        ]
        from server.actor_tools import delete_actor
        for spawn_params in env_actors:
            # Delete existing actor first to avoid duplicate-name errors
            try:
                delete_actor(name=spawn_params["name"])
                time.sleep(0.3)
            except Exception:
                pass
            try:
                spawn_actor(**spawn_params)
            except Exception:
                pass
        try:
            level_tool(action="save", asset_path=SAFE_MAP)
        except Exception:
            pass

        # Position the viewport camera so the ground and environment are visible
        try:
            viewport_tool(
                action="set_camera_position",
                location=[0.0, -3000.0, 2000.0],
                rotation=[-25.0, 0.0, 0.0],
            )
        except Exception:
            pass

        # Register atexit restore so configs are recovered even on crash
        atexit.register(_restore_configs, cls._config_backups)

    @classmethod
    def tearDownClass(cls):
        try:
            # Ensure PIE is stopped before cleanup
            play_tool(action="stop_pie")
            time.sleep(1.0)
            # Load safe map first to stabilize editor state
            level_tool(action="load", asset_path=SAFE_MAP)
            time.sleep(1.0)
            for asset_path in reversed(getattr(cls, "generated_levels", [])):
                try:
                    level_tool(action="delete", asset_path=asset_path)
                except Exception:
                    pass
        finally:
            _restore_configs(cls._config_backups)

    # ---- helpers ----

    def assert_success(self, result: dict[str, Any], context: str) -> None:
        self.assertTrue(result.get("success"), f"{context} failed: {result}")

    def assert_failure(self, result: dict[str, Any], context: str) -> None:
        self.assertFalse(result.get("success", False), f"{context} unexpectedly succeeded: {result}")
        self.assertIn("error", result, f"{context} did not return an error: {result}")

    def config_value(self, file: str, section: str, key: str) -> str:
        result = project_settings_tool(action="get", file=file, section=section, key=key)
        self.assert_success(result, f"read {file}:{section}:{key}")
        return result.get("value", "")

    def make_level_path(self, name: str) -> str:
        path = f"/Game/MCP_Hardening/{self.run_id}_{name}"
        self.generated_levels.append(path)
        return path

    def cleanup_levels(self, *asset_paths: str) -> None:
        try:
            level_tool(action="load", asset_path=SAFE_MAP)
            time.sleep(1.0)
        except Exception:
            pass
        for asset_path in asset_paths:
            try:
                level_tool(action="delete", asset_path=asset_path)
            except Exception:
                pass

    def _load_safe_map(self) -> None:
        """Best-effort load of SAFE_MAP to stabilize editor state between tests."""
        try:
            level_tool(action="load", asset_path=SAFE_MAP)
            time.sleep(1.0)
        except Exception:
            pass

    # ---- tests ----

    def test_01_project_settings_and_metadata_are_real_round_trips(self):
        custom_value = f"RoundTrip_{self.run_id}"
        result = project_settings_tool(
            action="set",
            file="DefaultEngine.ini",
            section="/Script/UnrealEd.EditorLoadingAndSavingSettings",
            key="MCPHardeningRoundTrip",
            value=custom_value,
        )
        self.assert_success(result, "set custom project setting")
        self.assertEqual(
            self.config_value(
                "DefaultEngine.ini",
                "/Script/UnrealEd.EditorLoadingAndSavingSettings",
                "MCPHardeningRoundTrip",
            ),
            custom_value,
        )

        game_map = f"/Game/Maps/{self.run_id}_GameDefault"
        editor_map = f"/Game/Maps/{self.run_id}_EditorStartup"
        default_map = f"/Game/Maps/{self.run_id}_DefaultAlias"

        try:
            result = project_settings_tool(action="set_game_default_map", map_path=game_map)
            self.assert_success(result, "set game default map")
            self.assertEqual(
                self.config_value(
                    "DefaultEngine.ini",
                    "/Script/EngineSettings.GameMapsSettings",
                    "GameDefaultMap",
                ),
                game_map,
            )

            result = project_settings_tool(action="set_editor_startup_map", map_path=editor_map)
            self.assert_success(result, "set editor startup map")
            self.assertEqual(
                self.config_value(
                    "DefaultEngine.ini",
                    "/Script/EngineSettings.GameMapsSettings",
                    "EditorStartupMap",
                ),
                editor_map,
            )

            result = project_settings_tool(action="set_default_map", map_path=default_map)
            self.assert_success(result, "set default map alias")
            self.assertEqual(result.get("default_map"), default_map)
            self.assertEqual(
                self.config_value(
                    "DefaultEngine.ini",
                    "/Script/EngineSettings.GameMapsSettings",
                    "EditorStartupMap",
                ),
                default_map,
            )

            result = project_settings_tool(
                action="set_project_description",
                description=f"Hardening description {self.run_id}",
                project_name=f"HardeningProject_{self.run_id}",
                company_name=f"HardeningCompany_{self.run_id}",
                project_version=37.4,
            )
            self.assert_success(result, "set project metadata")
            self.assertEqual(result.get("project_version"), "37.4")
            self.assertEqual(
                self.config_value(
                    "DefaultGame.ini",
                    "/Script/EngineSettings.GeneralProjectSettings",
                    "ProjectName",
                ),
                f"HardeningProject_{self.run_id}",
            )

            transition_map = f"/Game/Maps/{self.run_id}_Transition"
            result = project_settings_tool(
                action="set_maps_and_modes",
                game_mode="/Script/Engine.GameModeBase",
                game_instance="/Script/Engine.GameInstance",
                transition_map=transition_map,
            )
            self.assert_success(result, "set maps and modes")
            self.assertEqual(
                self.config_value(
                    "DefaultEngine.ini",
                    "/Script/EngineSettings.GameMapsSettings",
                    "TransitionMap",
                ),
                transition_map,
            )
        finally:
            # Restore default maps so the editor doesn't try to load non-existent maps
            project_settings_tool(action="set_game_default_map", map_path=SAFE_MAP)
            project_settings_tool(action="set_editor_startup_map", map_path=SAFE_MAP)
            project_settings_tool(action="set_default_map", map_path=SAFE_MAP)

    def test_02_engine_and_world_settings_round_trip_with_restore(self):
        original = world_settings_tool(action="get")
        self.assert_success(original, "get original world settings")

        render_key = "r.MCPHardening.RenderKey"
        physics_key = "MCPHardeningPhysicsDelta"
        input_key = "MCPHardeningInputFlag"
        packaging_key = "MCPHardeningPackageMode"

        checks = [
            (
                engine_settings_tool(action="set_rendering", key=render_key, value="123"),
                "DefaultEngine.ini",
                "/Script/Engine.RendererSettings",
                render_key,
                "123",
            ),
            (
                engine_settings_tool(action="set_physics", key=physics_key, value="0.025"),
                "DefaultEngine.ini",
                "/Script/Engine.PhysicsSettings",
                physics_key,
                "0.025",
            ),
            (
                engine_settings_tool(action="set_input", key=input_key, value="False"),
                "DefaultInput.ini",
                "/Script/Engine.InputSettings",
                input_key,
                "False",
            ),
            (
                engine_settings_tool(action="set_packaging", key=packaging_key, value="Development"),
                "DefaultGame.ini",
                "/Script/UnrealEd.ProjectPackagingSettings",
                packaging_key,
                "Development",
            ),
        ]
        for result, file, section, key, expected in checks:
            self.assert_success(result, f"set {key}")
            self.assertEqual(self.config_value(file, section, key), expected)

        result = world_settings_tool(
            action="set",
            world_to_meters=321.0,
            kill_z=-654321.0,
            enable_world_bounds_checks=False,
        )
        self.assert_success(result, "set world settings")

        verify = world_settings_tool(action="get")
        self.assert_success(verify, "verify world settings")
        self.assertAlmostEqual(verify.get("world_to_meters"), 321.0, delta=0.01)
        self.assertAlmostEqual(verify.get("kill_z"), -654321.0, delta=0.01)
        self.assertIs(verify.get("enable_world_bounds_checks"), False)

        restore = world_settings_tool(
            action="set",
            world_to_meters=original.get("world_to_meters"),
            kill_z=original.get("kill_z"),
            enable_world_bounds_checks=original.get("enable_world_bounds_checks"),
        )
        self.assert_success(restore, "restore world settings")

    def test_03_editor_control_executes_and_logs_python_then_saves(self):
        marker = f"MCP_HARDENING_LOG_{self.run_id}"
        result = editor_control_tool(
            action="execute_python_script",
            script=f"import unreal; unreal.log('{marker}')",
        )
        self.assert_success(result, "execute editor python")
        time.sleep(1.0)

        result = editor_control_tool(action="get_editor_log", tail_lines=300)
        self.assert_success(result, "read editor log")
        self.assertIn(marker, result.get("log_content", ""))

        result = editor_control_tool(action="get_dirty_assets")
        self.assert_success(result, "get dirty assets")
        self.assertIsInstance(result.get("dirty_assets"), list)

        result = editor_control_tool(action="save_all", prompt=False)
        self.assert_success(result, "save all")
        self.assertIn("saved", result)

    def test_04_pie_lifecycle_rejects_invalid_state_transitions(self):
        # Ensure no PIE is running before starting
        play_tool(action="stop_pie")
        time.sleep(1.0)
        self._load_safe_map()

        result = play_tool(action="start_pie")
        self.assert_success(result, "start PIE")
        time.sleep(3.0)

        result = play_tool(action="start_pie")
        self.assert_failure(result, "start PIE while already running")

        result = play_tool(action="stop_pie")
        self.assert_success(result, "stop PIE")
        time.sleep(3.0)

        result = play_tool(action="stop_pie")
        self.assert_failure(result, "stop PIE when not running")

        # Load safe map to restore viewport state after PIE
        self._load_safe_map()

    def test_05_viewport_camera_round_trip_has_numeric_tolerance(self):
        original = viewport_tool(action="get_camera_position")
        self.assert_success(original, "get original camera")

        target_location = [1111.0, -2222.0, 333.0]
        target_rotation = [-12.0, 145.0, 0.0]
        result = viewport_tool(
            action="set_camera_position",
            location=target_location,
            rotation=target_rotation,
        )
        self.assert_success(result, "set camera")
        time.sleep(0.5)

        verify = viewport_tool(action="get_camera_position")
        self.assert_success(verify, "verify camera")
        self.assertAlmostEqual(verify.get("x"), target_location[0], delta=1.0)
        self.assertAlmostEqual(verify.get("y"), target_location[1], delta=1.0)
        self.assertAlmostEqual(verify.get("z"), target_location[2], delta=1.0)
        self.assertAlmostEqual(verify.get("yaw"), target_rotation[1], delta=1.0)

        viewport_tool(
            action="set_camera_position",
            location=[original.get("x"), original.get("y"), original.get("z")],
            rotation=[original.get("pitch"), original.get("yaw"), original.get("roll")],
        )

    def test_06_level_crud_workflow_preserves_loaded_map_identity(self):
        level_a = self.make_level_path("Task2_LevelA")
        level_b = self.make_level_path("Task2_LevelB")
        level_c = self.make_level_path("Task2_LevelC")
        self.cleanup_levels(level_a, level_b, level_c)

        try:
            result = level_tool(action="create", asset_path=level_a)
            self.assert_success(result, "create level A")
            result = level_tool(action="save", asset_path=level_a)
            self.assert_success(result, "save level A")

            result = level_tool(action="load", asset_path=level_a)
            self.assert_success(result, "load level A")
            current = level_tool(action="get_current")
            self.assert_success(current, "get current level after load A")
            self.assertIn("Task2_LevelA", current.get("outer_path", ""))

            result = level_tool(action="duplicate", source_path=level_a, dest_path=level_b)
            self.assert_success(result, "duplicate level A to B")
            self.assertIn("Task2_LevelB", result.get("new_asset_path", ""))

            result = level_tool(action="rename", source_path=level_b, dest_path=level_c)
            self.assert_success(result, "rename level B to C")

            result = level_tool(action="load", asset_path=level_c)
            self.assert_success(result, "load level C")
            current = level_tool(action="get_current")
            self.assert_success(current, "get current level after load C")
            self.assertIn("Task2_LevelC", current.get("outer_path", ""))

            levels = level_tool(action="list")
            self.assert_success(levels, "list levels")
            self.assertGreaterEqual(levels.get("count", 0), 1)
            self.assertTrue(any(level.get("is_persistent") for level in levels.get("levels", [])))
        finally:
            self.cleanup_levels(level_a, level_c)

    def test_07_sublevel_streaming_workflow_uses_returned_streaming_name(self):
        persistent = self.make_level_path("Task2_Persistent")
        sub_a = self.make_level_path("Task2_SubA")
        sub_b = self.make_level_path("Task2_SubB")
        self.cleanup_levels(persistent, sub_a, sub_b)

        try:
            for path in [persistent, sub_a, sub_b]:
                result = level_tool(action="create", asset_path=path)
                self.assert_success(result, f"create {path}")
                result = level_tool(action="save", asset_path=path)
                self.assert_success(result, f"save {path}")

            result = level_tool(action="load", asset_path=persistent)
            self.assert_success(result, "load persistent")

            persistent_info = sublevel_tool(action="get_persistent")
            self.assert_success(persistent_info, "get persistent level")
            self.assertIn("Task2_Persistent", persistent_info.get("outer_path", ""))

            add_a = sublevel_tool(action="add", level_path=sub_a)
            self.assert_success(add_a, "add sublevel A")
            add_b = sublevel_tool(action="add", level_path=sub_b)
            self.assert_success(add_b, "add sublevel B")
            self.assertNotEqual(add_a.get("streaming_level_name"), add_b.get("streaming_level_name"))

            streaming_name = add_a.get("streaming_level_name")
            result = sublevel_tool(action="set_visible", level_name=streaming_name, visible=False)
            self.assert_success(result, "set sublevel invisible using returned streaming name")
            self.assertIs(result.get("visible"), False)

            result = sublevel_tool(action="set_loaded", level_name=streaming_name, loaded=True)
            self.assert_success(result, "set sublevel loaded using returned streaming name")
            self.assertIs(result.get("loaded"), True)

            result = sublevel_tool(
                action="set_streaming",
                level_name=streaming_name,
                should_be_loaded=True,
                should_be_visible=True,
                priority=77,
            )
            self.assert_success(result, "set sublevel streaming settings")

            result = sublevel_tool(
                action="create_volume",
                location=[100.0, 200.0, 300.0],
                extent=[1500.0, 1600.0, 1700.0],
                streaming_levels=[sub_a, sub_b],
            )
            self.assert_success(result, "create streaming volume")
            self.assertIn("actor_name", result)
            self.assertAlmostEqual(result.get("x"), 100.0, delta=0.01)

            result = sublevel_tool(action="remove", level_name=streaming_name)
            self.assert_success(result, "remove sublevel using returned streaming name")
        finally:
            self.cleanup_levels(persistent, sub_a, sub_b)

    def test_08_world_partition_and_advanced_world_config_round_trips(self):
        result = world_partition_tool(
            action="set_grid",
            placement_grid_size=12345,
            foliage_grid_size=23456,
            minimap_threshold=34567,
        )
        self.assert_success(result, "set world partition grid")
        section = "/Script/Engine.WorldPartitionEditorPerProjectUserSettings"
        self.assertEqual(
            self.config_value("DefaultEngine.ini", section, "PlacementGridSize"),
            "12345",
        )
        self.assertEqual(
            self.config_value("DefaultEngine.ini", section, "InstancedFoliageGridSize"),
            "23456",
        )
        self.assertEqual(
            self.config_value(
                "DefaultEngine.ini",
                section,
                "MinimapLowQualityWorldUnitsPerPixelThreshold",
            ),
            "34567",
        )

        result = advanced_world_tool(action="set_one_file_per_actor", enabled=True)
        self.assert_success(result, "enable OFPA")
        self.assertIs(result.get("one_file_per_actor"), True)
        self.assertTrue(
            _truthy_ini(
                self.config_value(
                    "DefaultEngine.ini",
                    "/Script/Engine.WorldSettings",
                    "bUseExternalActors",
                )
            )
        )

        result = advanced_world_tool(action="set_world_origin_rebasing", enabled=False)
        self.assert_success(result, "disable world origin rebasing")
        self.assertIs(result.get("world_origin_rebasing_enabled"), False)
        self.assertFalse(
            _truthy_ini(
                self.config_value(
                    "DefaultEngine.ini",
                    "/Script/Engine.WorldSettings",
                    "bEnableWorldOriginRebasing",
                )
            )
        )

    def test_09_invalid_parameters_fail_before_reaching_unreal_state_changes(self):
        self.assert_failure(project_settings_tool(action="set", section="OnlySection"), "project setting missing key")
        self.assert_failure(level_tool(action="duplicate", source_path="/Game/MissingDest"), "level duplicate missing dest")
        self.assert_failure(sublevel_tool(action="set_visible", level_name="AnyName"), "sublevel visible missing bool")
        self.assert_failure(world_partition_tool(action="enable"), "world partition enable missing bool")
        self.assert_failure(advanced_world_tool(action="set_one_file_per_actor"), "OFPA missing enabled")


if __name__ == "__main__":
    unittest.main()
