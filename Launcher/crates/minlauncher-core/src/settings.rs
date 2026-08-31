use std::fs;
use std::path::{Path, PathBuf};

use crate::error::{LauncherError, LauncherResult};
use crate::types::RecentProject;

const SETTINGS_FILE_NAME: &str = "settings.json";
const DEFAULT_MAX_RECENT: usize = 20;

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize, PartialEq, Eq)]
#[serde(rename_all = "PascalCase")]
pub struct LauncherSettings {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub editor_executable_path: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub engine_root: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub default_projects_directory: Option<String>,
    #[serde(default)]
    pub recent_projects: Vec<RecentProject>,
    #[serde(default = "default_max_recent")]
    pub max_recent_projects: usize,
}

fn default_max_recent() -> usize {
    DEFAULT_MAX_RECENT
}

impl Default for LauncherSettings {
    fn default() -> Self {
        Self {
            editor_executable_path: None,
            engine_root: None,
            default_projects_directory: None,
            recent_projects: Vec::new(),
            max_recent_projects: DEFAULT_MAX_RECENT,
        }
    }
}

impl LauncherSettings {
    pub fn config_dir() -> LauncherResult<PathBuf> {
        if let Ok(path) = std::env::var("MINENGINE_LAUNCHER_CONFIG") {
            return Ok(PathBuf::from(path));
        }

        let base = if cfg!(target_os = "windows") {
            std::env::var_os("APPDATA").map(PathBuf::from)
        } else if cfg!(target_os = "macos") {
            std::env::var_os("HOME")
                .map(|home| PathBuf::from(home).join("Library/Application Support"))
        } else {
            std::env::var_os("XDG_CONFIG_HOME")
                .map(PathBuf::from)
                .or_else(|| {
                    std::env::var_os("HOME")
                        .map(|home| PathBuf::from(home).join(".config"))
                })
        };

        base.map(|dir| dir.join("minEngine").join("Launcher"))
            .ok_or_else(|| LauncherError::Settings("could not resolve config directory".into()))
    }

    pub fn settings_path() -> LauncherResult<PathBuf> {
        Ok(Self::config_dir()?.join(SETTINGS_FILE_NAME))
    }

    pub fn load() -> LauncherResult<Self> {
        let path = Self::settings_path()?;
        if !path.exists() {
            return Ok(Self::default());
        }

        let contents = fs::read_to_string(&path).map_err(|e| LauncherError::io(&path, e))?;
        serde_json::from_str(&contents)
            .map_err(|e| LauncherError::Settings(format!("failed to parse {}: {e}", path.display())))
    }

    pub fn save(&self) -> LauncherResult<()> {
        let path = Self::settings_path()?;
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(|e| LauncherError::io(parent, e))?;
        }

        let contents = serde_json::to_string_pretty(self)
            .map_err(|e| LauncherError::Settings(format!("failed to serialize settings: {e}")))?;
        fs::write(&path, contents).map_err(|e| LauncherError::io(&path, e))?;
        Ok(())
    }

    pub fn set_editor_executable_path(&mut self, path: impl Into<PathBuf>) {
        self.editor_executable_path = Some(path.into().to_string_lossy().into_owned());
    }

    pub fn set_default_projects_directory(&mut self, path: impl Into<PathBuf>) {
        self.default_projects_directory = Some(path.into().to_string_lossy().into_owned());
    }
}

pub fn normalize_path(path: &Path) -> PathBuf {
    let path = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
    let display = path.to_string_lossy();
    if let Some(stripped) = display.strip_prefix(r"\\?\") {
        return PathBuf::from(stripped);
    }
    path
}
