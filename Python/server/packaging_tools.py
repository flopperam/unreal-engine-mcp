"""Packaging, build, and deployment tools for the Unreal MCP server.

Wraps Unreal Automation Tool (UAT) and UE Editor command-line operations.
No live Unreal Editor connection is required for most tools.
"""

import logging
import os
import subprocess
from pathlib import Path
from typing import Dict, Any, List, Optional

from server.core import mcp
from server.validation import validate_string, ValidationError, make_validation_error_response_from_exception
from utils.responses import make_error_response

logger = logging.getLogger("UnrealMCP_Advanced")

# ---------------------------------------------------------------------------
# Engine / Project path resolution
# ---------------------------------------------------------------------------

def _find_ue_engine_dir() -> Optional[str]:
    """Resolve the UE engine installation directory using multiple strategies."""
    # 1. Environment variable override
    env_dir = os.environ.get("UE_ENGINE_DIR")
    if env_dir and Path(env_dir).exists():
        return env_dir

    # 2. Windows registry (Epic Games Launcher)
    try:
        import winreg
        for version in ("5.5", "5.4", "5.3", "5.2", "5.1", "5.0"):
            try:
                key = winreg.OpenKey(
                    winreg.HKEY_LOCAL_MACHINE,
                    f"SOFTWARE\\EpicGames\\Unreal Engine\\{version}",
                )
                value, _ = winreg.QueryValueEx(key, "InstalledDirectory")
                winreg.CloseKey(key)
                if value and Path(value).exists():
                    return str(Path(value).resolve())
            except OSError:
                continue
    except ImportError:
        pass  # Not on Windows

    # 3. Default install locations
    defaults = [
        Path("C:/Program Files/Epic Games/UE_5.5"),
        Path("C:/Program Files/Epic Games/UE_5.4"),
        Path("C:/Program Files/Epic Games/UE_5.3"),
        Path("C:/Program Files/Epic Games/UE_5.2"),
        Path("C:/Program Files/Epic Games/UE_5.1"),
        Path("C:/Program Files/Epic Games/UE_5.0"),
    ]
    for p in defaults:
        if p.exists():
            return str(p.resolve())

    return None


def _find_project_file() -> Optional[str]:
    """Find the first .uproject file in the project directory tree."""
    env_project = os.environ.get("UE_PROJECT_FILE")
    if env_project and Path(env_project).exists():
        return env_project

    env_dir = os.environ.get("UE_PROJECT_DIR")
    if env_dir:
        candidates = list(Path(env_dir).rglob("*.uproject"))
        if candidates:
            return str(candidates[0].resolve())

    # Walk up from current directory
    cwd = Path.cwd()
    for parent in [cwd] + list(cwd.parents):
        candidates = list(parent.glob("*.uproject"))
        if candidates:
            return str(candidates[0].resolve())

    return None


def _uat_path() -> Optional[Path]:
    """Return the path to RunUAT.bat (Windows) or RunUAT.sh (Linux/Mac)."""
    engine_dir = _find_ue_engine_dir()
    if not engine_dir:
        return None
    bat = Path(engine_dir) / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"
    if bat.exists():
        return bat
    sh = Path(engine_dir) / "Engine" / "Build" / "BatchFiles" / "RunUAT.sh"
    if sh.exists():
        return sh
    return None


def _ue_cmd_path() -> Optional[Path]:
    """Return the path to UE4Editor-Cmd.exe or equivalent."""
    engine_dir = _find_ue_engine_dir()
    if not engine_dir:
        return None
    # Windows
    win = Path(engine_dir) / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
    if win.exists():
        return win
    # Fallback legacy name
    legacy = Path(engine_dir) / "Engine" / "Binaries" / "Win64" / "UE4Editor-Cmd.exe"
    if legacy.exists():
        return legacy
    return None


def _run_subprocess(
    cmd: List[str],
    cwd: Optional[str] = None,
    timeout: int = 3600,
) -> Dict[str, Any]:
    """Run a subprocess and return a standardized result dict."""
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
        return {
            "success": result.returncode == 0,
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
    except FileNotFoundError as e:
        logger.error(f"Command not found: {e}")
        return make_error_response(f"Command not found: {e.filename}")
    except subprocess.TimeoutExpired:
        logger.error(f"Command timed out after {timeout}s")
        return make_error_response(f"Command timed out after {timeout} seconds")
    except Exception as e:
        logger.error(f"Subprocess error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Build / Cook / Package tools
# ---------------------------------------------------------------------------

@mcp.tool()
def build_project(
    platform: str = "Win64",
    configuration: str = "Development",
    target: Optional[str] = None,
    extra_args: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Build the Unreal project using UAT BuildCookRun.

    This performs a full build + cook + stage cycle.

    platform: Target platform (Win64, Linux, Mac, Android, IOS, etc.)
    configuration: Build configuration (Development, Shipping, Test, Debug)
    target: Optional target name (defaults to project name)
    extra_args: Additional UAT arguments
    """
    uat = _uat_path()
    if not uat:
        return make_error_response(
            "Could not find Unreal Automation Tool (RunUAT). "
            "Set UE_ENGINE_DIR environment variable or install UE to the default path."
        )

    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. "
            "Set UE_PROJECT_FILE or UE_PROJECT_DIR environment variable."
        )

    cmd = [
        str(uat),
        "BuildCookRun",
        f'-project="{project_file}"',
        f"-platform={platform}",
        f"-clientconfig={configuration}",
        "-noP4",
        "-build",
        "-cook",
        "-stage",
        "-pak",
        "-archive",
    ]
    if target:
        cmd.append(f'-target={target}')
    if extra_args:
        cmd.extend(extra_args)

    return _run_subprocess(cmd)


@mcp.tool()
def cook_content(
    platform: str = "Win64",
    extra_args: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Cook content for the specified platform without building or packaging.

    platform: Target platform (Win64, Linux, Mac, Android, IOS, etc.)
    extra_args: Additional UAT arguments
    """
    uat = _uat_path()
    if not uat:
        return make_error_response(
            "Could not find Unreal Automation Tool (RunUAT). "
            "Set UE_ENGINE_DIR environment variable or install UE to the default path."
        )

    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. "
            "Set UE_PROJECT_FILE or UE_PROJECT_DIR environment variable."
        )

    cmd = [
        str(uat),
        "BuildCookRun",
        f'-project="{project_file}"',
        f"-platform={platform}",
        "-noP4",
        "-cook",
        "-skipstage",
        "-skiparchive",
    ]
    if extra_args:
        cmd.extend(extra_args)

    return _run_subprocess(cmd)


@mcp.tool()
def package_for_platform(
    platform: str = "Win64",
    configuration: str = "Shipping",
    output_directory: Optional[str] = None,
    extra_args: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Package the project for the specified platform.

    platform: Target platform (Win64, Linux, Mac, Android, IOS, etc.)
    configuration: Build configuration (Development, Shipping, Test, Debug)
    output_directory: Optional output directory for the packaged build
    extra_args: Additional UAT arguments
    """
    uat = _uat_path()
    if not uat:
        return make_error_response(
            "Could not find Unreal Automation Tool (RunUAT). "
            "Set UE_ENGINE_DIR environment variable or install UE to the default path."
        )

    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. "
            "Set UE_PROJECT_FILE or UE_PROJECT_FILE environment variable."
        )

    cmd = [
        str(uat),
        "BuildCookRun",
        f'-project="{project_file}"',
        f"-platform={platform}",
        f"-clientconfig={configuration}",
        "-noP4",
        "-build",
        "-cook",
        "-stage",
        "-pak",
        "-archive",
    ]
    if output_directory:
        cmd.append(f'-archivedirectory="{output_directory}"')
    if extra_args:
        cmd.extend(extra_args)

    return _run_subprocess(cmd)


@mcp.tool()
def run_commandlet(
    commandlet: str,
    extra_args: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Run an arbitrary Unreal Editor commandlet.

    commandlet: Name of the commandlet to run (e.g., "ResavePackages", "FixUpRedirects")
    extra_args: Additional arguments passed to the commandlet
    """
    try:
        validate_string(commandlet, "commandlet")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    ue_cmd = _ue_cmd_path()
    if not ue_cmd:
        return make_error_response(
            "Could not find Unreal Editor command-line executable. "
            "Set UE_ENGINE_DIR environment variable or install UE to the default path."
        )

    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. "
            "Set UE_PROJECT_FILE or UE_PROJECT_DIR environment variable."
        )

    cmd = [
        str(ue_cmd),
        f'"{project_file}"',
        f"-run={commandlet}",
        "-unattended",
        "-nullrhi",
    ]
    if extra_args:
        cmd.extend(extra_args)

    return _run_subprocess(cmd)


# ---------------------------------------------------------------------------
# Log / Summary tools
# ---------------------------------------------------------------------------

@mcp.tool()
def get_build_log(
    log_path: Optional[str] = None,
    tail_lines: int = 100,
) -> Dict[str, Any]:
    """Retrieve the Unreal build/cook log.

    log_path: Path to the log file. Defaults to Saved/Logs/Log.txt in the project directory.
    tail_lines: Number of trailing lines to return (0 = entire file).
    """
    if log_path:
        p = Path(log_path)
    else:
        project_file = _find_project_file()
        if not project_file:
            return make_error_response(
                "Could not find project. Set UE_PROJECT_FILE or UE_PROJECT_DIR."
            )
        project_dir = Path(project_file).parent
        p = project_dir / "Saved" / "Logs" / "Log.txt"

    if not p.exists():
        return make_error_response(f"Log file not found: {p}")

    try:
        text = p.read_text(encoding="utf-8", errors="replace")
        if tail_lines > 0:
            lines = text.splitlines()
            text = "\n".join(lines[-tail_lines:])
        return {"success": True, "log_path": str(p), "content": text}
    except Exception as e:
        logger.error(f"get_build_log error: {e}")
        return make_error_response(str(e))


@mcp.tool()
def get_cook_summary(
    cook_log_path: Optional[str] = None,
) -> Dict[str, Any]:
    """Parse the cook log and return a summary of cooked packages.

    cook_log_path: Path to Cook.txt. Defaults to Saved/Cooked/{Platform}/Cook.txt.
    """
    if cook_log_path:
        p = Path(cook_log_path)
    else:
        project_file = _find_project_file()
        if not project_file:
            return make_error_response(
                "Could not find project. Set UE_PROJECT_FILE or UE_PROJECT_DIR."
            )
        project_dir = Path(project_file).parent
        # Search for Cook.txt in Saved/Cooked/
        cooked_dir = project_dir / "Saved" / "Cooked"
        candidates = list(cooked_dir.rglob("Cook.txt")) if cooked_dir.exists() else []
        if candidates:
            p = candidates[0]
        else:
            return make_error_response(
                f"Cook.txt not found in {cooked_dir}. Run cook_content first."
            )

    if not p.exists():
        return make_error_response(f"Cook log not found: {p}")

    try:
        text = p.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()
        summary = {
            "total_lines": len(lines),
            "cooked_packages": 0,
            "warnings": 0,
            "errors": 0,
        }
        for line in lines:
            lower = line.lower()
            if "cook" in lower and "package" in lower and "done" not in lower:
                summary["cooked_packages"] += 1
            if "warning:" in lower or "warn:" in lower:
                summary["warnings"] += 1
            if "error:" in lower or "failed" in lower:
                summary["errors"] += 1
        return {"success": True, "cook_log_path": str(p), "summary": summary}
    except Exception as e:
        logger.error(f"get_cook_summary error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Build configuration (INI-based)
# ---------------------------------------------------------------------------

@mcp.tool()
def set_build_configuration(
    configuration: str = "Development",
) -> Dict[str, Any]:
    """Set the default build configuration in the project.

    configuration: Development, Shipping, Test, or Debug
    """
    try:
        validate_string(configuration, "configuration")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    valid = {"Development", "Shipping", "Test", "Debug"}
    if configuration not in valid:
        return make_error_response(
            f"Invalid configuration '{configuration}'. Must be one of: {', '.join(valid)}"
        )

    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. Set UE_PROJECT_FILE or UE_PROJECT_DIR."
        )

    # .uproject is a JSON file — update BuildConfiguration if present
    import json
    try:
        p = Path(project_file)
        data = json.loads(p.read_text(encoding="utf-8"))
        if "BuildConfiguration" not in data:
            data["BuildConfiguration"] = {}
        data["BuildConfiguration"]["Configuration"] = configuration
        p.write_text(json.dumps(data, indent=2), encoding="utf-8")
        return {
            "success": True,
            "project": str(p),
            "configuration": configuration,
        }
    except Exception as e:
        logger.error(f"set_build_configuration error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Packaging settings (INI-based)
# ---------------------------------------------------------------------------

@mcp.tool()
def get_packaging_settings() -> Dict[str, Any]:
    """Read packaging-related settings from DefaultEngine.ini and DefaultGame.ini."""
    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. Set UE_PROJECT_FILE or UE_PROJECT_DIR."
        )

    project_dir = Path(project_file).parent
    config_dir = project_dir / "Config"
    result = {"success": True, "settings": {}}

    for ini_name in ("DefaultEngine.ini", "DefaultGame.ini"):
        ini_path = config_dir / ini_name
        if ini_path.exists():
            try:
                text = ini_path.read_text(encoding="utf-8", errors="replace")
                result["settings"][ini_name] = text
            except Exception as e:
                result["settings"][ini_name] = f"Error reading file: {e}"

    return result


@mcp.tool()
def set_packaging_settings(
    ini_name: str = "DefaultGame.ini",
    section: str = "",
    key: str = "",
    value: str = "",
) -> Dict[str, Any]:
    """Set a packaging-related setting in a project INI file.

    ini_name: Target INI file (DefaultEngine.ini or DefaultGame.ini)
    section: INI section name (e.g., /Script/Engine.ProjectPackagingSettings)
    key: Property name
    value: Property value
    """
    try:
        validate_string(ini_name, "ini_name")
        validate_string(section, "section")
        validate_string(key, "key")
        validate_string(value, "value")
    except ValidationError as e:
        return make_validation_error_response_from_exception(e)

    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. Set UE_PROJECT_FILE or UE_PROJECT_DIR."
        )

    project_dir = Path(project_file).parent
    ini_path = project_dir / "Config" / ini_name

    if not ini_path.exists():
        return make_error_response(f"INI file not found: {ini_path}")

    try:
        lines = ini_path.read_text(encoding="utf-8").splitlines(keepends=True)
        section_idx = None
        key_idx = None
        for i, line in enumerate(lines):
            if line.strip() == f"[{section}]":
                section_idx = i
            elif section_idx is not None and line.startswith("["):
                break
            elif section_idx is not None and line.startswith(f"{key}="):
                key_idx = i
                break

        new_line = f"{key}={value}\n"
        if key_idx is not None:
            lines[key_idx] = new_line
        elif section_idx is not None:
            lines.insert(section_idx + 1, new_line)
        else:
            lines.append(f"\n[{section}]\n{new_line}")

        ini_path.write_text("".join(lines), encoding="utf-8")
        return {
            "success": True,
            "ini": str(ini_path),
            "section": section,
            "key": key,
            "value": value,
        }
    except Exception as e:
        logger.error(f"set_packaging_settings error: {e}")
        return make_error_response(str(e))


# ---------------------------------------------------------------------------
# Clean intermediate files
# ---------------------------------------------------------------------------

@mcp.tool()
def clean_intermediate_files(
    directories: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Delete intermediate and generated files to force a clean build.

    directories: Optional list of directories to clean. Defaults to:
      Intermediate, Saved/StagedBuilds, Saved/Archives, Binaries
    """
    project_file = _find_project_file()
    if not project_file:
        return make_error_response(
            "Could not find a .uproject file. Set UE_PROJECT_FILE or UE_PROJECT_DIR."
        )

    project_dir = Path(project_file).parent
    dirs_to_clean = directories or [
        "Intermediate",
        "Saved/StagedBuilds",
        "Saved/Archives",
        "Binaries",
    ]

    deleted = []
    errors = []
    for d in dirs_to_clean:
        target = project_dir / d
        if target.exists():
            try:
                import shutil
                shutil.rmtree(target)
                deleted.append(str(target))
            except Exception as e:
                errors.append(f"{target}: {e}")
        else:
            deleted.append(f"{target} (did not exist)")

    return {
        "success": len(errors) == 0,
        "deleted": deleted,
        "errors": errors,
    }
