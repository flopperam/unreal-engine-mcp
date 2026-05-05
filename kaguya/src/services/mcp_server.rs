use async_trait::async_trait;
use std::time::Duration;
use tokio::process::Child;
use tracing::info;

use crate::error::{KaguyaError, Result};
use crate::services::service::{
    build_command, HealthStatus, Service, ServiceContext, ServiceHandle,
};

pub struct McpServerService;

#[async_trait]
impl Service for McpServerService {
    fn name(&self) -> &str {
        "mcp-server"
    }

    fn icon(&self) -> &'static str {
        "\u{1F40D}"
    }


    async fn start(&self, ctx: &ServiceContext) -> Result<Child> {
        let python_root = ctx.repo_root.join("Python");
        let entry = python_root.join("unreal_mcp_server_advanced.py");
        if !entry.exists() {
            return Err(KaguyaError::ServiceStart {
                name: self.name().to_string(),
                reason: format!("Entry point not found: {}", entry.display()),
            });
        }

        // Prefer uv, fallback to python
        let (cmd, args) = if uv_available().await {
            (
                "uv".to_string(),
                vec![
                    "run".to_string(),
                    "unreal_mcp_server_advanced.py".to_string(),
                ],
            )
        } else {
            (
                "python".to_string(),
                vec![entry.to_string_lossy().to_string()],
            )
        };

        info!("Starting MCP server: {} {:?}", cmd, args);
        let mut command = build_command(
            &cmd,
            &args,
            Some(&python_root.to_string_lossy()),
            ctx.capture_output,
            ctx.kill_on_drop,
        );
        command.stdin(std::process::Stdio::piped()); // stdio transport needs stdin open

        let child = command.spawn().map_err(|e| KaguyaError::ServiceStart {
            name: self.name().to_string(),
            reason: format!("Failed to spawn: {}", e),
        })?;

        Ok(child)
    }

    async fn stop(&self, handle: &mut ServiceHandle) -> Result<()> {
        info!("Stopping MCP server (PID {:?})", handle.pid);
        if let Some(ref mut child) = handle.child {
            let _ = child.start_kill();
            let _ = tokio::time::timeout(Duration::from_secs(5), child.wait()).await;
        }
        Ok(())
    }

    async fn health_check(
        &self,
        handle: &ServiceHandle,
        client: &reqwest::Client,
    ) -> HealthStatus {
        if handle.pid.is_none() {
            return HealthStatus::Stopped;
        }

        let mut checks = vec![crate::health::runner::HealthCheckKind::ProcessAlive];
        if let Some(log) = &handle.config.log_file {
            checks.push(crate::health::runner::HealthCheckKind::LogFresh {
                path: std::path::PathBuf::from(log),
                max_age_secs: 60,
            });
        }
        let kind = crate::health::runner::HealthCheckKind::Composite(checks);
        let timeout = std::time::Duration::from_secs(handle.config.health_timeout_secs);
        crate::health::runner::run_health_check(&kind, timeout, client, handle).await
    }
}

async fn uv_available() -> bool {
    tokio::process::Command::new("uv")
        .arg("--version")
        .output()
        .await
        .map(|o| o.status.success())
        .unwrap_or(false)
}
