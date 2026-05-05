use async_trait::async_trait;
use std::path::PathBuf;
use std::time::Duration;
use tokio::process::Child;
use tracing::info;

use crate::config::PathConfig;
use crate::error::{KaguyaError, Result};
use crate::services::service::{
    build_command, HealthStatus, Service, ServiceContext, ServiceHandle,
};

pub struct UnrealService;

#[async_trait]
impl Service for UnrealService {
    fn name(&self) -> &str {
        "unreal"
    }

    fn icon(&self) -> &'static str {
        "\u{1F3F0}"
    }


    async fn start(&self, ctx: &ServiceContext) -> Result<Child> {
        if crate::platform::is_wsl() {
            return start_unreal_wsl(ctx).await;
        }

        let ue_root = resolve_unreal_engine_root(&ctx.paths).await?;
        let editor_exe = if cfg!(windows) {
            ue_root.join("Engine/Binaries/Win64/UnrealEditor.exe")
        } else {
            ue_root.join("Engine/Binaries/Mac/UnrealEditor")
        };

        let project_path = ctx
            .config
            .project_path
            .as_ref()
            .map(|p| ctx.repo_root.join(p))
            .unwrap_or_else(|| {
                ctx.repo_root
                    .join("FlopperamUnrealMCP/FlopperamUnrealMCP.uproject")
            });

        if !editor_exe.exists() {
            return Err(KaguyaError::ServiceStart {
                name: self.name().to_string(),
                reason: format!("Editor not found at {}", editor_exe.display()),
            });
        }
        if !project_path.exists() {
            return Err(KaguyaError::ServiceStart {
                name: self.name().to_string(),
                reason: format!(".uproject not found at {}", project_path.display()),
            });
        }

        let mut args = vec![
            project_path.to_string_lossy().to_string(),
            "-Windowed".to_string(),
            "-ResX=1280".to_string(),
            "-ResY=720".to_string(),
        ];

        if ctx.preset == "headless" {
            for arg in &ctx.config.headless_args {
                args.push(arg.clone());
            }
        }

        info!(
            "Starting Unreal Editor: {} {:?}",
            editor_exe.display(),
            args
        );
        let mut command = build_command(
            &editor_exe.to_string_lossy(),
            &args,
            Some(&ctx.repo_root.to_string_lossy()),
            ctx.capture_output,
            ctx.kill_on_drop,
        );

        // Unreal needs a window, so no CREATE_NO_WINDOW equivalent needed on tokio
        let child = command.spawn().map_err(|e| KaguyaError::ServiceStart {
            name: self.name().to_string(),
            reason: format!("Failed to spawn: {}", e),
        })?;

        Ok(child)
    }

    async fn stop(&self, handle: &mut ServiceHandle) -> Result<()> {
        info!("Stopping Unreal (PID {:?})", handle.pid);
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

        let port = handle.config.port.unwrap_or(55557);
        let kind = crate::health::runner::HealthCheckKind::Tcp {
            host: "127.0.0.1".to_string(),
            port,
        };
        let timeout = std::time::Duration::from_secs(handle.config.health_timeout_secs);
        crate::health::runner::run_health_check(&kind, timeout, client, handle).await
    }
}

async fn resolve_unreal_engine_root(paths: &PathConfig) -> Result<PathBuf> {
    // 1. Environment variable
    let env_key = paths.env_key.as_deref().unwrap_or("UNREAL_ENGINE_ROOT");
    if let Ok(env_root) = std::env::var(env_key) {
        let p = PathBuf::from(env_root);
        if p.exists() {
            return Ok(p);
        }
    }

    // 2. Windows registry (async-safe: spawn_blocking)
    #[cfg(windows)]
    {
        let registry_keys = if paths.registry_keys.is_empty() {
            vec![
                "SOFTWARE\\EpicGames\\Unreal Engine\\5.7".to_string(),
                "SOFTWARE\\EpicGames\\Unreal Engine\\5.5".to_string(),
                "SOFTWARE\\EpicGames\\Unreal Engine\\5.4".to_string(),
                "SOFTWARE\\EpicGames\\Unreal Engine\\5.3".to_string(),
            ]
        } else {
            paths.registry_keys.clone()
        };
        let result = tokio::task::spawn_blocking(move || {
            use winreg::enums::HKEY_LOCAL_MACHINE;
            use winreg::RegKey;
            for path in registry_keys {
                if let Ok(key) = RegKey::predef(HKEY_LOCAL_MACHINE).open_subkey(&path) {
                    if let Ok(value) = key.get_value::<String, _>("InstalledDirectory") {
                        let p = PathBuf::from(value);
                        if p.exists() {
                            return Some(p);
                        }
                    }
                }
            }
            None
        })
        .await
        .unwrap_or(None);
        if let Some(p) = result {
            return Ok(p);
        }
    }

    // 3. Common paths
    let common_paths = if paths.common_paths.is_empty() {
        vec![
            r"C:\Program Files\Epic Games\UE_5.7".to_string(),
            r"C:\Program Files\Epic Games\UE_5.5".to_string(),
            r"C:\Program Files\Epic Games\UE_5.4".to_string(),
        ]
    } else {
        paths.common_paths.clone()
    };
    for candidate in common_paths {
        let p = PathBuf::from(candidate);
        if p.exists() {
            return Ok(p);
        }
    }

    // 4. PATH
    let finder = if cfg!(windows) { "where" } else { "which" };
    let editor_name = if cfg!(windows) {
        "UnrealEditor.exe"
    } else {
        "UnrealEditor"
    };
    if let Ok(output) = tokio::process::Command::new(finder)
        .arg(editor_name)
        .output()
        .await
    {
        if output.status.success() {
            let line = String::from_utf8_lossy(&output.stdout);
            if let Some(first) = line.lines().next() {
                let p = PathBuf::from(first.trim());
                // UnrealEditor.exe is at <root>/Engine/Binaries/<platform>/UnrealEditor.exe.
                if let Some(root) = p
                    .parent()
                    .and_then(|p| p.parent())
                    .and_then(|p| p.parent())
                    .and_then(|p| p.parent())
                {
                    return Ok(root.to_path_buf());
                }
            }
        }
    }

    Err(KaguyaError::ServiceStart {
        name: "unreal".to_string(),
        reason: "Unreal Engine not found. Set UNREAL_ENGINE_ROOT or install UE 5.7+".to_string(),
    })
}

/// Launch Unreal Editor from within WSL2 via Windows cmd.exe
async fn start_unreal_wsl(ctx: &ServiceContext) -> Result<Child> {
    let ue_root = resolve_unreal_engine_root(&ctx.paths).await?;

    // Convert WSL paths to Windows paths for cmd.exe
    let editor_wsl = ue_root.join("Engine/Binaries/Win64/UnrealEditor.exe");
    let editor_win = crate::platform::wsl_to_windows_path(&editor_wsl)
        .ok_or_else(|| KaguyaError::ServiceStart {
            name: "unreal".to_string(),
            reason: format!("Cannot convert editor path to Windows: {}", editor_wsl.display()),
        })?;

    let project_wsl = ctx
        .config
        .project_path
        .as_ref()
        .map(|p| ctx.repo_root.join(p))
        .unwrap_or_else(|| {
            ctx.repo_root
                .join("FlopperamUnrealMCP/FlopperamUnrealMCP.uproject")
        });
    let project_win = crate::platform::wsl_to_windows_path(&project_wsl)
        .ok_or_else(|| KaguyaError::ServiceStart {
            name: "unreal".to_string(),
            reason: format!("Cannot convert project path to Windows: {}", project_wsl.display()),
        })?;

    let mut args = vec![
        project_win.to_string_lossy().to_string(),
        "-Windowed".to_string(),
        "-ResX=1280".to_string(),
        "-ResY=720".to_string(),
    ];

    if ctx.preset == "headless" {
        for arg in &ctx.config.headless_args {
            args.push(arg.clone());
        }
    }

    // Launch via cmd.exe which starts Unreal detached; cmd.exe exits immediately
    let mut cmd_args = vec![
        "/C".to_string(),
        "start".to_string(),
        "".to_string(),  // window title placeholder
        format!("\"{}\"", editor_win.to_string_lossy()),
    ];
    for arg in &args {
        cmd_args.push(format!("\"{}\"", arg));
    }

    info!("Starting Unreal via WSL: cmd.exe {:?}", cmd_args);
    let mut command = build_command(
        "cmd.exe",
        &cmd_args,
        None,
        ctx.capture_output,
        ctx.kill_on_drop,
    );

    let child = command.spawn().map_err(|e| KaguyaError::ServiceStart {
        name: "unreal".to_string(),
        reason: format!("Failed to spawn via WSL: {}", e),
    })?;

    Ok(child)
}
