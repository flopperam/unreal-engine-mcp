"""
Structured error codes for the Unreal MCP protocol.
Mirror of C++ MCPErrorCodes namespace in EpicUnrealMCPErrorCodes.h.
"""


class MCPErrorCode:
    OK = "OK"
    UNKNOWN_ERROR = "UNKNOWN_ERROR"
    UNKNOWN_COMMAND = "UNKNOWN_COMMAND"

    INVALID_PARAM = "INVALID_PARAM"
    MISSING_PARAM = "MISSING_PARAM"

    NOT_FOUND = "NOT_FOUND"
    ACTOR_NOT_FOUND = "ACTOR_NOT_FOUND"
    BLUEPRINT_NOT_FOUND = "BLUEPRINT_NOT_FOUND"
    ASSET_NOT_FOUND = "ASSET_NOT_FOUND"

    TRANSACTION_FAILED = "TRANSACTION_FAILED"
    TRANSACTION_NOT_ACTIVE = "TRANSACTION_NOT_ACTIVE"
    TRANSACTION_ALREADY_ACTIVE = "TRANSACTION_ALREADY_ACTIVE"
    UNDO_FAILED = "UNDO_FAILED"
    REDO_FAILED = "REDO_FAILED"

    CONNECTION_ERROR = "CONNECTION_ERROR"
    TIMEOUT = "TIMEOUT"
    PROTOCOL_ERROR = "PROTOCOL_ERROR"

    BATCH_PARTIAL_FAILURE = "BATCH_PARTIAL_FAILURE"
    BATCH_ABORTED = "BATCH_ABORTED"

    DUPLICATE_REQUEST = "DUPLICATE_REQUEST"
    VERSION_MISMATCH = "VERSION_MISMATCH"

    EDITOR_BUSY = "EDITOR_BUSY"
    OPERATION_NOT_SUPPORTED = "OPERATION_NOT_SUPPORTED"


def make_error_response(error_code: str, message: str, details: dict = None) -> dict:
    """Create a structured error response."""
    resp = {
        "success": False,
        "status": "error",
        "error_code": error_code,
        "message": message,
    }
    if details:
        resp["error_details"] = details
    return resp


def make_success_response(data: dict = None, message: str = "OK") -> dict:
    """Create a structured success response."""
    resp = {
        "success": True,
        "status": "success",
        "message": message,
    }
    if data:
        resp["result"] = data
    return resp
