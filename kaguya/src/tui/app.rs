use std::collections::{HashMap, VecDeque};
use tokio::sync::mpsc;

use crate::services::manager::{LogLevel, ManagerEvent};
use crate::services::service::HealthStatus;

/// All available TUI commands with descriptions.
pub const COMMANDS: &[(&str, &str)] = &[
    ("/up", "Start stack in dependency order"),
    ("/down", "Stop all services"),
    ("/status", "Force refresh status"),
    ("/logs", "Filter logs by service (or clear)"),
    ("/cleanup", "Free zombie processes / ports"),
    ("/preset", "Switch preset (minimal/full/headless)"),
    ("/health", "Run health checks"),
    ("/config", "Show current configuration"),
    ("/help", "Show help overlay"),
    ("/quit", "Exit TUI"),
    ("/q", "Exit TUI (short)"),
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum InputMode {
    Normal,
    Insert,
}

#[derive(Debug, Clone)]
pub struct ServiceUpdate {
    pub name: String,
    pub icon: String,
    pub health: HealthStatus,
    pub pid: Option<u32>,
    pub port: Option<u16>,
    pub restart_count: u32,
    pub uptime_secs: u64,
}

#[derive(Debug, Clone)]
pub struct ServiceRow {
    pub name: String,
    pub icon: String,
    pub status_text: String,
    pub status_color: (u8, u8, u8),
    pub pid: String,
    pub port: String,
    pub health: String,
    pub health_color: (u8, u8, u8),
    pub last_error: String,
    pub restart_count: u32,
    pub uptime_secs: u64,
    pub is_booting: bool,
}

#[derive(Debug, Clone)]
pub struct LogEntry {
    pub timestamp: String,
    pub level: LogLevel,
    pub service_icon: String,
    pub service_name: String,
    pub message: String,
}

#[derive(Debug, Clone)]
pub struct SakuraPetal {
    pub x: u16,
    pub y: f32,
    pub speed: f32,
    pub char: String,
}

pub struct App {
    pub services: HashMap<String, ServiceRow>,
    pub log_buffer: VecDeque<LogEntry>,
    pub input_mode: InputMode,
    pub input: String,
    pub cursor_pos: usize,
    pub auto_scroll: bool,
    pub log_scroll_offset: usize,
    pub uptime: std::time::Instant,
    pub preset: String,
    pub auto_heal: bool,
    pub version: String,
    pub moon_phase: usize,
    pub booting: bool,
    pub petals: Vec<SakuraPetal>,
    pub show_help: bool,
    pub dialog: Option<Dialog>,
    pub log_filter: Option<String>,
    pub max_log_lines: usize,
    pub should_exit: bool,
    pub event_rx: mpsc::UnboundedReceiver<ManagerEvent>,
    pub event_tx: mpsc::UnboundedSender<ManagerEvent>,
    pub selected_service: Option<String>,
    pub services_order: Vec<String>,
    pub is_starting_stack: bool,
    pub i18n: crate::i18n::I18n,
    pub use_emoji: bool,
    pub table_state: ratatui::widgets::TableState,
    pub manager_return_tx: Option<tokio::sync::mpsc::Sender<crate::services::manager::ServiceManager>>,
    /// Filtered command suggestions based on current input
    pub command_suggestions: Vec<(String, String)>,
    /// Currently highlighted suggestion index
    pub suggestion_index: Option<usize>,
    /// Whether to show the suggestion overlay
    pub show_suggestions: bool,
}

#[derive(Debug, Clone)]
pub enum DialogAction {
    KillPort(u16),
    CleanupAllVerified,
    Info,
}

#[derive(Debug, Clone)]
pub struct Dialog {
    pub title: String,
    pub message: String,
    pub options: Vec<String>,
    pub selected: usize,
    pub target_port: Option<u16>,
    pub dialog_action: DialogAction,
}

impl App {
    pub fn new(
        event_rx: mpsc::UnboundedReceiver<ManagerEvent>,
        event_tx: mpsc::UnboundedSender<ManagerEvent>,
        preset: String,
        auto_heal: bool,
        max_log_lines: usize,
        use_emoji: bool,
    ) -> Self {
        let mut petals = Vec::new();
        for i in 0..8 {
            petals.push(SakuraPetal {
                x: 50 + (i * 7) % 30,
                y: (i * 3) as f32,
                speed: 0.3 + (i as f32 * 0.1),
                char: match i % 3 {
                    0 => "\u{1F338}".to_string(), // 🌸
                    1 => "\u{2740}".to_string(),  // ❀
                    _ => ".".to_string(),
                },
            });
        }

        Self {
            services: HashMap::new(),
            log_buffer: VecDeque::with_capacity(max_log_lines),
            input_mode: InputMode::Normal,
            input: String::new(),
            cursor_pos: 0,
            auto_scroll: true,
            log_scroll_offset: 0,
            uptime: std::time::Instant::now(),
            preset,
            auto_heal,
            version: env!("CARGO_PKG_VERSION").to_string(),
            moon_phase: 0,
            booting: false,
            petals,
            show_help: false,
            dialog: None,
            log_filter: None,
            max_log_lines,
            should_exit: false,
            event_rx,
            event_tx,
            selected_service: None,
            services_order: Vec::new(),
            is_starting_stack: false,
            i18n: crate::i18n::I18n::new("en"),
            use_emoji,
            table_state: ratatui::widgets::TableState::default(),
            manager_return_tx: None,
            command_suggestions: Vec::new(),
            suggestion_index: None,
            show_suggestions: false,
        }
    }

    pub fn push_log(&mut self, name: String, icon: String, line: String, level: LogLevel) {
        let ts = chrono::Local::now().format("%H:%M:%S").to_string();
        self.log_buffer.push_back(LogEntry {
            timestamp: ts,
            level,
            service_icon: icon,
            service_name: name,
            message: line,
        });
        while self.log_buffer.len() > self.max_log_lines {
            self.log_buffer.pop_front();
        }
    }

    pub fn update_service(
        &mut self,
        name: String,
        icon: String,
        health: HealthStatus,
        pid: Option<u32>,
        port: Option<u16>,
    ) {
        let update = ServiceUpdate {
            name,
            icon,
            health,
            pid,
            port,
            restart_count: 0,
            uptime_secs: 0,
        };
        self.apply_service_update(update);
    }

    pub fn update_service_with(&mut self, update: ServiceUpdate) {
        self.apply_service_update(update);
    }

    fn apply_service_update(&mut self, update: ServiceUpdate) {
        let (status_text, status_color) = match &update.health {
            HealthStatus::Healthy => ("Run".to_string(), (74, 222, 128)),
            HealthStatus::Booting => ("Boot".to_string(), (251, 191, 36)),
            HealthStatus::Stopped => ("Stop".to_string(), (156, 163, 175)),
            HealthStatus::Unknown => ("?".to_string(), (156, 163, 175)),
            HealthStatus::Unhealthy(_) => ("Dead".to_string(), (248, 113, 113)),
        };

        let (health_str, last_error) = match &update.health {
            HealthStatus::Healthy => ("OK".to_string(), String::new()),
            HealthStatus::Unhealthy(msg) => (format!("FAIL: {}", msg), msg.clone()),
            HealthStatus::Booting => ("-".to_string(), String::new()),
            HealthStatus::Stopped => ("-".to_string(), String::new()),
            HealthStatus::Unknown => ("?".to_string(), String::new()),
        };

        let health_color = match &update.health {
            HealthStatus::Healthy => (74, 222, 128),
            HealthStatus::Booting => (251, 191, 36),
            HealthStatus::Stopped => (156, 163, 175),
            HealthStatus::Unknown => (156, 163, 175),
            HealthStatus::Unhealthy(_) => (248, 113, 113),
        };

        let is_booting = matches!(&update.health, HealthStatus::Booting);
        let row = ServiceRow {
            name: update.name.clone(),
            icon: update.icon,
            status_text,
            status_color,
            pid: update
                .pid
                .map(|p| p.to_string())
                .unwrap_or_else(|| "-".to_string()),
            port: update
                .port
                .map(|p| p.to_string())
                .unwrap_or_else(|| "-".to_string()),
            health: health_str,
            health_color,
            last_error,
            restart_count: update.restart_count,
            uptime_secs: update.uptime_secs,
            is_booting,
        };

        self.services.insert(update.name.clone(), row);
        if !self.services_order.contains(&update.name) {
            self.services_order.push(update.name.clone());
        }
        self.booting = self.services.values().any(|r| r.is_booting);
    }

    pub fn uptime_str(&self) -> String {
        let elapsed = self.uptime.elapsed();
        let hours = elapsed.as_secs() / 3600;
        let mins = (elapsed.as_secs() % 3600) / 60;
        let secs = elapsed.as_secs() % 60;
        format!("{:02}:{:02}:{:02}", hours, mins, secs)
    }

    pub fn moon_emoji(&self) -> &'static str {
        const PHASES: &[&str] = &[
            "\u{1F311}",
            "\u{1F312}",
            "\u{1F313}",
            "\u{1F314}",
            "\u{1F315}",
        ];
        PHASES[self.moon_phase % PHASES.len()]
    }

    pub fn tick(&mut self) {
        if self.booting {
            self.moon_phase = (self.moon_phase + 1) % 5;
        }
        // Update petals
        for p in &mut self.petals {
            p.y += p.speed;
            if p.y > 100.0 {
                p.y = 0.0;
                p.x = 40 + (p.x as usize * 7) as u16 % 35;
            }
        }
    }

    pub fn filtered_logs(&self) -> Vec<&LogEntry> {
        if let Some(filter) = &self.log_filter {
            self.log_buffer
                .iter()
                .filter(|l| l.service_name == *filter)
                .collect()
        } else {
            self.log_buffer.iter().collect()
        }
    }

    /// Update command suggestions based on current input text.
    pub fn update_suggestions(&mut self) {
        let input = self.input.trim().to_lowercase();
        if input.is_empty() || !input.starts_with('/') {
            self.command_suggestions.clear();
            self.show_suggestions = false;
            self.suggestion_index = None;
            return;
        }
        self.command_suggestions = COMMANDS
            .iter()
            .filter(|(cmd, _)| cmd.starts_with(&input) || cmd.contains(&input[1..]))
            .map(|(cmd, desc)| (cmd.to_string(), desc.to_string()))
            .collect();
        self.show_suggestions = !self.command_suggestions.is_empty();
        // Reset index if out of bounds
        if let Some(idx) = self.suggestion_index {
            if idx >= self.command_suggestions.len() {
                self.suggestion_index = if self.command_suggestions.is_empty() {
                    None
                } else {
                    Some(0)
                };
            }
        }
    }

    /// Accept the currently highlighted suggestion into the input buffer.
    pub fn accept_suggestion(&mut self) {
        if let Some(idx) = self.suggestion_index {
            if let Some((cmd, _)) = self.command_suggestions.get(idx) {
                self.input = format!("{} ", cmd);
                self.cursor_pos = self.input.len();
            }
        }
        self.command_suggestions.clear();
        self.show_suggestions = false;
        self.suggestion_index = None;
    }

    /// Find the closest matching command for typo correction.
    pub fn closest_command(input: &str) -> Option<&'static str> {
        let input_lower = input.to_lowercase();
        let mut best: Option<(&str, usize)> = None;
        for (cmd, _) in COMMANDS {
            let dist = levenshtein(&input_lower, cmd);
            if dist <= 3 {
                if best.map(|(_, d)| dist < d).unwrap_or(true) {
                    best = Some((cmd, dist));
                }
            }
        }
        best.map(|(cmd, _)| cmd)
    }
}

/// Simple Levenshtein distance calculation.
fn levenshtein(a: &str, b: &str) -> usize {
    let a: Vec<char> = a.chars().collect();
    let b: Vec<char> = b.chars().collect();
    let m = a.len();
    let n = b.len();
    let mut dp = vec![vec![0usize; n + 1]; m + 1];
    for i in 0..=m {
        dp[i][0] = i;
    }
    for j in 0..=n {
        dp[0][j] = j;
    }
    for i in 1..=m {
        for j in 1..=n {
            let cost = if a[i - 1] == b[j - 1] { 0 } else { 1 };
            dp[i][j] = (dp[i - 1][j] + 1)
                .min(dp[i][j - 1] + 1)
                .min(dp[i - 1][j - 1] + cost);
        }
    }
    dp[m][n]
}
