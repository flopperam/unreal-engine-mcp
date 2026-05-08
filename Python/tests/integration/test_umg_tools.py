import sys
import time
import unittest
from pathlib import Path

PYTHON_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(PYTHON_ROOT))

from server.core import get_unreal_connection
from server.project_editor_tools import editor_control_tool, play_tool
from server.umg_tools import umg_tool


class TestUMGToolsIntegration(unittest.TestCase):
    """Integration coverage for UMG WidgetTree, slots, events, GC, and PIE UI."""

    @classmethod
    def setUpClass(cls):
        unreal = get_unreal_connection()
        if not unreal.connect():
            raise unittest.SkipTest("Unreal Engine not running or unreachable")
        cls.unreal = unreal
        cls.suffix = str(int(time.time()))

    def _path(self, name: str) -> str:
        return f"/Game/Tests/UMG/WBP_{name}_{self.suffix}"

    def _assert_success(self, result, message="command failed"):
        self.assertTrue(result.get("success"), f"{message}: {result}")

    def _widgets_by_name(self, blueprint_path: str):
        result = umg_tool(action="inspect", widget_blueprint=blueprint_path)
        self._assert_success(result, "inspect")
        return {widget["name"]: widget for widget in result.get("widgets", [])}, result

    def test_umg_hierarchy_creation(self):
        blueprint_path = self._path("Hierarchy")
        self._assert_success(umg_tool(action="create", asset_path=blueprint_path), "create widget blueprint")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="CanvasPanel", widget_name="RootCanvas"), "add canvas")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Button", widget_name="TestButton", parent_name="RootCanvas", is_variable=True), "add button")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="TextBlock", widget_name="ButtonText", parent_name="TestButton", text="Click"), "add text")

        compile_result = umg_tool(action="compile", widget_blueprint=blueprint_path)
        self._assert_success(compile_result, "compile")
        self.assertTrue(compile_result.get("compiled_success"), compile_result)

        widgets, inspect_result = self._widgets_by_name(blueprint_path)
        self.assertEqual(inspect_result.get("root_widget"), "RootCanvas")
        self.assertIn("RootCanvas", widgets)
        self.assertIn("TestButton", widgets)
        self.assertIn("ButtonText", widgets)
        self.assertEqual(widgets["TestButton"].get("parent_name"), "RootCanvas")

    def test_umg_slot_properties(self):
        blueprint_path = self._path("Slots")
        self._assert_success(umg_tool(action="create", asset_path=blueprint_path), "create widget blueprint")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="CanvasPanel", widget_name="RootCanvas"), "add canvas")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Button", widget_name="ButtonA", parent_name="RootCanvas"), "add button a")
        self._assert_success(umg_tool(action="set_slot", widget_blueprint=blueprint_path, widget_name="ButtonA", anchors=[1.0, 1.0, 1.0, 1.0], position=[100.0, 50.0], size=[200.0, 80.0], alignment=[1.0, 1.0]), "set canvas slot")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="VerticalBox", widget_name="VBox", parent_name="RootCanvas"), "add vbox")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Button", widget_name="ButtonB", parent_name="VBox"), "add button b")
        self._assert_success(umg_tool(action="set_slot", widget_blueprint=blueprint_path, widget_name="ButtonB", padding=[10.0, 10.0, 10.0, 10.0], horizontal_alignment="Center", vertical_alignment="Center"), "set vertical slot")

        self._assert_success(umg_tool(action="compile", widget_blueprint=blueprint_path), "compile")
        widgets, _ = self._widgets_by_name(blueprint_path)

        slot_a = widgets["ButtonA"]["slot_properties"]
        self.assertIn("CanvasPanelSlot", widgets["ButtonA"]["slot_class"])
        self.assertEqual(slot_a["anchors"], [1.0, 1.0, 1.0, 1.0])
        self.assertEqual(slot_a["position"], [100.0, 50.0])
        self.assertEqual(slot_a["size"], [200.0, 80.0])

        slot_b = widgets["ButtonB"]["slot_properties"]
        self.assertIn("VerticalBoxSlot", widgets["ButtonB"]["slot_class"])
        self.assertEqual(slot_b["padding"], [10.0, 10.0, 10.0, 10.0])
        self.assertEqual(slot_b["horizontal_alignment"], "Center")
        self.assertEqual(slot_b["vertical_alignment"], "Center")

    def test_umg_event_binding_onclicked(self):
        blueprint_path = self._path("OnClicked")
        self._assert_success(umg_tool(action="create", asset_path=blueprint_path), "create widget blueprint")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="CanvasPanel", widget_name="RootCanvas"), "add canvas")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Button", widget_name="ClickButton", parent_name="RootCanvas", is_variable=True), "add button")

        result = umg_tool(action="bind_on_clicked", widget_blueprint=blueprint_path, button_name="ClickButton", print_string="UMG OnClicked Test")
        self._assert_success(result, "bind on clicked")
        self.assertEqual(result.get("event_node_class"), "K2Node_ComponentBoundEvent")
        self.assertTrue(result.get("compiled_success"), result)

        inspect_result = umg_tool(action="inspect", widget_blueprint=blueprint_path)
        self._assert_success(inspect_result, "inspect")
        bound_events = inspect_result.get("bound_event_nodes", [])
        self.assertTrue(any(node.get("delegate_name") == "OnClicked" and node.get("component_name") == "ClickButton" for node in bound_events), bound_events)

    def test_umg_destruction_and_recreation(self):
        blueprint_path = self._path("DestroyRecreate")
        self._assert_success(umg_tool(action="create", asset_path=blueprint_path), "create widget blueprint")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="CanvasPanel", widget_name="RootCanvas"), "add canvas")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Button", widget_name="NestedButton", parent_name="RootCanvas", is_variable=True), "add button")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Image", widget_name="NestedImage", parent_name="NestedButton"), "add image")

        remove_result = umg_tool(action="remove", widget_blueprint=blueprint_path, remove_root=True, force_gc=True)
        self._assert_success(remove_result, "remove root")
        self.assertGreaterEqual(remove_result.get("removed_widget_count", 0), 3)

        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="CanvasPanel", widget_name="RootCanvas", replace_existing=True), "readd canvas")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Button", widget_name="NestedButton", parent_name="RootCanvas", replace_existing=True, is_variable=True), "readd button")
        self._assert_success(umg_tool(action="add", widget_blueprint=blueprint_path, widget_type="Image", widget_name="NestedImage", parent_name="NestedButton", replace_existing=True), "readd image")
        compile_result = umg_tool(action="compile", widget_blueprint=blueprint_path)
        self._assert_success(compile_result, "compile")
        self.assertTrue(compile_result.get("compiled_success"), compile_result)

        widgets, _ = self._widgets_by_name(blueprint_path)
        self.assertEqual(len([name for name in widgets if name == "RootCanvas"]), 1)
        self.assertIn("NestedButton", widgets)
        self.assertIn("NestedImage", widgets)

    def test_e2e_umg_interactive_menu(self):
        blueprint_path = self._path("E2EMainMenu")
        instance_name = f"WBP_E2E_Instance_{self.suffix}"
        self._assert_success(umg_tool(action="create_main_menu", asset_path=blueprint_path), "create main menu")

        state = play_tool(action="get_state")
        if state.get("success") and not state.get("play_world_active"):
            self._assert_success(play_tool(action="start_pie"), "start pie")
            time.sleep(3.0)

        add_result = umg_tool(action="add_to_viewport", widget_blueprint=blueprint_path, instance_name=instance_name, show_mouse_cursor=True, z_order=10)
        self._assert_success(add_result, "add widget to viewport")

        click_result = umg_tool(action="click_button", instance_name=instance_name, button_name="StartButton")
        self._assert_success(click_result, "click start button")

        log_found = False
        for _ in range(10):
            log_result = editor_control_tool(action="get_editor_log", tail_lines=250)
            if log_result.get("success") and "Start Button Clicked" in "\n".join(log_result.get("log_lines", [])):
                log_found = True
                break
            time.sleep(0.5)

        umg_tool(action="remove_from_parent", instance_name=instance_name)
        play_tool(action="stop_pie")

        self.assertTrue(log_found, "Expected 'Start Button Clicked' in the editor log after simulated UMG click")


if __name__ == "__main__":
    unittest.main()
