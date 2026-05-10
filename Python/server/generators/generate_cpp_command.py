#!/usr/bin/env python3
"""C++ command boilerplate generator for the Unreal MCP plugin.

Given a command specification, generates the four pieces of boilerplate
required to add a new command:
1. Bridge routing map entry
2. Bridge switch-case entry
3. Handler header declaration
4. Handler implementation stub
5. Python MCP tool wrapper

Usage:
    python generate_cpp_command.py --spec spawn_camera_actor.json
    python generate_cpp_command.py --interactive
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List

try:
    import jinja2
except ImportError:  # pragma: no cover
    print("jinja2 is required. Install with: pip install jinja2", file=sys.stderr)
    sys.exit(1)


TEMPLATE_DIR = Path(__file__).parent / "templates"


# Mapping from Python-like types to C++ types for JSON parameter extraction.
PARAM_TYPE_MAP = {
    "str": {"cpp_type": "FString", "json_getter": "String", "py_type": "str"},
    "int": {"cpp_type": "int32", "json_getter": "Number", "py_type": "int"},
    "float": {"cpp_type": "double", "json_getter": "Number", "py_type": "float"},
    "bool": {"cpp_type": "bool", "json_getter": "Bool", "py_type": "bool"},
    "List[float]": {"cpp_type": "TArray<double>", "json_getter": "Array", "py_type": "List[float]"},
    "Dict[str, float]": {"cpp_type": "TSharedPtr<FJsonObject>", "json_getter": "Object", "py_type": "Dict[str, float]"},
    "Optional[str]": {"cpp_type": "FString", "json_getter": "String", "py_type": "Optional[str]"},
    "Optional[List[float]]": {"cpp_type": "TArray<double>", "json_getter": "Array", "py_type": "Optional[List[float]]"},
}


def to_pascal(name: str) -> str:
    """Convert snake_case to PascalCase."""
    return "".join(word.capitalize() for word in name.split("_"))


def to_camel(name: str) -> str:
    """Convert snake_case to camelCase."""
    parts = name.split("_")
    return parts[0] + "".join(word.capitalize() for word in parts[1:])


def enrich_params(params: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Add derived fields (cpp_type, getter, py_type) to each param."""
    enriched = []
    for p in params:
        typ = p.get("type", "str")
        mapping = PARAM_TYPE_MAP.get(typ, PARAM_TYPE_MAP["str"])
        enriched.append(
            {
                **p,
                "cpp_type": mapping["cpp_type"],
                "json_getter": mapping["json_getter"],
                "py_type": mapping["py_type"],
                "camel_name": to_camel(p["name"]),
                "required": p.get("required", True),
                "default": p.get("default", "None"),
            }
        )
    return enriched


def build_context(spec: Dict[str, Any]) -> Dict[str, Any]:
    """Prepare the Jinja2 context from a raw spec."""
    params = enrich_params(spec.get("params", []))
    name = spec["command_name"]
    category = spec.get("category", "Editor")
    return {
        "command": {
            "snake_name": name,
            "pascal_name": to_pascal(name),
            "camel_name": to_camel(name),
            "category": category,
            "category_id": spec.get("category_id", 1),
            "params": params,
            "actor_class": spec.get("actor_class"),
            "description": spec.get("description", ""),
        }
    }


def render_template(template_name: str, context: Dict[str, Any]) -> str:
    """Render a single Jinja2 template."""
    env = jinja2.Environment(loader=jinja2.FileSystemLoader(str(TEMPLATE_DIR)), trim_blocks=True, lstrip_blocks=True)
    template = env.get_template(template_name)
    return template.render(context)


def generate_all(spec: Dict[str, Any]) -> Dict[str, str]:
    """Generate all boilerplate pieces for a command."""
    ctx = build_context(spec)
    return {
        "bridge_entry": render_template("bridge_entry.cpp.j2", ctx),
        "header": render_template("cpp_handler.h.j2", ctx),
        "handler": render_template("cpp_handler.cpp.j2", ctx),
        "py_tool": render_template("py_tool.py.j2", ctx),
    }


def print_insertion_hints(command_name: str, category: str, category_id: int) -> None:
    """Print file paths and anchor comments for manual insertion."""
    header_file = f"Plugins/UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCP{category}Commands.h"
    cpp_file = f"Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCP{category}Commands.cpp"
    bridge_file = "Plugins/UnrealMCP/Source/UnrealMCP/Private/EpicUnrealMCPBridge.cpp"

    print("\n=== Insertion Hints ===")
    print(f"1. Bridge routing map (around line ~500 in {bridge_file}):")
    print(f"   Add: {{TEXT(\"{command_name}\"), {category_id}}},")
    print(f"2. Bridge switch case (around line ~900 in {bridge_file}):")
    print(f"   Ensure: case {category_id}: // {category}Commands")
    print(f"3. Header declaration ({header_file}):")
    print(f"   Insert before the closing '}};' in the private section.")
    print(f"4. Handler implementation ({cpp_file}):")
    print(f"   Add to the Dispatch TMap and implement the handler body.")
    print(f"5. Python tool (Python/server/<category>_tools.py):")
    print(f"   Paste the generated Python wrapper and register it in __init__.py if new file.")


def main() -> None:  # pragma: no cover
    parser = argparse.ArgumentParser(description="Generate C++ command boilerplate for Unreal MCP")
    parser.add_argument("--spec", type=Path, help="Path to JSON spec file")
    parser.add_argument("--interactive", action="store_true", help="Interactive mode")
    parser.add_argument("--output", type=Path, help="Write output to file instead of stdout")
    args = parser.parse_args()

    if args.interactive:
        print("Interactive mode not yet implemented. Use --spec.")
        sys.exit(1)

    if not args.spec:
        print("Usage: python generate_cpp_command.py --spec command_spec.json")
        sys.exit(1)

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    pieces = generate_all(spec)

    output_lines = []
    output_lines.append("=" * 60)
    output_lines.append("Generated Boilerplate")
    output_lines.append("=" * 60)

    for key, text in pieces.items():
        output_lines.append(f"\n--- {key} ---\n")
        output_lines.append(text)

    result = "\n".join(output_lines)

    if args.output:
        args.output.write_text(result, encoding="utf-8")
        print(f"Output written to {args.output}")
    else:
        print(result)

    print_insertion_hints(spec["command_name"], spec.get("category", "Editor"), spec.get("category_id", 1))


if __name__ == "__main__":  # pragma: no cover
    main()
