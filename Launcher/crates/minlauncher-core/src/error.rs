use std::path::PathBuf;
use thiserror::Error;

pub type LauncherResult<T> = Result<T, LauncherError>;

#[derive(Debug, Error)]
pub enum LauncherError {
    #[error("project descriptor not found in directory: {0}")]
    DescriptorNotFound(PathBuf),

    #[error("invalid project descriptor extension (expected .meproject): {0}")]
    WrongDescriptorExtension(PathBuf),

    #[error("failed to parse project descriptor {path}: {message}")]
    InvalidDescriptor { path: PathBuf, message: String },

    #[error("project path does not exist: {0}")]
    PathNotFound(PathBuf),

    #[error("editor executable not configured; set MINENGINE_EDITOR or run `minlauncher config set editor <path>`")]
    EditorNotConfigured,

    #[error("editor executable not found: {0}")]
    EditorNotFound(PathBuf),

    #[error("failed to launch editor: {0}")]
    LaunchFailed(String),

    #[error("project directory already exists: {0}")]
    ProjectDirectoryExists(PathBuf),

    #[error("invalid project name: {0}")]
    InvalidProjectName(String),

    #[error("template not found: {0}")]
    TemplateNotFound(PathBuf),

    #[error("recent project not found: {0}")]
    RecentProjectNotFound(PathBuf),

    #[error("io error at {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },

    #[error("settings error: {0}")]
    Settings(String),
}

impl LauncherError {
    pub fn io(path: impl Into<PathBuf>, source: std::io::Error) -> Self {
        Self::Io {
            path: path.into(),
            source,
        }
    }
}
