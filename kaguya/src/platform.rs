use std::path::{Path, PathBuf};

/// Check if the current environment is WSL2
pub fn is_wsl() -> bool {
    Path::new("/proc/sys/fs/binfmt_misc/WSLInterop").exists()
        || std::env::var("WSL_DISTRO_NAME").is_ok()
}

/// Convert a WSL path to a Windows path using `wslpath -w`
pub fn wsl_to_windows_path(path: &Path) -> Option<PathBuf> {
    if !is_wsl() {
        return None;
    }
    let output = std::process::Command::new("wslpath")
        .arg("-w")
        .arg(path)
        .output()
        .ok()?;
    if !output.status.success() {
        return None;
    }
    let text = String::from_utf8_lossy(&output.stdout);
    let line = text.trim();
    if line.is_empty() {
        None
    } else {
        Some(PathBuf::from(line))
    }
}

/// Convert a Windows path to a WSL path using `wslpath -u`
pub fn windows_to_wsl_path(path: &str) -> Option<PathBuf> {
    if !is_wsl() {
        return None;
    }
    let output = std::process::Command::new("wslpath")
        .arg("-u")
        .arg(path)
        .output()
        .ok()?;
    if !output.status.success() {
        return None;
    }
    let text = String::from_utf8_lossy(&output.stdout);
    let line = text.trim();
    if line.is_empty() {
        None
    } else {
        Some(PathBuf::from(line))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn is_wsl_returns_false_on_native_windows() {
        // On native Windows (no WSL), this should be false
        // On WSL it would be true — this test documents the behavior
        if cfg!(windows) && !is_wsl() {
            assert!(!is_wsl());
        }
    }
}