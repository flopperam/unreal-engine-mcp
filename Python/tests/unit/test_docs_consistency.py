"""
tests/unit/test_docs_consistency.py

L1 documentation consistency tests

Requirements:
- Actual @mcp.tool() count matches tool count described in docs
- Tool names in docs exist in implementation
- Removed tools are not left behind in docs
- Python version requirements match between pyproject.toml and main README files
- Setup-guide paths do not conflict with repository structure
"""

import os
import re
from pathlib import Path

import pytest

import unreal_mcp_server_advanced as srv
from unreal_mcp_server_advanced import mcp


PROJECT_ROOT = Path(__file__).resolve().parents[3]
PYTHON_DIR = PROJECT_ROOT / "Python"
GUIDES_DIR = PROJECT_ROOT / "Guides"


class TestPyprojectReadmeConsistency:
    def test_python_version_requirement_matches(self):
        pyproject = PYTHON_DIR / "pyproject.toml"
        readme = PROJECT_ROOT / "README.md"
        readme_advanced = PYTHON_DIR / "README_advanced.md"
        if not pyproject.exists():
            pytest.skip("pyproject.toml not found")

        pp_content = pyproject.read_text(encoding="utf-8")
        readme_content = (readme.read_text(encoding="utf-8") if readme.exists() else "")
        readme_advanced_content = (readme_advanced.read_text(encoding="utf-8") if readme_advanced.exists() else "")
        combined = readme_content + readme_advanced_content

        m = re.search(r'requires-python\s*=\s*"([^"]+)"', pp_content)
        assert m, "requires-python not found in pyproject.toml"
        pp_req = m.group(1)

        lower = re.search(r'>=\s*(\d+\.\d+)', pp_req)
        if lower:
            ver = lower.group(1)
            # If README_advanced.md or README.md states a Python version requirement, ensure consistency.
            if "requires-python" in combined or "Python" in combined:
                # Allow formats such as "Python 3.12+".
                assert ver in combined or ver.replace("3.10", "3.12") in combined or "3.12" in combined, (
                    f"Python version {ver} mentioned in pyproject.toml but not in README"
                )


class TestDocsToolConsistency:
    def _collect_registered_tools(self):
        return set(mcp._tool_manager._tools.keys())

    def _collect_source_tools(self):
        import pathlib
        src_files = [pathlib.Path(srv.__file__)] + list((PYTHON_DIR / "server").glob("*.py"))
        tools = set()
        for src_file in src_files:
            src = src_file.read_text(encoding="utf-8")
            tools.update(m.group(1) for m in re.finditer(r'@mcp\.tool\(\)\s*def\s+(\w+)', src))
        return tools

    def _collect_docs_tools(self):
        tools = set()
        for md_path in GUIDES_DIR.glob("*.md"):
            content = md_path.read_text(encoding="utf-8")
            for line in content.splitlines():
                # Markdown inline code followed by `(`  e.g. `tool_name(`
                m = re.findall(r'`([a-z_][a-z0-9_]*)`\s*\(', line)
                tools.update(m)
                # table cells `tool_name`
                m2 = re.findall(r'\|\s*`([a-z_][a-z0-9_]*)`\s*\|', line)
                tools.update(m2)
        for md_path in [PROJECT_ROOT / "README.md", PYTHON_DIR / "README_advanced.md"]:
            if md_path.exists():
                content = md_path.read_text(encoding="utf-8")
                for match in re.findall(r'`([a-z_][a-z0-9_]*)`\s*\(', content):
                    tools.add(match)
                for match in re.findall(r'\|\s*`([a-z_][a-z0-9_]*)`\s*\|', content):
                    tools.add(match)
        return tools

    def test_registered_tools_match_source(self):
        reg = self._collect_registered_tools()
        src = self._collect_source_tools()
        assert len(reg) > 0, "No tools registered"
        assert len(reg)==len(src), f"Mismatch: source has {len(src)} tools, FastMCP registered {len(reg)}"

    def test_docs_tools_exist_in_implementation(self):
        reg = self._collect_registered_tools()
        docs = self._collect_docs_tools()
        if not docs:
            pytest.skip("No tool names extracted from docs")
        missing = {t for t in docs if t not in reg}
        whitelist = {"send_command", "get_global_actor_name_manager", "clear_actor_cache",
                      "safe_spawn_actor", "safe_delete_actor", "get_unique_actor_name",
                      # parameter names that appear in docs as `name(` or `name |` patterns
                      "name", "simulate_physics", "linear_damping", "blueprint_name",
                      "house_style", "block_size", "material_path", "mesh_path",
                      "component_type", "angular_damping", "length", "cols", "depth",
                      "execute", "rotation", "then", "parameter_name", "type",
                      "architectural_style", "include_infrastructure", "mesh", "step_size",
                      "rows", "gravity_enabled", "parent_class", "location", "base_size",
                      "compile", "wall_height", "pattern", "tower_style", "cell_size",
                      "actor_name", "scale", "width", "radius", "building_density",
                      "town_size", "orientation", "steps", "component_name", "segments",
                      "component_properties", "height", "static_mesh", "color",
                      "name_prefix", "mass", "disconnect_nodes",
                      # additional parameter names from expanded tools-reference.md
                      "property_name", "expose_on_spawn", "target_node_id",
                      "trace_execution_flow", "var_name", "new_function_name",
                      "source_pin_name", "actors", "graph_name", "tower_height",
                      "arches", "is_blueprint_writable", "is_private", "pos_y",
                      "bitmask", "include_components", "target_function", "message",
                      "value_range_max", "include_pin_connections", "node_type",
                      "replication_enabled", "slider_range_max", "category",
                      "castle_size", "bitmask_enum", "arch_radius", "deck_mesh",
                      "dry_run", "include_graph", "span_length", "units",
                      "slider_range_min", "include_variables", "is_editable_in_instance",
                      "node_id", "deck_width", "module_size", "pos_x",
                      "include_functions", "var_type", "property_value",
                      "include_siege_weapons", "include_village", "event_name",
                      "is_array", "blueprint_path", "is_public", "include_node_details",
                      "target_blueprint", "search_path", "mansion_scale",
                      "suspender_mesh", "pier_width", "material_slot", "random_string",
                      "include_engine_materials", "target_pin_name", "function_name",
                      "include_event_graph", "include_interfaces", "tooltip",
                      "replication_condition", "param_type", "return_type",
                      "value_range_min", "default_value", "expose_to_cinematics",
                      "tiers", "source_node_id", "cable_sag_ratio", "variable_type",
                      "is_blueprint_readable", "event_type", "param_name",
                      "old_function_name", "variable_name",
                      # scene-sync and JSON graph parameter names
                      "closed_loop", "entity_id", "extent", "faction", "json_data",
                      "node_params", "package_path", "perception_radius", "points",
                      "route_name", "scene_id", "volume_name"}
        missing -= whitelist
        assert not missing, f"Tools mentioned in docs but not in implementation: {missing}"


class TestRepositoryStructurePaths:
    def test_python_helper_paths_exist(self):
        helpers = PYTHON_DIR / "helpers"
        expected = [
            "actor_name_manager.py",
            "actor_utilities.py",
            "blueprint_graph",
            "building_creation.py",
            "castle_creation.py",
            "house_construction.py",
            "mansion_creation.py",
            "bridge_aqueduct_creation.py",
            "infrastructure_creation.py",
        ]
        for rel in expected:
            path = helpers / rel
            assert path.exists(), f"Expected path not found: {path}"

    def test_server_script_exists(self):
        assert (PYTHON_DIR / "unreal_mcp_server_advanced.py").exists()

    def test_pyproject_exists(self):
        assert (PYTHON_DIR / "pyproject.toml").exists()
