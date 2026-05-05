use std::io;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use clap::{Parser, Subcommand};
use crossterm::{
    event::{
        self, DisableMouseCapture, EnableMouseCapture, Event, KeyCode, KeyEventKind, KeyModifiers,
    },
    execute,
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
};
use ratatui::prelude::*;
use tracing::warn;

pub mod config;
pub mod error;
pub mod health;
pub mod i18n;
pub mod platform;
pub mod services;
pub mod state;
pub mod tui;

use config::KaguyaConfig;
use error::{KaguyaError, Result};
use services::manager::{LogLevel, ManagerEvent, ServiceManager, StartOptions};
use services::service::HealthStatus;
use tui::app::{App, Dialog, DialogAction, InputMode, COMMANDS};
use tui::ui::draw;

const KAGUYA_ICON: &str = "🌙";

#[derive(Parser, Debug)]
#[command(name = "kaguya")]
#[command(version)]
#[command(about = "Rust CLI/TUI orchestrator for the Unreal MCP development stack")]
struct Cli {
    #[command(subcommand)]
    command: Option<Commands>,

    /// Preset to use (minimal, full, headless)
    #[arg(short, long, default_value = "full", global = true)]
    preset: String,

    /// Explicit kaguya.toml path
    #[arg(long, value_name = "PATH", global = true)]
    config: Option<PathBuf>,

    /// Explicit repository root. Useful after installing kaguya globally.
    #[arg(long, value_name = "PATH", global = true)]
    repo_root: Option<PathBuf>,
}

#[derive(Subcommand, Debug)]
enum Commands {
    /// Start the stack and keep it attached until Ctrl+C
    Up {
        /// Preset to use
        #[arg(short, long)]
        preset: Option<String>,

        /// Start and return immediately. Use mainly with presets that do not rely on MCP stdio.
        #[arg(long)]
        detach: bool,

        /// Skip the initial health probe after launch
        #[arg(long)]
        no_health_wait: bool,
    },
    /// Stop services recorded by kaguya and free known ports
    Down {
        /// Force-kill external processes occupying known ports
        #[arg(long)]
        force: bool,
    },
    /// Show service/process status
    Status,
    /// Cleanup zombie processes and free known ports
    Cleanup {
        /// Force-kill external processes occupying known ports
        #[arg(long)]
        force: bool,
    },
    /// Diagnose config, paths, tools, and port conflicts
    Doctor,
    /// Print the computed start order and dependency graph for a preset
    Plan {
        /// Preset to plan (defaults to the global preset or full)
        #[arg(short, long)]
        preset: Option<String>,
    },
    /// Write a user-level config so installed kaguya can run from any directory
    InstallConfig,
}

#[tokio::main]
async fn main() -> color_eyre::Result<()> {
    color_eyre::install()?;
    tracing_subscriber::fmt()
        .with_env_filter("warn,kaguya=info")
        .with_target(false)
        .init();

    let cli = Cli::parse();
    let config = load_config(&cli)?;

    match &cli.command {
        Some(Commands::Up {
            preset,
            detach,
            no_health_wait,
        }) => {
            let preset = preset.clone().unwrap_or_else(|| cli.preset.clone());
            cli_up(config, &preset, *detach, *no_health_wait).await?;
        }
        Some(Commands::Down { force }) => {
            cli_down(&config, *force).await?;
        }
        Some(Commands::Status) => {
            cli_status(&config).await?;
        }
        Some(Commands::Cleanup { force }) => {
            cli_cleanup(&config, *force).await?;
        }
        Some(Commands::Doctor) => {
            cli_doctor(&config).await?;
        }
        Some(Commands::Plan { preset }) => {
            let preset = preset.clone().unwrap_or_else(|| cli.preset.clone());
            cli_plan(&config, &preset).await?;
        }
        Some(Commands::InstallConfig) => {
            cli_install_config(&config)?;
        }
        None => {
            run_tui(config, &cli.preset).await?;
        }
    }

    Ok(())
}

fn load_config(cli: &Cli) -> Result<KaguyaConfig> {
    KaguyaConfig::load_with_overrides(cli.config.as_deref(), cli.repo_root.clone())
}

async fn cli_up(
    config: KaguyaConfig,
    preset: &str,
    detach: bool,
    no_health_wait: bool,
) -> Result<()> {
    let repo_root = config.repo_root();
    println!("kaguya: repo root {}", repo_root.display());
    println!("kaguya: starting preset '{}'", preset);

    if detach && preset != "minimal" {
        println!(
            "warning: --detach is best for minimal/headless service subsets. The MCP stdio server may exit when no parent process keeps stdin open."
        );
    }

    let mut manager = ServiceManager::new(config, preset);
    let options = StartOptions {
        capture_output: !detach,
        kill_on_drop: !detach,
        no_health_wait,
    };

    if let Err(err) = manager.start_all_with(options).await {
        let _ = manager.stop_all().await;
        return Err(err);
    }

    let state = state::StateFile::from_manager(&manager, preset);
    let state_path = state::save(&state)?;
    println!("kaguya: state saved to {}", state_path.display());

    if !no_health_wait {
        tokio::time::sleep(Duration::from_secs(3)).await;
        manager.check_all_health().await?;
    }
    print_manager_status(&manager);

    if detach {
        println!("kaguya: services started in detached mode. Use `kaguya down` to stop them.");
        return Ok(());
    }

    println!("kaguya: stack is attached. Press Ctrl+C to stop all managed services.");
    tokio::signal::ctrl_c().await.map_err(KaguyaError::Io)?;
    println!();
    println!("kaguya: stopping services...");
    manager.stop_all().await?;
    state::clear()?;
    println!("kaguya: stopped.");
    Ok(())
}

async fn cli_down(config: &KaguyaConfig, force: bool) -> Result<()> {
    println!("kaguya: stopping recorded services...");
    let mut killed = 0;

    let saved = state::load()?;

    if let Some(ref state_file) = saved {
        for service in &state_file.services {
            if kill_if_verified(service.pid, service.exe_path.as_deref(), service.cmdline.as_deref()) {
                println!("  stopped {:15} pid {}", service.name, service.pid);
                killed += 1;
            } else {
                println!("  skipped {:15} pid {} (not verified)", service.name, service.pid);
            }
        }
    }

    for port in known_ports(config) {
        if let Some(occupant) = health::checker::check_port_in_use(port) {
            let is_managed = saved.as_ref().is_some_and(|st| {
                st.services.iter().any(|s| s.pid == occupant.pid)
            });
            if is_managed {
                if health::checker::free_port(port)? {
                    println!("  freed port {}", port);
                    killed += 1;
                }
            } else if force {
                println!(
                    "  force-killing external PID {} on port {} ({})",
                    occupant.pid, port, occupant.name
                );
                if health::checker::free_port(port)? {
                    println!("  freed port {}", port);
                    killed += 1;
                }
            } else {
                println!(
                    "  warning: port {} occupied by external PID {} ({}) — use --force to kill",
                    port, occupant.pid, occupant.name
                );
            }
        }
    }

    state::clear()?;
    println!("kaguya: stopped {} process/port owner(s)", killed);
    Ok(())
}

async fn cli_status(config: &KaguyaConfig) -> Result<()> {
    let saved = state::load()?;
    println!("kaguya: repo root {}", config.repo_root().display());
    println!(
        "{:<15} {:<10} {:<8} {:<8} DETAIL",
        "SERVICE", "STATE", "PID", "PORT"
    );

    for name in service_order(config) {
        let port = config.services.get(&name).and_then(|svc| svc.port);
        let saved_service = saved
            .as_ref()
            .and_then(|s| s.services.iter().find(|svc| svc.name == name));
        let (state, pid, detail) = if let Some(service) = saved_service {
            if process_alive(service.pid) {
                (
                    "running",
                    service.pid.to_string(),
                    "recorded by kaguya".to_string(),
                )
            } else {
                (
                    "stale",
                    service.pid.to_string(),
                    "state pid is not alive".to_string(),
                )
            }
        } else if let Some(port) = port {
            if let Some(occupant) = health::checker::check_port_in_use(port) {
                (
                    "external",
                    occupant.pid.to_string(),
                    format!("port owned by {}", occupant.name),
                )
            } else {
                ("stopped", "-".to_string(), "port free".to_string())
            }
        } else {
            ("unknown", "-".to_string(), "no pid recorded".to_string())
        };

        println!(
            "{:<15} {:<10} {:<8} {:<8} {}",
            name,
            state,
            pid,
            port.map(|p| p.to_string())
                .unwrap_or_else(|| "-".to_string()),
            detail
        );
    }
    Ok(())
}

async fn cli_cleanup(config: &KaguyaConfig, force: bool) -> Result<()> {
    println!("kaguya: cleaning up known ports...");
    let saved = state::load()?;
    for port in known_ports(config) {
        if let Some(occupant) = health::checker::check_port_in_use(port) {
            let is_managed = saved.as_ref().is_some_and(|st| {
                st.services.iter().any(|s| s.pid == occupant.pid)
            });
            if is_managed {
                println!(
                    "  port {} occupied by PID {} ({}) — freeing verified process",
                    port, occupant.pid, occupant.name
                );
                if health::checker::free_port(port)? {
                    println!("    freed");
                }
            } else if force {
                println!(
                    "  force-killing external PID {} on port {} ({})",
                    occupant.pid, port, occupant.name
                );
                if health::checker::free_port(port)? {
                    println!("    freed");
                }
            } else {
                println!(
                    "  port {} occupied by external PID {} ({}) — use --force to kill",
                    port, occupant.pid, occupant.name
                );
            }
        } else {
            println!("  port {} is free", port);
        }
    }
    Ok(())
}

async fn cli_doctor(config: &KaguyaConfig) -> Result<()> {
    let repo_root = config.repo_root();
    println!("kaguya doctor");
    println!(
        "  config: {}",
        config
            .source_path
            .as_ref()
            .map(|p| p.display().to_string())
            .unwrap_or_else(|| "<generated>".to_string())
    );
    println!("  repo:   {}", repo_root.display());

    // Preset check
    let preset = "full";
    if config.presets.contains_key(preset) {
        println!("  preset '{}'   ok", preset);
    } else {
        println!("  preset '{}'   [WARN] not defined in config", preset);
    }

    // State file age
    let state_path = state::state_path();
    if let Ok(meta) = std::fs::metadata(&state_path) {
        if let Ok(mtime) = meta.modified() {
            let age = std::time::SystemTime::now()
                .duration_since(mtime)
                .unwrap_or_default()
                .as_secs();
            if age > 24 * 3600 {
                println!(
                    "  state file    [WARN] older than 24h ({}s)",
                    age
                );
            } else {
                println!("  state file    ok ({}s old)", age);
            }
        }
    } else {
        println!("  state file    none");
    }

    let uproject = config
        .services
        .get("unreal")
        .and_then(|svc| svc.project_path.as_deref())
        .map(|p| config.resolve_path(p))
        .unwrap_or_else(|| {
            repo_root
                .join("FlopperamUnrealMCP")
                .join("FlopperamUnrealMCP.uproject")
        });
    println!(
        "  uproject: {} {}",
        if uproject.exists() { "ok" } else { "missing" },
        uproject.display()
    );

    // Service-level checks
    println!("\n  Service checks:");
    for (name, svc) in &config.services {
        if !svc.enabled {
            println!("  {:15} [SKIP] disabled", name);
            continue;
        }

        let mut checks = Vec::new();

        // Command on PATH
        if let Some(cmd) = &svc.command {
            let cmd_name = cmd.split_whitespace().next().unwrap_or(cmd);
            if find_executable(cmd_name).await.is_some() {
                checks.push("cmd ok");
            } else {
                checks.push("cmd [NG]");
            }
        }

        // Working dir exists
        if let Some(dir) = &svc.working_dir {
            let path = config.resolve_path(dir);
            if path.exists() {
                checks.push("dir ok");
            } else {
                checks.push("dir [NG]");
            }
        }

        // Health URL validity
        if let Some(url) = &svc.health_url {
            if reqwest::Url::parse(url).is_ok() {
                checks.push("url ok");
            } else {
                checks.push("url [NG]");
            }
        }

        // Log file parent writable
        if let Some(log) = &svc.log_file {
            let path = config.resolve_path(log);
            if let Some(parent) = path.parent() {
                if parent.exists() {
                    checks.push("log ok");
                } else {
                    checks.push("log [NG] (parent missing)");
                }
            }
        }

        let status = if checks.is_empty() {
            "no checks".to_string()
        } else {
            checks.join(" | ")
        };
        println!("  {:15} {}", name, status);
    }

    // Global tool checks
    println!("\n  Tool checks:");
    for tool in ["cargo", "uv", "python", "surreal"] {
        match find_executable(tool).await {
            Some(path) => println!("  {:15} ok      {}", tool, path.display()),
            None => println!("  {:15} missing", tool),
        }
    }

    let ue_env_key = config
        .paths
        .env_key
        .as_deref()
        .unwrap_or("UNREAL_ENGINE_ROOT");
    if let Ok(root) = std::env::var(ue_env_key) {
        println!("  {:15} ok      {}={}", "unreal env", ue_env_key, root);
    } else if let Some(path) = find_executable(if cfg!(windows) {
        "UnrealEditor.exe"
    } else {
        "UnrealEditor"
    })
    .await
    {
        println!("  {:15} ok      {}", "unreal exe", path.display());
    } else {
        println!("  {:15} missing (set {})", "unreal exe", ue_env_key);
    }

    // Unreal plugin compiled check
    let plugin_binaries = repo_root
        .join("Plugins")
        .join("UnrealMCP")
        .join("Binaries");
    println!(
        "  {:15} {} {}",
        "plugin binaries",
        if plugin_binaries.exists() {
            "ok"
        } else {
            "[WARN] missing"
        },
        plugin_binaries.display()
    );

    // WSL detection
    if platform::is_wsl() {
        println!("  {:15} detected", "WSL");
    } else {
        println!("  {:15} not detected", "WSL");
    }

    // Port checks
    println!("\n  Port checks:");
    for port in known_ports(config) {
        if let Some(occupant) = health::checker::check_port_in_use(port) {
            println!(
                "  port {:<5} [WARN] busy    PID {} ({})",
                port, occupant.pid, occupant.name
            );
        } else {
            println!("  port {:<5} ok      free", port);
        }
    }

    Ok(())
}

fn cli_install_config(config: &KaguyaConfig) -> Result<()> {
    let mut installed = config.clone();
    installed.paths.repo_root = Some(config.repo_root().to_string_lossy().to_string());
    installed.source_path = None;

    let dirs = directories::ProjectDirs::from("", "unreal-mcp", "kaguya").ok_or_else(|| {
        KaguyaError::Config("Could not resolve user config directory".to_string())
    })?;
    let path = dirs.config_dir().join("kaguya.toml");
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let text = toml::to_string_pretty(&installed)
        .map_err(|e| KaguyaError::Config(format!("Failed to serialize config: {}", e)))?;
    std::fs::write(&path, text)?;
    println!("kaguya: user config written to {}", path.display());
    println!(
        "kaguya: install the binary with `cargo install --path {}`",
        config.repo_root().join("kaguya").display()
    );
    Ok(())
}

async fn cli_plan(config: &KaguyaConfig, preset: &str) -> Result<()> {
    println!("kaguya plan — preset '{}'", preset);

    let manager = ServiceManager::new(config.clone(), preset);
    let order = manager.resolve_start_order(preset)?;

    println!("\nStart order:");
    for (i, name) in order.iter().enumerate() {
        let deps = config
            .services
            .get(name)
            .map(|c| c.dependencies.join(", "))
            .unwrap_or_default();
        let dep_str = if deps.is_empty() {
            String::new()
        } else {
            format!(" (deps: {})", deps)
        };
        println!("  {}. {}{}", i + 1, name, dep_str);
    }

    println!("\nChecks:");
    for name in &order {
        let svc = config.services.get(name);
        let command = svc.and_then(|c| c.command.as_deref()).unwrap_or("(default)");
        let working_dir = svc.and_then(|c| c.working_dir.as_deref());
        let port = svc.and_then(|c| c.port);

        let mut checks = Vec::new();

        // Command check
        if command != "(default)" {
            let cmd_name = command.split_whitespace().next().unwrap_or(command);
            if find_executable(cmd_name).await.is_some() {
                checks.push(("cmd", "[OK]"));
            } else {
                checks.push(("cmd", "[NG]"));
            }
        } else {
            checks.push(("cmd", "[WARN] (default)"));
        }

        // Working dir check
        if let Some(dir) = working_dir {
            let path = config.resolve_path(dir);
            if path.exists() {
                checks.push(("dir", "[OK]"));
            } else {
                checks.push(("dir", "[NG]"));
            }
        } else {
            checks.push(("dir", "[WARN] (default)"));
        }

        // Port check
        let port_status = if let Some(port) = port {
            if let Some(occupant) = health::checker::check_port_in_use(port) {
                format!("[WARN] occupied by PID {} ({})", occupant.pid, occupant.name)
            } else {
                "[OK] free".to_string()
            }
        } else {
            "[WARN] none".to_string()
        };
        checks.push(("port", &port_status));

        let check_str = checks
            .iter()
            .map(|(k, v)| format!("{} {}", k, v))
            .collect::<Vec<_>>()
            .join(" | ");
        println!("  {:15} {}", name, check_str);
    }

    Ok(())
}

async fn run_tui(config: KaguyaConfig, preset: &str) -> Result<()> {
    let auto_heal = config.settings.auto_heal;
    let max_log_lines = config.settings.log_buffer_lines;
    let ui_fps = config.settings.ui_fps;
    let emoji = config.settings.emoji;

    let mut manager = ServiceManager::new(config, preset);
    let event_tx = manager.event_sender();
    let event_rx = manager.event_rx.take().expect("Event RX already taken");

    let mut app = App::new(
        event_rx,
        event_tx.clone(),
        preset.to_string(),
        auto_heal,
        max_log_lines,
        emoji,
    );
    seed_services(&mut app, &manager);

    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    let tick_rate = Duration::from_millis(1000 / ui_fps.clamp(1, 60) as u64);
    let mut last_tick = Instant::now();

    let health_tx = event_tx.clone();
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(3));
        loop {
            interval.tick().await;
            let _ = health_tx.send(ManagerEvent::HealthTick);
        }
    });

    let heal_tx = event_tx.clone();
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(10));
        loop {
            interval.tick().await;
            let _ = heal_tx.send(ManagerEvent::HealTick);
        }
    });

    let (return_tx, mut return_rx) = tokio::sync::mpsc::channel::<ServiceManager>(1);
    app.manager_return_tx = Some(return_tx);

    let mut manager = Some(manager);
    let result = run_tui_loop(
        &mut terminal,
        &mut app,
        &mut manager,
        &mut return_rx,
        tick_rate,
        &mut last_tick,
    )
    .await;

    disable_raw_mode()?;
    execute!(
        terminal.backend_mut(),
        LeaveAlternateScreen,
        DisableMouseCapture
    )?;
    terminal.show_cursor()?;

    result
}

async fn run_tui_loop<B: Backend>(
    terminal: &mut Terminal<B>,
    app: &mut App,
    manager: &mut Option<ServiceManager>,
    return_rx: &mut tokio::sync::mpsc::Receiver<ServiceManager>,
    tick_rate: Duration,
    last_tick: &mut Instant,
) -> Result<()> {
    loop {
        if app.should_exit {
            break;
        }

        // Check if a background startup task has returned the manager
        if let Ok(mgr) = return_rx.try_recv() {
            *manager = Some(mgr);
            app.is_starting_stack = false;
        }

        terminal.draw(|f| draw(f, app))?;

        let timeout = tick_rate.saturating_sub(last_tick.elapsed());

        if crossterm::event::poll(timeout)? {
            if let Event::Key(key) = event::read()? {
                if key.kind == KeyEventKind::Press {
                    if key.modifiers.contains(KeyModifiers::CONTROL)
                        && matches!(key.code, KeyCode::Char('c') | KeyCode::Char('C'))
                    {
                        if let Some(ref mut mgr) = manager {
                            let _ = mgr.stop_all().await;
                        }
                        state::clear()?;
                        break;
                    }
                    handle_key(key.code, key.modifiers, app, manager).await?;
                }
            }
        }

        while let Ok(evt) = app.event_rx.try_recv() {
            handle_manager_event(evt, app, manager).await?;
        }

        if last_tick.elapsed() >= tick_rate {
            app.tick();
            *last_tick = Instant::now();
        }
    }

    Ok(())
}

async fn handle_key(
    code: KeyCode,
    modifiers: KeyModifiers,
    app: &mut App,
    manager: &mut Option<ServiceManager>,
) -> Result<()> {
    if app.show_help {
        match code {
            KeyCode::Esc | KeyCode::Enter | KeyCode::Char('q') => app.show_help = false,
            _ => {}
        }
        return Ok(());
    }

    // Dialog has priority when open
    if app.dialog.is_some() {
        match code {
            KeyCode::Left => {
                if let Some(ref mut dialog) = app.dialog {
                    dialog.selected = dialog.selected.saturating_sub(1);
                }
            }
            KeyCode::Right => {
                if let Some(ref mut dialog) = app.dialog {
                    if dialog.selected + 1 < dialog.options.len() {
                        dialog.selected += 1;
                    }
                }
            }
            KeyCode::Enter => {
                if let Some(dialog) = app.dialog.take() {
                    handle_dialog_choice(dialog, app, manager).await?;
                }
            }
            KeyCode::Esc => app.dialog = None,
            _ => {}
        }
        return Ok(());
    }

    match app.input_mode {
        InputMode::Normal => match code {
            KeyCode::Char('i') => {
                app.input_mode = InputMode::Insert;
                app.cursor_pos = app.input.len();
            }
            KeyCode::Char('q') | KeyCode::Char('Q') => {
                if let Some(ref mut mgr) = manager {
                    let _ = mgr.stop_all().await;
                }
                state::clear()?;
                app.should_exit = true;
            }
            KeyCode::Char('l') if modifiers.contains(KeyModifiers::CONTROL) => {
                app.auto_scroll = !app.auto_scroll
            }
            KeyCode::Char('?') => {
                app.show_help = true;
            }
            KeyCode::Char('/') => {
                app.input_mode = InputMode::Insert;
                app.input.clear();
                app.input.push('/');
                app.cursor_pos = 1;
                // Show all commands immediately
                app.command_suggestions = COMMANDS
                    .iter()
                    .map(|(cmd, desc)| (cmd.to_string(), desc.to_string()))
                    .collect();
                app.show_suggestions = true;
                app.suggestion_index = Some(0);
            }
            KeyCode::Up
                if !app.auto_scroll => {
                    app.log_scroll_offset = app.log_scroll_offset.saturating_sub(1);
                }
            KeyCode::Down
                if !app.auto_scroll => {
                    app.log_scroll_offset += 1;
                }
            KeyCode::PageUp
                if !app.auto_scroll => {
                    app.log_scroll_offset = app.log_scroll_offset.saturating_sub(10);
                }
            KeyCode::PageDown
                if !app.auto_scroll => {
                    app.log_scroll_offset += 10;
                }
            KeyCode::Home
                if !app.auto_scroll => {
                    app.log_scroll_offset = 0;
                }
            KeyCode::End => {
                app.auto_scroll = true;
                app.log_scroll_offset = 0;
            }
            KeyCode::Char('j') => {
                if app.services_order.is_empty() {
                    app.selected_service = None;
                } else if let Some(ref current) = app.selected_service {
                    if let Some(idx) = app.services_order.iter().position(|n| n == current) {
                        let next = (idx + 1).min(app.services_order.len().saturating_sub(1));
                        app.selected_service = Some(app.services_order[next].clone());
                    } else {
                        app.selected_service = Some(app.services_order[0].clone());
                    }
                } else {
                    app.selected_service = Some(app.services_order[0].clone());
                }
            }
            KeyCode::Char('k') => {
                if let Some(ref current) = app.selected_service {
                    if let Some(idx) = app.services_order.iter().position(|n| n == current) {
                        let prev = idx.saturating_sub(1);
                        app.selected_service = Some(app.services_order[prev].clone());
                    }
                } else if !app.services_order.is_empty() {
                    app.selected_service = Some(app.services_order[0].clone());
                }
            }
            KeyCode::Char('s') => {
                if let Some(ref mut mgr) = manager {
                    if let Some(ref name) = app.selected_service {
                        if let Some(handle) = mgr.handles().get(name) {
                            if handle.health == HealthStatus::Stopped {
                                if let Some(svc) = mgr.services.iter().find(|s| s.name() == name).cloned() {
                                    let svc_config = mgr.config().services.get(name).cloned().unwrap_or_default();
                                    let ctx = crate::services::service::ServiceContext {
                                        repo_root: mgr.repo_root(),
                                        config: svc_config,
                                        paths: mgr.config().paths.clone(),
                                        preset: mgr.preset().to_string(),
                                        capture_output: true,
                                        kill_on_drop: true,
                                    };
                                    let _ = mgr.start_service(svc.as_ref(), ctx).await;
                                }
                            } else {
                                let _ = mgr.stop_service(name).await;
                            }
                        }
                    }
                }
            }
            KeyCode::Char('r') => {
                if let Some(ref mut mgr) = manager {
                    if let Some(ref name) = app.selected_service {
                        let _ = mgr.stop_service(name).await;
                        if let Some(svc) = mgr.services.iter().find(|s| s.name() == name).cloned() {
                            let svc_config = mgr.config().services.get(name).cloned().unwrap_or_default();
                            let ctx = crate::services::service::ServiceContext {
                                repo_root: mgr.repo_root(),
                                config: svc_config,
                                paths: mgr.config().paths.clone(),
                                preset: mgr.preset().to_string(),
                                capture_output: true,
                                kill_on_drop: true,
                            };
                            let _ = mgr.start_service(svc.as_ref(), ctx).await;
                        }
                    }
                }
            }
            KeyCode::Char('l') if !modifiers.contains(KeyModifiers::CONTROL) => {
                if let Some(ref name) = app.selected_service {
                    if app.log_filter.as_ref() == Some(name) {
                        app.log_filter = None;
                    } else {
                        app.log_filter = Some(name.clone());
                    }
                }
            }
            KeyCode::Enter => {
                if let Some(ref name) = app.selected_service {
                    if let Some(row) = app.services.get(name) {
                        let mut detail = format!(
                            "Service: {}\nPID: {}\nPort: {}\nHealth: {}\nRestarts: {}\nUptime: {}s",
                            row.name,
                            row.pid,
                            row.port,
                            row.health,
                            row.restart_count,
                            row.uptime_secs,
                        );
                        if !row.last_error.is_empty() {
                            detail.push_str(&format!("\nLast error: {}", row.last_error));
                        }
                        app.dialog = Some(Dialog {
                            title: format!("Detail: {}", name),
                            message: detail,
                            options: vec!["OK".to_string()],
                            selected: 0,
                            target_port: None,
                            dialog_action: DialogAction::Info,
                        });
                    }
                }
            }
            _ => {}
        },
        InputMode::Insert => match code {
            KeyCode::Enter => {
                let cmd = app.input.clone();
                app.input.clear();
                app.cursor_pos = 0;
                app.input_mode = InputMode::Normal;
                app.show_suggestions = false;
                app.command_suggestions.clear();
                app.suggestion_index = None;
                if let Err(e) = handle_command(cmd, app, manager).await {
                    app.push_log(
                        "kaguya".to_string(),
                        KAGUYA_ICON.to_string(),
                        format!("Error: {}", e),
                        LogLevel::Error,
                    );
                }
            }
            KeyCode::Tab => {
                if app.show_suggestions {
                    app.accept_suggestion();
                }
            }
            KeyCode::Up if app.show_suggestions && !app.command_suggestions.is_empty() => {
                let len = app.command_suggestions.len();
                app.suggestion_index = Some(
                    app.suggestion_index
                        .map(|i| if i == 0 { len - 1 } else { i - 1 })
                        .unwrap_or(len - 1),
                );
            }
            KeyCode::Down if app.show_suggestions && !app.command_suggestions.is_empty() => {
                let len = app.command_suggestions.len();
                app.suggestion_index = Some(
                    app.suggestion_index
                        .map(|i| (i + 1) % len)
                        .unwrap_or(0),
                );
            }
            KeyCode::Esc => {
                app.input.clear();
                app.cursor_pos = 0;
                app.input_mode = InputMode::Normal;
                app.show_suggestions = false;
                app.command_suggestions.clear();
                app.suggestion_index = None;
            }
            KeyCode::Char(c)
                if app.cursor_pos <= app.input.len() => {
                    app.input.insert(app.cursor_pos, c);
                    app.cursor_pos += 1;
                    app.update_suggestions();
                    if !app.command_suggestions.is_empty() && app.suggestion_index.is_none() {
                        app.suggestion_index = Some(0);
                    }
                }
            KeyCode::Backspace
                if app.cursor_pos > 0 => {
                    app.cursor_pos -= 1;
                    app.input.remove(app.cursor_pos);
                    app.update_suggestions();
                }
            KeyCode::Left
                if app.cursor_pos > 0 => {
                    app.cursor_pos -= 1;
                }
            KeyCode::Right
                if app.cursor_pos < app.input.len() => {
                    app.cursor_pos += 1;
                }
            KeyCode::Home => app.cursor_pos = 0,
            KeyCode::End => app.cursor_pos = app.input.len(),
            _ => {}
        },
    }
    Ok(())
}

async fn handle_manager_event(
    evt: ManagerEvent,
    app: &mut App,
    manager: &mut Option<ServiceManager>,
) -> Result<()> {
    match evt {
        ManagerEvent::ServiceStarted { name, pid } => {
            let icon = manager
                .as_ref()
                .map(|m| service_icon(m, &name))
                .unwrap_or_else(|| app.services.get(&name).map(|r| r.icon.clone()).unwrap_or_else(|| "?".to_string()));
            let port = manager
                .as_ref()
                .and_then(|m| service_port(m, &name))
                .or_else(|| app.services.get(&name).and_then(|r| r.port.parse::<u16>().ok()));
            app.update_service(
                name.clone(),
                icon.clone(),
                HealthStatus::Booting,
                Some(pid),
                port,
            );
            app.push_log(
                name,
                icon,
                format!("Started with PID {}", pid),
                LogLevel::Info,
            );
        }
        ManagerEvent::ServiceStopped { name, reason } => {
            let icon = manager
                .as_ref()
                .map(|m| service_icon(m, &name))
                .unwrap_or_else(|| app.services.get(&name).map(|r| r.icon.clone()).unwrap_or_else(|| "?".to_string()));
            let port = manager
                .as_ref()
                .and_then(|m| service_port(m, &name))
                .or_else(|| app.services.get(&name).and_then(|r| r.port.parse::<u16>().ok()));
            app.update_service(
                name.clone(),
                icon.clone(),
                HealthStatus::Stopped,
                None,
                port,
            );
            app.push_log(name, icon, format!("Stopped: {}", reason), LogLevel::Warn);
        }
        ManagerEvent::ServiceHealthChanged { name, health } => {
            let icon = manager
                .as_ref()
                .map(|m| service_icon(m, &name))
                .unwrap_or_else(|| app.services.get(&name).map(|r| r.icon.clone()).unwrap_or_else(|| "?".to_string()));
            let port = manager
                .as_ref()
                .and_then(|m| service_port(m, &name))
                .or_else(|| app.services.get(&name).and_then(|r| r.port.parse::<u16>().ok()));
            let pid = app
                .services
                .get(&name)
                .and_then(|r| r.pid.parse::<u32>().ok());
            app.update_service(name.clone(), icon.clone(), health.clone(), pid, port);
            if let HealthStatus::Unhealthy(ref msg) = health {
                app.push_log(
                    name,
                    icon,
                    format!("Health check failed: {}", msg),
                    LogLevel::Warn,
                );
            }
        }
        ManagerEvent::HealthTick => {
            if let Some(ref mut mgr) = manager {
                if let Err(e) = mgr.check_all_health().await {
                    warn!("Health check error: {}", e);
                }
            }
        }
        ManagerEvent::HealTick => {
            if let Some(ref mut mgr) = manager {
                mgr.run_auto_heal().await;
            }
        }
        ManagerEvent::LogLine { name, line, level } => {
            let icon = manager
                .as_ref()
                .map(|m| service_icon(m, &name))
                .unwrap_or_else(|| app.services.get(&name).map(|r| r.icon.clone()).unwrap_or_else(|| "?".to_string()));
            app.push_log(name, icon, line, level);
        }
        ManagerEvent::AllStarted => {
            if let Some(ref mgr) = manager {
                let state = state::StateFile::from_manager(mgr, &app.preset);
                let _ = state::save(&state);
            }
            app.is_starting_stack = false;
            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                "All services started".to_string(),
                LogLevel::Info,
            );
        }
        ManagerEvent::Shutdown => {
            let _ = state::clear();
            app.is_starting_stack = false;
            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                "Shutdown complete".to_string(),
                LogLevel::Info,
            );
        }
    }
    Ok(())
}

async fn handle_command(cmd: String, app: &mut App, manager: &mut Option<ServiceManager>) -> Result<()> {
    let parts: Vec<&str> = cmd.split_whitespace().collect();
    if parts.is_empty() {
        return Ok(());
    }

    match parts[0] {
        "/up" => {
            if app.is_starting_stack {
                app.push_log(
                    "kaguya".to_string(),
                    KAGUYA_ICON.to_string(),
                    "Stack is already starting; please wait.".to_string(),
                    LogLevel::Warn,
                );
                return Ok(());
            }

            let preset = parts
                .get(1)
                .map(|s| s.to_string())
                .unwrap_or_else(|| app.preset.clone());
            app.preset = preset.clone();

            if let Some(ref mgr) = manager {
                if let Some((port, occupant)) = first_port_conflict(mgr) {
                    app.dialog = Some(Dialog {
                        title: "Port conflict".to_string(),
                        message: format!(
                            "Port {} is occupied by PID {} ({}). Free it before starting?",
                            port, occupant.pid, occupant.name
                        ),
                        options: vec!["Kill it".to_string(), "Abort".to_string()],
                        selected: 0,
                        target_port: Some(port),
                        dialog_action: DialogAction::KillPort(port),
                    });
                    return Ok(());
                }
            }

            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                format!("Starting stack with preset '{}'", preset),
                LogLevel::Info,
            );

            if let Some(mut mgr) = manager.take() {
                mgr.set_preset(&app.preset);
                let event_tx = app.event_tx.clone();
                let preset = app.preset.clone();
                let return_tx = match app.manager_return_tx.clone() {
                    Some(tx) => tx,
                    None => {
                        // Cannot start without return channel — put manager back
                        *manager = Some(mgr);
                        app.is_starting_stack = false;
                        app.push_log(
                            "kaguya".to_string(),
                            KAGUYA_ICON.to_string(),
                            "Internal error: return channel not set".to_string(),
                            LogLevel::Error,
                        );
                        return Ok(());
                    }
                };
                app.is_starting_stack = true;
                tokio::spawn(async move {
                    let options = StartOptions::default();
                    if let Err(ref e) = mgr.start_all_with(options).await {
                        let _ = event_tx.send(ManagerEvent::LogLine {
                            name: "kaguya".to_string(),
                            line: format!("Startup failed: {}", e),
                            level: LogLevel::Error,
                        });
                    }
                    let state = state::StateFile::from_manager(&mgr, &preset);
                    let _ = state::save(&state);
                    let _ = event_tx.send(ManagerEvent::AllStarted);
                    let _ = return_tx.send(mgr).await;
                });
            } else {
                app.push_log(
                    "kaguya".to_string(),
                    KAGUYA_ICON.to_string(),
                    "Manager not available".to_string(),
                    LogLevel::Error,
                );
            }
        }
        "/down" => {
            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                "Stopping all services...".to_string(),
                LogLevel::Info,
            );
            if let Some(ref mut mgr) = manager {
                mgr.stop_all().await?;
            }
            state::clear()?;
        }
        "/status" => {
            if let Some(ref mut mgr) = manager {
                mgr.check_all_health().await?;
            }
            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                "Status refreshed".to_string(),
                LogLevel::Info,
            );
        }
        "/logs" => {
            if let Some(svc) = parts.get(1) {
                if *svc == "clear" {
                    app.log_buffer.clear();
                    app.push_log(
                        "kaguya".to_string(),
                        KAGUYA_ICON.to_string(),
                        "Log buffer cleared".to_string(),
                        LogLevel::Info,
                    );
                } else {
                    app.log_filter = Some(svc.to_string());
                    app.push_log(
                        "kaguya".to_string(),
                        KAGUYA_ICON.to_string(),
                        format!("Log filter: {}", svc),
                        LogLevel::Info,
                    );
                }
            } else {
                app.log_filter = None;
                app.push_log(
                    "kaguya".to_string(),
                    KAGUYA_ICON.to_string(),
                    "Log filter cleared".to_string(),
                    LogLevel::Info,
                );
            }
        }
        "/cleanup" => {
            if let Some(ref mgr) = manager {
                if let Some((port, occupant)) = first_port_conflict(mgr) {
                    app.dialog = Some(Dialog {
                        title: "Port cleanup".to_string(),
                        message: format!(
                            "Port {} is occupied by PID {} ({})",
                            port, occupant.pid, occupant.name
                        ),
                        options: vec!["Kill it".to_string(), "Abort".to_string()],
                        selected: 0,
                        target_port: Some(port),
                        dialog_action: DialogAction::KillPort(port),
                    });
                } else {
                    app.push_log(
                        "kaguya".to_string(),
                        KAGUYA_ICON.to_string(),
                        "Known ports are free".to_string(),
                        LogLevel::Info,
                    );
                }
            } else {
                app.push_log(
                    "kaguya".to_string(),
                    KAGUYA_ICON.to_string(),
                    "Known ports are free".to_string(),
                    LogLevel::Info,
                );
            }
        }
        "/preset" => {
            if let Some(p) = parts.get(1) {
                app.preset = p.to_string();
                if let Some(ref mut mgr) = manager {
                    mgr.set_preset(p);
                }
                app.push_log(
                    "kaguya".to_string(),
                    KAGUYA_ICON.to_string(),
                    format!("Preset changed to '{}'", p),
                    LogLevel::Info,
                );
            }
        }
        "/health" => {
            if let Some(ref mut mgr) = manager {
                mgr.check_all_health().await?;
            }
            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                "Health check executed".to_string(),
                LogLevel::Info,
            );
        }
        "/config" => {
            if let Some(ref mgr) = manager {
                let config_text = format!(
                    "Repo root: {}\nPreset: {}\nAuto-heal: {}\nMax retries: {}\nLog buffer: {}\nUI FPS: {}",
                    mgr.repo_root().display(),
                    app.preset,
                    app.auto_heal,
                    mgr.max_retries,
                    app.max_log_lines,
                    mgr.config().settings.ui_fps,
                );
                app.dialog = Some(Dialog {
                    title: "Config".to_string(),
                    message: config_text,
                    options: vec!["OK".to_string()],
                    selected: 0,
                    target_port: None,
                    dialog_action: DialogAction::Info,
                });
            } else {
                app.push_log(
                    "kaguya".to_string(),
                    KAGUYA_ICON.to_string(),
                    "Manager not available".to_string(),
                    LogLevel::Warn,
                );
            }
        }
        "/help" => app.show_help = true,
        "/quit" | "/q" => {
            app.should_exit = true;
            if let Some(ref mut mgr) = manager {
                mgr.stop_all().await?;
            }
            state::clear()?;
            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                "Goodbye".to_string(),
                LogLevel::Info,
            );
        }
        _ => {
            let suggestion = App::closest_command(parts[0])
                .map(|cmd| format!(" — did you mean '{}'?", cmd))
                .unwrap_or_default();
            app.push_log(
                "kaguya".to_string(),
                KAGUYA_ICON.to_string(),
                format!("Unknown command: {}{}", parts[0], suggestion),
                LogLevel::Warn,
            );
        }
    }
    Ok(())
}

async fn handle_dialog_choice(
    dialog: Dialog,
    app: &mut App,
    manager: &mut Option<ServiceManager>,
) -> Result<()> {
    let choice = &dialog.options[dialog.selected];
    match dialog.dialog_action {
        DialogAction::KillPort(port) => {
            if choice == "Kill it" {
                if let Some(occupant) = health::checker::check_port_in_use(port) {
                    let is_managed = manager.as_ref().is_some_and(|mgr| {
                        mgr.handles().values().any(|h| h.pid == Some(occupant.pid))
                    });
                    if is_managed {
                        if health::checker::free_port(port)? {
                            app.push_log(
                                "kaguya".to_string(),
                                KAGUYA_ICON.to_string(),
                                format!("Port {} freed", port),
                                LogLevel::Info,
                            );
                        }
                    } else {
                        app.push_log(
                            "kaguya".to_string(),
                            KAGUYA_ICON.to_string(),
                            format!(
                                "Refused to free port {} — occupant is not a managed process",
                                port
                            ),
                            LogLevel::Warn,
                        );
                    }
                }
            }
        }
        DialogAction::CleanupAllVerified => {
            if choice == "Kill it" {
                // reserved for future use
            }
        }
        DialogAction::Info => {}
    }
    Ok(())
}

fn seed_services(app: &mut App, manager: &ServiceManager) {
    app.services_order = manager.services.iter().map(|s| s.name().to_string()).collect();
    for svc in &manager.services {
        app.update_service(
            svc.name().to_string(),
            svc.icon().to_string(),
            HealthStatus::Stopped,
            None,
            service_port(manager, svc.name()),
        );
    }
}

fn first_port_conflict(manager: &ServiceManager) -> Option<(u16, health::checker::PortOccupant)> {
    for port in known_ports(manager.config()) {
        if let Some(occupant) = health::checker::check_port_in_use(port) {
            let is_ours = manager.handles().values().any(|h| h.pid == Some(occupant.pid));
            if !is_ours {
                return Some((port, occupant));
            }
        }
    }
    None
}

fn known_ports(config: &KaguyaConfig) -> Vec<u16> {
    let mut ports: Vec<u16> = config
        .services
        .values()
        .filter_map(|svc| svc.port)
        .collect();
    ports.sort_unstable();
    ports.dedup();
    ports
}

fn service_order(config: &KaguyaConfig) -> Vec<String> {
    let preferred = ["surrealdb", "scene-syncd", "unreal", "mcp-server"];
    let mut names: Vec<String> = config.services.keys().cloned().collect();
    names.sort_by_key(|name| {
        preferred
            .iter()
            .position(|candidate| candidate == name)
            .unwrap_or(preferred.len())
    });
    names
}

fn service_icon(manager: &ServiceManager, name: &str) -> String {
    manager
        .services
        .iter()
        .find(|svc| svc.name() == name)
        .map(|svc| svc.icon().to_string())
        .unwrap_or_else(|| "?".to_string())
}

fn service_port(manager: &ServiceManager, name: &str) -> Option<u16> {
    manager.config().services.get(name).and_then(|svc| svc.port)
}

fn print_manager_status(manager: &ServiceManager) {
    println!("{:<15} {:<8} HEALTH", "SERVICE", "PID");
    for (name, handle) in manager.handles() {
        let status = match &handle.health {
            HealthStatus::Healthy => "healthy".to_string(),
            HealthStatus::Unhealthy(msg) => format!("unhealthy: {}", msg),
            HealthStatus::Booting => "booting".to_string(),
            HealthStatus::Stopped => "stopped".to_string(),
            HealthStatus::Unknown => "unknown".to_string(),
        };
        println!(
            "{:<15} {:<8} {}",
            name,
            handle
                .pid
                .map(|p| p.to_string())
                .unwrap_or_else(|| "-".to_string()),
            status
        );
    }
}

fn process_alive(pid: u32) -> bool {
    use sysinfo::{ProcessRefreshKind, RefreshKind, System};
    let s = System::new_with_specifics(
        RefreshKind::nothing().with_processes(ProcessRefreshKind::everything()),
    );
    s.process(sysinfo::Pid::from_u32(pid)).is_some()
}

fn kill_if_verified(
    pid: u32,
    expected_exe: Option<&str>,
    expected_cmdline: Option<&str>,
) -> bool {
    use sysinfo::{ProcessRefreshKind, RefreshKind, System};
    let s = System::new_with_specifics(
        RefreshKind::nothing().with_processes(ProcessRefreshKind::everything()),
    );
    if let Some(process) = s.process(sysinfo::Pid::from_u32(pid)) {
        let exe = process.exe().map(|p| p.display().to_string()).unwrap_or_default();
        let cmd = process
            .cmd()
            .iter()
            .map(|s| s.to_string_lossy())
            .collect::<Vec<_>>()
            .join(" ");
        let exe_ok = expected_exe.map(|e| exe == e).unwrap_or(true);
        let cmd_ok = expected_cmdline.map(|c| cmd.contains(c)).unwrap_or(true);
        // Both available identity fields must match (AND, not OR)
        if exe_ok && cmd_ok {
            let _ = process.kill();
            return true;
        }
    }
    false
}

async fn find_executable(name: &str) -> Option<PathBuf> {
    let finder = if cfg!(windows) { "where" } else { "which" };
    let executable = if cfg!(windows) && name == "surreal" {
        "surreal.exe"
    } else {
        name
    };
    let output = tokio::process::Command::new(finder)
        .arg(executable)
        .output()
        .await
        .ok()?;
    if !output.status.success() {
        return None;
    }
    let text = String::from_utf8_lossy(&output.stdout);
    text.lines()
        .next()
        .map(|line| Path::new(line.trim()).to_path_buf())
}
