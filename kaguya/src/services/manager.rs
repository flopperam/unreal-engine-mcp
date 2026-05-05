use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::sync::mpsc;
use tokio::time::sleep;
use tracing::{debug, error, info, warn};

use crate::config::{KaguyaConfig, ServiceConfig};
use crate::error::{KaguyaError, Result};
use crate::services::{
    mcp_server::McpServerService, scene_syncd::SceneSyncdService, service::*,
    surrealdb::SurrealDbService, unreal::UnrealService,
};

#[derive(Debug, Clone)]
pub enum ManagerEvent {
    ServiceStarted {
        name: String,
        pid: u32,
    },
    ServiceStopped {
        name: String,
        reason: String,
    },
    ServiceHealthChanged {
        name: String,
        health: HealthStatus,
    },
    LogLine {
        name: String,
        line: String,
        level: LogLevel,
    },
    AllStarted,
    Shutdown,
    /// Periodic tick to trigger health checks
    HealthTick,
    /// Periodic tick to trigger auto-heal
    HealTick,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LogLevel {
    Info,
    Warn,
    Error,
    Debug,
}

#[derive(Debug, Clone, Copy)]
pub struct StartOptions {
    pub capture_output: bool,
    pub kill_on_drop: bool,
    pub no_health_wait: bool,
}

impl Default for StartOptions {
    fn default() -> Self {
        Self {
            capture_output: true,
            kill_on_drop: true,
            no_health_wait: false,
        }
    }
}

pub struct ServiceManager {
    config: KaguyaConfig,
    pub services: Vec<Arc<dyn Service>>,
    handles: HashMap<String, ServiceHandle>,
    preset: String,
    auto_heal: bool,
    pub max_retries: u32,
    pub event_tx: mpsc::UnboundedSender<ManagerEvent>,
    pub event_rx: Option<mpsc::UnboundedReceiver<ManagerEvent>>,
    client: reqwest::Client,
    pub session_id: String,
}

impl ServiceManager {
    pub fn new(config: KaguyaConfig, preset: &str) -> Self {
        let (event_tx, event_rx) = mpsc::unbounded_channel();
        let mut services: Vec<Arc<dyn Service>> = Vec::new();

        // Instantiate services based on config
        for (name, svc_config) in &config.services {
            if !svc_config.enabled {
                continue;
            }
            let svc: Arc<dyn Service> = match name.as_str() {
                "surrealdb" => Arc::new(SurrealDbService),
                "scene-syncd" => Arc::new(SceneSyncdService),
                "unreal" => Arc::new(UnrealService),
                "mcp-server" => Arc::new(McpServerService),
                _ => {
                    warn!("Unknown service '{}', skipping", name);
                    continue;
                }
            };
            services.push(svc);
        }

        let auto_heal = config.settings.auto_heal;
        let max_retries = config.settings.max_retries;
        let client = reqwest::Client::builder()
            .timeout(Duration::from_secs(10))
            .build()
            .unwrap_or_default();
        let session_id = crate::state::generate_session_id();

        Self {
            config,
            services,
            handles: HashMap::new(),
            preset: preset.to_string(),
            auto_heal,
            max_retries,
            event_tx,
            event_rx: Some(event_rx),
            client,
            session_id,
        }
    }

    pub fn event_sender(&self) -> mpsc::UnboundedSender<ManagerEvent> {
        self.event_tx.clone()
    }

    pub fn handles(&self) -> &HashMap<String, ServiceHandle> {
        &self.handles
    }

    pub fn handles_mut(&mut self) -> &mut HashMap<String, ServiceHandle> {
        &mut self.handles
    }

    pub fn config(&self) -> &KaguyaConfig {
        &self.config
    }

    pub fn repo_root(&self) -> std::path::PathBuf {
        self.config.repo_root()
    }

    pub fn preset(&self) -> &str {
        &self.preset
    }

    pub fn set_preset(&mut self, preset: &str) {
        self.preset = preset.to_string();
    }

    pub fn all_healthy_or_stopped(&self) -> bool {
        self.services.iter().all(|s| {
            let name = s.name().to_string();
            match self.handles.get(&name) {
                Some(h) => !h.health.is_booting(),
                None => !self.is_enabled_for_preset(&name, &self.preset),
            }
        })
    }

    pub fn all_required_healthy(&self) -> bool {
        self.services.iter().all(|s| {
            let name = s.name().to_string();
            if !self.is_enabled_for_preset(&name, &self.preset) {
                return true;
            }
            match self.handles.get(&name) {
                Some(h) => h.health.is_healthy(),
                None => false,
            }
        })
    }

    fn is_enabled_for_preset(&self, name: &str, preset: &str) -> bool {
        self.config
            .presets
            .get(preset)
            .map(|list| list.contains(&name.to_string()))
            .unwrap_or(true)
    }

    pub async fn start_all(&mut self) -> Result<()> {
        self.start_all_with(StartOptions::default()).await
    }

    pub async fn start_all_with(&mut self, options: StartOptions) -> Result<()> {
        let preset = self.preset.clone();
        let repo_root = self.config.repo_root();

        // Build dependency order
        let order = self.resolve_start_order(&preset)?;
        info!("Starting services in order: {:?}", order);

        for name in &order {
            let svc_config =
                self.config
                    .services
                    .get(name)
                    .cloned()
                    .unwrap_or_else(|| ServiceConfig {
                        enabled: true,
                        ..Default::default()
                    });

            let ctx = ServiceContext {
                repo_root: repo_root.clone(),
                config: svc_config,
                paths: self.config.paths.clone(),
                preset: preset.clone(),
                capture_output: options.capture_output,
                kill_on_drop: options.kill_on_drop,
            };

            if let Some(svc) = self.services.iter().find(|s| s.name() == name).cloned() {
                let timeout_secs = ctx.config.start_timeout_secs;
                self.start_service(svc.as_ref(), ctx).await?;
                // Wait for healthy before proceeding, unless skipped
                if !options.no_health_wait {
                    self.wait_for_healthy(name, timeout_secs).await?;
                }
            }
        }

        let _ = self.event_tx.send(ManagerEvent::AllStarted);
        Ok(())
    }

    async fn wait_for_healthy(&mut self, name: &str, timeout_secs: u64) -> Result<()> {
        let deadline = std::time::Instant::now() + Duration::from_secs(timeout_secs);
        while std::time::Instant::now() < deadline {
            if let Some(svc) = self.services.iter().find(|s| s.name() == name).cloned() {
                if let Some(handle) = self.handles.get_mut(name) {
                    let health = svc.health_check(handle, &self.client).await;
                    handle.health = health.clone();
                    let _ = self.event_tx.send(ManagerEvent::ServiceHealthChanged {
                        name: name.to_string(),
                        health: health.clone(),
                    });
                    if health.is_healthy() {
                        return Ok(());
                    }
                }
            }
            tokio::time::sleep(Duration::from_millis(500)).await;
        }
        if let Some(handle) = self.handles.get_mut(name) {
            handle.health = HealthStatus::Unhealthy("Start timeout".to_string());
        }
        Err(KaguyaError::ServiceStart {
            name: name.to_string(),
            reason: "Start timeout: service did not become healthy".to_string(),
        })
    }

    pub async fn start_service(&mut self, svc: &dyn Service, ctx: ServiceContext) -> Result<()> {
        let name = svc.name().to_string();
        info!("Starting service: {}", name);

        // Check if already running
        if let Some(handle) = self.handles.get(&name) {
            if handle.pid.is_some() && handle.health != HealthStatus::Stopped {
                warn!("Service {} is already running (PID {:?})", name, handle.pid);
                return Ok(());
            }
        }

        let handle = ServiceHandle {
            name: name.clone(),
            child: None,
            pid: None,
            health: HealthStatus::Booting,
            log_lines: Vec::new(),
            start_time: Some(Instant::now()),
            restart_count: 0,
            config: ctx.config.clone(),
            repo_root: ctx.repo_root.clone(),
        };

        self.handles.insert(name.clone(), handle);

        match svc.start(&ctx).await {
            Ok(mut child) => {
                let pid = child.id();
                if let Some(p) = pid {
                    info!("Service {} started with PID {}", name, p);
                    let _ = self.event_tx.send(ManagerEvent::ServiceStarted {
                        name: name.clone(),
                        pid: p,
                    });
                }

                // Take stdout/stderr before moving child into handle
                let stdout = child.stdout.take();
                let stderr = child.stderr.take();

                // Store child first
                if let Some(h) = self.handles.get_mut(&name) {
                    h.child = Some(child);
                    h.pid = pid;
                }

                // Spawn log readers
                if let Some(stdout) = stdout {
                    let event_tx = self.event_tx.clone();
                    let svc_name = name.clone();
                    tokio::spawn(async move {
                        Self::read_stdout(stdout, &svc_name, event_tx).await;
                    });
                }
                if let Some(stderr) = stderr {
                    let event_tx = self.event_tx.clone();
                    let svc_name = name.clone();
                    tokio::spawn(async move {
                        Self::read_stderr(stderr, &svc_name, event_tx).await;
                    });
                }
            }
            Err(e) => {
                error!("Failed to start service {}: {}", name, e);
                if let Some(h) = self.handles.get_mut(&name) {
                    h.health = HealthStatus::Unhealthy(format!("Start failed: {}", e));
                }
                return Err(e);
            }
        }

        Ok(())
    }

    pub async fn stop_all(&mut self) -> Result<()> {
        info!("Stopping all services...");
        let order = self.resolve_start_order(&self.preset).unwrap_or_default();
        for name in order.into_iter().rev() {
            if let Some(svc) = self.services.iter().find(|s| s.name() == name) {
                if let Some(handle) = self.handles.get_mut(&name) {
                    let _ = svc.stop(handle).await;
                    handle.health = HealthStatus::Stopped;
                    let _ = self.event_tx.send(ManagerEvent::ServiceStopped {
                        name: name.clone(),
                        reason: "Stopped by user".to_string(),
                    });
                }
            }
        }
        let _ = self.event_tx.send(ManagerEvent::Shutdown);
        Ok(())
    }

    pub async fn stop_service(&mut self, name: &str) -> Result<()> {
        if let Some(svc) = self.services.iter().find(|s| s.name() == name) {
            if let Some(handle) = self.handles.get_mut(name) {
                svc.stop(handle).await?;
                handle.health = HealthStatus::Stopped;
            }
        }
        Ok(())
    }

    pub async fn check_all_health(&mut self) -> Result<()> {
        for svc in &self.services {
            let name = svc.name();
            if let Some(handle) = self.handles.get_mut(name) {
                let health = svc.health_check(handle, &self.client).await;
                if handle.health != health {
                    debug!(
                        "Service {} health changed: {:?} -> {:?}",
                        name, handle.health, health
                    );
                    handle.health = health.clone();
                    let _ = self.event_tx.send(ManagerEvent::ServiceHealthChanged {
                        name: name.to_string(),
                        health,
                    });
                }
            }
        }
        Ok(())
    }

    pub async fn run_auto_heal(&mut self) {
        if !self.auto_heal {
            return;
        }
        let repo_root = self.config.repo_root();
        for i in 0..self.services.len() {
            let name = self.services[i].name().to_string();
            let should_heal = self
                .handles
                .get(&name)
                .map(|h| {
                    matches!(h.health, HealthStatus::Unhealthy(_))
                        && h.restart_count < self.max_retries
                })
                .unwrap_or(false);

            if !should_heal {
                continue;
            }

            let restart_count = self.handles.get(&name).unwrap().restart_count;
            warn!(
                "Auto-healing service {} (restart {}/{})",
                name,
                restart_count + 1,
                self.max_retries
            );
            // Wait with exponential backoff, capped at 60s
            let backoff = Duration::from_secs(2u64.pow(restart_count.min(5)))
                .min(Duration::from_secs(60));
            sleep(backoff).await;

            let svc_config =
                self.config
                    .services
                    .get(&name)
                    .cloned()
                    .unwrap_or_else(|| ServiceConfig {
                        enabled: true,
                        ..Default::default()
                    });

            let ctx = ServiceContext {
                repo_root: repo_root.clone(),
                config: svc_config,
                paths: self.config.paths.clone(),
                preset: self.preset.clone(),
                capture_output: true,
                kill_on_drop: true,
            };

            // Stop and remove old handle before restarting
            let svc = self.services[i].clone();
            if let Some(handle) = self.handles.get_mut(&name) {
                let _ = svc.stop(handle).await;
            }
            self.handles.remove(&name);

            if let Err(e) = self.start_service(svc.as_ref(), ctx).await {
                error!("Auto-heal failed for {}: {}", name, e);
            } else if let Some(h) = self.handles.get_mut(&name) {
                h.restart_count = restart_count + 1;
                // Update state file with new PID after auto-heal restart
                let state = crate::state::StateFile::from_manager(self, &self.preset);
                if let Err(e) = crate::state::save(&state) {
                    warn!("Failed to update state file after auto-heal: {}", e);
                }
            }
        }
    }

    pub fn resolve_start_order(&self, preset: &str) -> Result<Vec<String>> {
        let preset_services: Vec<String> = self
            .config
            .presets
            .get(preset)
            .cloned()
            .unwrap_or_else(|| {
                self.services.iter().map(|s| s.name().to_string()).collect()
            });
        resolve_start_order_from_config(&self.config, &preset_services)
    }

    async fn read_stdout(
        stdout: tokio::process::ChildStdout,
        svc_name: &str,
        tx: mpsc::UnboundedSender<ManagerEvent>,
    ) {
        use tokio::io::{AsyncBufReadExt, BufReader};

        let reader = BufReader::new(stdout);
        let mut lines = reader.lines();
        let name = svc_name.to_string();
        while let Ok(Some(line)) = lines.next_line().await {
            let line = Self::sanitize_log_line(&line);
            let level = Self::infer_log_level(&line);
            let _ = tx.send(ManagerEvent::LogLine {
                name: name.clone(),
                line,
                level,
            });
        }
    }

    async fn read_stderr(
        stderr: tokio::process::ChildStderr,
        svc_name: &str,
        tx: mpsc::UnboundedSender<ManagerEvent>,
    ) {
        use tokio::io::{AsyncBufReadExt, BufReader};

        let reader = BufReader::new(stderr);
        let mut lines = reader.lines();
        let name = svc_name.to_string();
        while let Ok(Some(line)) = lines.next_line().await {
            let line = Self::sanitize_log_line(&line);
            let level = Self::infer_log_level(&line);
            let _ = tx.send(ManagerEvent::LogLine {
                name: name.clone(),
                line,
                level,
            });
        }
    }

    fn sanitize_log_line(line: &str) -> String {
        // Strip ANSI escape sequences: ESC[...m, ESC]...BEL, etc.
        let mut result = String::with_capacity(line.len());
        let mut chars = line.chars().peekable();
        while let Some(c) = chars.next() {
            if c == '\x1B' {
                // CSI sequence: ESC [ ... letter
                if chars.peek() == Some(&'[') {
                    chars.next();
                    while let Some(&inner) = chars.peek() {
                        if inner.is_ascii_alphabetic() || inner == '~' {
                            chars.next();
                            break;
                        }
                        chars.next();
                    }
                }
                // OSC sequence: ESC ] ... BEL or ESC \
                else if chars.peek() == Some(&']') {
                    chars.next();
                    while let Some(&inner) = chars.peek() {
                        if inner == '\x07' {
                            chars.next();
                            break;
                        }
                        if inner == '\x1B' {
                            chars.next();
                            if chars.peek() == Some(&'\\') {
                                chars.next();
                            }
                            break;
                        }
                        chars.next();
                    }
                }
                // Other escape sequences: consume one more char
                else {
                    chars.next();
                }
                continue;
            }
            result.push(c);
        }

        // Normalize line endings
        result = result.replace("\r\n", "\n").replace('\r', "\n");
        // Expand tabs
        result = result.replace('\t', "    ");
        // Remove/replace control characters except newline and space
        result = result
            .chars()
            .map(|c| {
                if c == '\n' || c == ' ' {
                    c
                } else if c.is_ascii_control() {
                    '\u{FFFD}' // replacement character
                } else {
                    c
                }
            })
            .collect();

        // Truncate extreme lines
        const MAX_LEN: usize = 8000;
        if result.len() > MAX_LEN {
            result.truncate(MAX_LEN);
            result.push_str(" … (truncated)");
        }
        result
    }

    fn infer_log_level(line: &str) -> LogLevel {
        let lower = line.to_lowercase();
        if lower.contains("error") || lower.contains("panic") || lower.contains("fatal") {
            LogLevel::Error
        } else if lower.contains("warn") {
            LogLevel::Warn
        } else if lower.contains("debug") || lower.contains("trace") {
            LogLevel::Debug
        } else {
            LogLevel::Info
        }
    }
}

/// Pure function for resolving start order from config, testable without ServiceManager
pub fn resolve_start_order_from_config(
    config: &KaguyaConfig,
    preset_services: &[String],
) -> Result<Vec<String>> {
    let mut order = Vec::new();
    let mut visited = std::collections::HashSet::new();
    let mut stack = std::collections::HashSet::new();

    fn visit(
        name: &str,
        config: &KaguyaConfig,
        preset_services: &[String],
        visited: &mut std::collections::HashSet<String>,
        order: &mut Vec<String>,
        stack: &mut std::collections::HashSet<String>,
    ) -> Result<()> {
        if visited.contains(name) {
            return Ok(());
        }
        if stack.contains(name) {
            return Err(KaguyaError::Config(format!(
                "Dependency cycle detected involving service '{}'",
                name
            )));
        }
        stack.insert(name.to_string());
        let deps = config
            .services
            .get(name)
            .map(|c| c.dependencies.clone())
            .unwrap_or_default();
        for dep in &deps {
            if !config.services.contains_key(dep) {
                return Err(KaguyaError::Config(format!(
                    "Service '{}' declares missing dependency '{}'",
                    name, dep
                )));
            }
            if !preset_services.contains(dep) {
                continue;
            }
            visit(dep, config, preset_services, visited, order, stack)?;
        }
        stack.remove(name);
        visited.insert(name.to_string());
        order.push(name.to_string());
        Ok(())
    }

    for name in preset_services {
        visit(name, config, preset_services, &mut visited, &mut order, &mut stack)?;
    }

    Ok(order)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    fn make_config(
        presets: HashMap<String, Vec<String>>,
        services: HashMap<String, ServiceConfig>,
    ) -> KaguyaConfig {
        KaguyaConfig {
            presets,
            settings: crate::config::Settings::default(),
            services,
            paths: crate::config::PathConfig::default(),
            source_path: None,
        }
    }

    #[test]
    fn linear_dependencies() {
        // a -> b -> c: order is c, b, a (dependencies first)
        let mut services = HashMap::new();
        services.insert("a".to_string(), ServiceConfig {
            dependencies: vec!["b".to_string()],
            ..Default::default()
        });
        services.insert("b".to_string(), ServiceConfig {
            dependencies: vec!["c".to_string()],
            ..Default::default()
        });
        services.insert("c".to_string(), ServiceConfig {
            dependencies: vec![],
            ..Default::default()
        });
        let config = make_config(HashMap::new(), services);
        let preset = vec!["a".to_string(), "b".to_string(), "c".to_string()];
        let order = resolve_start_order_from_config(&config, &preset).unwrap();
        assert_eq!(order, vec!["c", "b", "a"]);
    }

    #[test]
    fn diamond_dependencies() {
        // d depends on a, b; a and b depend on c
        let mut services = HashMap::new();
        services.insert("a".to_string(), ServiceConfig {
            dependencies: vec!["c".to_string()],
            ..Default::default()
        });
        services.insert("b".to_string(), ServiceConfig {
            dependencies: vec!["c".to_string()],
            ..Default::default()
        });
        services.insert("c".to_string(), ServiceConfig {
            dependencies: vec![],
            ..Default::default()
        });
        services.insert("d".to_string(), ServiceConfig {
            dependencies: vec!["a".to_string(), "b".to_string()],
            ..Default::default()
        });
        let config = make_config(HashMap::new(), services);
        let preset = vec!["a".to_string(), "b".to_string(), "c".to_string(), "d".to_string()];
        let order = resolve_start_order_from_config(&config, &preset).unwrap();
        // c must come before a and b, which must come before d
        let c_pos = order.iter().position(|x| x == "c").unwrap();
        let a_pos = order.iter().position(|x| x == "a").unwrap();
        let b_pos = order.iter().position(|x| x == "b").unwrap();
        let d_pos = order.iter().position(|x| x == "d").unwrap();
        assert!(c_pos < a_pos);
        assert!(c_pos < b_pos);
        assert!(a_pos < d_pos);
        assert!(b_pos < d_pos);
    }

    #[test]
    fn cycle_detection() {
        let mut services = HashMap::new();
        services.insert("a".to_string(), ServiceConfig {
            dependencies: vec!["b".to_string()],
            ..Default::default()
        });
        services.insert("b".to_string(), ServiceConfig {
            dependencies: vec!["a".to_string()],
            ..Default::default()
        });
        let config = make_config(HashMap::new(), services);
        let preset = vec!["a".to_string(), "b".to_string()];
        let result = resolve_start_order_from_config(&config, &preset);
        assert!(result.is_err());
        let err_msg = result.unwrap_err().to_string();
        assert!(err_msg.contains("cycle"), "Expected cycle error, got: {}", err_msg);
    }

    #[test]
    fn missing_dependency_returns_error() {
        let mut services = HashMap::new();
        services.insert("a".to_string(), ServiceConfig {
            dependencies: vec!["nonexistent".to_string()],
            ..Default::default()
        });
        let config = make_config(HashMap::new(), services);
        let preset = vec!["a".to_string()];
        let result = resolve_start_order_from_config(&config, &preset);
        assert!(result.is_err());
        let err_msg = result.unwrap_err().to_string();
        assert!(err_msg.contains("missing dependency"), "Expected missing dep error, got: {}", err_msg);
    }

    #[test]
    fn dependency_not_in_preset_is_skipped() {
        let mut services = HashMap::new();
        services.insert("a".to_string(), ServiceConfig {
            dependencies: vec!["b".to_string()],
            ..Default::default()
        });
        services.insert("b".to_string(), ServiceConfig {
            dependencies: vec![],
            ..Default::default()
        });
        let config = make_config(HashMap::new(), services);
        // Only 'a' in preset, 'b' is not — should skip b dependency
        let preset = vec!["a".to_string()];
        let order = resolve_start_order_from_config(&config, &preset).unwrap();
        assert_eq!(order, vec!["a"]);
    }
}
