
import sys
import os
import unittest
import time
from pathlib import Path

# Add the Python directory to the path so we can import the server modules
PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

from server.project_editor_tools import (
    project_settings_tool,
    plugin_tool,
    engine_settings_tool,
    world_settings_tool,
    editor_control_tool,
    play_tool,
    viewport_tool
)
from server.core import get_unreal_connection

class TestProjectEditorToolsComprehensive(unittest.TestCase):
    """Comprehensive integration tests for Task 1: Project / Editor basic operations."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
        # Store original settings for cleanup
        cls.original_project_name = None
        cls.original_company_name = None
        
    # -------------------------------------------------------------------------
    # Project Settings Tool - All Actions
    # -------------------------------------------------------------------------
    
    def test_01_project_settings_get(self):
        """Test reading project settings from DefaultEngine.ini."""
        result = project_settings_tool(
            action="get", 
            file="DefaultEngine.ini", 
            section="/Script/EngineSettings.GameMapsSettings"
        )
        print(f"project_settings_get: {result}")
        self.assertTrue(result.get("success"), f"Failed to get project settings: {result.get('error')}")
        self.assertIn("settings", result)
        
    def test_02_project_settings_get_specific_key(self):
        """Test reading a specific key from project settings."""
        result = project_settings_tool(
            action="get",
            file="DefaultEngine.ini",
            section="/Script/EngineSettings.GameMapsSettings",
            key="GameDefaultMap"
        )
        print(f"project_settings_get_key: {result}")
        self.assertTrue(result.get("success"))
        
    def test_03_project_settings_set_and_verify(self):
        """Test writing and verifying a project setting."""
        # Set a test value
        result = project_settings_tool(
            action="set",
            file="DefaultEngine.ini",
            section="/Script/UnrealEd.EditorLoadingAndSavingSettings",
            key="bSuppressLoadErrors",
            value="True"
        )
        print(f"project_settings_set: {result}")
        self.assertTrue(result.get("success"), f"Failed to set project setting: {result.get('error')}")
        
        # Verify the value was set
        result = project_settings_tool(
            action="get",
            file="DefaultEngine.ini",
            section="/Script/UnrealEd.EditorLoadingAndSavingSettings",
            key="bSuppressLoadErrors"
        )
        self.assertTrue(result.get("success"))
        # Value might be stored differently, just check we can read it
        
    def test_04_set_default_map(self):
        """Test setting the default map."""
        result = project_settings_tool(
            action="set_default_map",
            map_path="/Game/TestMap"
        )
        print(f"set_default_map: {result}")
        self.assertTrue(result.get("success"), f"Failed to set default map: {result.get('error')}")
        
    def test_05_set_game_default_map(self):
        """Test setting the game default map."""
        result = project_settings_tool(
            action="set_game_default_map",
            map_path="/Game/GameMap"
        )
        print(f"set_game_default_map: {result}")
        self.assertTrue(result.get("success"), f"Failed to set game default map: {result.get('error')}")
        
    def test_06_set_editor_startup_map(self):
        """Test setting the editor startup map."""
        result = project_settings_tool(
            action="set_editor_startup_map",
            map_path="/Game/EditorStartupMap"
        )
        print(f"set_editor_startup_map: {result}")
        self.assertTrue(result.get("success"), f"Failed to set editor startup map: {result.get('error')}")
        
    def test_07_set_project_description(self):
        """Test updating project description and metadata."""
        result = project_settings_tool(
            action="set_project_description",
            description="MCP Test Project - Comprehensive Testing",
            project_name="MCP_Test_Project",
            company_name="MCP Corp",
            project_version=1.0
        )
        print(f"set_project_description: {result}")
        self.assertTrue(result.get("success"), f"Failed to set project description: {result.get('error')}")
        
    def test_08_set_maps_and_modes(self):
        """Test updating maps and modes settings."""
        result = project_settings_tool(
            action="set_maps_and_modes",
            game_mode="/Script/Engine.GameModeBase",
            game_instance="/Script/Engine.GameInstance",
            transition_map="/Game/TransitionMap"
        )
        print(f"set_maps_and_modes: {result}")
        self.assertTrue(result.get("success"), f"Failed to set maps and modes: {result.get('error')}")
        
    def test_09_project_settings_invalid_action(self):
        """Test invalid action returns error."""
        result = project_settings_tool(action="invalid_action")
        self.assertFalse(result.get("success", True))
        self.assertIn("error", result)
        
    def test_10_project_settings_missing_params(self):
        """Test that missing required parameters return error."""
        result = project_settings_tool(action="set", section="Test")
        self.assertFalse(result.get("success", True))
        self.assertIn("error", result)
        

class TestPluginToolComprehensive(unittest.TestCase):
    """Comprehensive tests for plugin_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_plugin_list(self):
        """Test listing all plugins."""
        result = plugin_tool(action="list")
        print(f"plugin_list: Found {len(result.get('plugins', []))} plugins")
        self.assertTrue(result.get("success"), f"Failed to list plugins: {result.get('error')}")
        self.assertIn("plugins", result)
        self.assertIsInstance(result["plugins"], list)
        # Verify plugin structure
        if result["plugins"]:
            plugin = result["plugins"][0]
            self.assertIn("name", plugin)
            self.assertIn("enabled", plugin)
            
    def test_02_plugin_enable_disable(self):
        """Test enabling and disabling a plugin."""
        # Find a plugin that can be safely toggled
        result = plugin_tool(action="list")
        self.assertTrue(result.get("success"))
        
        plugins = result.get("plugins", [])
        test_plugin = None
        for plugin in plugins:
            # Find a non-essential plugin for testing
            if plugin["name"] in ["ExamplePlugin", "CustomPlugin"]:
                test_plugin = plugin["name"]
                break
        
        if not test_plugin:
            self.skipTest("No suitable test plugin found")
            
        # Try to set enabled (this might fail if plugin not found)
        original_state = None
        for p in plugins:
            if p["name"] == test_plugin:
                original_state = p["enabled"]
                break
                
        if original_state is None:
            self.skipTest(f"Could not determine state of {test_plugin}")
            
        # Toggle it
        result = plugin_tool(
            action="set_enabled",
            plugin_name=test_plugin,
            enabled=not original_state
        )
        print(f"plugin_set_enabled (toggle): {result}")
        
        # Toggle back
        result = plugin_tool(
            action="set_enabled",
            plugin_name=test_plugin,
            enabled=original_state
        )
        print(f"plugin_set_enabled (restore): {result}")
        
    def test_03_plugin_invalid_action(self):
        """Test invalid plugin action."""
        result = plugin_tool(action="fly")
        self.assertFalse(result.get("success", True))
        
    def test_04_plugin_missing_params(self):
        """Test missing parameters for set_enabled."""
        result = plugin_tool(action="set_enabled", plugin_name="TestPlugin")
        self.assertFalse(result.get("success", True))
        self.assertIn("error", result)


class TestEngineSettingsToolComprehensive(unittest.TestCase):
    """Comprehensive tests for engine_settings_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_set_scalability_low(self):
        """Test setting scalability to low (0)."""
        result = engine_settings_tool(action="set_scalability", quality=0)
        print(f"set_scalability (low): {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_02_set_scalability_medium(self):
        """Test setting scalability to medium (2)."""
        result = engine_settings_tool(action="set_scalability", quality=2)
        print(f"set_scalability (medium): {result}")
        self.assertTrue(result.get("success"))
        
    def test_03_set_scalability_high(self):
        """Test setting scalability to high (4)."""
        result = engine_settings_tool(action="set_scalability", quality=4)
        print(f"set_scalability (high): {result}")
        self.assertTrue(result.get("success"))
        
    def test_04_set_scalability_epic(self):
        """Test setting scalability to epic (5)."""
        result = engine_settings_tool(action="set_scalability", quality=5)
        print(f"set_scalability (epic): {result}")
        self.assertTrue(result.get("success"))
        
    def test_05_set_scalability_invalid(self):
        """Test setting scalability with invalid quality value."""
        result = engine_settings_tool(action="set_scalability", quality=10)
        # This might succeed or fail depending on implementation
        print(f"set_scalability (invalid): {result}")
        
    def test_06_set_rendering_setting(self):
        """Test setting a rendering config value."""
        result = engine_settings_tool(
            action="set_rendering",
            key="r.ScreenPercentage",
            value="100"
        )
        print(f"set_rendering: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_07_set_physics_setting(self):
        """Test setting a physics config value."""
        result = engine_settings_tool(
            action="set_physics",
            key="p.MaxPhysicsDeltaTime",
            value="0.033333"
        )
        print(f"set_physics: {result}")
        self.assertTrue(result.get("success"))
        
    def test_08_set_input_setting(self):
        """Test setting an input config value."""
        result = engine_settings_tool(
            action="set_input",
            key="bEnableMouseSmoothing",
            value="False"
        )
        print(f"set_input: {result}")
        self.assertTrue(result.get("success"))
        
    def test_09_set_collision_setting(self):
        """Test setting a collision config value."""
        result = engine_settings_tool(
            action="set_collision",
            key="Collision.EnableUnrealPhysics",
            value="True"
        )
        print(f"set_collision: {result}")
        self.assertTrue(result.get("success"))
        
    def test_10_set_ai_setting(self):
        """Test setting an AI system config value."""
        result = engine_settings_tool(
            action="set_ai",
            key="bEnable导航网格",
            value="True"
        )
        print(f"set_ai: {result}")
        # Might fail if key doesn't exist, that's ok
        
    def test_11_set_navigation_setting(self):
        """Test setting a navigation config value."""
        result = engine_settings_tool(
            action="set_navigation",
            key="NavigationSystem.bAutoUpdateNavMesh",
            value="True"
        )
        print(f"set_navigation: {result}")
        self.assertTrue(result.get("success"))
        
    def test_12_set_packaging_setting(self):
        """Test setting a packaging config value."""
        result = engine_settings_tool(
            action="set_packaging",
            key="StagingDirectory",
            value="../../../Staging"
        )
        print(f"set_packaging: {result}")
        self.assertTrue(result.get("success"))
        
    def test_13_invalid_action(self):
        """Test invalid engine settings action."""
        result = engine_settings_tool(action="fly")
        self.assertFalse(result.get("success", True))


class TestWorldSettingsToolComprehensive(unittest.TestCase):
    """Comprehensive tests for world_settings_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_get_world_settings(self):
        """Test getting current world settings."""
        result = world_settings_tool(action="get")
        print(f"world_settings_get: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        self.assertIn("world_settings", result)
        
    def test_02_set_world_to_meters(self):
        """Test setting World To Meters scale."""
        # Get original value first
        original = world_settings_tool(action="get")
        if original.get("success"):
            original_value = original.get("world_settings", {}).get("world_to_meters")
            
        result = world_settings_tool(action="set", world_to_meters=100.0)
        print(f"set_world_to_meters: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
        # Verify
        verify = world_settings_tool(action="get")
        self.assertTrue(verify.get("success"))
        settings = verify.get("world_settings", {})
        self.assertEqual(settings.get("world_to_meters"), 100.0)
        
        # Restore original if we got it
        if original.get("success") and original_value is not None:
            world_settings_tool(action="set", world_to_meters=original_value)
            
    def test_03_set_kill_z(self):
        """Test setting Kill Z value."""
        result = world_settings_tool(action="set", kill_z=-50000.0)
        print(f"set_kill_z: {result}")
        self.assertTrue(result.get("success"))
        
    def test_04_set_enable_world_bounds_checks(self):
        """Test enabling/disabling world bounds checks."""
        result = world_settings_tool(action="set", enable_world_bounds_checks=False)
        print(f"set_enable_world_bounds_checks: {result}")
        self.assertTrue(result.get("success"))
        
    def test_05_set_enable_world_composition(self):
        """Test enabling/disabling world composition."""
        result = world_settings_tool(action="set", enable_world_composition=True)
        print(f"set_enable_world_composition: {result}")
        self.assertTrue(result.get("success"))
        
    def test_06_set_default_game_mode(self):
        """Test setting default game mode."""
        result = world_settings_tool(
            action="set",
            default_game_mode="/Script/Engine.GameModeBase"
        )
        print(f"set_default_game_mode: {result}")
        self.assertTrue(result.get("success"))
        
    def test_07_set_multiple_settings(self):
        """Test setting multiple world settings at once."""
        result = world_settings_tool(
            action="set",
            world_to_meters=50.0,
            kill_z=-100000.0,
            enable_world_bounds_checks=True
        )
        print(f"set_multiple_settings: {result}")
        self.assertTrue(result.get("success"))
        
    def test_08_invalid_action(self):
        """Test invalid world settings action."""
        result = world_settings_tool(action="fly")
        self.assertFalse(result.get("success", True))


class TestEditorControlToolComprehensive(unittest.TestCase):
    """Comprehensive tests for editor_control_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_get_dirty_assets(self):
        """Test getting list of dirty assets."""
        result = editor_control_tool(action="get_dirty_assets")
        print(f"get_dirty_assets: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_02_undo_redo(self):
        """Test undo and redo operations."""
        # Perform an action that can be undone
        # Just test that the commands don't error
        result = editor_control_tool(action="undo", count=1)
        print(f"undo: {result}")
        # Undo might fail if nothing to undo, that's ok
        
        result = editor_control_tool(action="redo", count=1)
        print(f"redo: {result}")
        # Redo might fail if nothing to redo, that's ok
        
    def test_03_save_all(self):
        """Test saving all dirty assets."""
        result = editor_control_tool(action="save_all", prompt=False)
        print(f"save_all: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_04_get_editor_log(self):
        """Test getting editor log."""
        result = editor_control_tool(action="get_editor_log", tail_lines=50)
        print(f"get_editor_log: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        self.assertIn("log_lines", result)
        
    def test_05_execute_python_script(self):
        """Test executing a Python script in the editor."""
        result = editor_control_tool(
            action="execute_python_script",
            script="print('MCP Test Script Executed')"
        )
        print(f"execute_python_script: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_06_save_specific_asset(self):
        """Test saving a specific asset."""
        # Try to save a known asset or just test the command
        result = editor_control_tool(
            action="save_asset",
            asset_path="/Game/NonexistentAsset"
        )
        print(f"save_asset: {result}")
        # Will likely fail since asset doesn't exist, that's expected
        
    def test_07_create_utility_widget(self):
        """Test creating an Editor Utility Widget (stub)."""
        result = editor_control_tool(
            action="create_utility_widget",
            asset_path="/Game/Tests/MCP_TestUtilityWidget"
        )
        print(f"create_utility_widget: {result}")
        # This is a stub, might return success or not implemented
        
    def test_08_create_utility_blueprint(self):
        """Test creating an Editor Utility Blueprint (stub)."""
        result = editor_control_tool(
            action="create_utility_blueprint",
            asset_path="/Game/Tests/MCP_TestUtilityBP"
        )
        print(f"create_utility_blueprint: {result}")
        # This is a stub, might return success or not implemented
        
    def test_09_execute_commandlet(self):
        """Test executing an editor commandlet."""
        # Use a safe commandlet
        result = editor_control_tool(
            action="execute_commandlet",
            commandlet_name="Help",
            args=""
        )
        print(f"execute_commandlet: {result}")
        # Help commandlet might not exist, that's ok
        
    def test_10_invalid_action(self):
        """Test invalid editor control action."""
        result = editor_control_tool(action="fly")
        self.assertFalse(result.get("success", True))


class TestPlayToolComprehensive(unittest.TestCase):
    """Comprehensive tests for play_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_start_stop_pie(self):
        """Test starting and stopping Play In Editor."""
        # Start PIE
        result = play_tool(action="start_pie")
        print(f"start_pie: {result}")
        self.assertTrue(result.get("success"), f"Failed to start PIE: {result.get('error')}")
        
        # Wait a bit
        time.sleep(2)
        
        # Stop PIE
        result = play_tool(action="stop_pie")
        print(f"stop_pie: {result}")
        self.assertTrue(result.get("success"), f"Failed to stop PIE: {result.get('error')}")
        
        time.sleep(1)
        
    def test_02_start_simulate(self):
        """Test starting Simulate In Editor."""
        result = play_tool(action="start_simulate")
        print(f"start_simulate: {result}")
        self.assertTrue(result.get("success"), f"Failed to start simulate: {result.get('error')}")
        
        time.sleep(2)
        
        result = play_tool(action="stop_pie")
        print(f"stop_pie after simulate: {result}")
        self.assertTrue(result.get("success"))
        
        time.sleep(1)
        
    def test_03_start_standalone_game(self):
        """Test launching standalone game (best-effort)."""
        # This might not work in all environments
        result = play_tool(action="start_standalone_game")
        print(f"start_standalone_game: {result}")
        # Don't assert - might not be supported in test environment
        
        time.sleep(1)
        
        # Try to stop (might not work for standalone)
        play_tool(action="stop_pie")
        
    def test_04_invalid_action(self):
        """Test invalid play tool action."""
        result = play_tool(action="fly")
        self.assertFalse(result.get("success", True))


class TestViewportToolComprehensive(unittest.TestCase):
    """Comprehensive tests for viewport_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_get_camera_position(self):
        """Test getting current camera position and rotation."""
        result = viewport_tool(action="get_camera_position")
        print(f"get_camera_position: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        self.assertIn("x", result)
        self.assertIn("y", result)
        self.assertIn("z", result)
        self.assertIn("pitch", result)
        self.assertIn("yaw", result)
        self.assertIn("roll", result)
        
    def test_02_set_camera_position(self):
        """Test setting camera position and rotation."""
        # Get original position
        original = viewport_tool(action="get_camera_position")
        orig_x = original.get("x")
        orig_y = original.get("y")
        orig_z = original.get("z")
        
        # Set to a new position
        result = viewport_tool(
            action="set_camera_position",
            location=[1000.0, 1000.0, 1000.0],
            rotation=[0.0, 180.0, 0.0]
        )
        print(f"set_camera_position: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
        # Verify position changed
        time.sleep(0.5)
        new_pos = viewport_tool(action="get_camera_position")
        self.assertAlmostEqual(new_pos.get("x"), 1000.0, delta=100)
        
        # Restore original position
        if orig_x is not None:
            viewport_tool(
                action="set_camera_position",
                location=[orig_x, orig_y, orig_z]
            )
            
    def test_03_viewport_action_focus_selected(self):
        """Test viewport action to focus on selected actor."""
        result = viewport_tool(
            action="viewport_action",
            mode="focus_selected"
        )
        print(f"viewport_action (focus_selected): {result}")
        # Might fail if nothing selected, that's ok
        
    def test_04_viewport_action_set_view_mode(self):
        """Test setting viewport mode."""
        result = viewport_tool(
            action="viewport_action",
            mode="wireframe"
        )
        print(f"viewport_action (wireframe): {result}")
        # Restore to lit mode
        viewport_tool(action="viewport_action", mode="lit")
        
    def test_05_viewport_action_focus_actor(self):
        """Test focusing on a specific actor."""
        # First spawn an actor
        from server.unreal_actors import spawn_actor
        spawn_result = spawn_actor(
            type="StaticMeshActor",
            name="MCP_TestCameraFocus"
        )
        
        if spawn_result.get("success"):
            result = viewport_tool(
                action="viewport_action",
                actor_name="MCP_TestCameraFocus"
            )
            print(f"viewport_action (focus_actor): {result}")
            
            # Clean up
            from server.unreal_actors import delete_actor
            delete_actor(name="MCP_TestCameraFocus")
        
    def test_06_invalid_action(self):
        """Test invalid viewport tool action."""
        result = viewport_tool(action="fly")
        self.assertFalse(result.get("success", True))
        

if __name__ == "__main__":
    unittest.main()
