use std::env;
use std::path::{Path, PathBuf};

use crate::error::{LauncherError, LauncherResult};
use crate::settings::LauncherSettings;

const ENV_EDITOR: &str = "MINENGINE_EDITOR";

pub fn editor_executable_name() -> &'static str {
    if cfg!(windows) {
        "Editor.exe"
    } else {
        "Editor"
    }
}

pub struct EngineLocator;

impl EngineLocator {
    /// Resolve editor path: CLI override > env > settings > auto-discover.
    pub fn resolve_editor(
        cli_override: Option<&Path>,
        settings: &LauncherSettings,
    ) -> LauncherResult<PathBuf> {
        if let Some(path) = cli_override {
            return Self::validate_editor(path);
        }

        if let Ok(path) = env::var(ENV_EDITOR) {
            if !path.trim().is_empty() {
                return Self::validate_editor(Path::new(&path));
            }
        }

        if let Some(path) = settings.editor_executable_path.as_deref() {
            if !path.trim().is_empty() {
                return Self::validate_editor(Path::new(path));
            }
        }

        if let Some(path) = Self::discover_editor_from_cwd()? {
            return Self::validate_editor(&path);
        }

        Err(LauncherError::EditorNotConfigured)
    }

    pub fn validate_editor(path: &Path) -> LauncherResult<PathBuf> {
        let path = if path.is_file() {
            path.to_path_buf()
        } else {
            return Err(LauncherError::EditorNotFound(path.to_path_buf()));
        };

        let file_name = path
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or_default();

        if !file_name.eq_ignore_ascii_case(editor_executable_name()) {
            return Err(LauncherError::LaunchFailed(format!(
                "expected executable named {}, got {}",
                editor_executable_name(),
                file_name
            )));
        }

        Ok(path)
    }

    fn discover_editor_from_cwd() -> LauncherResult<Option<PathBuf>> {
        let mut current = env::current_dir().map_err(|e| {
            LauncherError::LaunchFailed(format!("failed to read current directory: {e}"))
        })?;

        for _ in 0..12 {
            for candidate in Self::editor_candidates(&current) {
                if candidate.is_file() {
                    return Ok(Some(candidate));
                }
            }

            if !current.pop() {
                break;
            }
        }

        if let Ok(exe) = env::current_exe() {
            let mut current = exe
                .parent()
                .map(Path::to_path_buf)
                .unwrap_or_else(|| PathBuf::from("."));

            for _ in 0..12 {
                for candidate in Self::editor_candidates(&current) {
                    if candidate.is_file() {
                        return Ok(Some(candidate));
                    }
                }

                if !current.pop() {
                    break;
                }
            }
        }

        Ok(None)
    }

    fn editor_candidates(root: &Path) -> [PathBuf; 3] {
        let name = editor_executable_name();
        [
            root.join("minEngine").join("bin").join(name),
            root.join("bin").join(name),
            root.join(name),
        ]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn editor_name_matches_platform() {
        if cfg!(windows) {
            assert_eq!(editor_executable_name(), "Editor.exe");
        } else {
            assert_eq!(editor_executable_name(), "Editor");
        }
    }
}
