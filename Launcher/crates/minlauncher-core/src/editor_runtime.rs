use std::path::Path;

use crate::error::{LauncherError, LauncherResult};

const RELEASE_ENGINE_DLL: &str = "libminEngine.dll";
const DEBUG_ENGINE_DLL: &str = "libminEngined.dll";

/// Ensure Editor's runtime DLL is present next to the executable.
pub fn validate_editor_runtime(editor: &Path) -> LauncherResult<()> {
    let bin_dir = editor.parent().ok_or_else(|| {
        LauncherError::LaunchFailed(format!(
            "editor path has no parent directory: {}",
            editor.display()
        ))
    })?;

    let release_dll = bin_dir.join(RELEASE_ENGINE_DLL);
    let debug_dll = bin_dir.join(DEBUG_ENGINE_DLL);

    if release_dll.is_file() || debug_dll.is_file() {
        return Ok(());
    }

    Err(LauncherError::LaunchFailed(format!(
        "missing engine runtime DLL next to Editor (expected {RELEASE_ENGINE_DLL} or {DEBUG_ENGINE_DLL} in {})",
        bin_dir.display()
    )))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn accepts_release_runtime_dll() {
        let dir = tempdir().unwrap();
        let editor = dir.path().join("Editor.exe");
        fs::write(&editor, b"").unwrap();
        fs::write(dir.path().join(RELEASE_ENGINE_DLL), b"").unwrap();
        validate_editor_runtime(&editor).unwrap();
    }

    #[test]
    fn accepts_debug_runtime_dll() {
        let dir = tempdir().unwrap();
        let editor = dir.path().join("Editor.exe");
        fs::write(&editor, b"").unwrap();
        fs::write(dir.path().join(DEBUG_ENGINE_DLL), b"").unwrap();
        validate_editor_runtime(&editor).unwrap();
    }

    #[test]
    fn rejects_missing_runtime_dll() {
        let dir = tempdir().unwrap();
        let editor = dir.path().join("Editor.exe");
        fs::write(&editor, b"").unwrap();
        let err = validate_editor_runtime(&editor).unwrap_err();
        assert!(matches!(err, LauncherError::LaunchFailed(_)));
    }
}
