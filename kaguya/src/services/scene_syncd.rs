use async_trait::async_trait;
use std::path::PathBuf;
use tokio::process::Child;
use tracing::{info, warn};

use crate::error::{KaguyaError, Result};
use crate::services::service::{
    build_command, HealthStatus, Service, ServiceContext, ServiceHandle,
};

pub struct SceneSyncdService;

#[async_trait]
impl Service for SceneSyncdService {
    fn name(&self) -> &str {
        "scene-syncd"
    }

    fn icon(&self) -> &'static str {
        "\u{1F338}"
    }


    async fn start(&self, ctx: &ServiceContext) -> Result<Child> {
        // Prefer prebuilt binary, fallback to cargo run
        let default_binary = if cfg!(windows) {
            "rust/scene-syncd/target/debug/scene-syncd.exe"
        } else {
            "rust/scene-syncd/target/debug/scene-syncd"
        };
        let binary_path = ctx
            .paths
            .scene_syncd_binary
            .as_deref()
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from(default_binary));
        let binary_path = if binary_path.is_absolute() {
            binary_path
        } else {
            ctx.repo_root.join(binary_path)
        };
        let default_cwd = ctx
            .config
            .working_dir
            .as_ref()
            .map(|dir| ctx.repo_root.join(dir))
            .unwrap_or_else(|| ctx.repo_root.join("rust/scene-syncd"));

        let (cmd, args, cwd): (String, Vec<String>, Option<PathBuf>) = if binary_path.exists() {
            (
                binary_path.to_string_lossy().to_string(),
                Vec::new(),
                Some(default_cwd),
            )
        } else {
            warn!("scene-syncd binary not found, falling back to cargo run");
            (
                ctx.config
                    .command
                    .clone()
                    .unwrap_or_else(|| "cargo".to_string()),
                if ctx.config.args.is_empty() {
                    vec!["run".to_string()]
                } else {
                    ctx.config.args.clone()
                },
                Some(default_cwd),
            )
        };

        info!("Starting scene-syncd: {} {:?} in {:?}", cmd, args, cwd);
        let mut command = build_command(
            &cmd,
            &args,
            cwd.as_ref().map(|p| p.to_str().unwrap_or(".")),
            ctx.capture_output,
            ctx.kill_on_drop,
        );

        // Set environment variables
        let surreal_bind = ctx
            .config
            .health_url
            .as_ref()
            .and_then(|u| u.split("//").nth(1))
            .map(|h| h.split('/').next().unwrap_or("127.0.0.1:8000").to_string())
            .unwrap_or_else(|| "127.0.0.1:8000".to_string());

        command.env("SCENE_SYNCD_HOST", "127.0.0.1");
        command.env("SCENE_SYNCD_PORT", "8787");
        command.env("SURREAL_URL", format!("ws://{}", surreal_bind));
        command.env("SURREAL_NS", "unreal_mcp");
        command.env("SURREAL_DB", "scene");
        command.env("SURREAL_USER", "root");
        command.env("SURREAL_PASS", "secret");
        command.env("UNREAL_MCP_HOST", "127.0.0.1");
        command.env("UNREAL_MCP_PORT", "55557");
        command.env("SCENE_SYNCD_AUTOSYNC", "false");
        command.env("SCENE_SYNCD_LOG", "info");

        let child = command.spawn().map_err(|e| KaguyaError::ServiceStart {
            name: self.name().to_string(),
            reason: format!("Failed to spawn: {}", e),
        })?;

        Ok(child)
    }

    async fn stop(&self, handle: &mut ServiceHandle) -> Result<()> {
        info!("Stopping scene-syncd (PID {:?})", handle.pid);
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
            .unwrap_or_else(|| "http://127.0.0.1:8787/health".to_string());
        let kind = crate::health::runner::HealthCheckKind::Http { url };
        let timeout = std::time::Duration::from_secs(handle.config.health_timeout_secs);
        crate::health::runner::run_health_check(&kind, timeout, client, handle).await
    }
}
