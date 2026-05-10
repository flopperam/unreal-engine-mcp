"""Validation and testing tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def compile_all_blueprints() -> Dict[str, Any]:
    """Compile all Blueprint assets in the project and report errors/warnings."""
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("compile_all_blueprints", {})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"compile_all_blueprints error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def run_map_check() -> Dict[str, Any]:
    """Run the editor's map check on the current level.

    Returns errors and warnings such as actors without root components,
    missing static meshes, etc.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("run_map_check", {})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"run_map_check error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def find_broken_references() -> Dict[str, Any]:
    """Scan the current level for actors with broken references (missing mesh/material).

    Returns a list of affected actors and the specific issues found.
    """
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        response = unreal.send_command("find_broken_references", {})
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"find_broken_references error: {e}")
        return make_error_response(str(e))
