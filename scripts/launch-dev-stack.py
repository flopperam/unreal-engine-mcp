#!/usr/bin/env python3
"""Launch the full Unreal MCP development stack in one command.

Starts SurrealDB, scene-syncd, Unreal Editor, and the Python MCP server.
Each service runs in its own subprocess. Logs are color-coded and streamed
to the console. Press Ctrl+C to gracefully shut everything down.

Usage:
    python scripts/launch-dev-stack.py --all
    python scripts/launch-dev-stack.py --surreal --scene-syncd
    python scripts/launch-dev-stack.py --unreal --mcp-server
    python scripts/launch-dev-stack.py --help
"""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import threading
import time
import urllib.request
from pathlib import Path
from typing import List, Optional

REPO_ROOT = Path(__file__).resolve().parents[1]
PYTHON_ROOT = REPO_ROOT / "Python"
RUST_ROOT = REPO_ROOT / "rust" / "scene-syncd"
UPROJECT_PATH = REPO_ROOT / "FlopperamUnrealMCP" / "FlopperamUnrealMCP.uproject"

DEFAULT_SURREAL_BIND = "127.0.0.1:8000"
DEFAULT_SCENE_SYNCD_URL = "http://127.0.0.1:8787"
DEFAULT_UNREAL_HOST = "127.0.0.1"
DEFAULT_UNREAL_PORT = 55557

NO_COLOR = os.getenv("NO_COLOR") is not None


class Style:
    RESET = "" if NO_COLOR else "\033[0m"
    BOLD = "" if NO_COLOR else "\033[1m"
    DIM = "" if NO_COLOR else "\033[2m"
    RED = "" if NO_COLOR else "\033[31m"
    GREEN = "" if NO_COLOR else "\033[32m"
    YELLOW = "" if NO_COLOR else "\033[33m"
    BLUE = "" if NO_COLOR else "\033[34m"
    MAGENTA = "" if NO_COLOR else "\033[35m"
    CYAN = "" if NO_COLOR else "\033[36m"


def log(service: str, message: str, color: str = Style.RESET) -> None:
    prefix = f"[{color}{Style.BOLD}{service}{Style.RESET}]"
    print(f"{prefix} {message}", flush=True)


def resolve_unreal_engine_root() -> Optional[Path]:
    """Find Unreal Engine installation directory."""
    # 1. Environment variable
    env_root = os.getenv("UNREAL_ENGINE_ROOT")
    if env_root:
        p = Path(env_root)
        if p.exists():
            return p

    # 2. Windows registry
    if sys.platform == "win32":
        try:
            import winreg

            for subkey in ["5.7", "5.5", "5.4", "5.3"]:
                try:
                    with winreg.OpenKey(
                        winreg.HKEY_LOCAL_MACHINE,
                        f"SOFTWARE\\EpicGames\\Unreal Engine\\{subkey}",
                    ) as key:
                        value, _ = winreg.QueryValueEx(key, "InstalledDirectory")
                        p = Path(value)
                        if p.exists():
                            return p
                except FileNotFoundError:
                    continue
        except Exception:
            pass

    # 3. Common paths
    for candidate in [
        r"C:\Program Files\Epic Games\UE_5.7",
        r"C:\Program Files\Epic Games\UE_5.5",
        r"C:\Program Files\Epic Games\UE_5.4",
    ]:
        p = Path(candidate)
        if p.exists():
            return p

    # 4. PATH
    if sys.platform == "win32":
        try:
            result = subprocess.run(
                ["where", "UnrealEditor.exe"],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0:
                exe_path = Path(result.stdout.strip().splitlines()[0])
                # UnrealEditor.exe is at Engine/Binaries/Win64/UnrealEditor.exe
                return exe_path.parents[2]
        except Exception:
            pass

    return None


def resolve_surrealdb_exe() -> Optional[Path]:
    """Find SurrealDB executable."""
    # Check PATH
    if sys.platform == "win32":
        try:
            result = subprocess.run(
                ["where", "surreal"], capture_output=True, text=True, check=False
            )
            if result.returncode == 0:
                return Path(result.stdout.strip().splitlines()[0])
        except Exception:
            pass
    else:
        try:
            result = subprocess.run(
                ["which", "surreal"], capture_output=True, text=True, check=False
            )
            if result.returncode == 0:
                return Path(result.stdout.strip())
        except Exception:
            pass

    # Check repo-local copies
    for candidate in [
        REPO_ROOT / "tools" / "surrealdb" / "surreal.exe",
        REPO_ROOT / "surreal.exe",
        REPO_ROOT / "tools" / "surrealdb" / "surreal",
        REPO_ROOT / "surreal",
    ]:
        if candidate.exists():
            return candidate

    return None


def resolve_scene_syncd_exe() -> Optional[Path]:
    """Find scene-syncd executable or indicate cargo build is needed."""
    exe = RUST_ROOT / "target" / "debug" / "scene-syncd.exe"
    if sys.platform != "win32":
        exe = RUST_ROOT / "target" / "debug" / "scene-syncd"
    if exe.exists():
        source_files = list((RUST_ROOT / "src").rglob("*.rs"))
        source_files.append(RUST_ROOT / "Cargo.toml")
        exe_mtime = exe.stat().st_mtime
        if any(path.exists() and path.stat().st_mtime > exe_mtime for path in source_files):
            return None
        return exe
    return None


def stream_output(proc: subprocess.Popen, service: str, color: str) -> None:
    """Stream subprocess stdout/stderr to console with color-coded prefix."""
    try:
        if proc.stdout:
            for line in iter(proc.stdout.readline, b""):
                if not line:
                    break
                text = line.decode("utf-8", errors="replace").rstrip()
                if text:
                    log(service, text, color)
        if proc.stderr:
            for line in iter(proc.stderr.readline, b""):
                if not line:
                    break
                text = line.decode("utf-8", errors="replace").rstrip()
                if text:
                    log(service, f"{Style.RED}{text}{Style.RESET}", color)
    except Exception:
        pass


def wait_for_service(url: str, timeout: float = 30.0, interval: float = 0.5) -> bool:
    """Poll a URL until it responds or timeout."""
    start = time.time()
    while time.time() - start < timeout:
        try:
            with urllib.request.urlopen(url, timeout=2) as resp:
                if resp.status == 200:
                    return True
        except Exception:
            pass
        time.sleep(interval)
    return False


def wait_for_tcp(host: str, port: int, timeout: float = 60.0, interval: float = 0.5) -> bool:
    """Poll a TCP port until it accepts connections or timeout."""
    import socket

    start = time.time()
    while time.time() - start < timeout:
        try:
            with socket.create_connection((host, port), timeout=2):
                return True
        except Exception:
            pass
        time.sleep(interval)
    return False


def wait_for_tcp_or_process_exit(
    host: str,
    port: int,
    proc: subprocess.Popen,
    timeout: float = 60.0,
    interval: float = 0.5,
) -> tuple[bool, bool]:
    """Poll a TCP port until it accepts connections or the process exits.

    Returns (tcp_ready, process_exited).
    """
    import socket

    start = time.time()
    while time.time() - start < timeout:
        if proc.poll() is not None:
            return False, True
        try:
            with socket.create_connection((host, port), timeout=2):
                return True, False
        except Exception:
            pass
        time.sleep(interval)
    return False, proc.poll() is not None


class DevStackLauncher:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.processes: List[subprocess.Popen] = []
        self.threads: List[threading.Thread] = []
        self._shutdown = False

    def start_surrealdb(self) -> bool:
        exe = resolve_surrealdb_exe()
        if not exe:
            log("SurrealDB", f"{Style.RED}executable not found. Install surreal or place surreal.exe in repo root.{Style.RESET}")
            return False

        db_dir = REPO_ROOT / ".surreal"
        db_dir.mkdir(exist_ok=True)
        db_path = db_dir / "unreal_mcp.db"

        cmd = [
            str(exe),
            "start",
            "--bind",
            self.args.surreal_bind,
            "--user",
            "root",
            "--pass",
            "secret",
            f"rocksdb://{db_path}" if not self.args.surreal_memory else "memory",
        ]

        log("SurrealDB", f"Starting: {Style.DIM}{' '.join(cmd)}{Style.RESET}", Style.CYAN)
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=str(REPO_ROOT),
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )
        self.processes.append(proc)
        t = threading.Thread(target=stream_output, args=(proc, "SurrealDB", Style.CYAN), daemon=True)
        t.start()
        self.threads.append(t)

        health_url = f"http://{self.args.surreal_bind}/health"
        log("SurrealDB", f"Waiting for {health_url} ...", Style.CYAN)
        if wait_for_service(health_url, timeout=10.0):
            log("SurrealDB", f"{Style.GREEN}Ready{Style.RESET}", Style.CYAN)
            return True
        else:
            log("SurrealDB", f"{Style.YELLOW}Health check timed out; proceeding anyway{Style.RESET}", Style.CYAN)
            return True

    def start_scene_syncd(self) -> bool:
        exe = resolve_scene_syncd_exe()
        if exe:
            cmd = [str(exe)]
            cwd = str(exe.parent)
        else:
            # Fallback: cargo run
            cargo = "cargo"
            if sys.platform == "win32":
                # Try to find cargo in typical rustup location
                cargo_path = Path.home() / ".cargo" / "bin" / "cargo.exe"
                if cargo_path.exists():
                    cargo = str(cargo_path)
            cmd = [cargo, "run"]
            cwd = str(RUST_ROOT)
            log("scene-syncd", f"{Style.YELLOW}Binary not found; using 'cargo run' (slower startup){Style.RESET}", Style.BLUE)

        env = os.environ.copy()
        env.setdefault("SCENE_SYNCD_HOST", "127.0.0.1")
        env.setdefault("SCENE_SYNCD_PORT", "8787")
        env.setdefault("SURREAL_URL", f"ws://{self.args.surreal_bind}")
        env.setdefault("SURREAL_NS", "unreal_mcp")
        env.setdefault("SURREAL_DB", "scene")
        env.setdefault("SURREAL_USER", "root")
        env.setdefault("SURREAL_PASS", "secret")
        env.setdefault("UNREAL_MCP_HOST", DEFAULT_UNREAL_HOST)
        env.setdefault("UNREAL_MCP_PORT", str(DEFAULT_UNREAL_PORT))
        env.setdefault("SCENE_SYNCD_AUTOSYNC", "false")
        env.setdefault("SCENE_SYNCD_LOG", "info")

        log("scene-syncd", f"Starting on {env['SCENE_SYNCD_HOST']}:{env['SCENE_SYNCD_PORT']}", Style.BLUE)
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=cwd,
            env=env,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )
        self.processes.append(proc)
        t = threading.Thread(target=stream_output, args=(proc, "scene-syncd", Style.BLUE), daemon=True)
        t.start()
        self.threads.append(t)

        health_url = f"{DEFAULT_SCENE_SYNCD_URL}/health"
        log("scene-syncd", f"Waiting for {health_url} ...", Style.BLUE)
        if wait_for_service(health_url, timeout=30.0):
            log("scene-syncd", f"{Style.GREEN}Ready{Style.RESET}", Style.BLUE)
            return True
        else:
            log("scene-syncd", f"{Style.YELLOW}Health check timed out; proceeding anyway{Style.RESET}", Style.BLUE)
            return True

    def start_unreal(self) -> bool:
        ue_root = resolve_unreal_engine_root()
        if not ue_root:
            log("Unreal", f"{Style.RED}Engine not found. Set UNREAL_ENGINE_ROOT or install UE 5.7+{Style.RESET}")
            return False

        editor_exe = ue_root / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
        if sys.platform != "win32":
            editor_exe = ue_root / "Engine" / "Binaries" / "Mac" / "UnrealEditor"
        if not editor_exe.exists():
            log("Unreal", f"{Style.RED}Editor not found at {editor_exe}{Style.RESET}")
            return False

        if not UPROJECT_PATH.exists():
            log("Unreal", f"{Style.RED}.uproject not found at {UPROJECT_PATH}{Style.RESET}")
            return False

        cmd = [
            str(editor_exe),
            str(UPROJECT_PATH),
            "-Windowed",
            "-ResX=1280",
            "-ResY=720",
        ]
        
        if getattr(self.args, "headless", False):
            cmd.append("-NullRHI")
        
        if getattr(self.args, "render_offscreen", False):
            cmd.append("-RenderOffScreen")
        log("Unreal", f"Starting: {Style.DIM}{editor_exe.name} {UPROJECT_PATH.name}{Style.RESET}", Style.MAGENTA)
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=str(REPO_ROOT),
            # Unreal Editor needs a window, so we don't use CREATE_NO_WINDOW here
            creationflags=0,
        )
        self.processes.append(proc)
        t = threading.Thread(target=stream_output, args=(proc, "Unreal", Style.MAGENTA), daemon=True)
        t.start()
        self.threads.append(t)

        log("Unreal", f"Waiting for TCP {DEFAULT_UNREAL_HOST}:{DEFAULT_UNREAL_PORT} ...", Style.MAGENTA)
        ready, exited = wait_for_tcp_or_process_exit(DEFAULT_UNREAL_HOST, DEFAULT_UNREAL_PORT, proc, timeout=120.0)
        if ready:
            log("Unreal", f"{Style.GREEN}Ready{Style.RESET}", Style.MAGENTA)
            return True
        elif exited:
            log("Unreal", f"{Style.RED}Editor exited before MCP TCP became ready (code {proc.returncode}){Style.RESET}", Style.MAGENTA)
            return False
        else:
            log("Unreal", f"{Style.YELLOW}TCP probe timed out; editor may still be loading{Style.RESET}", Style.MAGENTA)
            return True

    def start_mcp_server(self) -> bool:
        entry = PYTHON_ROOT / "unreal_mcp_server_advanced.py"
        if not entry.exists():
            log("MCP", f"{Style.RED}Server entry point not found: {entry}{Style.RESET}")
            return False

        # Prefer uv; fallback to python -m
        uv_exe = "uv"
        if sys.platform == "win32":
            uv_win = Path.home() / ".local" / "bin" / "uv.exe"
            if uv_win.exists():
                uv_exe = str(uv_win)

        # Check if uv is available
        uv_available = False
        try:
            result = subprocess.run([uv_exe, "--version"], capture_output=True, check=False)
            uv_available = result.returncode == 0
        except Exception:
            pass

        if uv_available:
            cmd = [uv_exe, "run", str(entry)]
            cwd = str(PYTHON_ROOT)
        else:
            cmd = [sys.executable, str(entry)]
            cwd = str(PYTHON_ROOT)

        log("MCP", f"Starting Python MCP server", Style.GREEN)
        # MCP server uses stdio transport; keep stdin open so it doesn't
        # exit immediately when there's no connected client.
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=cwd,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )
        self.processes.append(proc)
        t = threading.Thread(target=stream_output, args=(proc, "MCP", Style.GREEN), daemon=True)
        t.start()
        self.threads.append(t)

        # MCP server uses stdio transport; just give it a moment
        time.sleep(2.0)
        if proc.poll() is None:
            log("MCP", f"{Style.GREEN}Running (stdio transport){Style.RESET}", Style.GREEN)
            return True
        else:
            log("MCP", f"{Style.RED}Process exited early (code {proc.returncode}){Style.RESET}", Style.GREEN)
            return False

    def shutdown(self, signum=None, frame=None) -> None:
        if self._shutdown:
            return
        self._shutdown = True
        print(f"\n{Style.YELLOW}{Style.BOLD}Shutting down development stack...{Style.RESET}")

        for proc in self.processes:
            if proc.poll() is None:
                try:
                    proc.terminate()
                except Exception:
                    pass

        # Wait a bit, then kill if needed
        time.sleep(2.0)
        for proc in self.processes:
            if proc.poll() is None:
                try:
                    proc.kill()
                except Exception:
                    pass

        print(f"{Style.GREEN}{Style.BOLD}All services stopped.{Style.RESET}")
        sys.exit(0)

    def run(self) -> int:
        signal.signal(signal.SIGINT, self.shutdown)
        if hasattr(signal, "SIGTERM"):
            signal.signal(signal.SIGTERM, self.shutdown)

        log("Launcher", f"{Style.BOLD}Repo root:{Style.RESET} {REPO_ROOT}", Style.YELLOW)

        ok = True
        if self.args.surreal:
            ok = self.start_surrealdb() and ok
        if self.args.scene_syncd:
            ok = self.start_scene_syncd() and ok
        if self.args.unreal:
            ok = self.start_unreal() and ok
        if self.args.mcp_server:
            ok = self.start_mcp_server() and ok

        if not ok:
            log("Launcher", f"{Style.RED}Some services failed to start. Shutting down.{Style.RESET}", Style.YELLOW)
            self.shutdown()
            return 1

        print(f"\n{Style.GREEN}{Style.BOLD}Development stack is running!{Style.RESET}")
        print(f"{Style.DIM}Press Ctrl+C to stop all services.{Style.RESET}\n")

        # Keep main thread alive
        try:
            while True:
                time.sleep(1.0)
                # Check if any process died unexpectedly
                for proc in self.processes:
                    if proc.poll() is not None and proc.returncode not in (0, -15, -9):
                        log("Launcher", f"{Style.RED}Process exited unexpectedly (code {proc.returncode}){Style.RESET}", Style.YELLOW)
        except KeyboardInterrupt:
            self.shutdown()

        return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Launch the full Unreal MCP development stack")
    parser.add_argument("--all", action="store_true", help="Start all services (default)")
    parser.add_argument("--surreal", action="store_true", help="Start SurrealDB")
    parser.add_argument("--scene-syncd", action="store_true", help="Start scene-syncd")
    parser.add_argument("--unreal", action="store_true", help="Start Unreal Editor")
    parser.add_argument("--mcp-server", action="store_true", help="Start Python MCP server")
    parser.add_argument("--surreal-memory", action="store_true", help="Use in-memory SurrealDB instead of rocksdb")
    parser.add_argument("--headless", action="store_true", help="Run Unreal Engine in headless mode (no window)")
    parser.add_argument("--render-offscreen", action="store_true", help="Use -RenderOffScreen for Unreal Engine")
    parser.add_argument("--cleanup", action="store_true", help="Cleanup ports (55557, 8787, 8000) before starting")
    return parser


def main(argv: List[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    # Default to --all if no service flags given
    if not any([args.all, args.surreal, args.scene_syncd, args.unreal, args.mcp_server]):
        args.all = True

    if args.all:
        args.surreal = True
        args.scene_syncd = True
        args.unreal = True
        args.mcp_server = True

    if getattr(args, "cleanup", False):
        log("Launcher", "Cleaning up ports before start...", Style.YELLOW)
        cleanup_script = REPO_ROOT / "scripts" / "cleanup-ports.py"
        if cleanup_script.exists():
            subprocess.run([sys.executable, str(cleanup_script)], check=False)
        else:
            log("Launcher", f"{Style.RED}Cleanup script not found at {cleanup_script}{Style.RESET}")

    launcher = DevStackLauncher(args)
    return launcher.run()


if __name__ == "__main__":
    sys.exit(main())
