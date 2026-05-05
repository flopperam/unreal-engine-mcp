use async_trait::async_trait;
use std::process::Stdio;
use tokio::process::{Child, Command};

use crate::config::{PathConfig, ServiceConfig};
use crate::error::Result;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HealthStatus {
    Unknown,
    Healthy,
    Unhealthy(String),
    Booting,
    Stopped,
}

impl HealthStatus {
    pub fn is_healthy(&self) -> bool {
        matches!(self, HealthStatus::Healthy)
    }
    pub fn is_booting(&self) -> bool {
        matches!(self, HealthStatus::Booting)
    }
}

#[derive(Debug, Clone)]
pub struct ServiceContext {
    pub repo_root: std::path::PathBuf,
    pub config: ServiceConfig,
    pub paths: PathConfig,
    pub preset: String,
    pub capture_output: bool,
    pub kill_on_drop: bool,
}

#[derive(Debug)]
pub struct ServiceHandle {
    pub name: String,
    pub child: Option<Child>,
    pub pid: Option<u32>,
    pub health: HealthStatus,
    pub log_lines: Vec<String>,
    pub start_time: Option<std::time::Instant>,
    pub restart_count: u32,
    pub config: ServiceConfig,
    pub repo_root: std::path::PathBuf,
}

#[async_trait]
pub trait Service: Send + Sync {
    fn name(&self) -> &str;
    fn icon(&self) -> &'static str;

    async fn start(&self, ctx: &ServiceContext) -> Result<Child>;

    async fn stop(&self, handle: &mut ServiceHandle) -> Result<()>;

    async fn health_check(
        &self,
        handle: &ServiceHandle,
        client: &reqwest::Client,
    ) -> HealthStatus;
}

pub fn build_command(
    cmd: &str,
    args: &[String],
    cwd: Option<&str>,
    capture_output: bool,
    kill_on_drop: bool,
) -> Command {
    let mut command = Command::new(cmd);
    command.args(args);
    if capture_output {
        command.stdout(Stdio::piped()).stderr(Stdio::piped());
    } else {
        command.stdout(Stdio::null()).stderr(Stdio::null());
    }
    command.kill_on_drop(kill_on_drop);
    if let Some(dir) = cwd {
        command.current_dir(dir);
    }
    command
}
