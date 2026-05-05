"""Asset Import / Export tools for the Unreal MCP server.

Individual MCP tools for each import format (FBX/Texture/Audio) and export.
Provides UE5.7 API compliant import/export with GameThread safety and no forced GC.
"""

import logging
from typing import Dict, Any, Optional

from server.core import mcp, get_unreal_connection
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_AssetImport")


@mcp.tool()
def fbx_mesh_import_tool(
    source_path: str,
    destination_path: str,
    asset_name: Optional[str] = None,
    import_type: str = "auto",
    scale: float = 1.0,
    convert_scene_unit: bool = False,
    import_collision: bool = False,
    generate_lightmap_uv: bool = True,
    nanite_enabled: bool = False,
    lod_group: Optional[str] = None,
) -> Dict[str, Any]:
    """Import an FBX file as a Static or Skeletal Mesh into Unreal Engine.

    Supports FBX files containing either static meshes (geometry only) or 
    skeletal meshes (with bones/rigging). Auto-detects type if import_type 
    is 'auto'.

    Args:
        source_path: Absolute disk path to the FBX file (e.g., "C:/Models/car.fbx")
        destination_path: Package path where the mesh will be imported (e.g., "/Game/Imported")
        asset_name: Optional name for the imported asset. Defaults to FBX filename without extension.
        import_type: "static" for StaticMesh, "skeletal" for SkeletalMesh, "auto" to detect (default)
        scale: Uniform scale factor applied during import (default 1.0)
        convert_scene_unit: Convert FBX scene units to Unreal units (default False)
        import_collision: Import UCX_ collision meshes embedded in FBX (default False)
        generate_lightmap_uv: Auto-generate lightmap UV channel (default True)
        nanite_enabled: Enable Nanite for static meshes (UE5.7+, default False)
        lod_group: LOD group preset name (e.g., "SmallProp", "LargeProp")

    Returns:
        Dict with success status, imported_assets list, count, source_path, destination_path
    """
    try:
        validate_string(source_path, "source_path")
        validate_string(destination_path, "destination_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    if import_type not in ("auto", "static", "skeletal"):
        return make_error_response("import_type must be 'auto', 'static', or 'skeletal'")

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "source_path": source_path,
            "destination_path": destination_path,
            "import_type": import_type,
            "scale": scale,
            "convert_scene_unit": convert_scene_unit,
            "import_collision": import_collision,
            "generate_lightmap_uv": generate_lightmap_uv,
            "nanite_enabled": nanite_enabled,
        }
        
        if asset_name is not None:
            params["asset_name"] = asset_name
        if lod_group is not None:
            params["lod_group"] = lod_group

        return unreal.send_command("import_fbx_mesh", params)
    except Exception as e:
        logger.error(f"fbx_mesh_import_tool error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def texture_import_tool(
    source_path: str,
    destination_path: str,
    asset_name: Optional[str] = None,
    texture_type: str = "default",
    compression: Optional[str] = None,
    srgb: Optional[bool] = None,
    flip_green_channel: Optional[bool] = None,
    mip_gen_settings: Optional[str] = None,
) -> Dict[str, Any]:
    """Import an image file as a Texture into Unreal Engine.

    Supports PNG, JPG/JPEG, EXR, HDR, TGA, BMP, PSD formats. Handles special 
    texture types like Normal Maps, ORM (Occlusion/Roughness/Metallic) masks, 
    and HDR images with appropriate compression settings.

    Args:
        source_path: Absolute disk path to the image file (e.g., "C:/Textures/wood.png")
        destination_path: Package path where the texture will be imported (e.g., "/Game/Textures")
        asset_name: Optional name for the imported asset. Defaults to filename without extension.
        texture_type: Preset type - "default", "normal" (for normal maps), "orm" (for ORM masks), "hdr" (for HDR/EXR)
        compression: Compression format - "default", "BC1" (for opaque), "BC5" (for normals), "BC7" (for high quality)
        srgb: Enable sRGB color space (default True for diffuse, False for normal/ORM)
        flip_green_channel: Flip green channel for normal maps (default False)
        mip_gen_settings: Mipmap generation - "Default", "NoMipmaps", "Blur1", etc.

    Returns:
        Dict with success status, imported_assets list, count, source_path, destination_path
    """
    try:
        validate_string(source_path, "source_path")
        validate_string(destination_path, "destination_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    if texture_type not in ("default", "normal", "orm", "hdr"):
        return make_error_response("texture_type must be 'default', 'normal', 'orm', or 'hdr'")

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "source_path": source_path,
            "destination_path": destination_path,
            "texture_type": texture_type,
        }
        
        if asset_name is not None:
            params["asset_name"] = asset_name
        if compression is not None:
            params["compression"] = compression
        if srgb is not None:
            params["srgb"] = srgb
        if flip_green_channel is not None:
            params["flip_green_channel"] = flip_green_channel
        if mip_gen_settings is not None:
            params["mip_gen_settings"] = mip_gen_settings

        return unreal.send_command("import_texture", params)
    except Exception as e:
        logger.error(f"texture_import_tool error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def audio_import_tool(
    source_path: str,
    destination_path: str,
    asset_name: Optional[str] = None,
    auto_create_cue: bool = False,
    include_attenuation: bool = False,
    include_looping: bool = False,
    include_modulator: bool = False,
    cue_volume: float = 1.0,
) -> Dict[str, Any]:
    """Import an audio file as a Sound Wave (and optionally Sound Cue) into Unreal Engine.

    Supports WAV and OGG formats. Can automatically create Sound Cue assets
    with optional attenuation, looping, and modulation nodes.

    Args:
        source_path: Absolute disk path to the audio file (e.g., "C:/Audio/explosion.wav")
        destination_path: Package path where the sound will be imported (e.g., "/Game/Audio")
        asset_name: Optional name for the imported asset. Defaults to filename without extension.
        auto_create_cue: Automatically create a Sound Cue asset (default False)
        include_attenuation: Add attenuation node to created Sound Cue (default False)
        include_looping: Add looping node to created Sound Cue (default False)
        include_modulator: Add modulator node to created Sound Cue (default False)
        cue_volume: Volume multiplier for the created Sound Cue (default 1.0)

    Returns:
        Dict with success status, imported_assets list, count, source_path, destination_path
    """
    try:
        validate_string(source_path, "source_path")
        validate_string(destination_path, "destination_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "source_path": source_path,
            "destination_path": destination_path,
            "auto_create_cue": auto_create_cue,
            "include_attenuation": include_attenuation,
            "include_looping": include_looping,
            "include_modulator": include_modulator,
            "cue_volume": cue_volume,
        }
        
        if asset_name is not None:
            params["asset_name"] = asset_name

        return unreal.send_command("import_audio", params)
    except Exception as e:
        logger.error(f"audio_import_tool error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def asset_export_tool(
    asset_path: str,
    output_path: str,
    export_format: Optional[str] = None,
) -> Dict[str, Any]:
    """Export an Unreal Engine asset to a file on disk.

    Exports assets to various formats supported by their type:
    - StaticMesh/SkeletalMesh -> FBX, OBJ
    - Texture -> PNG, JPG, EXR, HDR
    - Sound -> WAV

    Args:
        asset_path: Unreal asset path (e.g., "/Game/Imported/MyMesh")
        output_path: Absolute disk path for the exported file (e.g., "C:/Exports/my_mesh.fbx")
        export_format: Optional format override. If not specified, inferred from output_path extension.
                       Supported: "fbx", "obj", "png", "jpg", "exr", "hdr", "wav"

    Returns:
        Dict with success status, asset_path, output_path, format, message
    """
    try:
        validate_string(asset_path, "asset_path")
        validate_string(output_path, "output_path")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    unreal = get_unreal_connection()
    if not unreal:
        return make_error_response("Failed to connect to Unreal Engine")

    try:
        params = {
            "asset_path": asset_path,
            "output_path": output_path,
        }
        
        if export_format is not None:
            params["export_format"] = export_format

        return unreal.send_command("export_asset", params)
    except Exception as e:
        logger.error(f"asset_export_tool error: {e}")
        return make_error_response(str(e))
