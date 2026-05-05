use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};

use crate::error::{KaguyaError, Result};

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct KaguyaConfig {
    pub presets: HashMap<String, Vec<String>>,
    #[serde(default)]
    pub settings: Settings,
    #[serde(default)]
    pub services: HashMap<String, ServiceConfig>,
    #[serde(default)]
    pub paths: PathConfig,
    #[serde(skip)]
    pub source_path: Option<PathBuf>,
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct Settings {
    #[serde(default = "default_auto_heal")]
    pub auto_heal: bool,
    #[serde(default = "default_max_retries")]
    pub max_retries: u32,
    #[serde(default = "default_log_buffer_lines")]
    pub log_buffer_lines: usize,
    #[serde(default = "default_ui_fps")]
    pub ui_fps: u32,
    #[serde(default)]
    pub language: String,
    #[serde(default = "default_true")]
    pub emoji: bool,
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct ServiceConfig {
    #[serde(default = "default_true")]
    pub enabled: bool,
    pub command: Option<String>,
    #[serde(default)]
    pub args: Vec<String>,
    pub working_dir: Option<String>,
    pub port: Option<u16>,
    pub health_url: Option<String>,
    #[serde(default = "default_health_timeout")]
    pub health_timeout_secs: u64,
    #[serde(default = "default_start_timeout")]
    pub start_timeout_secs: u64,
    #[serde(default)]
    pub dependencies: Vec<String>,
    pub icon: Option<String>,
    pub health_type: Option<String>,
    pub log_file: Option<String>,
    pub project_path: Option<String>,
    #[serde(default)]
    pub headless_args: Vec<String>,
}

#[derive(Debug, Clone, Deserialize, Serialize)]
#[derive(Default)]
pub struct PathConfig {
    pub repo_root: Option<String>,
    pub env_key: Option<String>,
    #[serde(default)]
    pub registry_keys: Vec<String>,
    #[serde(default)]
    pub common_paths: Vec<String>,
    #[serde(default)]
    pub surrealdb_search: Vec<String>,
    pub scene_syncd_binary: Option<String>,
}

fn default_auto_heal() -> bool {
    true
}
fn default_max_retries() -> u32 {
    3
}
fn default_log_buffer_lines() -> usize {
    1000
}
fn default_ui_fps() -> u32 {
    30
}
fn default_health_timeout() -> u64 {
    10
}
fn default_start_timeout() -> u64 {
    60
}
fn default_true() -> bool {
    true
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            auto_heal: default_auto_heal(),
            max_retries: default_max_retries(),
            log_buffer_lines: default_log_buffer_lines(),
            ui_fps: default_ui_fps(),
            language: "en".to_string(),
            emoji: true,
        }
    }
}

impl Default for ServiceConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            command: None,
            args: Vec::new(),
            working_dir: None,
            port: None,
            health_url: None,
            health_timeout_secs: default_health_timeout(),
            start_timeout_secs: default_start_timeout(),
            dependencies: Vec::new(),
            icon: None,
            health_type: None,
            log_file: None,
            project_path: None,
            headless_args: Vec::new(),
        }
    }
}


impl KaguyaConfig {
    pub fn load() -> Result<Self> {
        Self::load_with_overrides(None, None)
    }

    pub fn load_with_overrides(
        config_path: Option<&Path>,
        repo_root: Option<PathBuf>,
    ) -> Result<Self> {
        let candidates = Self::config_paths(config_path);
        for path in &candidates {
            if path.exists() {
                let mut config = Self::load_from(path)?;
                config.source_path = Some(path.clone());
                if let Some(root) = &repo_root {
                    config.paths.repo_root = Some(root.to_string_lossy().to_string());
                }
                return Ok(config);
            }
        }
        Err(KaguyaError::Config(format!(
            "No kaguya.toml found. Searched: {}",
            candidates
                .iter()
                .map(|p| p.display().to_string())
                .collect::<Vec<_>>()
                .join(", ")
        )))
    }

    pub fn load_from(path: &Path) -> Result<Self> {
        let content = std::fs::read_to_string(path).map_err(|e| {
            KaguyaError::Config(format!("Failed to read {}: {}", path.display(), e))
        })?;
        let mut config: KaguyaConfig = toml::from_str(&content).map_err(|e| {
            KaguyaError::Config(format!("Failed to parse {}: {}", path.display(), e))
        })?;
        config.source_path = Some(path.to_path_buf());
        Ok(config)
    }

    fn config_paths(explicit: Option<&Path>) -> Vec<PathBuf> {
        let mut paths = Vec::new();

        if let Some(path) = explicit {
            paths.push(path.to_path_buf());
        }
        if let Ok(path) = std::env::var("KAGUYA_CONFIG") {
            paths.push(PathBuf::from(path));
        }

        if let Ok(cwd) = std::env::current_dir() {
            for ancestor in cwd.ancestors() {
                paths.push(ancestor.join("kaguya.toml"));
                paths.push(ancestor.join("kaguya").join("kaguya.toml"));
            }
        }

        if let Ok(exe) = std::env::current_exe() {
            if let Some(parent) = exe.parent() {
                for ancestor in parent.ancestors() {
                    paths.push(ancestor.join("kaguya.toml"));
                    paths.push(ancestor.join("kaguya").join("kaguya.toml"));
                }
            }
        }

        if let Some(proj_dirs) = directories::ProjectDirs::from("", "unreal-mcp", "kaguya") {
            paths.push(proj_dirs.config_dir().join("kaguya.toml"));
        }

        let mut seen = HashSet::new();
        paths
            .into_iter()
            .filter(|p| seen.insert(p.clone()))
            .collect()
    }

    pub fn repo_root(&self) -> PathBuf {
        if let Some(root) = &self.paths.repo_root {
            return normalize_path(PathBuf::from(root));
        }
        if let Ok(root) = std::env::var("KAGUYA_REPO_ROOT") {
            return normalize_path(PathBuf::from(root));
        }
        if let Some(source) = &self.source_path {
            if let Some(parent) = source.parent() {
                if parent.file_name().map(|n| n == "kaguya").unwrap_or(false) {
                    if let Some(repo) = parent.parent() {
                        return normalize_path(repo.to_path_buf());
                    }
                }
                return normalize_path(parent.to_path_buf());
            }
        }
        if let Ok(cwd) = std::env::current_dir() {
            for ancestor in cwd.ancestors() {
                if ancestor
                    .join("FlopperamUnrealMCP")
                    .join("FlopperamUnrealMCP.uproject")
                    .exists()
                    || ancestor.join(".git").exists()
                {
                    return normalize_path(ancestor.to_path_buf());
                }
            }
        }
        normalize_path(std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")))
    }

    pub fn resolve_path(&self, path: &str) -> PathBuf {
        let repo = self.repo_root();
        let clean = path
            .strip_prefix("./")
            .or_else(|| path.strip_prefix(".\\"))
            .unwrap_or(path);
        let p = PathBuf::from(clean);
        if p.is_absolute() {
            normalize_path(p)
        } else {
            normalize_path(repo.join(p))
        }
    }
}

fn normalize_path(path: PathBuf) -> PathBuf {
    let canonical = std::fs::canonicalize(&path).unwrap_or(path);
    let text = canonical.to_string_lossy();
    if let Some(stripped) = text.strip_prefix(r"\\?\UNC\") {
        PathBuf::from(format!(r"\\{}", stripped))
    } else if let Some(stripped) = text.strip_prefix(r"\\?\") {
        PathBuf::from(stripped)
    } else {
        canonical
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn resolve_path_uses_configured_repo_root_and_cleans_dot_prefix() {
        let repo = std::env::current_dir().expect("current dir");
        let mut config = KaguyaConfig {
            presets: HashMap::new(),
            settings: Settings::default(),
            services: HashMap::new(),
            paths: PathConfig::default(),
            source_path: None,
        };
        config.paths.repo_root = Some(repo.to_string_lossy().to_string());

        let resolved = config.resolve_path("./FlopperamUnrealMCP/FlopperamUnrealMCP.uproject");
        assert!(resolved.ends_with(Path::new("FlopperamUnrealMCP/FlopperamUnrealMCP.uproject")));
        assert!(!resolved.to_string_lossy().contains(r"\.\"));
    }

    #[test]
    fn settings_default_returns_expected_values() {
        let s = Settings::default();
        assert!(s.auto_heal);
        assert_eq!(s.max_retries, 3);
        assert_eq!(s.log_buffer_lines, 1000);
        assert_eq!(s.ui_fps, 30);
        assert_eq!(s.language, "en");
        assert!(s.emoji);
    }

    #[test]
    fn service_config_default_enabled_is_true() {
        let sc = ServiceConfig::default();
        assert!(sc.enabled);
        assert!(sc.command.is_none());
        assert!(sc.args.is_empty());
        assert!(sc.dependencies.is_empty());
        assert_eq!(sc.health_timeout_secs, 10);
        assert_eq!(sc.start_timeout_secs, 60);
    }

    #[test]
    fn preset_parsing() {
        let toml_str = r#"
[presets]
full = ["surrealdb", "scene-syncd", "unreal", "mcp-server"]
minimal = ["surrealdb", "scene-syncd"]

[settings]
auto_heal = false
max_retries = 5

[services.surrealdb]
port = 8000

[services.unreal]
port = 55557
"#;
        let config: KaguyaConfig = toml::from_str(toml_str).unwrap();
        assert_eq!(config.presets["full"].len(), 4);
        assert_eq!(config.presets["minimal"].len(), 2);
        assert!(!config.settings.auto_heal);
        assert_eq!(config.settings.max_retries, 5);
        assert_eq!(config.services["surrealdb"].port, Some(8000));
        assert_eq!(config.services["unreal"].port, Some(55557));
    }
}
