use std::path::Path;
use std::process::Command;

use crate::error::{LauncherError, LauncherResult};

pub struct ProcessLauncher;

impl ProcessLauncher {
    pub fn launch_editor(editor: &Path, project_descriptor: &Path) -> LauncherResult<u32> {
        let editor = editor
            .canonicalize()
            .map_err(|_| LauncherError::EditorNotFound(editor.to_path_buf()))?;
        let project_descriptor = project_descriptor
            .canonicalize()
            .map_err(|_| LauncherError::PathNotFound(project_descriptor.to_path_buf()))?;

        let working_dir = editor
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| Path::new(".").to_path_buf());

        let child = Command::new(&editor)
            .current_dir(working_dir)
            .arg("--project")
            .arg(&project_descriptor)
            .spawn()
            .map_err(|e| LauncherError::LaunchFailed(e.to_string()))?;

        Ok(child.id())
    }
}
