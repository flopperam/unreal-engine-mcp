use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::error::{KaguyaError, Result};
use crate::services::manager::ServiceManager;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StateFile {
    pub repo_root: String,
    pub preset: String,
    pub updated_at_unix: u64,
    pub session_id: String,
    pub services: Vec<ServiceState>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceState {
    pub name: String,
    pub pid: u32,
    pub port: Option<u16>,
    pub exe_path: Option<String>,
    pub cmdline: Option<String>,
    pub started_at_unix: u64,
    pub session_id: String,
}

impl StateFile {
    pub fn from_manager(manager: &ServiceManager, preset: &str) -> Self {
        use sysinfo::{ProcessRefreshKind, RefreshKind, System};
        let s = System::new_with_specifics(
            RefreshKind::nothing().with_processes(ProcessRefreshKind::everything()),
        );

        let services = manager
            .handles()
            .values()
            .filter_map(|handle| {
                handle.pid.map(|pid| {
                    let (exe, cmd) = if let Some(process) =
                        s.process(sysinfo::Pid::from_u32(pid))
                    {
                        (
                            Some(process.exe().map(|p| p.display().to_string()).unwrap_or_default()),
                            Some(process.cmd().iter().map(|s| s.to_string_lossy()).collect::<Vec<_>>().join(" ")),
                        )
                    } else {
                        (None, None)
                    };
                    ServiceState {
                        name: handle.name.clone(),
                        pid,
                        port: handle.config.port,
                        exe_path: exe,
                        cmdline: cmd,
                        started_at_unix: handle
                            .start_time
                            .map(|t| {
                                let elapsed = t.elapsed().as_secs();
                                SystemTime::now()
                                    .duration_since(UNIX_EPOCH)
                                    .unwrap_or_default()
                                    .as_secs()
                                    .saturating_sub(elapsed)
                            })
                            .unwrap_or_default(),
                        session_id: manager.session_id.clone(),
                    }
                })
            })
            .collect();

        Self {
            repo_root: manager.repo_root().to_string_lossy().to_string(),
            preset: preset.to_string(),
            updated_at_unix: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or_default(),
            session_id: manager.session_id.clone(),
            services,
        }
    }
}

pub fn generate_session_id() -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    let rand = std::process::id();
    format!("kaguya-{}-{}", now, rand)
}

pub fn state_path() -> PathBuf {
    if let Some(proj_dirs) = directories::ProjectDirs::from("", "unreal-mcp", "kaguya") {
        return proj_dirs.data_local_dir().join("state.toml");
    }
    std::env::temp_dir().join("kaguya-state.toml")
}

pub fn save(state: &StateFile) -> Result<PathBuf> {
    let path = state_path();
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let text = toml::to_string_pretty(state)
        .map_err(|e| KaguyaError::Config(format!("Failed to serialize state: {}", e)))?;
    std::fs::write(&path, text)?;
    Ok(path)
}

pub fn load() -> Result<Option<StateFile>> {
    let path = state_path();
    if !path.exists() {
        return Ok(None);
    }
    let text = std::fs::read_to_string(&path)?;
    let state = toml::from_str(&text)
        .map_err(|e| KaguyaError::Config(format!("Failed to parse {}: {}", path.display(), e)))?;
    Ok(Some(state))
}

pub fn clear() -> Result<()> {
    let path = state_path();
    if path.exists() {
        std::fs::remove_file(path)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn state_roundtrip_serialization() {
        let state = StateFile {
            repo_root: "/tmp/test".to_string(),
            preset: "full".to_string(),
            updated_at_unix: 1700000000,
            session_id: "kaguya-test-123".to_string(),
            services: vec![
                ServiceState {
                    name: "surrealdb".to_string(),
                    pid: 1234,
                    port: Some(8000),
                    exe_path: Some("/usr/bin/surreal".to_string()),
                    cmdline: Some("surreal start --bind 127.0.0.1:8000".to_string()),
                    started_at_unix: 1699999900,
                    session_id: "kaguya-test-123".to_string(),
                },
            ],
        };

        let text = toml::to_string_pretty(&state).unwrap();
        let parsed: StateFile = toml::from_str(&text).unwrap();
        assert_eq!(parsed.repo_root, state.repo_root);
        assert_eq!(parsed.preset, state.preset);
        assert_eq!(parsed.session_id, state.session_id);
        assert_eq!(parsed.services.len(), 1);
        assert_eq!(parsed.services[0].pid, 1234);
        assert_eq!(parsed.services[0].port, Some(8000));
        assert_eq!(parsed.services[0].exe_path, Some("/usr/bin/surreal".to_string()));
        assert_eq!(parsed.services[0].session_id, "kaguya-test-123");
    }

    #[test]
    fn generate_session_id_format() {
        let id = generate_session_id();
        assert!(id.starts_with("kaguya-"));
        assert!(id.len() > 10);
    }
}
