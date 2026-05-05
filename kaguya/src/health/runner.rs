use std::path::PathBuf;
use std::time::{Duration, SystemTime};

use crate::services::service::{HealthStatus, ServiceHandle};

#[derive(Debug, Clone)]
pub enum HealthCheckKind {
    Http { url: String },
    Tcp { host: String, port: u16 },
    ProcessAlive,
    LogFresh { path: PathBuf, max_age_secs: u64 },
    Composite(Vec<HealthCheckKind>),
}

pub async fn run_health_check(
    kind: &HealthCheckKind,
    timeout: Duration,
    client: &reqwest::Client,
    handle: &ServiceHandle,
) -> HealthStatus {
    match kind {
        HealthCheckKind::Http { url } => {
            match tokio::time::timeout(timeout, client.get(url).send()).await {
                Ok(Ok(resp)) if resp.status().is_success() => HealthStatus::Healthy,
                Ok(Ok(resp)) => HealthStatus::Unhealthy(format!("HTTP {}", resp.status())),
                Ok(Err(e)) => HealthStatus::Unhealthy(format!("Request failed: {}", e)),
                Err(_) => HealthStatus::Unhealthy("HTTP timeout".to_string()),
            }
        }
        HealthCheckKind::Tcp { host, port } => {
            match tokio::time::timeout(
                timeout,
                tokio::net::TcpStream::connect((host.as_str(), *port)),
            )
            .await
            {
                Ok(Ok(_)) => HealthStatus::Healthy,
                Ok(Err(e)) => HealthStatus::Unhealthy(format!("TCP connect failed: {}", e)),
                Err(_) => HealthStatus::Unhealthy("TCP connect timeout".to_string()),
            }
        }
        HealthCheckKind::ProcessAlive => {
            if let Some(pid) = handle.pid {
                use sysinfo::{ProcessRefreshKind, RefreshKind, System};
                let s = System::new_with_specifics(
                    RefreshKind::nothing().with_processes(ProcessRefreshKind::everything()),
                );
                if s.process(sysinfo::Pid::from_u32(pid)).is_some() {
                    HealthStatus::Healthy
                } else {
                    HealthStatus::Unhealthy("Process not alive".to_string())
                }
            } else {
                HealthStatus::Unhealthy("No PID recorded".to_string())
            }
        }
        HealthCheckKind::LogFresh { path, max_age_secs } => {
            let path = if path.is_absolute() {
                path.clone()
            } else {
                handle.repo_root.join(path)
            };
            if !path.exists() {
                HealthStatus::Unhealthy(format!("Log file not found: {}", path.display()))
            } else if let Ok(meta) = std::fs::metadata(&path) {
                if let Ok(mtime) = meta.modified() {
                    if let Ok(elapsed) = SystemTime::now().duration_since(mtime) {
                        if elapsed < Duration::from_secs(*max_age_secs) {
                            HealthStatus::Healthy
                        } else {
                            HealthStatus::Unhealthy(format!(
                                "Log file stale (>{}s)",
                                max_age_secs
                            ))
                        }
                    } else {
                        HealthStatus::Healthy
                    }
                } else {
                    HealthStatus::Healthy
                }
            } else {
                HealthStatus::Unhealthy(format!("Log file not readable: {}", path.display()))
            }
        }
        HealthCheckKind::Composite(kinds) => {
            for k in kinds {
                let status = Box::pin(run_health_check(k, timeout, client, handle)).await;
                if !status.is_healthy() {
                    return status;
                }
            }
            HealthStatus::Healthy
        }
    }
}
