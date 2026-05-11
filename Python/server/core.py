"""
Core connection and infrastructure for the Unreal MCP server.
"""

import logging
import os
import socket
import json
import struct
import time
import threading
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional
from mcp.server.fastmcp import FastMCP

from utils.responses import make_error_response


def configure_logging():
    """Configure file logging only if no handlers exist yet."""
    root = logging.getLogger("UnrealMCP_Advanced")
    if not root.handlers:
        handler = logging.FileHandler("unreal_mcp_advanced.log")
        handler.setFormatter(
            logging.Formatter(
                '%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s'
            )
        )
        root.addHandler(handler)
        root.setLevel(logging.DEBUG)


logger = logging.getLogger("UnrealMCP_Advanced")


def _get_env_int(name: str, default: int) -> int:
    value = os.environ.get(name)
    if value is None:
        return default
    try:
        parsed = int(value)
    except ValueError as exc:
        raise ValueError(f"{name} must be an integer, got {value!r}") from exc
    if not 1 <= parsed <= 65535:
        raise ValueError(f"{name} must be between 1 and 65535, got {parsed}")
    return parsed


UNREAL_HOST = os.environ.get("UNREAL_MCP_HOST", "127.0.0.1")
UNREAL_PORT = _get_env_int("UNREAL_MCP_PORT", 55557)


class UnrealConnection:
    """
    Robust connection to Unreal Engine with automatic retry and reconnection.
    """
    MAX_RETRIES = 3
    BASE_RETRY_DELAY = 0.5
    MAX_RETRY_DELAY = 5.0
    CONNECT_TIMEOUT = 10
    SEND_TIMEOUT = 10
    DEFAULT_RECV_TIMEOUT = 30
    LARGE_OP_RECV_TIMEOUT = 300
    BUFFER_SIZE = 8192
    MAX_RESPONSE_SIZE = 10_000_000

    LARGE_OPERATION_COMMANDS = {
        "get_available_materials",
        "create_town",
        "create_castle_fortress",
        "construct_mansion",
        "create_suspension_bridge",
        "create_aqueduct",
        "create_maze",
        "execute_commandlet",
        "build_hlod",
        "rebuild_hlod",
        "import_fbx_mesh",
        "import_texture",
        "import_audio",
        "import_gltf",
        "import_obj",
        "import_usd",
        "import_datasmith",
        "import_alembic",
        "reimport_asset",
        "export_asset",
        "mesh_boolean",
        "mesh_remesh",
        "mesh_simplify",
        "mesh_voxel_remesh",
        "mesh_uv_layout",
        "mesh_merge",
        "mesh_bake",
        "poly_edit",
        "modeling_tool_execute",
        "generate_lods",
        "generate_lightmap_uvs",
        "join_static_mesh_actors",
        "merge_static_mesh_actors",
        "create_proxy_mesh_actor",
        "create_widget_blueprint",
        "add_widget_to_widget_blueprint",
        "remove_widget_from_widget_blueprint",
        "bind_widget_button_on_clicked",
        "bind_widget_property",
        "create_widget_animation",
        "compile_widget_blueprint",
        "create_ui_template",
        "add_widget_to_viewport",
        "execute_python_script",
        "vroid_import_vrm",
    }

    COMMAND_RECV_TIMEOUT_OVERRIDES = {
        "execute_python_script": 900,
        "execute_commandlet": 1800,
        "build_hlod": 3600,
        "rebuild_hlod": 3600,
    }

    def __init__(self):
        self.socket = None
        self.connected = False
        self._lock = threading.RLock()
        self._last_error = None

    def _create_socket(self) -> socket.socket:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(self.CONNECT_TIMEOUT)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 131072)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 131072)
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('hh', 1, 0))
        except OSError:
            pass
        return sock

    def connect(self) -> bool:
        with self._lock:
            if self.connected and self.socket is not None:
                return True
            for attempt in range(self.MAX_RETRIES + 1):
                self._close_socket_unsafe()
                try:
                    logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT} (attempt {attempt + 1}/{self.MAX_RETRIES + 1})...")
                    self.socket = self._create_socket()
                    self.socket.connect((UNREAL_HOST, UNREAL_PORT))
                    self.connected = True
                    self._last_error = None
                    logger.info("Successfully connected to Unreal Engine")
                    return True
                except socket.timeout as e:
                    self._last_error = f"Connection timeout: {e}"
                    logger.warning(f"Connection timeout (attempt {attempt + 1})")
                except ConnectionRefusedError as e:
                    self._last_error = f"Connection refused: {e}"
                    logger.warning(f"Connection refused (attempt {attempt + 1})")
                except OSError as e:
                    self._last_error = f"OS error: {e}"
                    logger.warning(f"OS error during connection: {e} (attempt {attempt + 1})")
                except Exception as e:
                    self._last_error = f"Unexpected error: {e}"
                    logger.error(f"Unexpected connection error: {e} (attempt {attempt + 1})")
                self._close_socket_unsafe()
                self.connected = False
                if attempt < self.MAX_RETRIES:
                    delay = min(self.BASE_RETRY_DELAY * (2 ** attempt), self.MAX_RETRY_DELAY)
                    logger.info(f"Retrying connection in {delay:.1f}s...")
                    time.sleep(delay)
            logger.error(f"Failed to connect after {self.MAX_RETRIES + 1} attempts. Last error: {self._last_error}")
            return False

    def _close_socket_unsafe(self):
        if self.socket:
            try:
                self.socket.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                self.socket.close()
            except OSError:
                pass
            self.socket = None
        self.connected = False

    def disconnect(self):
        with self._lock:
            self._close_socket_unsafe()
            logger.debug("Disconnected from Unreal Engine")

    def _get_timeout_for_command(self, command_type: str) -> int:
        if command_type in self.COMMAND_RECV_TIMEOUT_OVERRIDES:
            return self.COMMAND_RECV_TIMEOUT_OVERRIDES[command_type]
        if command_type in self.LARGE_OPERATION_COMMANDS:
            return self.LARGE_OP_RECV_TIMEOUT
        return self.DEFAULT_RECV_TIMEOUT

    def _receive_response(self, command_type: str) -> bytes:
        timeout = self._get_timeout_for_command(command_type)
        self.socket.settimeout(timeout)
        buffer = bytearray()
        start_time = time.time()
        while True:
            elapsed = time.time() - start_time
            if elapsed > timeout:
                raise TimeoutError(
                    f"Timeout after {elapsed:.1f}s waiting for response to {command_type} "
                    f"(received {len(buffer)} bytes)"
                )
            if len(buffer) > self.MAX_RESPONSE_SIZE:
                raise ValueError(
                    f"Response size exceeded {self.MAX_RESPONSE_SIZE} bytes for {command_type}"
                )
            try:
                chunk = self.socket.recv(self.BUFFER_SIZE)
            except socket.timeout:
                if buffer:
                    line = buffer.decode('utf-8', errors='replace').split('\n', 1)[0]
                    try:
                        json.loads(line)
                        logger.info(f"Got complete response after recv timeout ({len(buffer)} bytes)")
                        return bytes(buffer)
                    except (json.JSONDecodeError, UnicodeDecodeError):
                        pass
                raise TimeoutError(
                    f"Timeout after {elapsed:.1f}s waiting for response to {command_type}"
                )
            if not chunk:
                if not buffer:
                    raise ConnectionError("Connection closed before receiving any data")
                break
            buffer.extend(chunk)
            if b'\n' in buffer:
                line, _sep, _rest = buffer.partition(b'\n')
                logger.info(f"Received complete response ({len(line)} bytes) for {command_type}")
                return bytes(line)
        if buffer:
            line = buffer.decode('utf-8', errors='replace').split('\n', 1)[0]
            try:
                json.loads(line)
                return bytes(buffer)
            except (json.JSONDecodeError, UnicodeDecodeError):
                raise ConnectionError(
                    f"Connection closed with incomplete data ({len(buffer)} bytes)"
                )
        raise ConnectionError("Connection closed without response")

    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        last_error = None
        for attempt in range(self.MAX_RETRIES + 1):
            try:
                return self._send_command_once(command, params, attempt)
            except (ConnectionError, TimeoutError, socket.error, OSError) as e:
                last_error = str(e)
                logger.warning(f"Command failed (attempt {attempt + 1}/{self.MAX_RETRIES + 1}): {e}")
                self.disconnect()
                if attempt < self.MAX_RETRIES:
                    delay = min(self.BASE_RETRY_DELAY * (2 ** attempt), self.MAX_RETRY_DELAY)
                    logger.info(f"Retrying command in {delay:.1f}s...")
                    time.sleep(delay)
            except Exception as e:
                logger.error(f"Unexpected error sending command: {e}")
                self.disconnect()
                return make_error_response(str(e))
        return make_error_response(f"Command failed after {self.MAX_RETRIES + 1} attempts: {last_error}")

    @staticmethod
    def _normalize_response(response: Dict[str, Any]) -> Dict[str, Any]:
        """Normalize a C++ bridge response into a consistent envelope.

        The C++ bridge returns one of three shapes:
        1. ``{"status": "success", "result": {…}}`` — normal success
        2. ``{"status": "error", "error": "…"}`` — bridge-reported error
        3. ``{"success": false, "error": "…"}`` — legacy inner error

        After normalization the response always has:
        - ``success``: bool  (True on success, False on error)
        - ``error``: str     (present on error only)
        - All data fields from ``result`` are promoted to top-level (on success)
        """
        if response.get("status") == "error":
            error_msg = response.get("error") or response.get("message") or "Unknown error"
            return make_error_response(error_msg)

        if response.get("success") is False:
            error_msg = response.get("error") or response.get("message") or "Unknown error"
            return make_error_response(error_msg)

        if response.get("status") == "success":
            result_data = response.get("result")
            if isinstance(result_data, dict):
                normalized: Dict[str, Any] = {"success": True}
                for key, value in result_data.items():
                    if key != "success":
                        normalized[key] = value
                if "name" in result_data or "actors" in result_data or "deleted_actor" in result_data:
                    pass
                return normalized
            return {"success": True}

        if response.get("success") is True and "status" not in response:
            return response

        return response

    def _send_command_once(self, command: str, params: Dict[str, Any], attempt: int) -> Dict[str, Any]:
        with self._lock:
            if not self.connect():
                raise ConnectionError(f"Failed to connect to Unreal Engine: {self._last_error}")
            try:
                command_obj: Dict[str, Any] = {
                    "command": command,
                    "params": params or {},
                }
                auth_token = os.environ.get("UNREAL_MCP_AUTH_TOKEN")
                if auth_token:
                    command_obj["auth_token"] = auth_token
                command_json = json.dumps(command_obj) + '\n'
                logger.info(f"Sending command (attempt {attempt + 1}): {command}")
                logger.debug(f"Command payload: {command_json[:500]}...")
                self.socket.settimeout(self.SEND_TIMEOUT)
                self.socket.sendall(command_json.encode('utf-8'))
                response_data = self._receive_response(command)
                try:
                    response = json.loads(response_data.decode('utf-8'))
                except json.JSONDecodeError as e:
                    logger.error(f"JSON decode error: {e}")
                    logger.debug(f"Raw response: {response_data[:500]}")
                    raise ValueError(f"Invalid JSON response: {e}")
                logger.info(f"Command {command} completed successfully")
                response = self._normalize_response(response)
                if not response.get("success"):
                    error_msg = response.get("error", "Unknown error")
                    logger.warning(f"Unreal returned error: {error_msg}")
                return response
            except (ConnectionError, TimeoutError, socket.error, OSError, ValueError):
                self._close_socket_unsafe()
                self.connected = False
                raise


# Global connection instance
_unreal_connection: Optional[UnrealConnection] = None
_connection_lock = threading.Lock()


def get_unreal_connection() -> UnrealConnection:
    global _unreal_connection
    with _connection_lock:
        if _unreal_connection is None:
            logger.info("Creating new UnrealConnection instance")
            _unreal_connection = UnrealConnection()
        return _unreal_connection


def reset_unreal_connection():
    global _unreal_connection
    with _connection_lock:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal connection reset")


# FastMCP server instance
mcp = FastMCP("UnrealMCP_Advanced")


@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    logger.info("UnrealMCP Advanced server starting up")
    try:
        yield {}
    finally:
        reset_unreal_connection()
        logger.info("Unreal MCP Advanced server shut down")


mcp._lifespan = server_lifespan
