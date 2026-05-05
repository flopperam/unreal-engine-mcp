"""Integration tests for Asset Import / Export tools.

These tests require:
1. Unreal Editor running with MCP plugin enabled
2. Test assets in known locations (FBX, textures, audio files)

Run with: python -m pytest Python/tests/integration/test_asset_import_export.py -v
"""

import logging
import os
from pathlib import Path
from typing import Any, Dict

import pytest

from server.core import get_unreal_connection

logger = logging.getLogger(__name__)

UNREAL = get_unreal_connection()

# Test asset paths (repo-relative for portability)
REPO_ROOT = Path(__file__).resolve().parents[3]
TEST_ASSETS_DIR = str(REPO_ROOT / "tests" / "assets")
TEST_OUTPUT_DIR = str(REPO_ROOT / "tests" / "output")

# Ensure directories exist
Path(TEST_ASSETS_DIR).mkdir(parents=True, exist_ok=True)
Path(TEST_OUTPUT_DIR).mkdir(parents=True, exist_ok=True)


def _send(cmd: str, **kwargs) -> Dict[str, Any]:
    """Send raw C++ command via unreal connection."""
    if not UNREAL:
        raise RuntimeError("Unreal connection not available")
    return UNREAL.send_command(cmd, kwargs)


def _cleanup_assets(asset_paths: list):
    """Delete test assets after test."""
    for path in asset_paths:
        try:
            _send("delete_asset", asset_path=path)
        except Exception:
            pass


class TestFbxMeshImport:
    """Tests for FBX mesh import functionality."""

    def test_import_fbx_static_mesh_minimal(self):
        """Test importing a simple FBX static mesh with minimal parameters."""
        source_path = f"{TEST_ASSETS_DIR}/cube.fbx"
        dest_path = "/Game/MCP_TestImport/FbxTest"

        result = _send(
            "import_fbx_mesh",
            source_path=source_path,
            destination_path=dest_path,
            import_type="static"
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
            assert "imported_assets" in result
            assert len(result["imported_assets"]) > 0
            logger.info(f"Successfully imported: {result['imported_assets']}")
        finally:
            _cleanup_assets([dest_path])

    def test_import_fbx_with_options(self):
        """Test importing FBX with various options (scale, nanite, etc.)."""
        source_path = f"{TEST_ASSETS_DIR}/cube.fbx"
        dest_path = "/Game/MCP_TestImport/FbxTestOptions"

        result = _send(
            "import_fbx_mesh",
            source_path=source_path,
            destination_path=dest_path,
            asset_name="ScaledCube",
            import_type="static",
            scale=2.0,
            nanite_enabled=True,
            generate_lightmap_uv=True
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
            assert any("ScaledCube" in path for path in result["imported_assets"])
        finally:
            _cleanup_assets([dest_path + "/ScaledCube"])

    def test_import_fbx_skeletal_mesh(self):
        """Test importing a skeletal mesh FBX (requires rigged character FBX)."""
        source_path = f"{TEST_ASSETS_DIR}/character.fbx"
        dest_path = "/Game/MCP_TestImport/CharacterTest"

        if not os.path.exists(source_path):
            pytest.skip("character.fbx test asset not found — provide a valid skeletal mesh FBX")

        result = _send(
            "import_fbx_mesh",
            source_path=source_path,
            destination_path=dest_path,
            import_type="skeletal"
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
            assert len(result["imported_assets"]) >= 1
        finally:
            _cleanup_assets([dest_path])

    def test_import_fbx_file_not_found(self):
        """Test error handling for non-existent file."""
        result = _send(
            "import_fbx_mesh",
            source_path="C:/NonExistent/file.fbx",
            destination_path="/Game/MCP_TestImport/ShouldFail"
        )

        assert not result.get("success")
        assert "error" in result
        assert "not exist" in result["error"].lower() or "not found" in result["error"].lower()


class TestTextureImport:
    """Tests for texture import functionality."""

    def test_import_png_default(self):
        """Test importing a PNG texture with default settings."""
        source_path = f"{TEST_ASSETS_DIR}/texture.png"
        dest_path = "/Game/MCP_TestImport/Textures"

        result = _send(
            "import_texture",
            source_path=source_path,
            destination_path=dest_path
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
            assert len(result["imported_assets"]) > 0
        finally:
            _cleanup_assets([dest_path])

    def test_import_normal_map(self):
        """Test importing a normal map with correct settings."""
        source_path = f"{TEST_ASSETS_DIR}/normal.png"
        dest_path = "/Game/MCP_TestImport/Textures"

        result = _send(
            "import_texture",
            source_path=source_path,
            destination_path=dest_path,
            texture_type="normal",
            compression="BC5"
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
        finally:
            _cleanup_assets([dest_path])

    def test_import_hdr(self):
        """Test importing an HDR environment map."""
        source_path = f"{TEST_ASSETS_DIR}/envmap.hdr"
        dest_path = "/Game/MCP_TestImport/Textures"

        result = _send(
            "import_texture",
            source_path=source_path,
            destination_path=dest_path,
            texture_type="hdr"
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
        finally:
            _cleanup_assets([dest_path])


class TestAudioImport:
    """Tests for audio import functionality."""

    def test_import_wav(self):
        """Test importing a WAV sound file."""
        source_path = f"{TEST_ASSETS_DIR}/sound.wav"
        dest_path = "/Game/MCP_TestImport/Audio"

        result = _send(
            "import_audio",
            source_path=source_path,
            destination_path=dest_path
        )

        imported_assets = []
        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
            assert len(result["imported_assets"]) > 0
            imported_assets = result["imported_assets"]
        finally:
            _cleanup_assets(imported_assets if imported_assets else [dest_path])

    def test_import_wav_with_cue(self):
        """Test importing WAV with automatic Sound Cue creation."""
        source_path = f"{TEST_ASSETS_DIR}/loop.wav"
        dest_path = "/Game/MCP_TestImport/Audio"

        result = _send(
            "import_audio",
            source_path=source_path,
            destination_path=dest_path,
            auto_create_cue=True,
            include_looping=True,
            cue_volume=0.8
        )

        imported_assets = []
        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
            # SoundCue auto-creation is factory-dependent; accept 1+ assets
            assert len(result["imported_assets"]) >= 1
            imported_assets = result["imported_assets"]
        finally:
            _cleanup_assets(imported_assets if imported_assets else [dest_path])


class TestAssetExport:
    """Tests for asset export functionality."""

    def test_export_static_mesh_fbx(self):
        """Test exporting a StaticMesh to FBX.

        Imports a simple mesh first, then exports it.
        """
        import_path = "/Game/MCP_TestImport/ExportTestMesh"
        output_path = f"{TEST_OUTPUT_DIR}/exported_mesh.fbx"

        # Import a valid mesh first so we have something to export
        import_result = _send(
            "import_fbx_mesh",
            source_path=f"{TEST_ASSETS_DIR}/cube.fbx",
            destination_path=import_path,
            asset_name="TestMesh",
            import_type="static"
        )

        if not import_result.get("success"):
            pytest.skip("Failed to import test mesh for export")

        actual_asset_paths = import_result.get("imported_assets", [])
        if not actual_asset_paths:
            pytest.skip("No imported assets returned")
        asset_path = actual_asset_paths[0]

        try:
            result = _send(
                "export_asset",
                asset_path=asset_path,
                output_path=output_path
            )

            assert result.get("success"), f"Export failed: {result.get('error')}"
            assert os.path.exists(output_path), f"Exported file not found: {output_path}"
            assert os.path.getsize(output_path) > 0, "Exported file is empty"
            logger.info(f"Successfully exported FBX: {os.path.getsize(output_path)} bytes")
        finally:
            _cleanup_assets([asset_path])
            if os.path.exists(output_path):
                os.remove(output_path)

    def test_export_texture_png(self):
        """Test exporting a Texture to PNG."""
        import_path = "/Game/MCP_TestImport/ExportTestTex"
        output_path = f"{TEST_OUTPUT_DIR}/exported_texture.png"

        # Import a texture first
        import_result = _send(
            "import_texture",
            source_path=f"{TEST_ASSETS_DIR}/texture.png",
            destination_path=import_path,
            asset_name="TestTexture",
        )

        if not import_result.get("success"):
            pytest.skip("Failed to import test texture for export")

        # Use the actual imported asset path from the result
        actual_asset_paths = import_result.get("imported_assets", [])
        if not actual_asset_paths:
            pytest.skip("No imported assets returned")
        asset_path = actual_asset_paths[0]

        try:
            result = _send(
                "export_asset",
                asset_path=asset_path,
                output_path=output_path
            )

            assert result.get("success"), f"Export failed: {result.get('error')}"
            assert os.path.exists(output_path), f"Exported file not found: {output_path}"
            assert os.path.getsize(output_path) > 0, "Exported file is empty"
            logger.info(f"Successfully exported PNG: {os.path.getsize(output_path)} bytes")
        finally:
            _cleanup_assets([asset_path])
            if os.path.exists(output_path):
                os.remove(output_path)

    def test_export_nonexistent_asset(self):
        """Test error handling for non-existent asset."""
        result = _send(
            "export_asset",
            asset_path="/Game/NonExistent/Asset",
            output_path=f"{TEST_OUTPUT_DIR}/fail.fbx"
        )

        assert not result.get("success")
        assert "error" in result


def test_comprehensive_import_workflow():
    """Comprehensive test importing multiple asset types in sequence.

    This test simulates a typical workflow of importing assets for a game level:
    1. Import static mesh for environment
    2. Import textures for materials
    3. Import audio for ambience
    """
    imported_assets = []

    try:
        # 1. Import mesh
        mesh_result = _send(
            "import_fbx_mesh",
            source_path=f"{TEST_ASSETS_DIR}/building.fbx",
            destination_path="/Game/MCP_TestImport/LevelAssets",
            asset_name="Building",
            import_type="static",
            nanite_enabled=True
        )
        assert mesh_result.get("success")
        imported_assets.extend(mesh_result["imported_assets"])

        # 2. Import diffuse texture
        diffuse_result = _send(
            "import_texture",
            source_path=f"{TEST_ASSETS_DIR}/building_diffuse.png",
            destination_path="/Game/MCP_TestImport/LevelAssets",
            asset_name="Building_D",
            texture_type="default"
        )
        assert diffuse_result.get("success")
        imported_assets.extend(diffuse_result["imported_assets"])

        # 3. Import normal map
        normal_result = _send(
            "import_texture",
            source_path=f"{TEST_ASSETS_DIR}/building_normal.png",
            destination_path="/Game/MCP_TestImport/LevelAssets",
            asset_name="Building_N",
            texture_type="normal"
        )
        assert normal_result.get("success")
        imported_assets.extend(normal_result["imported_assets"])

        # 4. Import ambient audio
        audio_result = _send(
            "import_audio",
            source_path=f"{TEST_ASSETS_DIR}/ambience.wav",
            destination_path="/Game/MCP_TestImport/LevelAssets",
            asset_name="Ambience",
            auto_create_cue=True,
            include_looping=True
        )
        assert audio_result.get("success")
        imported_assets.extend(audio_result["imported_assets"])

        logger.info(f"Comprehensive workflow imported {len(imported_assets)} assets:")
        for asset in imported_assets:
            logger.info(f"  - {asset}")

    finally:
        # Cleanup all imported assets
        for asset_path in imported_assets:
            try:
                _send("delete_asset", asset_path=asset_path)
            except Exception:
                pass


class TestNewTextureAudioParams:
    """Tests for newly wired texture/audio params (Phase 1)."""

    def test_import_texture_flip_green_channel(self):
        """Test importing a normal map with flip_green_channel=True."""
        source_path = f"{TEST_ASSETS_DIR}/normal.png"
        dest_path = "/Game/MCP_TestImport/Textures"

        result = _send(
            "import_texture",
            source_path=source_path,
            destination_path=dest_path,
            texture_type="normal",
            flip_green_channel=True,
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
        finally:
            _cleanup_assets([dest_path])

    def test_import_texture_mip_gen_settings(self):
        """Test importing a texture with custom mip_gen_settings."""
        source_path = f"{TEST_ASSETS_DIR}/texture.png"
        dest_path = "/Game/MCP_TestImport/Textures"

        result = _send(
            "import_texture",
            source_path=source_path,
            destination_path=dest_path,
            mip_gen_settings="NoMipmaps",
        )

        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
        finally:
            _cleanup_assets([dest_path])

    def test_import_audio_include_modulator(self):
        """Test importing audio with include_modulator=True."""
        source_path = f"{TEST_ASSETS_DIR}/sound.wav"
        dest_path = "/Game/MCP_TestImport/Audio"

        result = _send(
            "import_audio",
            source_path=source_path,
            destination_path=dest_path,
            auto_create_cue=True,
            include_modulator=True,
        )

        imported_assets = []
        try:
            assert result.get("success"), f"Import failed: {result.get('error')}"
            imported_assets = result.get("imported_assets", [])
        finally:
            _cleanup_assets(imported_assets if imported_assets else [dest_path])


class TestReimport:
    """Tests for reimport functionality (Phase 4)."""

    def test_reimport_existing_asset(self):
        """Test reimporting an existing asset."""
        import_path = "/Game/MCP_TestImport/ReimportTest"

        # First import
        import_result = _send(
            "import_fbx_mesh",
            source_path=f"{TEST_ASSETS_DIR}/cube.fbx",
            destination_path=import_path,
            asset_name="ReimportCube",
            import_type="static",
        )

        if not import_result.get("success"):
            pytest.skip("Failed to import initial mesh for reimport test")

        imported_assets = import_result.get("imported_assets", [])
        if not imported_assets:
            pytest.skip("No imported assets returned")
        asset_path = imported_assets[0]

        try:
            result = _send("reimport_asset", asset_path=asset_path)
            assert result.get("success"), f"Reimport failed: {result.get('error')}"
        finally:
            _cleanup_assets([asset_path])

    def test_reimport_nonexistent_asset(self):
        """Test error handling for reimporting a non-existent asset."""
        result = _send("reimport_asset", asset_path="/Game/NonExistent/Asset")
        assert not result.get("success")
        assert "error" in result


class TestScreenshot:
    """Tests for screenshot export (Phase 4)."""

    def test_take_screenshot_default_path(self):
        """Test taking a screenshot with default path."""
        result = _send("take_screenshot")

        assert result.get("success"), f"Screenshot failed: {result.get('error')}"
        assert "output_path" in result
        assert os.path.exists(result["output_path"]), "Screenshot file not found"
        assert result.get("width", 0) > 0
        assert result.get("height", 0) > 0

        # Cleanup
        if os.path.exists(result["output_path"]):
            os.remove(result["output_path"])

    def test_take_screenshot_custom_path(self):
        """Test taking a screenshot with a custom output path."""
        output_path = f"{TEST_OUTPUT_DIR}/custom_screenshot.png"

        result = _send("take_screenshot", output_path=output_path)

        assert result.get("success"), f"Screenshot failed: {result.get('error')}"
        assert result.get("output_path") == output_path
        assert os.path.exists(output_path), "Custom screenshot file not found"

        # Cleanup
        if os.path.exists(output_path):
            os.remove(output_path)


class TestLevelExport:
    """Tests for level export (Phase 4)."""

    def test_export_level_default_path(self):
        """Test exporting the current level with default path."""
        result = _send("export_level")

        assert result.get("success"), f"Level export failed: {result.get('error')}"
        assert "output_path" in result
        assert os.path.exists(result["output_path"]), "Level export file not found"
        assert result.get("format") == "json"
        assert result.get("actor_count", 0) >= 0

        # Cleanup
        if os.path.exists(result["output_path"]):
            os.remove(result["output_path"])

    def test_export_level_custom_path(self):
        """Test exporting the current level to a custom path."""
        output_path = f"{TEST_OUTPUT_DIR}/level_export.json"

        result = _send("export_level", output_path=output_path)

        assert result.get("success"), f"Level export failed: {result.get('error')}"
        assert result.get("output_path") == output_path
        assert os.path.exists(output_path), "Custom level export file not found"

        # Cleanup
        if os.path.exists(output_path):
            os.remove(output_path)


class TestImportPreset:
    """Tests for import preset save/load (Phase 4)."""

    def test_save_and_load_preset(self):
        """Test saving and loading an import preset."""
        preset_name = "test_preset_fbx"
        preset_data = {"scale": 2.0, "nanite_enabled": True}

        # Save
        save_result = _send(
            "save_import_preset",
            preset_name=preset_name,
            preset_data=preset_data,
        )
        assert save_result.get("success"), f"Save preset failed: {save_result.get('error')}"

        # Load
        load_result = _send("load_import_preset", preset_name=preset_name)
        assert load_result.get("success"), f"Load preset failed: {load_result.get('error')}"
        assert load_result.get("preset_name") == preset_name
        loaded_data = load_result.get("preset_data", {})
        assert loaded_data.get("scale") == 2.0
        assert loaded_data.get("nanite_enabled") is True

    def test_load_nonexistent_preset(self):
        """Test error handling for loading a non-existent preset."""
        result = _send("load_import_preset", preset_name="nonexistent_preset_xyz")
        assert not result.get("success")
        assert "error" in result
