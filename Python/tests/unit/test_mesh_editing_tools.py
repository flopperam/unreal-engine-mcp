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

    # test mesh_uv_unwrap with auto_plot mode
    res = asset_mesh_editing_tool(action="mesh_uv_unwrap", asset_path="/Game/MyMesh", unwrap_mode="auto_plot", uv_channel=0)
    mock_conn.send_command.assert_called_with("mesh_uv_unwrap", {"asset_path": "/Game/MyMesh", "unwrap_mode": "auto_plot", "uv_channel": 0})

    # test mesh_uv_unwrap with repack mode
    res = asset_mesh_editing_tool(action="mesh_uv_unwrap", asset_path="/Game/MyMesh", unwrap_mode="repack", uv_channel=1)
    mock_conn.send_command.assert_called_with("mesh_uv_unwrap", {"asset_path": "/Game/MyMesh", "unwrap_mode": "repack", "uv_channel": 1})

    # test mesh_voxel_remesh
    res = asset_mesh_editing_tool(action="mesh_voxel_remesh", asset_path="/Game/MyMesh", voxel_count=128)
    mock_conn.send_command.assert_called_with("mesh_voxel_remesh", {"asset_path": "/Game/MyMesh", "voxel_count": 128})

    # test mesh_uv_layout
    res = asset_mesh_editing_tool(action="mesh_uv_layout", asset_path="/Game/MyMesh", uv_channel=0)
    mock_conn.send_command.assert_called_with("mesh_uv_layout", {"asset_path": "/Game/MyMesh", "uv_channel": 0})

    # test set_pivot
    res = asset_mesh_editing_tool(action="set_pivot", asset_path="/Game/MyMesh", pivot_location={"x": 10.0, "y": 20.0, "z": 30.0})
    mock_conn.send_command.assert_called_with("set_pivot", {"asset_path": "/Game/MyMesh", "pivot_location": {"x": 10.0, "y": 20.0, "z": 30.0}})

    # test mesh_merge
    res = asset_mesh_editing_tool(action="mesh_merge", asset_path="/Game/MergedMesh", source_mesh_paths=["/Game/Mesh1", "/Game/Mesh2"])
    mock_conn.send_command.assert_called_with("mesh_merge", {"asset_path": "/Game/MergedMesh", "source_mesh_paths": ["/Game/Mesh1", "/Game/Mesh2"]})

    # test set_vertex_colors
    res = asset_mesh_editing_tool(action="set_vertex_colors", asset_path="/Game/MyMesh", color={"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0}, paint_mode="fill")
    mock_conn.send_command.assert_called_with("set_vertex_colors", {"asset_path": "/Game/MyMesh", "color": {"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0}, "paint_mode": "fill"})

    # test mesh_bake
    res = asset_mesh_editing_tool(action="mesh_bake", asset_path="/Game/MyMesh", bake_type="ambient_occlusion", output_size=1024)
    mock_conn.send_command.assert_called_with("mesh_bake", {"asset_path": "/Game/MyMesh", "bake_type": "ambient_occlusion", "output_size": 1024})

    # test poly_edit
    res = asset_mesh_editing_tool(action="poly_edit", asset_path="/Game/MyMesh", operation="delete_selected", plane_origin={"x": 0.0, "y": 0.0, "z": 0.0}, plane_normal={"x": 0.0, "y": 0.0, "z": 1.0})
    mock_conn.send_command.assert_called_with("poly_edit", {"asset_path": "/Game/MyMesh", "operation": "delete_selected", "plane_origin": {"x": 0.0, "y": 0.0, "z": 0.0}, "plane_normal": {"x": 0.0, "y": 0.0, "z": 1.0}})

    # test generate_lods
    res = asset_mesh_editing_tool(action="generate_lods", asset_path="/Game/MyMesh", num_lods=4)
    mock_conn.send_command.assert_called_with("generate_lods", {"asset_path": "/Game/MyMesh", "num_lods": 4})

    # test generate_lightmap_uvs
    res = asset_mesh_editing_tool(action="generate_lightmap_uvs", asset_path="/Game/MyMesh", lod_index=0)
    mock_conn.send_command.assert_called_with("generate_lightmap_uvs", {"asset_path": "/Game/MyMesh", "lod_index": 0})

    # test import_ucx_collision
    res = asset_mesh_editing_tool(action="import_ucx_collision", asset_path="/Game/MyMesh", collision_mesh_path="/Game/CollisionMesh", replace_existing=True)
    mock_conn.send_command.assert_called_with("import_ucx_collision", {"asset_path": "/Game/MyMesh", "collision_mesh_path": "/Game/CollisionMesh", "replace_existing": True})

    # --- New UStaticMeshEditorSubsystem alternative API tests ---

    # test generate_box_uv_channel
    res = asset_mesh_editing_tool(action="generate_box_uv_channel", asset_path="/Game/MyMesh", uv_channel=1, lod_index=0)
    mock_conn.send_command.assert_called_with("generate_box_uv_channel", {"asset_path": "/Game/MyMesh", "uv_channel": 1, "lod_index": 0})

    # test generate_planar_uv_channel
    res = asset_mesh_editing_tool(action="generate_planar_uv_channel", asset_path="/Game/MyMesh", uv_channel=2)
    mock_conn.send_command.assert_called_with("generate_planar_uv_channel", {"asset_path": "/Game/MyMesh", "uv_channel": 2})

    # test generate_cylindrical_uv_channel
    res = asset_mesh_editing_tool(action="generate_cylindrical_uv_channel", asset_path="/Game/MyMesh", uv_channel=0, lod_index=1)
    mock_conn.send_command.assert_called_with("generate_cylindrical_uv_channel", {"asset_path": "/Game/MyMesh", "uv_channel": 0, "lod_index": 1})

    # test add_uv_channel
    res = asset_mesh_editing_tool(action="add_uv_channel", asset_path="/Game/MyMesh", lod_index=0)
    mock_conn.send_command.assert_called_with("add_uv_channel", {"asset_path": "/Game/MyMesh", "lod_index": 0})

    # test remove_uv_channel
    res = asset_mesh_editing_tool(action="remove_uv_channel", asset_path="/Game/MyMesh", uv_channel=3)
    mock_conn.send_command.assert_called_with("remove_uv_channel", {"asset_path": "/Game/MyMesh", "uv_channel": 3})

    # test set_lods
    res = asset_mesh_editing_tool(action="set_lods", asset_path="/Game/MyMesh")
    mock_conn.send_command.assert_called_with("set_lods", {"asset_path": "/Game/MyMesh"})

    # test remove_lods
    res = asset_mesh_editing_tool(action="remove_lods", asset_path="/Game/MyMesh")
    mock_conn.send_command.assert_called_with("remove_lods", {"asset_path": "/Game/MyMesh"})

    # test join_static_mesh_actors
    res = asset_mesh_editing_tool(action="join_static_mesh_actors", asset_path="/Game/MergedMesh", actor_paths=["/Game/Mesh1", "/Game/Mesh2"])
    mock_conn.send_command.assert_called_with("join_static_mesh_actors", {"asset_path": "/Game/MergedMesh", "actor_paths": ["/Game/Mesh1", "/Game/Mesh2"]})

    # test merge_static_mesh_actors
    res = asset_mesh_editing_tool(action="merge_static_mesh_actors", asset_path="/Game/MergedMesh", actor_paths=["/Game/Mesh1", "/Game/Mesh2"])
    mock_conn.send_command.assert_called_with("merge_static_mesh_actors", {"asset_path": "/Game/MergedMesh", "actor_paths": ["/Game/Mesh1", "/Game/Mesh2"]})

    # test create_proxy_mesh_actor
    res = asset_mesh_editing_tool(action="create_proxy_mesh_actor", asset_path="/Game/ProxyMesh", actor_paths=["/Game/Mesh1", "/Game/Mesh2"])
    mock_conn.send_command.assert_called_with("create_proxy_mesh_actor", {"asset_path": "/Game/ProxyMesh", "actor_paths": ["/Game/Mesh1", "/Game/Mesh2"]})

    # test set_generate_lightmap_uvs
    res = asset_mesh_editing_tool(action="set_generate_lightmap_uvs", asset_path="/Game/MyMesh", generate=True, lightmap_coordinate_index=1)
    mock_conn.send_command.assert_called_with("set_generate_lightmap_uvs", {"asset_path": "/Game/MyMesh", "generate": True, "lightmap_coordinate_index": 1})

    # test has_vertex_colors
    res = asset_mesh_editing_tool(action="has_vertex_colors", asset_path="/Game/MyMesh")
    mock_conn.send_command.assert_called_with("has_vertex_colors", {"asset_path": "/Game/MyMesh"})
