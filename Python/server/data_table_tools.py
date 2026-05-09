"""Data Table tools for the Unreal MCP server."""

import logging
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")


@mcp.tool()
def create_data_table(table_path: str, row_struct_path: str) -> Dict[str, Any]:
    """Create a new DataTable asset with the specified row struct.

    table_path: Asset path (e.g., /Game/Data/MyTable)
    row_struct_path: Path to the UScriptStruct to use as row type (e.g., /Game/Structs/MyRowStruct)
    """
    try:
        validate_string(table_path, "table_path")
        validate_string(row_struct_path, "row_struct_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "create_data_table", {"table_path": table_path, "row_struct_path": row_struct_path}
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"create_data_table error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def import_csv_to_data_table(table_path: str, csv_content: str) -> Dict[str, Any]:
    """Import CSV content into an existing DataTable.

    table_path: Asset path to the DataTable
    csv_content: Raw CSV string (first row = headers matching struct properties, first column = RowName)
    """
    try:
        validate_string(table_path, "table_path")
        validate_string(csv_content, "csv_content")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "import_csv_to_data_table", {"table_path": table_path, "csv_content": csv_content}
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"import_csv_to_data_table error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def add_data_table_row(table_path: str, row_name: str, row_data: Dict[str, Any]) -> Dict[str, Any]:
    """Add a single row to an existing DataTable.

    table_path: Asset path to the DataTable
    row_name: Unique name for the row
    row_data: JSON object where keys are struct property names and values are the data
    """
    try:
        validate_string(table_path, "table_path")
        validate_string(row_name, "row_name")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)
    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")
    try:
        response = unreal.send_command(
            "add_data_table_row",
            {"table_path": table_path, "row_name": row_name, "row_data": row_data},
        )
        return response or make_error_response("No response from Unreal")
    except Exception as e:
        logger.error(f"add_data_table_row error: {e}")
        return make_error_response(str(e))
