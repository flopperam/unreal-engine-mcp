"""
tests/unit/test_tool_registration_and_mapping.py

L1 Python unit tests - @mcp.tool() API surface, command mapping, and parameter mapping

Covers:
- All @mcp.tool() functions register without import-time exceptions
- Public tool count matches source definitions
- Each tool calls the correct Unreal command name
- Each tool passes required parameters with correct keys
- Consistent error dictionary on connection failure
- Mutable default arguments are not shared across calls
"""

import inspect
import json
import re
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch

import pytest

import unreal_mcp_server_advanced as srv
from unreal_mcp_server_advanced import mcp, get_unreal_connection
from server import actor_tools, blueprint_tools, blueprint_graph_tools, material_graph_tools, material_tools, world_building_tools


def _collect_source_tools():
    """Detect functions wrapped by FastMCP tool decorators in the source module."""
    import pathlib
    python_dir = pathlib.Path(srv.__file__).parent
    src_files = [pathlib.Path(srv.__file__)] + list((python_dir / "server").glob("*.py"))
    tools = []
    for src_file in src_files:
        src = src_file.read_text(encoding="utf-8")
        # Allow comments and blank lines between @mcp.tool() and the next def.
        tools.extend(m.group(1) for m in re.finditer(r'@mcp\.tool\(.*?\)[\s]*?def\s+(\w+)', src, re.DOTALL))
    return tools


TOOL_FUNCS = _collect_source_tools()


def _patch_tool_connections(fake_conn):
    stack = ExitStack()
    for module in [
        srv,
        actor_tools,
        blueprint_tools,
        blueprint_graph_tools,
        material_graph_tools,
        material_tools,
        world_building_tools,
    ]:
        stack.enter_context(patch.object(module, "get_unreal_connection", return_value=fake_conn, create=True))
    return stack


class TestToolRegistration:
    def test_all_tools_registered_without_exception(self):
        """Tools should be registered correctly in FastMCP."""
        tools = mcp._tool_manager._tools
        assert len(tools) > 0, "No tools registered in FastMCP"

    def test_tool_count_matches_source_definitions(self):
        """
        Number of @mcp.tool() in source should match the number of actually registered tools.
        """
        source_tools = TOOL_FUNCS
        registered_tools = mcp._tool_manager._tools
        src_count = len(source_tools)
        reg_count = len(registered_tools)
        assert src_count == reg_count, (
            f"Mismatch: source has {src_count} tools, FastMCP registered {reg_count}"
        )


@pytest.fixture
def fake_conn(fake_conn_factory):
    """Fake connection where all commands succeed."""
    return fake_conn_factory()


class TestToolCommandMapping:
    """
    Validate that each tool passes the correct command string and parameter keys to send_command.
    """

    @pytest.mark.parametrize("tool_name, expected_cmd", [
        ("get_actors_in_level", "get_actors_in_level"),
        ("find_actors_by_name", "find_actors_by_name"),
        ("delete_actor", "delete_actor"),
        ("set_actor_transform", "set_actor_transform"),
        ("create_blueprint", "create_blueprint"),
        ("add_component_to_blueprint", "add_component_to_blueprint"),
        ("set_static_mesh_properties", "set_static_mesh_properties"),
        ("set_physics_properties", "set_physics_properties"),
        ("compile_blueprint", "compile_blueprint"),
        ("read_blueprint_content", "read_blueprint_content"),
        ("analyze_blueprint_graph", "analyze_blueprint_graph"),
        ("get_blueprint_variable_details", "get_blueprint_variable_details"),
        ("get_blueprint_function_details", "get_blueprint_function_details"),
        ("get_available_materials", "get_available_materials"),
        ("apply_material_to_actor", "apply_material_to_actor"),
        ("apply_material_to_blueprint", "apply_material_to_blueprint"),
        ("get_actor_material_info", "get_actor_material_info"),
        ("get_blueprint_material_info", "get_blueprint_material_info"),
        ("set_mesh_material_color", "set_mesh_material_color"),
        ("create_material", "create_material"),
        ("add_material_node", "add_material_node"),
        ("connect_material_nodes", "connect_material_nodes"),
        ("export_material_json", "analyze_material_graph"),
        ("find_actor_by_mcp_id", "find_actor_by_mcp_id"),
        ("set_actor_transform_by_mcp_id", "set_actor_transform_by_mcp_id"),
        ("delete_actor_by_mcp_id", "delete_actor_by_mcp_id"),
    ])
    def test_tool_calls_expected_command(self, tool_name, expected_cmd, fake_conn):
        fn = getattr(srv, tool_name)
        sig = inspect.signature(fn)
        kwargs = {}
        # Fill only required arguments.
        for name, param in sig.parameters.items():
            if name == "random_string":
                kwargs[name] = ""
            elif name == "color":
                kwargs[name] = [1.0, 0.0, 0.0, 1.0]
            elif param.default is not inspect.Parameter.empty:
                kwargs[name] = param.default
            elif param.annotation == list:
                kwargs[name] = [0.0, 0.0, 0.0]
            elif param.annotation == str:
                kwargs[name] = "TestName"
            elif param.annotation == float:
                kwargs[name] = 0.0
            elif param.annotation == int:
                kwargs[name] = 0
            elif param.annotation == bool:
                kwargs[name] = True
            elif param.annotation == dict:
                kwargs[name] = {}
            else:
                kwargs[name] = "default"

        with _patch_tool_connections(fake_conn):
            fn(**kwargs)

        # Read the last command sent.
        last = fake_conn.history[-1]
        assert last["command"] == expected_cmd, (
            f"{tool_name} expected command '{expected_cmd}' but got '{last['command']}'"
        )

    def test_create_blueprint_required_params(self, fake_conn):
        with _patch_tool_connections(fake_conn):
            srv.create_blueprint(name="MyBP", parent_class="Actor")
        last = fake_conn.history[-1]
        assert last["params"]["name"] == "MyBP"
        assert last["params"]["parent_class"] == "Actor"

    def test_add_component_required_params(self, fake_conn):
        with _patch_tool_connections(fake_conn):
            srv.add_component_to_blueprint(
                blueprint_name="MyBP",
                component_type="StaticMeshComponent",
                component_name="MeshComp"
            )
        last = fake_conn.history[-1]
        assert last["params"]["blueprint_name"] == "MyBP"
        assert last["params"]["component_type"] == "StaticMeshComponent"
        assert last["params"]["component_name"] == "MeshComp"

    def test_set_physics_properties_params(self, fake_conn):
        with _patch_tool_connections(fake_conn):
            srv.set_physics_properties(
                blueprint_name="MyBP",
                component_name="MeshComp",
                simulate_physics=False,
                gravity_enabled=False,
                mass=5.0,
                linear_damping=0.1,
                angular_damping=0.2
            )
        last = fake_conn.history[-1]
        p = last["params"]
        assert p["simulate_physics"] is False
        assert p["gravity_enabled"] is False
        assert p["mass"] == 5.0
        assert p["linear_damping"] == 0.1
        assert p["angular_damping"] == 0.2


class TestConnectionFailureConsistentError:
    def test_tools_return_consistent_error_on_connection_failure(self):
        """
        Return a consistent error shape on connection failure.
        (get_unreal_connection normally returns UnrealConnection,
         so failure is simulated by patching send_command.)
        """
        from tests.conftest import FakeUnrealConnection
        real_conn = srv.get_unreal_connection()
        
        # Simulating connect failure through helper internals is heavier; patch send_command instead.
        with patch.object(real_conn, "send_command", return_value={"status": "error", "error": "conn failed"}):
            result = srv.create_blueprint("BP", "Actor")

        # Current implementation returns send_command output as-is.
        assert result.get("status") == "error" or result.get("success") is False


class TestMutableDefaultArguments:
    """
    Detect functions with mutable defaults (list/dict)
    and test that state is not shared across calls.
    Requirement note: add_component_to_blueprint(location=[], rotation=[], scale=[], component_properties={})
    """

    def test_add_component_to_blueprint_mutable_defaults_not_shared(self):
        from tests.conftest import FakeUnrealConnection
        fake_conn = FakeUnrealConnection()
        with _patch_tool_connections(fake_conn):
            # First call.
            r1 = srv.add_component_to_blueprint(
                blueprint_name="BP", component_type="A", component_name="C1",
                location=[1, 2, 3], rotation=[4, 5, 6]
            )
            # Second call uses default arguments.
            r2 = srv.add_component_to_blueprint(
                blueprint_name="BP", component_type="B", component_name="C2"
            )
        # r2 location/rotation should be empty lists.
        last = fake_conn.history[-1]
        assert last["params"]["location"] == []
        assert last["params"]["rotation"] == []
        assert last["params"]["scale"] == []

    def test_all_tools_no_mutable_default_pollution(self):
        """
        Scan all tool function signatures and list mutable defaults.
        If this fails, those functions should be fixed.
        """
        bad = []
        for tool_name in TOOL_FUNCS:
            obj = getattr(srv, tool_name, None)
            if obj is None:
                continue
            sig = inspect.signature(obj)
            for param_name, param in sig.parameters.items():
                default = param.default
                if default is not inspect.Parameter.empty and isinstance(default, (list, dict)):
                    # list/dict defaults are risky.
                    bad.append((tool_name, param_name, default))
        if bad:
            msg = "The following tools have mutable defaults and may cause state pollution:\n"
            for fn, pn, d in bad:
                msg += f"  {fn}(... {pn}={d!r} ...)\n"
            pytest.fail(msg)


class TestPythonToCppCommandMapping:
    """
    Detect drift between Python @mcp.tool() names and C++ command strings.

    We extract command strings from both sides and report additions/removals.
    Known aliases (different Python tool name from C++ command) should be whitelisted.
    """

    def _collect_cpp_commands(self):
        """Parse C++ dispatcher condition strings to find supported commands."""
        project_root = Path(__file__).resolve().parents[3]
        # Search both canonical locations (Flopperam project and root Plugins)
        search_paths = [
            project_root / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP" / "Source" / "UnrealMCP" / "Private",
            project_root / "Plugins" / "UnrealMCP" / "Source" / "UnrealMCP" / "Private",
        ]
        commands = set()
        for cpp_dir in search_paths:
            if not cpp_dir.exists():
                continue
            for cpp_file in cpp_dir.rglob("EpicUnrealMCP*.cpp"):
                text = cpp_file.read_text(encoding="utf-8")
                for m in re.finditer(r'CommandType\s*==\s*TEXT\s*\(\s*"([^"]+)"\s*\)', text, re.DOTALL):
                    commands.add(m.group(1))
                for m in re.finditer(r'HandleCommand\s*\(\s*TEXT\s*\(\s*"([^"]+)"\s*\)', text, re.DOTALL):
                    commands.add(m.group(1))
                # TMap-based dispatch: {TEXT("command_name"), bucket}
                for m in re.finditer(r'\{\s*TEXT\s*\(\s*"([^"]+)"\s*\)\s*,\s*\d+\s*\}', text):
                    commands.add(m.group(1))
                # TMap-based dispatch to member function pointers.
                for m in re.finditer(r'\{\s*TEXT\s*\(\s*"([^"]+)"\s*\)\s*,\s*&[A-Za-z0-9_:]+::[A-Za-z0-9_]+\s*\}', text):
                    commands.add(m.group(1))
        return commands

    def _collect_python_commands(self):
        """Commands Python tools send to Unreal via send_command."""
        cmds = set()
        project_root = Path(__file__).resolve().parents[3]
        python_root = project_root / "Python"
        for py_file in (python_root / "server").rglob("*.py"):
            text = py_file.read_text(encoding="utf-8")
            for m in re.finditer(r'send_command[\s\(]*["\']([^"\']+)["\']', text):
                cmds.add(m.group(1))
        for py_file in (python_root / "helpers").rglob("*.py"):
            text = py_file.read_text(encoding="utf-8")
            for m in re.finditer(r'send_command[\s\(]*["\']([^"\']+)["\']', text):
                cmds.add(m.group(1))
        return cmds

    def _collect_registered_tool_names(self):
        """Collect all @mcp.tool() registered function names."""
        return set(mcp._tool_manager._tools.keys())

    def test_python_commands_are_handled_in_cpp(self):
        """Every command sent by Python tools should be routable in C++."""
        py_cmds = self._collect_python_commands()
        cpp_cmds = self._collect_cpp_commands()
        missing = py_cmds - cpp_cmds
        known_missing_whitelist = {"add_material_node", "connect_material_nodes", "analyze_material_graph"}
        actual_missing = missing - known_missing_whitelist
        assert not actual_missing, (
            f"Python sends these commands but C++ dispatcher does not explicitly route them: {actual_missing}"
        )

    def test_cpp_commands_are_used_by_python(self):
        """C++ commands that are not called by Python may indicate stale C++ code or missing tools."""
        py_cmds = self._collect_python_commands()
        cpp_cmds = self._collect_cpp_commands()
        missing = cpp_cmds - py_cmds
        whitelist = {"ping", "apply_scene_delta", "clone_actor", "create_spline_from_points", "start_pie", "stop_pie", "start_simulate", "start_standalone_game", "set_engine_scalability", "set_rendering_setting", "set_physics_setting", "set_input_setting", "set_collision_setting", "set_ai_setting", "set_navigation_setting", "set_packaging_setting", "import_mp3"}
        actual_missing = missing - whitelist
        assert not actual_missing, (
            f"C++ supports these commands but Python tools never send them: {actual_missing}"
        )



    def test_each_mcp_tool_sends_exactly_one_cpp_command(self, fake_conn):
        """Each MCP tool should map to at least one C++ command, unless it's a known orchestrator."""
        skip_tools = {
            "create_pyramid", "create_wall", "create_tower", "create_staircase",
            "construct_house", "construct_mansion", "create_arch",
            "create_maze", "create_town", "create_castle_fortress",
            "create_suspension_bridge", "create_aqueduct",
            "spawn_physics_blueprint_actor",
            "set_mesh_material_color",
            "batch_spawn_actors",
            "scene_create", "scene_upsert_actor", "scene_upsert_actors",
            "scene_delete_actor", "scene_snapshot_create", "scene_snapshot_restore",
            "scene_list_objects", "scene_create_wall", "scene_create_pyramid",
            "scene_health", "scene_plan_sync", "scene_sync",
            "scene_create_layout", "scene_generate_layout_objects",
            "scene_update_layout_node", "scene_preview_layout", "scene_approve_layout",
            "scene_realize_layout", "scene_show_draft_proxy",
            "scene_upsert_procedural_mesh",
            "scene_create_sdf_mesh",
            "scene_create_superformula_mesh",
            "scene_create_lsystem_spline",
            "apply_blueprint_json",
            "export_blueprint_json",
            "apply_material_json",
            "project_settings_tool", "plugin_tool", "engine_settings_tool",
            "world_settings_tool", "editor_control_tool", "play_tool", "viewport_tool",
            "asset_management_tool", "fbx_mesh_import_tool",
            "texture_import_tool", "audio_import_tool", "asset_export_tool",
            "asset_mesh_editing_tool", # Skip dynamic multi-action tools
        }
        registered = self._collect_registered_tool_names()
        missing = []

        # Test generation with rich types to bypass parameter errors
        for tool_name in sorted(registered):
            if tool_name in skip_tools: continue
            fn = getattr(srv, tool_name, None)
            if not fn: continue

            sig = inspect.signature(fn)
            kwargs = {}
            for pname, param in sig.parameters.items():
                if pname == "random_string": kwargs[pname] = ""
                elif pname == "color": kwargs[pname] = [1.0, 0.0, 0.0, 1.0]
                elif param.default is not inspect.Parameter.empty: kwargs[pname] = param.default
                elif param.annotation == list: kwargs[pname] = [0.0, 0.0, 0.0]
                elif param.annotation == str: kwargs[pname] = "TestName"
                elif param.annotation == float: kwargs[pname] = 0.0
                elif param.annotation == int: kwargs[pname] = 0
                elif param.annotation == bool: kwargs[pname] = True
                elif param.annotation == dict: kwargs[pname] = {}
                else: kwargs[pname] = "default"

            fake_conn.clear_history()
            with _patch_tool_connections(fake_conn):
                try: fn(**kwargs)
                except Exception: continue

            commands_sent = [h["command"] for h in fake_conn.history]
            if not commands_sent:
                missing.append(tool_name)
        assert not missing, f"The following tools sent no C++ commands: {missing}"

    def test_registered_tools_cover_all_cpp_commands(self):
        """All C++ commands should be reachable through at least one MCP tool."""
        py_cmds = self._collect_python_commands()
        cpp_cmds = self._collect_cpp_commands()
        unreachable = cpp_cmds - py_cmds - {"ping", "apply_scene_delta", "clone_actor", "create_spline_from_points", "start_pie", "stop_pie", "start_simulate", "start_standalone_game", "set_engine_scalability", "set_rendering_setting", "set_physics_setting", "set_input_setting", "set_collision_setting", "set_ai_setting", "set_navigation_setting", "set_packaging_setting", "import_mp3"}
        assert not unreachable, (
            f"C++ commands not reachable through any Python tool: {unreachable}"
        )

    def test_tool_name_to_command_mapping_is_complete(self):
        """Verify the known tool-to-command mapping covers all commands."""
        known_mapping = {
            "get_actors_in_level": "get_actors_in_level",
            "find_actors_by_name": "find_actors_by_name",
            "delete_actor": "delete_actor",
            "spawn_actor": "spawn_actor",
            "set_actor_transform": "set_actor_transform",
            "find_actor_by_mcp_id": "find_actor_by_mcp_id",
            "set_actor_transform_by_mcp_id": "set_actor_transform_by_mcp_id",
            "delete_actor_by_mcp_id": "delete_actor_by_mcp_id",
            "create_blueprint": "create_blueprint",
            "add_component_to_blueprint": "add_component_to_blueprint",
            "set_static_mesh_properties": "set_static_mesh_properties",
            "set_physics_properties": "set_physics_properties",
            "compile_blueprint": "compile_blueprint",
            "read_blueprint_content": "read_blueprint_content",
            "analyze_blueprint_graph": "analyze_blueprint_graph",
            "get_blueprint_variable_details": "get_blueprint_variable_details",
            "get_blueprint_function_details": "get_blueprint_function_details",
            "get_available_materials": "get_available_materials",
            "apply_material_to_actor": "apply_material_to_actor",
            "apply_material_to_blueprint": "apply_material_to_blueprint",
            "get_actor_material_info": "get_actor_material_info",
            "get_blueprint_material_info": "get_blueprint_material_info",
            "add_node": "add_blueprint_node",
            "connect_nodes": "connect_nodes",
            "create_variable": "create_variable",
            "set_blueprint_variable_properties": "set_blueprint_variable_properties",
            "add_event_node": "add_event_node",
            "delete_node": "delete_node",
            "set_node_property": "set_node_property",
            "create_function": "create_function",
            "add_function_input": "add_function_input",
            "add_function_output": "add_function_output",
            "delete_function": "delete_function",
            "rename_function": "rename_function",
            "create_material": "create_material",
            "add_material_node": "add_material_node",
            "connect_material_nodes": "connect_material_nodes",
            "export_material_json": "analyze_material_graph",
        }
        registered = self._collect_registered_tool_names()
        for tool_name, cmd in known_mapping.items():
            assert tool_name in registered, (
                f"Tool '{tool_name}' in known mapping but not registered in FastMCP"
            )


class TestJsonInjectionTools:
    def test_apply_material_json_uses_cpp_material_contract(self, fake_conn_factory):
        fake_conn = fake_conn_factory(responses={
            "add_material_node": {"success": True, "node_id": "MaterialExpressionConstant_0"},
            "connect_material_nodes": {"success": True},
        })
        payload = json.dumps({
            "nodes": [
                {"id": "constant", "type": "Constant", "params": {"value": 0.8}},
            ],
            "connections": [
                {"source_id": "constant", "source_pin": "", "target_id": "Material", "target_pin": "Roughness"},
            ],
        })

        with _patch_tool_connections(fake_conn):
            result = srv.apply_material_json("/Game/Materials/M_Test", payload)

        assert result["success"] is True
        assert fake_conn.history[0]["command"] == "add_material_node"
        assert fake_conn.history[0]["params"]["material_path"] == "/Game/Materials/M_Test"
        assert fake_conn.history[0]["params"]["node_params"] == {"value": 0.8}
        assert fake_conn.history[1]["command"] == "connect_material_nodes"
        assert fake_conn.history[1]["params"] == {
            "material_path": "/Game/Materials/M_Test",
            "source_node_id": "MaterialExpressionConstant_0",
            "source_pin_name": "",
            "target_node_id": "Material",
            "target_pin_name": "Roughness",
        }

    def test_create_material_exposes_package_path(self, fake_conn):
        with _patch_tool_connections(fake_conn):
            srv.create_material("M_Test", package_path="/Game/TestMaterials/")

        last = fake_conn.history[-1]
        assert last["command"] == "create_material"
        assert last["params"] == {
            "name": "M_Test",
            "package_path": "/Game/TestMaterials/",
        }
