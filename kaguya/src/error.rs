use std::io;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum KaguyaError {
    #[error("IO error: {0}")]
    Io(#[from] io::Error),

    #[error("Configuration error: {0}")]
    Config(String),

    #[error("Service '{name}' failed to start: {reason}")]
    ServiceStart { name: String, reason: String },

    #[error("Service '{name}' health check failed: {reason}")]
    HealthCheck { name: String, reason: String },

    #[error("Service '{name}' is already running (PID {pid})")]
    AlreadyRunning { name: String, pid: u32 },

    #[error("Port {port} is already in use by PID {pid} ({exe})")]
    PortInUse { port: u16, pid: u32, exe: String },

    #[error("Process not found: {0}")]
    ProcessNotFound(String),

    #[error("TUI error: {0}")]
    Tui(String),

    #[error("Command error: {0}")]
    Command(String),

    #[error("{0}")]
    Generic(String),
}

pub type Result<T> = std::result::Result<T, KaguyaError>;
