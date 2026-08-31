use std::path::{Path, PathBuf};

use minlauncher_core::engine_locator::{EditorStatus, EngineLocator};
use minlauncher_core::process_launcher::ProcessLauncher;
use minlauncher_core::project_catalog::ProjectCatalog;
use minlauncher_core::project_factory::ProjectFactory;
use minlauncher_core::project_validator::ProjectValidator;
use minlauncher_core::settings::LauncherSettings;
use minlauncher_core::templates;
use minlauncher_core::LauncherError;

fn map_error(error: LauncherError) -> String {
    error.to_string()
}

#[tauri::command]
pub fn list_recent() -> Result<Vec<minlauncher_core::project_catalog::RecentEntryStatus>, String> {
    let settings = LauncherSettings::load().map_err(map_error)?;
    Ok(ProjectCatalog::list_recent(&settings))
}

#[tauri::command]
pub fn remove_recent(path: String) -> Result<(), String> {
    let mut settings = LauncherSettings::load().map_err(map_error)?;
    ProjectCatalog::remove_recent(&mut settings, Path::new(&path)).map_err(map_error)?;
    settings.save().map_err(map_error)?;
    Ok(())
}

#[tauri::command]
pub fn clear_recent() -> Result<(), String> {
    let mut settings = LauncherSettings::load().map_err(map_error)?;
    ProjectCatalog::clear_all_recent(&mut settings);
    settings.save().map_err(map_error)?;
    Ok(())
}

#[tauri::command]
pub fn open_project(path: String) -> Result<(), String> {
    let mut settings = LauncherSettings::load().map_err(map_error)?;
    let resolved = ProjectValidator::resolve(Path::new(&path)).map_err(map_error)?;
    let editor = EngineLocator::resolve_editor(None, &settings).map_err(map_error)?;
    ProcessLauncher::launch_editor(&editor, &resolved.descriptor_path).map_err(map_error)?;
    ProjectCatalog::add_recent(&mut settings, &resolved.descriptor_path).map_err(map_error)?;
    settings.save().map_err(map_error)?;
    Ok(())
}

#[tauri::command]
pub fn create_project(name: String, parent: String, template: Option<String>) -> Result<String, String> {
    let mut settings = LauncherSettings::load().map_err(map_error)?;
    let templates_root = templates::resolve_templates_root().map_err(map_error)?;
    let template = template.unwrap_or_else(|| ProjectFactory::default_template_name().to_owned());
    let descriptor_path = ProjectFactory::create(
        &mut settings,
        &name,
        Path::new(&parent),
        &template,
        &templates_root,
    )
    .map_err(map_error)?;
    settings.save().map_err(map_error)?;
    Ok(descriptor_path.to_string_lossy().into_owned())
}

#[tauri::command]
pub fn get_settings() -> Result<LauncherSettings, String> {
    LauncherSettings::load().map_err(map_error)
}

#[tauri::command]
pub fn save_settings(settings: LauncherSettings) -> Result<(), String> {
    settings.save().map_err(map_error)
}

#[tauri::command]
pub fn set_editor_path(path: String) -> Result<(), String> {
    let mut settings = LauncherSettings::load().map_err(map_error)?;
    let validated = EngineLocator::validate_editor(Path::new(&path)).map_err(map_error)?;
    settings.set_editor_executable_path(validated);
    settings.save().map_err(map_error)?;
    Ok(())
}

#[tauri::command]
pub fn set_projects_dir(path: String) -> Result<(), String> {
    let mut settings = LauncherSettings::load().map_err(map_error)?;
    let path = PathBuf::from(&path)
        .canonicalize()
        .map_err(|e| format!("invalid projects directory: {e}"))?;
    settings.set_default_projects_directory(path);
    settings.save().map_err(map_error)?;
    Ok(())
}

#[tauri::command]
pub fn resolve_editor_status() -> Result<EditorStatus, String> {
    let settings = LauncherSettings::load().map_err(map_error)?;
    Ok(EngineLocator::editor_status(&settings))
}

#[tauri::command]
pub fn reveal_in_explorer(path: String) -> Result<(), String> {
    let path = Path::new(&path);
    let target = if path.is_file() {
        path.parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| path.to_path_buf())
    } else {
        path.to_path_buf()
    };

    #[cfg(target_os = "windows")]
    {
        std::process::Command::new("explorer")
            .arg(target)
            .spawn()
            .map_err(|e| format!("failed to open explorer: {e}"))?;
    }

    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .arg(target)
            .spawn()
            .map_err(|e| format!("failed to open finder: {e}"))?;
    }

    #[cfg(target_os = "linux")]
    {
        std::process::Command::new("xdg-open")
            .arg(target)
            .spawn()
            .map_err(|e| format!("failed to open file manager: {e}"))?;
    }

    Ok(())
}
