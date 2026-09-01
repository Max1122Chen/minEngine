use std::path::PathBuf;

use crate::error::{LauncherError, LauncherResult};

pub fn resolve_templates_root() -> LauncherResult<PathBuf> {
    let mut candidates = Vec::new();

    if let Ok(dir) = std::env::var("MINENGINE_LAUNCHER_TEMPLATES") {
        candidates.push(PathBuf::from(dir));
    }

    if let Ok(cwd) = std::env::current_dir() {
        candidates.push(cwd.join("Templates"));
        candidates.push(cwd.join("Launcher").join("Templates"));
    }

    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            candidates.push(parent.join("Templates"));
            candidates.push(normalize_components(parent.join("../../Templates")));
        }
    }

    candidates.push(PathBuf::from("Launcher/Templates"));

    for candidate in candidates {
        if candidate.is_dir() {
            return Ok(candidate);
        }
    }

    Err(LauncherError::TemplateNotFound(PathBuf::from(
        "Launcher/Templates (set MINENGINE_LAUNCHER_TEMPLATES)",
    )))
}

fn normalize_components(path: PathBuf) -> PathBuf {
    path.components()
        .fold(PathBuf::new(), |mut acc, component| {
            acc.push(component.as_os_str());
            acc
        })
}
