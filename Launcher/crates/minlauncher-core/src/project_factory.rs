use std::fs;
use std::path::{Path, PathBuf};

use crate::error::{LauncherError, LauncherResult};
use crate::guid::generate_guid;
use crate::project_catalog::ProjectCatalog;
use crate::settings::{normalize_path, LauncherSettings};
use crate::types::{ProjectDescriptor, ProjectSettings};

const DEFAULT_TEMPLATE: &str = "Empty";
const DEFAULT_SCENE_NAME: &str = "default";

pub struct ProjectFactory;

impl ProjectFactory {
    pub fn create(
        settings: &mut LauncherSettings,
        name: &str,
        parent: &Path,
        template: &str,
        templates_root: &Path,
    ) -> LauncherResult<PathBuf> {
        Self::validate_project_name(name)?;

        let parent = parent
            .canonicalize()
            .map_err(|e| LauncherError::io(parent, e))?;
        let project_dir = parent.join(name);
        if project_dir.exists() {
            return Err(LauncherError::ProjectDirectoryExists(project_dir));
        }

        let template_dir = templates_root.join(template);
        if !template_dir.is_dir() {
            return Err(LauncherError::TemplateNotFound(template_dir));
        }

        fs::create_dir_all(&project_dir).map_err(|e| LauncherError::io(&project_dir, e))?;
        Self::copy_dir_recursive(&template_dir, &project_dir)?;

        let project_root = normalize_path(&project_dir);
        let descriptor_path = project_dir.join(format!("{name}.meproject"));
        let settings_path = project_dir.join(format!("{name}Settings.mesettings"));

        let descriptor = ProjectDescriptor {
            project_name: name.to_owned(),
            project_id: generate_guid(),
            project_root: project_root.to_string_lossy().into_owned(),
        };

        let project_settings = Self::load_partial_settings(&template_dir).unwrap_or(ProjectSettings {
            editor_default_scene_name: DEFAULT_SCENE_NAME.to_owned(),
        });

        Self::write_json(&descriptor_path, &descriptor)?;
        Self::write_json(&settings_path, &project_settings)?;

        ProjectCatalog::add_recent(settings, &descriptor_path)?;
        Ok(descriptor_path)
    }

    fn validate_project_name(name: &str) -> LauncherResult<()> {
        if name.trim().is_empty() {
            return Err(LauncherError::InvalidProjectName(
                "project name cannot be empty".into(),
            ));
        }

        if name == "." || name == ".." {
            return Err(LauncherError::InvalidProjectName(name.into()));
        }

        for ch in name.chars() {
            if matches!(ch, '/' | '\\' | ':' | '*' | '?' | '"' | '<' | '>' | '|') {
                return Err(LauncherError::InvalidProjectName(format!(
                    "invalid character '{ch}' in project name"
                )));
            }
        }

        Ok(())
    }

    fn load_partial_settings(template_dir: &Path) -> Option<ProjectSettings> {
        let partial = template_dir.join("mesettings.partial.json");
        if !partial.is_file() {
            return None;
        }

        let contents = fs::read_to_string(&partial).ok()?;
        serde_json::from_str(&contents).ok()
    }

    fn write_json<T: serde::Serialize>(path: &Path, value: &T) -> LauncherResult<()> {
        let contents = serde_json::to_string_pretty(value).map_err(|e| {
            LauncherError::Settings(format!("failed to serialize {}: {e}", path.display()))
        })?;
        fs::write(path, contents).map_err(|e| LauncherError::io(path, e))?;
        Ok(())
    }

    fn copy_dir_recursive(from: &Path, to: &Path) -> LauncherResult<()> {
        for entry in fs::read_dir(from).map_err(|e| LauncherError::io(from, e))? {
            let entry = entry.map_err(|e| LauncherError::io(from, e))?;
            let file_type = entry.file_type().map_err(|e| LauncherError::io(from, e))?;
            let src = entry.path();
            let dst = to.join(entry.file_name());

            if file_type.is_dir() {
                fs::create_dir_all(&dst).map_err(|e| LauncherError::io(&dst, e))?;
                Self::copy_dir_recursive(&src, &dst)?;
            } else if file_type.is_file() {
                if entry.file_name() == "mesettings.partial.json" {
                    continue;
                }
                if let Some(parent) = dst.parent() {
                    fs::create_dir_all(parent).map_err(|e| LauncherError::io(parent, e))?;
                }
                fs::copy(&src, &dst).map_err(|e| LauncherError::io(&dst, e))?;
            }
        }

        Ok(())
    }

    pub fn default_template_name() -> &'static str {
        DEFAULT_TEMPLATE
    }
}
