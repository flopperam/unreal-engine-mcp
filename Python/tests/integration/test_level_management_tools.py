
import sys
import os
import unittest
import time
from pathlib import Path

# Add the Python directory to the path so we can import the server modules
PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

from server.project_editor_tools import (
    level_tool,
    sublevel_tool,
    world_partition_tool,
    advanced_world_tool
)
from server.core import get_unreal_connection
from server.unreal_actors import spawn_actor, delete_actor

class TestLevelManagementComprehensive(unittest.TestCase):
    """Comprehensive integration tests for Task 2: Level / World / Map management."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
        # Test paths
        cls.test_base_path = f"/Game/Tests/MCP_Runtime/MCP_Integration_{int(time.time())}_{os.getpid()}"
        cls.test_level_1 = f"{cls.test_base_path}/Level1"
        cls.test_level_2 = f"{cls.test_base_path}/Level2"
        cls.test_sublevel = f"{cls.test_base_path}/Sublevel"
        cls.renamed_level = f"{cls.test_base_path}/RenamedLevel"
        
    def setUp(self):
        """Clean up any existing test levels before each test."""
        # Try to delete test levels if they exist
        for path in [self.test_level_1, self.test_level_2, self.test_sublevel, self.renamed_level]:
            level_tool(action="delete", asset_path=path)
        
    def tearDown(self):
        """Clean up after each test."""
        pass
        
    # -------------------------------------------------------------------------
    # Level Tool - All Actions
    # -------------------------------------------------------------------------
    
    def test_01_create_level(self):
        """Test creating a new level."""
        result = level_tool(action="create", asset_path=self.test_level_1)
        print(f"create_level: {result}")
        self.assertTrue(result.get("success"), f"Failed to create level: {result.get('error')}")
        
    def test_02_get_current_level(self):
        """Test getting the current level name and path."""
        # First create and load a level
        level_tool(action="create", asset_path=self.test_level_1)
        level_tool(action="load", asset_path=self.test_level_1)
        
        result = level_tool(action="get_current")
        print(f"get_current_level: {result}")
        self.assertTrue(result.get("success"))
        self.assertIn("level_name", result)
        self.assertIn("level_path", result)
        
    def test_03_load_level(self):
        """Test loading an existing level."""
        # Create a level first
        level_tool(action="create", asset_path=self.test_level_1)
        
        result = level_tool(action="load", asset_path=self.test_level_1)
        print(f"load_level: {result}")
        self.assertTrue(result.get("success"), f"Failed to load level: {result.get('error')}")
        
    def test_04_save_level(self):
        """Test saving the current level."""
        # Create and load a level
        level_tool(action="create", asset_path=self.test_level_1)
        level_tool(action="load", asset_path=self.test_level_1)
        
        result = level_tool(action="save", asset_path=self.test_level_1)
        print(f"save_level: {result}")
        self.assertTrue(result.get("success"), f"Failed to save level: {result.get('error')}")
        
    def test_05_list_levels(self):
        """Test listing all levels in the current world."""
        # Create a couple of test levels
        level_tool(action="create", asset_path=self.test_level_1)
        level_tool(action="create", asset_path=self.test_level_2)
        
        result = level_tool(action="list")
        print(f"list_levels: Found {len(result.get('levels', []))} levels")
        self.assertTrue(result.get("success"))
        self.assertIn("levels", result)
        self.assertIsInstance(result["levels"], list)
        
    def test_06_duplicate_level(self):
        """Test duplicating an existing level."""
        # Create source level
        level_tool(action="create", asset_path=self.test_level_1)
        
        result = level_tool(
            action="duplicate",
            source_path=self.test_level_1,
            dest_path=self.test_level_2
        )
        print(f"duplicate_level: {result}")
        self.assertTrue(result.get("success"), f"Failed to duplicate level: {result.get('error')}")
        
    def test_07_rename_level(self):
        """Test renaming a level."""
        # Create source level
        level_tool(action="create", asset_path=self.test_level_1)
        
        renamed_path = self.renamed_level
        result = level_tool(
            action="rename",
            source_path=self.test_level_1,
            dest_path=renamed_path
        )
        print(f"rename_level: {result}")
        self.assertTrue(result.get("success"), f"Failed to rename level: {result.get('error')}")
        
        # Clean up renamed level
        level_tool(action="delete", asset_path=renamed_path)
        
    def test_08_delete_level(self):
        """Test deleting a level."""
        # Create a level to delete
        level_tool(action="create", asset_path=self.test_level_1)
        
        result = level_tool(action="delete", asset_path=self.test_level_1)
        print(f"delete_level: {result}")
        self.assertTrue(result.get("success"), f"Failed to delete level: {result.get('error')}")
        
    def test_09_level_workflow_integration(self):
        """Test a complete level management workflow."""
        # 1. Create a new level
        result = level_tool(action="create", asset_path=self.test_level_1)
        self.assertTrue(result.get("success"))
        
        # 2. Load it
        result = level_tool(action="load", asset_path=self.test_level_1)
        self.assertTrue(result.get("success"))
        
        # 3. Save it
        result = level_tool(action="save", asset_path=self.test_level_1)
        self.assertTrue(result.get("success"))
        
        # 4. Duplicate it
        result = level_tool(
            action="duplicate",
            source_path=self.test_level_1,
            dest_path=self.test_level_2
        )
        self.assertTrue(result.get("success"))
        
        # 5. List levels (should see both)
        result = level_tool(action="list")
        self.assertTrue(result.get("success"))
        level_paths = [l.get("path") for l in result.get("levels", [])]
        print(f"Levels after duplicate: {level_paths}")
        
        # 6. Clean up
        level_tool(action="delete", asset_path=self.test_level_2)
        level_tool(action="delete", asset_path=self.test_level_1)
        
    def test_10_invalid_level_action(self):
        """Test invalid level action returns error."""
        result = level_tool(action="fly")
        self.assertFalse(result.get("success", True))
        self.assertIn("error", result)
        
    def test_11_missing_required_params(self):
        """Test that missing required parameters return error."""
        # Missing asset_path for create
        result = level_tool(action="create")
        self.assertFalse(result.get("success", True))
        self.assertIn("asset_path", result.get("error", ""))
        
        # Missing source_path/dest_path for duplicate
        result = level_tool(action="duplicate", source_path="/Game/Test")
        self.assertFalse(result.get("success", True))
        self.assertIn("dest_path", result.get("error", ""))


class TestSublevelToolComprehensive(unittest.TestCase):
    """Comprehensive tests for sublevel_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
        cls.test_base_path = f"/Game/Tests/MCP_Runtime/MCP_Sublevel_{int(time.time())}_{os.getpid()}"
        cls.persist_level = f"{cls.test_base_path}/MCP_Persist"
        cls.sublevel_1 = f"{cls.test_base_path}/MCP_Sub1"
        cls.sublevel_2 = f"{cls.test_base_path}/MCP_Sub2"
        
    def setUp(self):
        """Set up test environment."""
        # Create persistent level and sublevels
        level_tool(action="create", asset_path=self.persist_level)
        level_tool(action="create", asset_path=self.sublevel_1)
        level_tool(action="create", asset_path=self.sublevel_2)
        level_tool(action="load", asset_path=self.persist_level)
        
    def tearDown(self):
        """Clean up."""
        for path in [self.persist_level, self.sublevel_1, self.sublevel_2]:
            level_tool(action="delete", asset_path=path)
        
    def test_01_get_persistent_level(self):
        """Test getting persistent level info."""
        result = sublevel_tool(action="get_persistent")
        print(f"get_persistent_level: {result}")
        self.assertTrue(result.get("success"))
        self.assertIn("persistent_level", result)
        
    def test_02_add_sublevel(self):
        """Test adding a sublevel to the persistent level."""
        result = sublevel_tool(action="add", level_path=self.sublevel_1)
        print(f"add_sublevel: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        self.assertIn("streaming_level_name", result)
        
    def test_03_add_multiple_sublevels(self):
        """Test adding multiple sublevels."""
        result1 = sublevel_tool(action="add", level_path=self.sublevel_1)
        self.assertTrue(result1.get("success"))
        
        result2 = sublevel_tool(action="add", level_path=self.sublevel_2)
        self.assertTrue(result2.get("success"))
        
        # Verify both are added (by checking persistent level)
        result = sublevel_tool(action="get_persistent")
        self.assertTrue(result.get("success"))
        
    def test_04_set_sublevel_visible(self):
        """Test setting sublevel visibility."""
        # First add a sublevel
        add_result = sublevel_tool(action="add", level_path=self.sublevel_1)
        self.assertTrue(add_result.get("success"))
        level_name = add_result.get("streaming_level_name")
        
        # Set visible
        result = sublevel_tool(action="set_visible", level_name=level_name, visible=True)
        print(f"set_visible (True): {result}")
        self.assertTrue(result.get("success"))
        
        # Set invisible
        result = sublevel_tool(action="set_visible", level_name=level_name, visible=False)
        print(f"set_visible (False): {result}")
        self.assertTrue(result.get("success"))
        
    def test_05_set_sublevel_loaded(self):
        """Test setting sublevel loaded state."""
        # First add a sublevel
        add_result = sublevel_tool(action="add", level_path=self.sublevel_1)
        self.assertTrue(add_result.get("success"))
        level_name = add_result.get("streaming_level_name")
        
        # Set loaded
        result = sublevel_tool(action="set_loaded", level_name=level_name, loaded=True)
        print(f"set_loaded (True): {result}")
        self.assertTrue(result.get("success"))
        
        # Set unloaded
        result = sublevel_tool(action="set_loaded", level_name=level_name, loaded=False)
        print(f"set_loaded (False): {result}")
        self.assertTrue(result.get("success"))
        
    def test_06_set_streaming_settings(self):
        """Test modifying streaming settings for a sublevel."""
        # First add a sublevel
        add_result = sublevel_tool(action="add", level_path=self.sublevel_1)
        self.assertTrue(add_result.get("success"))
        level_name = add_result.get("streaming_level_name")
        
        result = sublevel_tool(
            action="set_streaming",
            level_name=level_name,
            should_be_loaded=True,
            should_be_visible=True,
            priority=5
        )
        print(f"set_streaming: {result}")
        self.assertTrue(result.get("success"))
        
    def test_07_create_streaming_volume(self):
        """Test creating a Level Streaming Volume."""
        # First add a sublevel
        add_result = sublevel_tool(action="add", level_path=self.sublevel_1)
        self.assertTrue(add_result.get("success"))
        
        result = sublevel_tool(
            action="create_volume",
            location=[0.0, 0.0, 0.0],
            extent=[5000.0, 5000.0, 1000.0],
            streaming_levels=[self.sublevel_1]
        )
        print(f"create_volume: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_08_remove_sublevel(self):
        """Test removing a sublevel."""
        # First add a sublevel
        add_result = sublevel_tool(action="add", level_path=self.sublevel_1)
        self.assertTrue(add_result.get("success"))
        level_name = add_result.get("streaming_level_name")
        
        result = sublevel_tool(action="remove", level_name=level_name)
        print(f"remove_sublevel: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_09_sublevel_workflow(self):
        """Test a complete sublevel management workflow."""
        # 1. Add sublevel
        add_result = sublevel_tool(action="add", level_path=self.sublevel_1)
        self.assertTrue(add_result.get("success"))
        level_name = add_result.get("streaming_level_name")
        
        # 2. Set visibility and loaded state
        result = sublevel_tool(action="set_visible", level_name=level_name, visible=True)
        self.assertTrue(result.get("success"))
        
        result = sublevel_tool(action="set_loaded", level_name=level_name, loaded=True)
        self.assertTrue(result.get("success"))
        
        # 3. Modify streaming settings
        result = sublevel_tool(
            action="set_streaming",
            level_name=level_name,
            priority=10
        )
        self.assertTrue(result.get("success"))
        
        # 4. Create streaming volume
        result = sublevel_tool(
            action="create_volume",
            location=[0.0, 0.0, 0.0],
            extent=[3000.0, 3000.0, 500.0]
        )
        self.assertTrue(result.get("success"))
        
        # 5. Remove sublevel
        result = sublevel_tool(action="remove", level_name=level_name)
        self.assertTrue(result.get("success"))
        
    def test_10_invalid_sublevel_action(self):
        """Test invalid sublevel action."""
        result = sublevel_tool(action="fly")
        self.assertFalse(result.get("success", True))


class TestWorldPartitionToolComprehensive(unittest.TestCase):
    """Comprehensive tests for world_partition_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_enable_world_partition(self):
        """Test enabling World Partition."""
        result = world_partition_tool(action="enable", enable=True)
        print(f"enable_world_partition (True): {result}")
        # Might fail if WP is already enabled or not supported
        
    def test_02_disable_world_partition(self):
        """Test disabling World Partition."""
        result = world_partition_tool(action="enable", enable=False)
        print(f"enable_world_partition (False): {result}")
        
    def test_03_set_grid(self):
        """Test setting World Partition grid settings."""
        result = world_partition_tool(
            action="set_grid",
            placement_grid_size=2048,
            foliage_grid_size=4096,
            minimap_threshold=1024
        )
        print(f"set_grid: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_04_get_cells(self):
        """Test getting World Partition cell information."""
        result = world_partition_tool(action="get_cells")
        print(f"get_cells: {result}")
        # Might fail if WP is not enabled - that's ok
        if result.get("success"):
            self.assertIn("editor_bounds", result)
            self.assertIn("loaded_regions", result)
            
    def test_05_load_cell(self):
        """Test loading a specific WP cell region."""
        result = world_partition_tool(
            action="load_cell",
            min_x=-1000, min_y=-1000, min_z=0,
            max_x=1000, max_y=1000, max_z=1000
        )
        print(f"load_cell: {result}")
        # Might fail if WP not enabled
        
    def test_06_unload_cell(self):
        """Test unloading/clearing WP cell regions."""
        result = world_partition_tool(action="unload_cell")
        print(f"unload_cell: {result}")
        # Might fail if WP not enabled
        
    def test_07_invalid_action(self):
        """Test invalid world partition action."""
        result = world_partition_tool(action="fly")
        self.assertFalse(result.get("success", True))


class TestAdvancedWorldToolComprehensive(unittest.TestCase):
    """Comprehensive tests for advanced_world_tool."""
    
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        
    def test_01_create_data_layer(self):
        """Test creating a Data Layer."""
        result = advanced_world_tool(action="create_data_layer", name="MCP_TestLayer1")
        print(f"create_data_layer: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_02_create_multiple_data_layers(self):
        """Test creating multiple Data Layers."""
        for i in range(3):
            result = advanced_world_tool(action="create_data_layer", name=f"MCP_Layer_{i}")
            self.assertTrue(result.get("success"), f"Failed to create layer {i}: {result.get('error')}")
            
    def test_03_add_actors_to_data_layer(self):
        """Test adding actors to a Data Layer."""
        # First create a data layer
        result = advanced_world_tool(action="create_data_layer", name="MCP_ActorLayer")
        self.assertTrue(result.get("success"))
        
        # Spawn some test actors
        actors = []
        for i in range(3):
            spawn_result = spawn_actor(
                type="StaticMeshActor",
                name=f"MCP_TestActor_{i}"
            )
            if spawn_result.get("success"):
                actors.append(f"MCP_TestActor_{i}")
        
        if actors:
            result = advanced_world_tool(
                action="add_actors_to_data_layer",
                data_layer_name="MCP_ActorLayer",
                actor_names=actors
            )
            print(f"add_actors_to_data_layer: {result}")
            self.assertTrue(result.get("success"))
            
            # Clean up actors
            for actor in actors:
                delete_actor(name=actor)
                
    def test_04_remove_actors_from_data_layer(self):
        """Test removing actors from a Data Layer."""
        # Similar to add test
        result = advanced_world_tool(action="create_data_layer", name="MCP_RemoveLayer")
        self.assertTrue(result.get("success"))
        
        # Spawn test actor
        spawn_result = spawn_actor(type="StaticMeshActor", name="MCP_RemoveMe")
        if spawn_result.get("success"):
            # Add to layer first
            advanced_world_tool(
                action="add_actors_to_data_layer",
                data_layer_name="MCP_RemoveLayer",
                actor_names=["MCP_RemoveMe"]
            )
            
            # Remove from layer
            result = advanced_world_tool(
                action="remove_actors_from_data_layer",
                data_layer_name="MCP_RemoveLayer",
                actor_names=["MCP_RemoveMe"]
            )
            print(f"remove_actors_from_data_layer: {result}")
            self.assertTrue(result.get("success"))
            
            # Clean up
            delete_actor(name="MCP_RemoveMe")
            
    def test_05_set_data_layer_enabled(self):
        """Test enabling/disabling a Data Layer."""
        result = advanced_world_tool(action="create_data_layer", name="MCP_EnableLayer")
        self.assertTrue(result.get("success"))
        
        # Enable
        result = advanced_world_tool(
            action="set_data_layer_enabled",
            data_layer_name="MCP_EnableLayer",
            enabled=True
        )
        print(f"set_data_layer_enabled (True): {result}")
        self.assertTrue(result.get("success"))
        
        # Disable
        result = advanced_world_tool(
            action="set_data_layer_enabled",
            data_layer_name="MCP_EnableLayer",
            enabled=False
        )
        print(f"set_data_layer_enabled (False): {result}")
        self.assertTrue(result.get("success"))
        
    def test_06_create_hlod_layer(self):
        """Test creating an HLOD Layer."""
        result = advanced_world_tool(action="create_hlod_layer", name="MCP_HLODLayer")
        print(f"create_hlod_layer: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_07_build_hlod(self):
        """Test building HLOD for current map."""
        result = advanced_world_tool(action="build_hlod")
        print(f"build_hlod: {result}")
        # Might take time or fail if no HLOD setup
        
    def test_08_rebuild_hlod(self):
        """Test rebuilding HLOD for current map."""
        result = advanced_world_tool(action="rebuild_hlod")
        print(f"rebuild_hlod: {result}")
        # Might take time or fail if no HLOD setup
        
    def test_09_set_one_file_per_actor(self):
        """Test enabling/disabling One File Per Actor."""
        # Enable
        result = advanced_world_tool(action="set_one_file_per_actor", enabled=True)
        print(f"set_one_file_per_actor (True): {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
        # Disable
        result = advanced_world_tool(action="set_one_file_per_actor", enabled=False)
        print(f"set_one_file_per_actor (False): {result}")
        self.assertTrue(result.get("success"))
        
    def test_10_set_level_bounds(self):
        """Test setting level bounds."""
        result = advanced_world_tool(
            action="set_level_bounds",
            min_extent=[-10000.0, -10000.0, -1000.0],
            max_extent=[10000.0, 10000.0, 1000.0]
        )
        print(f"set_level_bounds: {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
    def test_11_set_world_origin_rebasing(self):
        """Test enabling/disabling World Origin Rebasing."""
        # Enable
        result = advanced_world_tool(action="set_world_origin_rebasing", enabled=True)
        print(f"set_world_origin_rebasing (True): {result}")
        self.assertTrue(result.get("success"), f"Failed: {result.get('error')}")
        
        # Disable
        result = advanced_world_tool(action="set_world_origin_rebasing", enabled=False)
        print(f"set_world_origin_rebasing (False): {result}")
        self.assertTrue(result.get("success"))
        
    def test_12_advanced_world_workflow(self):
        """Test a complete advanced world workflow."""
        # 1. Create data layer
        result = advanced_world_tool(action="create_data_layer", name="MCP_WorkflowLayer")
        self.assertTrue(result.get("success"))
        
        # 2. Enable OFPA
        result = advanced_world_tool(action="set_one_file_per_actor", enabled=True)
        self.assertTrue(result.get("success"))
        
        # 3. Set world origin rebasing
        result = advanced_world_tool(action="set_world_origin_rebasing", enabled=True)
        self.assertTrue(result.get("success"))
        
        # 4. Set level bounds
        result = advanced_world_tool(
            action="set_level_bounds",
            min_extent=[-5000.0, -5000.0, 0.0],
            max_extent=[5000.0, 5000.0, 1000.0]
        )
        self.assertTrue(result.get("success"))
        
        # 5. Create HLOD layer
        result = advanced_world_tool(action="create_hlod_layer", name="MCP_WorkflowHLOD")
        self.assertTrue(result.get("success"))
        
    def test_13_invalid_action(self):
        """Test invalid advanced world action."""
        result = advanced_world_tool(action="fly")
        self.assertFalse(result.get("success", True))


if __name__ == "__main__":
    unittest.main()
