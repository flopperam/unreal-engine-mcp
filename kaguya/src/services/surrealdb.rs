use async_trait::async_trait;
use std::path::PathBuf;
use tokio::process::Child;
use tracing::info;

use crate::error::{KaguyaError, Result};
use crate::services::service::{
    build_command, HealthStatus, Service, ServiceContext, ServiceHandle,
};

pub struct SurrealDbService;

#[async_trait]
impl Service for SurrealDbService {
    fn name(&self) -> &str {
        "surrealdb"
    }

    fn icon(&self) -> &'static str {
        "\u{1F38B}"
    }


    async fn start(&self, ctx: &ServiceContext) -> Result<Child> {
        let cmd = resolve_surreal_command(ctx);
        let args = if ctx.config.args.is_empty() {
            vec![
                "start".to_string(),
                "--bind".to_string(),
                "127.0.0.1:8000".to_string(),
                "--user".to_string(),
                "root".to_string(),
                "--pass".to_string(),
                "secret".to_string(),
                "rocksdb://./.surreal/unreal_mcp.db".to_string(),
            ]
        } else {
            ctx.config.args.clone()
        };

        let cwd = ctx
            .config
            .working_dir
            .as_ref()
            .map(|dir| ctx.repo_root.join(dir))
            .unwrap_or_else(|| ctx.repo_root.clone());

        info!("Starting SurrealDB: {} {:?} in {:?}", cmd, args, cwd);
        let command_cwd = cwd.to_string_lossy();
        let mut command = build_command(
            &cmd,
            &args,
            Some(&command_cwd),
            ctx.capture_output,
            ctx.kill_on_drop,
        );

        let child = command.spawn().map_err(|e| KaguyaError::ServiceStart {
            name: self.name().to_string(),
            reason: format!("Failed to spawn: {}", e),
        })?;

        Ok(child)
    }

    async fn stop(&self, handle: &mut ServiceHandle) -> Result<()> {
        info!("Stopping SurrealDB (PID {:?})", handle.pid);
        if let Some(ref mut child) = handle.child {
            let _ = child.start_kill();
            let _ = tokio::time::timeout(tokio::time::Duration::from_secs(5), child.wait()).await;
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

        let url = handle
            .config
            .health_url
            .clone()
            .unwrap_or_else(|| "http://127.0.0.1:8000/health".to_string());
        let kind = crate::health::runner::HealthCheckKind::Http { url };
        let timeout = std::time::Duration::from_secs(handle.config.health_timeout_secs);
        crate::health::runner::run_health_check(&kind, timeout, client, handle).await
    }
}

fn resolve_surreal_command(ctx: &ServiceContext) -> String {
    if let Some(command) = &ctx.config.command {
        let configured = PathBuf::from(command);
        if configured.is_absolute() && configured.exists() {
            return command.clone();
        }
    }

    for candidate in &ctx.paths.surrealdb_search {
        let path = PathBuf::from(candidate);
        let path = if path.is_absolute() {
            path
        } else {
            ctx.repo_root.join(path)
        };
        if path.exists() {
            return path.to_string_lossy().to_string();
        }
    }

    ctx.config
        .command
        .clone()
        .unwrap_or_else(|| "surreal".to_string())
}
