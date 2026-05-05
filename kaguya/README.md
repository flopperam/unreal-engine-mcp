# Kaguya

Rust TUI/CLI orchestrator for the Unreal MCP development stack. Launches, monitors, and manages SurrealDB, scene-syncd, Unreal Editor, and the MCP Server from a single interface.

## Installation

```bash
cd kaguya
cargo build --release
# Or install globally:
cargo install --path .
```

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Windows | Full support | Registry-based UE discovery |
| WSL2 | Supported | Launches Unreal via `cmd.exe`, uses TCP health checks |
| macOS | Partial | No registry; set `UNREAL_ENGINE_ROOT` |

## Quick Start

```bash
# Launch the TUI dashboard
kaguya

# Or use CLI subcommands
kaguya up --preset full
kaguya down
kaguya status
kaguya doctor
kaguya plan --preset minimal
```

## Configuration

Kaguya reads `kaguya.toml` from the current directory, parent directories, or the path specified by `--config` / `KAGUYA_CONFIG`.

### Example `kaguya.toml`

```toml
[settings]
auto_heal = true
max_retries = 3
log_buffer_lines = 1000
ui_fps = 30
language = "en"   # "en" or "ja"
emoji = true

[presets]
full = ["surrealdb", "scene-syncd", "unreal", "mcp-server"]
minimal = ["surrealdb", "scene-syncd"]
headless = ["surrealdb", "scene-syncd", "mcp-server"]

[services.surrealdb]
enabled = true
port = 8000
health_url = "http://127.0.0.1:8000/health"
health_timeout_secs = 10
start_timeout_secs = 60

[services.scene-syncd]
enabled = true
port = 8787
health_url = "http://127.0.0.1:8787/health"
dependencies = ["surrealdb"]
working_dir = "rust/scene-syncd"   # optional, defaults to rust/scene-syncd

[services.unreal]
enabled = true
port = 55557
health_type = "tcp"
project_path = "FlopperamUnrealMCP/FlopperamUnrealMCP.uproject"

[services.mcp-server]
enabled = true
dependencies = ["unreal"]
log_file = "logs/mcp-server.log"
```

### Service Config Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | true | Whether to include this service |
| `command` | string | (service default) | Override the start command |
| `args` | string list | (service default) | Override arguments |
| `working_dir` | string | (service default) | Working directory for the process |
| `port` | u16 | (none) | Port for health checks and conflict detection |
| `health_url` | string | (none) | HTTP endpoint for health checks |
| `health_type` | string | (auto) | `http`, `tcp`, `process`, `log_fresh`, or `composite` |
| `health_timeout_secs` | u64 | 10 | Timeout for each health probe |
| `start_timeout_secs` | u64 | 60 | Timeout for dependency health gating |
| `dependencies` | string list | [] | Services that must be healthy before this one starts |
| `log_file` | string | (none) | Log file path for `log_fresh` health checks |
| `project_path` | string | (auto) | `.uproject` path (Unreal only) |
| `headless_args` | string list | [] | Extra args when using the `headless` preset |

## Presets

Presets define which services to start. Use `--preset` or `/preset` in the TUI.

- **full**: All four services (surrealdb -> scene-syncd -> unreal -> mcp-server)
- **minimal**: Database + sync daemon only (no Unreal, no MCP)
- **headless**: No Unreal window, MCP server connects to an already-running editor

## TUI Key Map

### Normal Mode

| Key | Action |
|-----|--------|
| `i` | Enter insert mode |
| `/` | Enter command (pre-fills `/`) |
| `j` / `k` | Select service row |
| `s` | Start/stop selected service |
| `r` | Restart selected service |
| `l` | Filter logs to selected service |
| `Enter` | Service detail popup |
| `Ctrl+L` | Toggle auto-scroll |
| `Up/Down` | Scroll logs (when auto-scroll is off) |
| `PgUp/PgDn` | Scroll logs by page |
| `Home/End` | Jump to oldest/newest log |
| `q` | Quit TUI |
| `?` | Show help |

### Insert Mode

| Key | Action |
|-----|--------|
| `Enter` | Execute command |
| `Esc` | Cancel input |
| `Left/Right` | Move cursor |
| `Home/End` | Jump to start/end |
| `Backspace` | Delete character before cursor |

### Dialog

| Key | Action |
|-----|--------|
| `Left/Right` | Select option |
| `Enter` | Confirm |
| `Esc` | Cancel |

## TUI Commands

| Command | Description |
|---------|-------------|
| `/up [preset]` | Start the stack (optionally with a different preset) |
| `/down` | Stop all services |
| `/status` | Refresh health checks |
| `/logs [service]` | Filter logs (or clear filter) |
| `/cleanup` | Free occupied ports |
| `/preset <name>` | Switch preset |
| `/health` | Run health checks |
| `/config` | Show current config |
| `/help` | Show help |
| `/quit` | Quit |

## CLI Subcommands

| Command | Description |
|---------|-------------|
| `kaguya up` | Start stack in attached mode |
| `kaguya up --detach` | Start and return immediately |
| `kaguya up --no-health-wait` | Skip dependency health gating |
| `kaguya down` | Stop recorded services and free ports |
| `kaguya down --force` | Also kill external processes on known ports |
| `kaguya status` | Show service/process status |
| `kaguya cleanup` | Free occupied ports |
| `kaguya cleanup --force` | Force-kill external processes on ports |
| `kaguya doctor` | Diagnose config, paths, tools, ports |
| `kaguya plan` | Show computed start order and checks |
| `kaguya install-config` | Write user-level config for global install |

## Dangerous Operations

`kaguya down` and `kaguya cleanup` kill processes and free ports. Safety measures:

- **Identity verification**: Processes are only killed if their `exe_path` and `cmdline` match what was recorded in the state file.
- **Port ownership**: Ports occupied by external (non-kaguya) processes are **not** killed by default. A warning is printed instead.
- **`--force` flag**: Required to kill external processes occupying known ports. Use with caution.
- **Session tracking**: The state file records session IDs and timestamps. Stale state files (>24h) trigger a warning from `kaguya doctor`.