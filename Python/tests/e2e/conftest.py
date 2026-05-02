"""E2E test fixtures for scene-sync integration tests.

Requires:
  - SurrealDB running on ws://127.0.0.1:8000
  - scene-syncd running on http://127.0.0.1:8787
  - (Optional) Unreal Editor with MCP Bridge on 127.0.0.1:55557

Usage:
    pytest tests/e2e                    # Full E2E (needs all services)
    pytest tests/e2e --skip-unreal     # Skip tests requiring Unreal
"""

import json
import socket
import time
import urllib.error
import urllib.request

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--skip-unreal",
        action="store_true",
        default=False,
        help="Skip tests that require a running Unreal Editor session",
    )


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "requires_unreal: test needs a running Unreal Editor"
    )


def pytest_collection_modifyitems(config, items):
    if config.getoption("--skip-unreal"):
        skip_unreal = pytest.mark.skip(reason="--skip-unreal flag provided")
        for item in items:
            if "requires_unreal" in item.keywords:
                item.add_marker(skip_unreal)


# --- HTTP helpers ---

SCENE_SYNCD_URL = "http://127.0.0.1:8787"
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557


def api_post(path: str, payload: dict) -> dict:
    url = f"{SCENE_SYNCD_URL}{path}"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"HTTP {e.code} on {path}: {body}") from e


def api_get(path: str) -> dict:
    url = f"{SCENE_SYNCD_URL}{path}"
    with urllib.request.urlopen(url, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))


# --- Persistent TCP connection for unreal_command ---
_unreal_socket: socket.socket | None = None


def unreal_command(command: str, params: dict = None) -> dict:
    """Send command to Unreal via persistent TCP connection.

    Reuses a module-level socket across calls. Falls back to new connection
    on any failure, avoiding the 6-retry chain overhead.
    """
    global _unreal_socket
    payload = json.dumps({"command": command, "params": params or {}}).encode("utf-8") + b"\n"
    last_error = None

    for attempt in range(3):
        try:
            if _unreal_socket is None:
                _unreal_socket = socket.create_connection((UNREAL_HOST, UNREAL_PORT), timeout=10)
                _unreal_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                _unreal_socket.settimeout(30)

            _unreal_socket.sendall(payload)
            data = bytearray()
            while b"\n" not in data:
                chunk = _unreal_socket.recv(262144)
                if not chunk:
                    raise ConnectionError("Unreal connection closed by peer")
                data.extend(chunk)
            parsed = json.loads(bytes(data).split(b"\n", 1)[0].decode("utf-8"))
            result = parsed.get("result") if isinstance(parsed, dict) else None
            if isinstance(result, dict):
                for key, value in result.items():
                    parsed.setdefault(key, value)
            return parsed
        except (ConnectionAbortedError, ConnectionResetError, ConnectionError, OSError, socket.timeout) as exc:
            last_error = exc
            try:
                _unreal_socket.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                _unreal_socket.close()
            except OSError:
                pass
            _unreal_socket = None
            if attempt == 2:
                break
            time.sleep(0.3 * (attempt + 1))

    raise last_error


def assert_success(response: dict, context: str):
    if not response.get("success"):
        raise AssertionError(f"{context} failed: {response.get('error', response)}")
    return response.get("data", {})


# --- Fixtures ---

@pytest.fixture(scope="session")
def scene_syncd_available():
    """Check if scene-syncd is reachable."""
    try:
        api_get("/health")
        return True
    except Exception:
        return False


@pytest.fixture(scope="session")
def unreal_available():
    """Check if Unreal MCP bridge is reachable."""
    try:
        with socket.create_connection((UNREAL_HOST, UNREAL_PORT), timeout=3):
            return True
    except (ConnectionRefusedError, socket.timeout, OSError):
        return False


@pytest.fixture
def isolated_scene(scene_syncd_available):
    """Create a unique scene for each test and clean up after."""
    if not scene_syncd_available:
        pytest.skip("scene-syncd not available")
    suffix = f"{time.strftime('%Y%m%d%H%M%S')}_{time.time_ns()}"
    scene_id = f"e2e_test_{suffix}_{id(scene_syncd_available)}"
    api_post("/scenes/create", {
        "scene_id": scene_id,
        "name": f"E2E test {suffix}",
        "description": "Created by E2E test suite",
    })
    yield scene_id
    # Cleanup: delete all objects then let scene be
    try:
        objects = api_post("/objects/list", {"scene_id": scene_id}).get("data", {}).get("objects", [])
        for obj in objects:
            try:
                api_post("/objects/delete", {"scene_id": scene_id, "mcp_id": obj.get("mcp_id", "")})
            except Exception:
                pass
        # Apply with delete to clean up Unreal side
        try:
            api_post("/sync/apply", {
                "scene_id": scene_id,
                "mode": "apply_safe",
                "allow_delete": True,
                "max_operations": 100,
            })
        except Exception:
            pass
    except Exception:
        pass


@pytest.fixture
def upsert_test_object(isolated_scene):
    """Factory fixture to upsert objects into the isolated scene."""
    def _upsert(mcp_id: str, location: dict = None, tags: list = None, **kwargs):
        loc = location or {"x": 0.0, "y": 0.0, "z": 0.0}
        payload = {
            "scene_id": isolated_scene,
            "mcp_id": mcp_id,
            "desired_name": f"E2E_{mcp_id}",
            "actor_type": "StaticMeshActor",
            "asset_ref": {"path": "/Engine/BasicShapes/Cube.Cube"},
            "transform": {
                "location": loc,
                "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
            },
            "tags": tags or ["e2e_test"],
        }
        payload.update(kwargs)
        return api_post("/objects/upsert", payload)
    return _upsert
