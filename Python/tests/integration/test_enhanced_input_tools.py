import sys
import time
import unittest
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = PYTHON_ROOT.parent
sys.path.append(str(PYTHON_ROOT))

from server.blueprint_tools import create_blueprint
from server.core import get_unreal_connection
from server.enhanced_input_tools import enhanced_input_tool
from server.project_editor_tools import play_tool


class TestEnhancedInputTools(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        cls.suffix = int(time.time())
        cls.base = f"/Game/Tests/MCP_EnhancedInput/EI_{cls.suffix}"
        cls.ia_jump = f"{cls.base}/IA_Jump"
        cls.ia_move = f"{cls.base}/IA_Move"
        cls.imc = f"{cls.base}/IMC_Default"
        cls.default_input_ini = REPO_ROOT / "FlopperamUnrealMCP" / "Config" / "DefaultInput.ini"
        cls.default_input_snapshot = (
            cls.default_input_ini.read_bytes() if cls.default_input_ini.exists() else None
        )

    @classmethod
    def tearDownClass(cls):
        try:
            play_tool(action="stop_pie")
        except Exception:
            pass
        if getattr(cls, "default_input_snapshot", None) is not None:
            cls.default_input_ini.write_bytes(cls.default_input_snapshot)

    def assert_success(self, result):
        self.assertTrue(result.get("success"), f"Expected success, got: {result}")

    def test_01_create_actions_and_mapping_context(self):
        self.assert_success(enhanced_input_tool(
            action="create_input_action",
            asset_path=self.ia_jump,
            value_type="Boolean",
            triggers=[{"type": "pressed"}],
            overwrite=True,
        ))
        self.assert_success(enhanced_input_tool(
            action="create_input_action",
            asset_path=self.ia_move,
            value_type="Axis2D",
            overwrite=True,
        ))
        self.assert_success(enhanced_input_tool(
            action="create_input_mapping_context",
            asset_path=self.imc,
            mappings=[
                {
                    "input_action_path": self.ia_jump,
                    "key": "SpaceBar",
                    "triggers": [{"type": "pressed"}],
                    "player_mappable": True,
                    "mapping_name": "Jump",
                },
                {
                    "input_action_path": self.ia_move,
                    "key": "W",
                    "modifiers": [{"type": "swizzle_axis", "order": "YXZ"}],
                },
            ],
            overwrite=True,
        ))

    def test_02_add_keyboard_mouse_gamepad_mappings_and_modifiers(self):
        self.assert_success(enhanced_input_tool(
            action="add_keyboard_mapping",
            mapping_context_path=self.imc,
            input_action_path=self.ia_move,
            key="S",
            modifiers=[
                {"type": "swizzle_axis", "order": "YXZ"},
                {"type": "negate", "x": False, "y": True, "z": False},
            ],
        ))
        self.assert_success(enhanced_input_tool(
            action="add_mouse_mapping",
            mapping_context_path=self.imc,
            input_action_path=self.ia_move,
            key="MouseX",
        ))
        self.assert_success(enhanced_input_tool(
            action="add_gamepad_mapping",
            mapping_context_path=self.imc,
            input_action_path=self.ia_move,
            key="Gamepad_LeftX",
            modifiers=[{"type": "dead_zone", "lower_threshold": 0.15, "upper_threshold": 1.0}],
        ))
        self.assert_success(enhanced_input_tool(
            action="set_dead_zone",
            mapping_context_path=self.imc,
            input_action_path=self.ia_move,
            key="Gamepad_LeftX",
            lower_threshold=0.2,
            upper_threshold=0.95,
            dead_zone_type="radial",
        ))

    def test_03_rebind_ui_and_debug_metadata(self):
        self.assert_success(enhanced_input_tool(
            action="setup_rebind_ui",
            mapping_context_path=self.imc,
            enable_user_settings=True,
            save_settings=False,
        ))
        result = enhanced_input_tool(action="get_debug_info", mapping_context_path=self.imc)
        self.assert_success(result)

    def test_04_update_remove_and_list_mappings(self):
        self.assert_success(enhanced_input_tool(
            action="configure_input_action",
            input_action_path=self.ia_jump,
            value_type="Boolean",
            triggers=[
                {"type": "hold", "hold_time_threshold": 0.1},
                {"type": "tap", "tap_release_time_threshold": 0.25},
                {"type": "pressed"},
                {"type": "released"},
            ],
        ))
        self.assert_success(enhanced_input_tool(
            action="set_trigger",
            mapping_context_path=self.imc,
            input_action_path=self.ia_jump,
            key="SpaceBar",
            trigger_type="hold",
            hold_time_threshold=0.2,
        ))
        self.assert_success(enhanced_input_tool(
            action="configure_key_mapping",
            mapping_context_path=self.imc,
            input_action_path=self.ia_move,
            key="W",
            new_key="D",
            modifiers=[{"type": "swizzle_axis", "order": "YXZ"}],
        ))
        self.assert_success(enhanced_input_tool(
            action="configure_key_mapping",
            mapping_context_path=self.imc,
            input_action_path=self.ia_move,
            key="D",
            new_key="W",
            modifiers=[{"type": "swizzle_axis", "order": "YXZ"}],
        ))
        self.assert_success(enhanced_input_tool(
            action="add_key_mapping",
            mapping_context_path=self.imc,
            input_action_path=self.ia_jump,
            key="LeftShift",
        ))
        self.assert_success(enhanced_input_tool(
            action="remove_key_mapping",
            mapping_context_path=self.imc,
            input_action_path=self.ia_jump,
            key="LeftShift",
        ))
        self.assert_success(enhanced_input_tool(action="list", asset_path=self.base))

    def test_05_player_controller_and_character_bindings(self):
        pc_name = f"BP_EI_PC_{self.suffix}"
        character_name = f"BP_EI_Character_{self.suffix}"
        self.assert_success(create_blueprint(pc_name, "PlayerController"))
        self.assert_success(create_blueprint(character_name, "Character"))
        self.assert_success(enhanced_input_tool(
            action="generate_player_controller_binding",
            blueprint_path=f"/Game/Blueprints/{pc_name}",
            target_type="player_controller",
            input_action_path=self.ia_jump,
            trigger_event="Started",
            compile=False,
            save=False,
        ))
        self.assert_success(enhanced_input_tool(
            action="generate_character_binding",
            blueprint_path=f"/Game/Blueprints/{character_name}",
            target_type="character",
            bindings=[
                {"input_action_path": self.ia_move, "trigger_event": "Triggered"},
                {"input_action_path": self.ia_jump, "trigger_event": "Completed"},
            ],
            compile=False,
            save=False,
        ))

    def test_06_runtime_rebind_and_local_multiplayer(self):
        self.assert_success(enhanced_input_tool(
            action="configure_local_multiplayer",
            mapping_context_path=self.imc,
            priority=5,
            filter_input_by_platform_user=True,
            enable_user_settings=True,
            enable_default_mapping_contexts=True,
            create_local_players=0,
        ))

        start_result = play_tool(action="start_pie")
        if not start_result.get("success"):
            self.skipTest(f"PIE could not be started: {start_result}")
        try:
            time.sleep(1.0)
            self.assert_success(enhanced_input_tool(
                action="add_runtime_mapping_context",
                mapping_context_path=self.imc,
                player_index=0,
                priority=10,
                force_immediately=True,
                notify_user_settings=True,
            ))
            self.assert_success(enhanced_input_tool(
                action="rebind_key",
                mapping_context_path=self.imc,
                mapping_name="Jump",
                key="J",
                slot="first",
                player_index=0,
                save_settings=False,
            ))
            self.assert_success(enhanced_input_tool(
                action="remove_runtime_mapping_context",
                mapping_context_path=self.imc,
                player_index=0,
                force_immediately=True,
                notify_user_settings=True,
            ))
        finally:
            play_tool(action="stop_pie")


if __name__ == "__main__":
    unittest.main()
