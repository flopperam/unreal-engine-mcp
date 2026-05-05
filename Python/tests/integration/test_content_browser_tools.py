"""Integration tests for Content Browser / Asset Management tools."""

import logging
from typing import Any, Dict

from server.core import get_unreal_connection

logger = logging.getLogger(__name__)

UNREAL = get_unreal_connection()


def _send(cmd: str, **kwargs) -> Dict[str, Any]:
    """Send raw C++ command via unreal connection."""
    if not UNREAL:
        raise RuntimeError("Unreal connection not available")
    return UNREAL.send_command(cmd, kwargs)


def _ensure_blueprint(path: str) -> str:
    """Create a dummy blueprint at the given path if it doesn't exist."""
    resolve = _send("resolve_asset_path", asset_path=path)
    if resolve.get("success") and resolve.get("exists"):
        return path

    name = path.rstrip("/").split("/")[-1]
    temp_path = f"/Game/Blueprints/{name}"
    result = _send("create_blueprint", name=name, parent_class="Actor")
    if not result.get("success"):
        temp_resolve = _send("resolve_asset_path", asset_path=temp_path)
        if not temp_resolve.get("success") or not temp_resolve.get("exists"):
            raise RuntimeError(f"Failed to create test blueprint: {result}")

    if path != temp_path:
        move_result = _send("move_asset", source_path=temp_path, dest_path=path)
        if not move_result.get("success"):
            raise RuntimeError(f"Failed to move test blueprint into place: {move_result}")

    save_result = _send("save_assets", asset_paths=[path])
    if not save_result.get("success"):
        logger.warning("Blueprint save warning: %s", save_result)
    return path


def _cleanup(paths: list, folders: list = None):
    folders = folders or []
    for p in paths:
        try:
            _send("delete_asset", asset_path=p)
        except Exception:
            pass
    for f in folders:
        try:
            _send("delete_folder", folder_path=f)
        except Exception:
            pass


def test_create_and_delete_folder():
    folder = "/Game/MCP_TestFolder"
    try:
        _send("delete_folder", folder_path=folder)
    except Exception:
        pass
    result = _send("create_folder", folder_path=folder)
    assert result.get("success"), f"create_folder failed: {result}"
    assert result["folder_path"] == folder
    result = _send("delete_folder", folder_path=folder)
    assert result.get("success"), f"delete_folder failed: {result}"
    result = _send("list_assets", folder_path=folder)
    assert result.get("success")
    assert result.get("count", -1) == 0


def test_list_assets():
    folder = "/Game/MCP_ListTest"
    bp_path = "/Game/MCP_ListTest/MCP_ListBP"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("list_assets", folder_path=folder, recursive=False)
        assert result.get("success")
        assets = result.get("assets", [])
        assert any(a["asset_name"] == "MCP_ListBP" for a in assets), f"Expected blueprint not listed: {assets}"
    finally:
        _cleanup([bp_path], [folder])


def test_resolve_asset_path():
    folder = "/Game/MCP_ResolveTest"
    bp_path = f"{folder}/MCP_ResolveBP"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("resolve_asset_path", asset_path=bp_path)
        assert result.get("success")
        assert result["exists"] is True
        assert bp_path in result["resolved_path"]
    finally:
        _cleanup([bp_path], [folder])


def test_search_assets():
    folder = "/Game/MCP_SearchTest"
    bp_path = f"{folder}/MCP_SearchBP"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("search_assets", query="MCP_SearchBP", folder_path=folder)
        assert result.get("success")
        assets = result.get("assets", [])
        assert any("MCP_SearchBP" in a["asset_path"] for a in assets), f"Search did not find asset: {assets}"
    finally:
        _cleanup([bp_path], [folder])


def test_rename_asset():
    folder = "/Game/MCP_RenameTest"
    bp_path = f"{folder}/MCP_RenameBP"
    new_name = "MCP_RenameBP_Renamed"
    new_path = f"{folder}/{new_name}"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("rename_asset", asset_path=bp_path, new_name=new_name)
        assert result.get("success"), f"rename failed: {result}"
        assert result["new_name"] == new_name
        resolve = _send("resolve_asset_path", asset_path=bp_path)
        assert not resolve["exists"], "Old asset path should not exist after rename"
        resolve = _send("resolve_asset_path", asset_path=new_path)
        assert resolve["exists"], "New asset path should exist after rename"
    finally:
        _cleanup([bp_path, new_path], [folder])


def test_duplicate_asset():
    folder = "/Game/MCP_DupTest"
    bp_path = f"{folder}/MCP_DupBP"
    dup_path = f"{folder}/MCP_DupBP_Copy"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("duplicate_asset", asset_path=bp_path)
        assert result.get("success"), f"duplicate failed: {result}"
        dup = _send("resolve_asset_path", asset_path=dup_path)
        assert dup["exists"], "Duplicated asset should exist"
    finally:
        _cleanup([bp_path, dup_path], [folder])


def test_move_asset():
    src_folder = "/Game/MCP_MoveSrc"
    dst_folder = "/Game/MCP_MoveDst"
    bp_path = f"{src_folder}/MCP_MoveBP"
    moved_path = f"{dst_folder}/MCP_MoveBP"
    _send("create_folder", folder_path=src_folder)
    _send("create_folder", folder_path=dst_folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("move_asset", source_path=bp_path, dest_path=moved_path)
        assert result.get("success"), f"move failed: {result}"
        dst = _send("resolve_asset_path", asset_path=moved_path)
        assert dst["exists"], "Moved asset should exist at destination"
    finally:
        _cleanup([bp_path, moved_path], [src_folder, dst_folder])


def test_copy_asset():
    src_folder = "/Game/MCP_CopySrc"
    dst_folder = "/Game/MCP_CopyDst"
    bp_path = f"{src_folder}/MCP_CopyBP"
    copied_path = f"{dst_folder}/MCP_CopyBP"
    _send("create_folder", folder_path=src_folder)
    _send("create_folder", folder_path=dst_folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("copy_asset", source_path=bp_path, dest_path=copied_path)
        assert result.get("success"), f"copy failed: {result}"
        src = _send("resolve_asset_path", asset_path=bp_path)
        dst = _send("resolve_asset_path", asset_path=copied_path)
        assert src["exists"], "Source should still exist after copy"
        assert dst["exists"], "Copied asset should exist"
    finally:
        _cleanup([bp_path, copied_path], [src_folder, dst_folder])


def test_save_and_delete_asset():
    folder = "/Game/MCP_SaveDelTest"
    bp_path = f"{folder}/MCP_SaveDelBP"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        save_result = _send("save_assets", asset_paths=[bp_path])
        assert save_result.get("success"), f"save failed: {save_result}"
        saved = save_result.get("saved", [])
        assert any(bp_path in s for s in saved), f"Expected asset in saved list: {saved}"
        del_result = _send("delete_asset", asset_path=bp_path)
        assert del_result.get("success"), f"delete failed: {del_result}"
        resolve = _send("resolve_asset_path", asset_path=bp_path)
        assert not resolve["exists"], "Asset should not exist after delete"
    finally:
        _cleanup([], [folder])


def test_load_and_unload_asset():
    folder = "/Game/MCP_LoadUnloadTest"
    bp_path = f"{folder}/MCP_LoadUnloadBP"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        load_result = _send("load_asset", asset_path=bp_path)
        assert load_result.get("success"), f"load failed: {load_result}"
        unload_result = _send("unload_asset", asset_path=bp_path)
        assert unload_result.get("success"), f"unload failed: {unload_result}"
    finally:
        _cleanup([bp_path], [folder])


def test_primary_asset_label():
    folder = "/Game/MCP_LabelTest"
    bp_path = f"{folder}/MCP_LabelBP"
    label_path = f"{folder}/MCP_TestLabel"
    _send("create_folder", folder_path=folder)
    _ensure_blueprint(bp_path)
    try:
        result = _send("create_primary_asset_label", label_path=label_path, asset_paths=[bp_path])
        assert result.get("success"), f"create_primary_asset_label failed: {result}"
        assert "label_path" in result

        list_result = _send("list_primary_asset_labels", folder_path=folder, recursive=True)
        assert list_result.get("success"), f"list_primary_asset_labels failed: {list_result}"
        labels = list_result.get("labels", [])
        assert any("MCP_TestLabel" in l.get("asset_name", "") for l in labels), f"Expected label not found: {labels}"

        delete_result = _send("delete_primary_asset_label", label_path=label_path)
        assert delete_result.get("success"), f"delete_primary_asset_label failed: {delete_result}"
    finally:
        _cleanup([bp_path], [folder])


def test_asset_manager_settings():
    result = _send("get_asset_manager_settings")
    assert result.get("success"), f"get_asset_manager_settings failed: {result}"
    assert "primary_asset_types" in result

    set_result = _send("set_asset_manager_settings", default_priority=10)
    assert set_result.get("success"), f"set_asset_manager_settings failed: {set_result}"


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    test_create_and_delete_folder()
    logger.info("PASS: create_and_delete_folder")
    test_list_assets()
    logger.info("PASS: list_assets")
    test_resolve_asset_path()
    logger.info("PASS: resolve_asset_path")
    test_search_assets()
    logger.info("PASS: search_assets")
    test_rename_asset()
    logger.info("PASS: rename_asset")
    test_duplicate_asset()
    logger.info("PASS: duplicate_asset")
    test_move_asset()
    logger.info("PASS: move_asset")
    test_copy_asset()
    logger.info("PASS: copy_asset")
    test_save_and_delete_asset()
    logger.info("PASS: save_and_delete_asset")
    test_load_and_unload_asset()
    logger.info("PASS: load_and_unload_asset")
    test_primary_asset_label()
    logger.info("PASS: primary_asset_label")
    test_asset_manager_settings()
    logger.info("PASS: asset_manager_settings")
    logger.info("All Content Browser integration tests passed.")
