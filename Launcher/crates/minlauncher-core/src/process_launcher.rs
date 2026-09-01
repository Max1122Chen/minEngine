use std::path::Path;
use std::process::Command;
use std::thread;
use std::time::Duration;

use crate::error::{LauncherError, LauncherResult};
use crate::settings::normalize_path;

pub struct ProcessLauncher;

impl ProcessLauncher {
    pub fn launch_editor(editor: &Path, project_descriptor: &Path) -> LauncherResult<u32> {
        let editor = editor
            .canonicalize()
            .map_err(|_| LauncherError::EditorNotFound(editor.to_path_buf()))?;
        let project_descriptor = normalize_path(project_descriptor);

        if !project_descriptor.is_file() {
            return Err(LauncherError::PathNotFound(project_descriptor));
        }

        let working_dir = editor
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| Path::new(".").to_path_buf());

        let mut command = Command::new(&editor);
        command
            .current_dir(&working_dir)
            .arg("--project")
            .arg(&project_descriptor);

        #[cfg(windows)]
        apply_windows_spawn_flags(&mut command);

        let mut child = command
            .spawn()
            .map_err(|e| LauncherError::LaunchFailed(e.to_string()))?;

        let pid = child.id();
        thread::sleep(Duration::from_millis(750));

        match child.try_wait() {
            Ok(Some(status)) => {
                return Err(LauncherError::LaunchFailed(format!(
                    "Editor exited immediately (status: {status}). \
                     Ensure {}/libminEngine.dll or libminEngined.dll exists next to Editor.exe",
                    working_dir.display()
                )));
            }
            Ok(None) => {}
            Err(e) => {
                return Err(LauncherError::LaunchFailed(format!(
                    "failed to verify Editor process: {e}"
                )));
            }
        }

        Ok(pid)
    }
}

#[cfg(windows)]
fn apply_windows_spawn_flags(command: &mut Command) {
    use std::os::windows::process::CommandExt;

    const CREATE_NEW_PROCESS_GROUP: u32 = 0x0000_0200;
    const DETACHED_PROCESS: u32 = 0x0000_0008;

    command.creation_flags(CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS);
}

#[cfg(not(windows))]
fn apply_windows_spawn_flags(_command: &mut Command) {}
