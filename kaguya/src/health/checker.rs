use std::net::TcpListener;
use sysinfo::{ProcessRefreshKind, ProcessesToUpdate, RefreshKind, System};
use tracing::{info, warn};

use crate::error::Result;

#[derive(Debug, Clone)]
pub struct PortOccupant {
    pub port: u16,
    pub pid: u32,
    pub name: String,
}

pub fn check_port_in_use(port: u16) -> Option<PortOccupant> {
    // Try to bind; if it fails, something is using it
    if TcpListener::bind(("127.0.0.1", port)).is_ok() {
        return None;
    }
    find_process_by_port(port)
}

pub fn find_process_by_port(port: u16) -> Option<PortOccupant> {
    #[cfg(windows)]
    {
        use std::process::Command;
        let output = Command::new("netstat").args(["-ano"]).output().ok()?;

        if output.status.success() {
            let text = String::from_utf8_lossy(&output.stdout);
            let target = format!(":{}", port);
            for line in text.lines() {
                let parts: Vec<&str> = line.split_whitespace().collect();
                if parts.len() >= 5 {
                    if let Some(addr) = parts.get(1) {
                        if addr.ends_with(&target) {
                            if let Ok(pid_val) = parts.last().unwrap_or(&"0").parse::<u32>() {
                                if pid_val == 0 {
                                    continue;
                                }
                                // Now get name from sysinfo
                                let s = System::new_with_specifics(
                                    RefreshKind::nothing()
                                        .with_processes(ProcessRefreshKind::everything()),
                                );
                                let name = s
                                    .process(sysinfo::Pid::from_u32(pid_val))
                                    .map(|p| p.name().to_string_lossy().to_string())
                                    .unwrap_or_else(|| "Unknown".to_string());

                                return Some(PortOccupant {
                                    port,
                                    pid: pid_val,
                                    name,
                                });
                            }
                        }
                    }
                }
            }
        }
    }

    // Heuristic fallback or non-Windows
    let s = System::new_with_specifics(
        RefreshKind::nothing().with_processes(ProcessRefreshKind::everything()),
    );

    for (pid, process) in s.processes() {
        let name = process.name().to_string_lossy().to_string();
        if name.contains("surreal") || name.contains("scene-syncd") || name.contains("UnrealEditor")
        {
            return Some(PortOccupant {
                port,
                pid: pid.as_u32(),
                name,
            });
        }
    }

    None
}

/// Cleanup zombie processes (processes for known services that no longer have a parent)
pub fn kill_zombies(target_pids: &[u32]) -> Vec<u32> {
    let mut s = System::new_with_specifics(
        RefreshKind::nothing().with_processes(ProcessRefreshKind::everything()),
    );
    s.refresh_processes(ProcessesToUpdate::All, true);

    let mut killed = Vec::new();
    for pid in target_pids {
        if let Some(process) = s.process(sysinfo::Pid::from_u32(*pid)) {
            info!(
                "Killing zombie process PID {} ({})",
                pid,
                process.name().to_string_lossy()
            );
            let _ = process.kill();
            killed.push(*pid);
        }
    }
    killed
}

/// Force-free a port by killing the owning process
pub fn free_port(port: u16) -> Result<bool> {
    if let Some(occupant) = check_port_in_use(port) {
        let s = System::new_with_specifics(
            RefreshKind::nothing().with_processes(ProcessRefreshKind::everything()),
        );
        if let Some(process) = s.process(sysinfo::Pid::from_u32(occupant.pid)) {
            warn!(
                "Killing process {} (PID {}) to free port {}",
                process.name().to_string_lossy(),
                occupant.pid,
                port
            );
            let _ = process.kill();
            return Ok(true);
        }
    }
    Ok(false)
}

/// Parse netstat output on Windows to find PID by port (Phase 3 enhancement)
pub async fn find_pid_by_port_windows(port: u16) -> Option<u32> {
    use tokio::process::Command;

    let output = Command::new("netstat").args(["-ano"]).output().await.ok()?;

    if !output.status.success() {
        return None;
    }

    let text = String::from_utf8_lossy(&output.stdout);
    for line in text.lines() {
        // Format: Proto  Local Address          Foreign Address        State           PID
        // TCP    0.0.0.0:8000           0.0.0.0:0              LISTENING       1234
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() >= 5 {
            if let Some(addr) = parts.get(1) {
                if addr.ends_with(&format!(":{}", port)) {
                    if let Ok(pid) = parts.last().unwrap_or(&"0").parse::<u32>() {
                        return Some(pid);
                    }
                }
            }
        }
    }
    None
}
