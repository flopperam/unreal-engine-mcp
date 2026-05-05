"""Content Browser / Asset Management tools for the Unreal MCP server.

Grouped tools exposing Content Browser C++ commands through a single Python MCP tool.
Each tool uses an `action` parameter to dispatch to the correct C++ command.
"""

import logging
from typing import Dict, Any, Optional, List

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_ContentBrowser")


@mcp.tool()
def asset_management_tool(
    action: str,
    folder_path: Optional[str] = None,
    asset_path: Optional[str] = None,
    source_path: Optional[str] = None,
    dest_path: Optional[str] = None,
    new_name: Optional[str] = None,
    query: Optional[str] = None,
    class_name: Optional[str] = None,
    recursive: Optional[bool] = None,
    asset_paths: Optional[List[str]] = None,
    new_names: Optional[List[str]] = None,
    key: Optional[str] = None,
    value: Optional[str] = None,
    remove: Optional[bool] = None,
    tag_key: Optional[str] = None,
    tag_value: Optional[str] = None,
    max_depth: Optional[int] = None,
    search_text: Optional[str] = None,
    replace_text: Optional[str] = None,
    prefix: Optional[str] = None,
    suffix: Optional[str] = None,
    label_path: Optional[str] = None,
    label_color: Optional[str] = None,
    bundle_name: Optional[str] = None,
    default_priority: Optional[int] = None,
    default_chunk_id: Optional[int] = None,
    default_should_be_in_main_manifest: Optional[bool] = None,
    default_should_be_loaded_on_demand: Optional[bool] = None,
) -> Dict[str, Any]:
    """Manage Content Browser folders and assets.

    Actions:
      create_folder       - Create a folder (requires folder_path like /Game/MyFolder).
      delete_folder       - Delete an empty folder (requires folder_path).
      list_assets         - List assets in a folder (requires folder_path; optional class_name, recursive).
      search_assets       - Search assets by query/class (optional folder_path, class_name, query, recursive).
      resolve_asset_path  - Resolve and check existence of an asset path (requires asset_path).
      move_asset          - Move an asset (requires source_path, dest_path).
      copy_asset          - Copy an asset (requires source_path, dest_path).
      duplicate_asset     - Duplicate in-place (requires asset_path; optional new_name).
      rename_asset        - Rename an asset (requires asset_path, new_name).
      delete_asset        - Delete an asset (requires asset_path).
      load_asset          - Load an asset into memory (requires asset_path).
      unload_asset        - Unload a package from memory (requires asset_path).
      save_assets         - Save multiple assets (requires asset_paths list).
      get_metadata        - Return registry tags and package metadata (requires asset_path).
      set_metadata        - Set/remove package metadata (requires asset_path, key; value unless remove=True).
      tag_asset           - Store a lightweight metadata tag (requires asset_path, tag_key; optional tag_value).
      find_redirectors    - List redirector assets (optional folder_path, recursive).
      fixup_redirectors   - Fix redirectors (optional asset_paths or folder_path).
      find_unused_assets  - List assets with no on-disk referencers (optional folder_path).
      get_references      - List assets that reference asset_path.
      get_dependencies    - List assets referenced by asset_path.
      reference_graph     - Return reference/dependency graph (requires asset_path; optional max_depth).
      audit_assets        - Return audit data for asset_paths or folder_path.
      registry_search     - Search Asset Registry with optional tag filter.
      bulk_rename         - Rename assets using new_names or search/replace/prefix/suffix.
      bulk_move           - Move asset_paths into dest_path folder.
      bulk_delete         - Delete asset_paths.
      create_primary_asset_label - Create Primary Asset Label (requires label_path; optional asset_paths, label_color).
      delete_primary_asset_label - Delete Primary Asset Label (requires label_path).
      list_primary_asset_labels - List all Primary Asset Labels (optional folder_path, recursive).
      get_asset_manager_settings - Get Asset Manager type info and default rules.
      set_asset_manager_settings - Update Asset Manager default rules (optional: default_priority, default_chunk_id, etc.).
      add_primary_asset_bundle - Add an asset to a primary asset bundle (requires bundle_name, asset_path).
    """
    try:
        validate_string(action, "action")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        if action == "create_folder":
            if not folder_path:
                return make_error_response("folder_path is required")
            return unreal.send_command("create_folder", {"folder_path": folder_path})

        elif action == "delete_folder":
            if not folder_path:
                return make_error_response("folder_path is required")
            return unreal.send_command("delete_folder", {"folder_path": folder_path})

        elif action == "list_assets":
            if not folder_path:
                return make_error_response("folder_path is required")
            params = {"folder_path": folder_path}
            if class_name is not None:
                params["class_name"] = class_name
            if recursive is not None:
                params["recursive"] = recursive
            return unreal.send_command("list_assets", params)

        elif action == "search_assets":
            params = {}
            if folder_path is not None:
                params["folder_path"] = folder_path
            if class_name is not None:
                params["class_name"] = class_name
            if query is not None:
                params["query"] = query
            if recursive is not None:
                params["recursive"] = recursive
            else:
                params["recursive"] = True
            return unreal.send_command("search_assets", params)

        elif action == "resolve_asset_path":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("resolve_asset_path", {"asset_path": asset_path})

        elif action == "move_asset":
            if not source_path or not dest_path:
                return make_error_response("source_path and dest_path are required")
            return unreal.send_command("move_asset", {"source_path": source_path, "dest_path": dest_path})

        elif action == "copy_asset":
            if not source_path or not dest_path:
                return make_error_response("source_path and dest_path are required")
            return unreal.send_command("copy_asset", {"source_path": source_path, "dest_path": dest_path})

        elif action == "duplicate_asset":
            if not asset_path:
                return make_error_response("asset_path is required")
            params = {"asset_path": asset_path}
            if new_name is not None:
                params["new_name"] = new_name
            return unreal.send_command("duplicate_asset", params)

        elif action == "rename_asset":
            if not asset_path or not new_name:
                return make_error_response("asset_path and new_name are required")
            return unreal.send_command("rename_asset", {"asset_path": asset_path, "new_name": new_name})

        elif action == "delete_asset":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("delete_asset", {"asset_path": asset_path})

        elif action == "load_asset":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("load_asset", {"asset_path": asset_path})

        elif action == "unload_asset":
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("unload_asset", {"asset_path": asset_path})

        elif action == "save_assets":
            if not asset_paths:
                return make_error_response("asset_paths list is required")
            return unreal.send_command("save_assets", {"asset_paths": asset_paths})

        elif action in ("get_metadata", "get_asset_metadata"):
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("get_asset_metadata", {"asset_path": asset_path})

        elif action in ("set_metadata", "set_asset_metadata"):
            if not asset_path or not key:
                return make_error_response("asset_path and key are required")
            params = {"asset_path": asset_path, "key": key}
            if value is not None:
                params["value"] = value
            if remove is not None:
                params["remove"] = remove
            return unreal.send_command("set_asset_metadata", params)

        elif action == "tag_asset":
            if not asset_path or not tag_key:
                return make_error_response("asset_path and tag_key are required")
            params = {"asset_path": asset_path, "tag_key": tag_key}
            if tag_value is not None:
                params["tag_value"] = tag_value
            return unreal.send_command("tag_asset", params)

        elif action == "find_redirectors":
            params = {}
            if folder_path is not None:
                params["folder_path"] = folder_path
            if recursive is not None:
                params["recursive"] = recursive
            return unreal.send_command("find_redirectors", params)

        elif action == "fixup_redirectors":
            params = {}
            if asset_paths is not None:
                params["asset_paths"] = asset_paths
            if folder_path is not None:
                params["folder_path"] = folder_path
            if recursive is not None:
                params["recursive"] = recursive
            return unreal.send_command("fixup_redirectors", params)

        elif action == "find_unused_assets":
            params = {}
            if folder_path is not None:
                params["folder_path"] = folder_path
            if recursive is not None:
                params["recursive"] = recursive
            return unreal.send_command("find_unused_assets", params)

        elif action in ("get_references", "get_asset_references"):
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("get_asset_references", {"asset_path": asset_path})

        elif action in ("get_dependencies", "get_asset_dependencies"):
            if not asset_path:
                return make_error_response("asset_path is required")
            return unreal.send_command("get_asset_dependencies", {"asset_path": asset_path})

        elif action in ("reference_graph", "get_asset_reference_graph"):
            if not asset_path:
                return make_error_response("asset_path is required")
            params = {"asset_path": asset_path}
            if max_depth is not None:
                params["max_depth"] = max_depth
            return unreal.send_command("get_asset_reference_graph", params)

        elif action in ("audit_assets", "asset_audit"):
            params = {}
            if asset_paths is not None:
                params["asset_paths"] = asset_paths
            if folder_path is not None:
                params["folder_path"] = folder_path
            if recursive is not None:
                params["recursive"] = recursive
            return unreal.send_command("audit_assets", params)

        elif action in ("registry_search", "asset_registry_search"):
            params = {}
            if folder_path is not None:
                params["folder_path"] = folder_path
            if class_name is not None:
                params["class_name"] = class_name
            if query is not None:
                params["query"] = query
            if recursive is not None:
                params["recursive"] = recursive
            if tag_key is not None:
                params["tag_key"] = tag_key
            if tag_value is not None:
                params["tag_value"] = tag_value
            return unreal.send_command("asset_registry_search", params)

        elif action == "bulk_rename":
            if not asset_paths:
                return make_error_response("asset_paths list is required")
            params = {"asset_paths": asset_paths}
            if new_names is not None:
                params["new_names"] = new_names
            if search_text is not None:
                params["search_text"] = search_text
            if replace_text is not None:
                params["replace_text"] = replace_text
            if prefix is not None:
                params["prefix"] = prefix
            if suffix is not None:
                params["suffix"] = suffix
            return unreal.send_command("bulk_rename", params)

        elif action == "bulk_move":
            if not asset_paths or not dest_path:
                return make_error_response("asset_paths and dest_path are required")
            return unreal.send_command("bulk_move", {"asset_paths": asset_paths, "dest_path": dest_path})

        elif action == "bulk_delete":
            if not asset_paths:
                return make_error_response("asset_paths list is required")
            return unreal.send_command("bulk_delete", {"asset_paths": asset_paths})

        elif action == "create_primary_asset_label":
            if not label_path:
                return make_error_response("label_path is required")
            params = {"label_path": label_path}
            if asset_paths is not None:
                params["asset_paths"] = asset_paths
            if label_color is not None:
                params["label_color"] = label_color
            return unreal.send_command("create_primary_asset_label", params)

        elif action == "delete_primary_asset_label":
            if not label_path:
                return make_error_response("label_path is required")
            return unreal.send_command("delete_primary_asset_label", {"label_path": label_path})

        elif action == "list_primary_asset_labels":
            params = {}
            if folder_path is not None:
                params["folder_path"] = folder_path
            if recursive is not None:
                params["recursive"] = recursive
            return unreal.send_command("list_primary_asset_labels", params)

        elif action == "get_asset_manager_settings":
            return unreal.send_command("get_asset_manager_settings", {})

        elif action == "set_asset_manager_settings":
            params = {}
            if default_priority is not None:
                params["default_priority"] = default_priority
            if default_chunk_id is not None:
                params["default_chunk_id"] = default_chunk_id
            if default_should_be_in_main_manifest is not None:
                params["default_should_be_in_main_manifest"] = default_should_be_in_main_manifest
            if default_should_be_loaded_on_demand is not None:
                params["default_should_be_loaded_on_demand"] = default_should_be_loaded_on_demand
            return unreal.send_command("set_asset_manager_settings", params)

        elif action == "add_primary_asset_bundle":
            if not bundle_name or not asset_path:
                return make_error_response("bundle_name and asset_path are required")
            return unreal.send_command("add_primary_asset_bundle", {"bundle_name": bundle_name, "asset_path": asset_path})

        else:
            return make_error_response(f"Unknown asset_management_tool action: {action}")
    except Exception as e:
        logger.error(f"asset_management_tool error: {e}")
        return make_error_response(str(e))
