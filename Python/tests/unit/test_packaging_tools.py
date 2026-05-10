"""L1 Unit tests for packaging and build tools.

Verifies UAT path resolution, subprocess invocation, and INI manipulation
without a live Unreal Engine installation.
"""

import json
import os
from pathlib import Path
from unittest.mock import patch, MagicMock

import pytest

import server.packaging_tools as packaging_tools


class TestBuildProject:
    def test_returns_error_when_uat_not_found(self):
        with patch.object(packaging_tools, "_uat_path", return_value=None):
            result = packaging_tools.build_project()
        assert result.get("success") is False
        assert "uat" in result.get("error", "").lower() or "automation" in result.get("error", "").lower()

    def test_returns_error_when_project_not_found(self):
        with patch.object(packaging_tools, "_uat_path", return_value=Path("/fake/UAT.bat")):
            with patch.object(packaging_tools, "_find_project_file", return_value=None):
                result = packaging_tools.build_project()
        assert result.get("success") is False
        assert "uproject" in result.get("error", "").lower()

    def test_runs_uat_with_expected_args(self):
        fake_uat = Path("C:/UE/Engine/Build/BatchFiles/RunUAT.bat")
        fake_project = "C:/Project/Game.uproject"
        with patch.object(packaging_tools, "_uat_path", return_value=fake_uat):
            with patch.object(packaging_tools, "_find_project_file", return_value=fake_project):
                with patch.object(packaging_tools, "_run_subprocess", return_value={"success": True}) as mock_run:
                    result = packaging_tools.build_project(
                        platform="Linux", configuration="Shipping", target="GameServer"
                    )

        assert result["success"] is True
        cmd = mock_run.call_args[0][0]
        assert str(fake_uat) in cmd[0]
        assert "BuildCookRun" in cmd[1]
        assert "-platform=Linux" in cmd
        assert "-clientconfig=Shipping" in cmd
        assert "-target=GameServer" in cmd


class TestCookContent:
    def test_returns_error_when_uat_not_found(self):
        with patch.object(packaging_tools, "_uat_path", return_value=None):
            result = packaging_tools.cook_content()
        assert result.get("success") is False

    def test_runs_cook_only(self):
        fake_uat = Path("C:/UE/Engine/Build/BatchFiles/RunUAT.bat")
        fake_project = "C:/Project/Game.uproject"
        with patch.object(packaging_tools, "_uat_path", return_value=fake_uat):
            with patch.object(packaging_tools, "_find_project_file", return_value=fake_project):
                with patch.object(packaging_tools, "_run_subprocess", return_value={"success": True}) as mock_run:
                    result = packaging_tools.cook_content(platform="Android")

        assert result["success"] is True
        cmd = mock_run.call_args[0][0]
        assert "-cook" in cmd
        assert "-skipstage" in cmd
        assert "-skiparchive" in cmd
        assert "-platform=Android" in cmd


class TestPackageForPlatform:
    def test_returns_error_when_uat_not_found(self):
        with patch.object(packaging_tools, "_uat_path", return_value=None):
            result = packaging_tools.package_for_platform()
        assert result.get("success") is False

    def test_runs_package_with_output_dir(self):
        fake_uat = Path("C:/UE/Engine/Build/BatchFiles/RunUAT.bat")
        fake_project = "C:/Project/Game.uproject"
        with patch.object(packaging_tools, "_uat_path", return_value=fake_uat):
            with patch.object(packaging_tools, "_find_project_file", return_value=fake_project):
                with patch.object(packaging_tools, "_run_subprocess", return_value={"success": True}) as mock_run:
                    result = packaging_tools.package_for_platform(
                        platform="Win64",
                        configuration="Development",
                        output_directory="C:/Output",
                        extra_args=["-compress"],
                    )

        assert result["success"] is True
        cmd = mock_run.call_args[0][0]
        assert "-archivedirectory=\"C:/Output\"" in cmd
        assert "-compress" in cmd


class TestRunCommandlet:
    def test_returns_error_when_ue_cmd_not_found(self):
        with patch.object(packaging_tools, "_ue_cmd_path", return_value=None):
            result = packaging_tools.run_commandlet(commandlet="ResavePackages")
        assert result.get("success") is False
        assert "command" in result.get("error", "").lower() or "executable" in result.get("error", "").lower()

    def test_rejects_empty_commandlet(self):
        result = packaging_tools.run_commandlet(commandlet="")
        assert result.get("success") is False

    def test_runs_commandlet_with_args(self):
        fake_cmd = Path("C:/UE/Engine/Binaries/Win64/UnrealEditor-Cmd.exe")
        fake_project = "C:/Project/Game.uproject"
        with patch.object(packaging_tools, "_ue_cmd_path", return_value=fake_cmd):
            with patch.object(packaging_tools, "_find_project_file", return_value=fake_project):
                with patch.object(packaging_tools, "_run_subprocess", return_value={"success": True}) as mock_run:
                    result = packaging_tools.run_commandlet(
                        commandlet="ResavePackages",
                        extra_args=["-fixup"],
                    )

        assert result["success"] is True
        cmd = mock_run.call_args[0][0]
        assert str(fake_cmd) in cmd[0]
        assert "-run=ResavePackages" in cmd
        assert "-fixup" in cmd
        assert "-unattended" in cmd
        assert "-nullrhi" in cmd


class TestGetBuildLog:
    def test_reads_default_log(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")
        log_dir = tmp_path / "Saved" / "Logs"
        log_dir.mkdir(parents=True)
        log_file = log_dir / "Log.txt"
        log_file.write_text("Build started\nBuild completed\n", encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.get_build_log(tail_lines=1)

        assert result["success"] is True
        assert result["content"] == "Build completed"

    def test_returns_error_when_log_missing(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.get_build_log()

        assert result.get("success") is False
        assert "not found" in result.get("error", "").lower()


class TestGetCookSummary:
    def test_parses_cook_log(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")
        cooked_dir = tmp_path / "Saved" / "Cooked" / "Win64"
        cooked_dir.mkdir(parents=True)
        cook_file = cooked_dir / "Cook.txt"
        cook_file.write_text(
            "Cooking package /Game/Maps/Level\n"
            "Warning: Texture missing\n"
            "Error: Material failed\n"
            "Done cooking\n",
            encoding="utf-8",
        )

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.get_cook_summary()

        assert result["success"] is True
        summary = result["summary"]
        assert summary["total_lines"] == 4
        assert summary["cooked_packages"] == 1
        assert summary["warnings"] == 1
        assert summary["errors"] == 1

    def test_returns_error_when_no_cook_log(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.get_cook_summary()

        assert result.get("success") is False
        assert "cook" in result.get("error", "").lower()


class TestSetBuildConfiguration:
    def test_updates_uproject_json(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text(json.dumps({"EngineAssociation": "5.5"}), encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.set_build_configuration(configuration="Shipping")

        assert result["success"] is True
        data = json.loads(fake_project.read_text(encoding="utf-8"))
        assert data["BuildConfiguration"]["Configuration"] == "Shipping"

    def test_rejects_invalid_configuration(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.set_build_configuration(configuration="Invalid")

        assert result.get("success") is False


class TestPackagingSettings:
    def test_reads_ini_files(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")
        config_dir = tmp_path / "Config"
        config_dir.mkdir()
        (config_dir / "DefaultEngine.ini").write_text("[Core.System]\nPaths=/Game\n", encoding="utf-8")
        (config_dir / "DefaultGame.ini").write_text("[/Script/Engine.GameMapsSettings]\nEditorStartupMap=/Game/Maps/Start\n", encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.get_packaging_settings()

        assert result["success"] is True
        assert "DefaultEngine.ini" in result["settings"]
        assert "DefaultGame.ini" in result["settings"]

    def test_writes_ini_key(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")
        config_dir = tmp_path / "Config"
        config_dir.mkdir()
        ini = config_dir / "DefaultGame.ini"
        ini.write_text("[/Script/Engine.ProjectPackagingSettings]\nBuildConfiguration=Development\n", encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.set_packaging_settings(
                ini_name="DefaultGame.ini",
                section="/Script/Engine.ProjectPackagingSettings",
                key="BuildConfiguration",
                value="Shipping",
            )

        assert result["success"] is True
        text = ini.read_text(encoding="utf-8")
        assert "BuildConfiguration=Shipping" in text

    def test_creates_section_when_missing(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")
        config_dir = tmp_path / "Config"
        config_dir.mkdir()
        ini = config_dir / "DefaultGame.ini"
        ini.write_text("", encoding="utf-8")

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.set_packaging_settings(
                ini_name="DefaultGame.ini",
                section="/Script/Engine.ProjectPackagingSettings",
                key="BuildConfiguration",
                value="Test",
            )

        assert result["success"] is True
        text = ini.read_text(encoding="utf-8")
        assert "[/Script/Engine.ProjectPackagingSettings]" in text
        assert "BuildConfiguration=Test" in text


class TestCleanIntermediateFiles:
    def test_deletes_directories(self, tmp_path: Path):
        fake_project = tmp_path / "Game.uproject"
        fake_project.write_text("{}", encoding="utf-8")
        (tmp_path / "Intermediate").mkdir()
        (tmp_path / "Saved" / "StagedBuilds").mkdir(parents=True)

        with patch.object(packaging_tools, "_find_project_file", return_value=str(fake_project)):
            result = packaging_tools.clean_intermediate_files()

        assert result["success"] is True
        assert len(result["deleted"]) == 4  # Intermediate, StagedBuilds, Archives, Binaries
        assert not (tmp_path / "Intermediate").exists()

    def test_returns_error_when_project_not_found(self):
        with patch.object(packaging_tools, "_find_project_file", return_value=None):
            result = packaging_tools.clean_intermediate_files()
        assert result.get("success") is False
