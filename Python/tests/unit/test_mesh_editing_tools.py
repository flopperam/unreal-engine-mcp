from unittest.mock import patch, MagicMock
from server.mesh_editing_tools import asset_mesh_editing_tool

@patch("server.mesh_editing_tools.get_unreal_connection")
def test_mesh_editing_tool_dispatch(mock_get_conn):
    mock_conn = MagicMock()
    mock_get_conn.return_value = mock_conn

    # test generate_collision
    mock_conn.send_command.return_value = {"success": True}
    res = asset_mesh_editing_tool(action="generate_collision", asset_path="/Game/MyMesh", shape_type="Box")
    mock_conn.send_command.assert_called_with("generate_collision", {"asset_path": "/Game/MyMesh", "shape_type": "Box"})
    assert res == {"success": True}

    # test get_details
    res = asset_mesh_editing_tool(action="get_details", asset_path="/Game/MyMesh")
    mock_conn.send_command.assert_called_with("get_static_mesh_details", {"asset_path": "/Game/MyMesh"})

    # test mesh_boolean
    res = asset_mesh_editing_tool(action="mesh_boolean", asset_path="/Game/MyMesh", tool_mesh_path="/Game/ToolMesh", operation="Union")
    mock_conn.send_command.assert_called_with("mesh_boolean", {"asset_path": "/Game/MyMesh", "tool_mesh_path": "/Game/ToolMesh", "operation": "Union"})

    # test set_nanite_settings
    res = asset_mesh_editing_tool(action="set_nanite_settings", asset_path="/Game/MyMesh", enabled=True, fallback_percent=50.0)
    mock_conn.send_command.assert_called_with("set_nanite_settings", {"asset_path": "/Game/MyMesh", "enabled": True, "fallback_percent": 50.0})
