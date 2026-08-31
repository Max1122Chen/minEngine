use std::path::{Path, PathBuf};

use chrono::Utc;

use crate::error::{LauncherError, LauncherResult};
use crate::project_validator::ProjectValidator;
use crate::settings::{normalize_path, LauncherSettings};
use crate::types::RecentProject;

pub struct ProjectCatalog;

impl ProjectCatalog {
    pub fn add_recent(settings: &mut LauncherSettings, descriptor_path: &Path) -> LauncherResult<()> {
        let resolved = ProjectValidator::resolve(descriptor_path)?;
        let descriptor_path = normalize_path(&resolved.descriptor_path);
        let descriptor_path_str = descriptor_path.to_string_lossy().into_owned();

        settings.recent_projects.retain(|entry| {
            entry.descriptor_path != descriptor_path_str
        });

        settings.recent_projects.insert(
            0,
            RecentProject {
                project_name: resolved.descriptor.project_name,
                descriptor_path: descriptor_path_str,
                last_opened_utc: Utc::now().to_rfc3339(),
            },
        );

        let max = settings.max_recent_projects.max(1);
        settings.recent_projects.truncate(max);
        Ok(())
    }

    pub fn remove_recent(settings: &mut LauncherSettings, path: &Path) -> LauncherResult<()> {
        let target = match ProjectValidator::resolve(path) {
            Ok(resolved) => normalize_path(&resolved.descriptor_path),
            Err(LauncherError::PathNotFound(_)) | Err(LauncherError::DescriptorNotFound(_)) => {
                normalize_path(path)
            }
            Err(error) => return Err(error),
        };
        let target_str = target.to_string_lossy();
        let before = settings.recent_projects.len();
        settings
            .recent_projects
            .retain(|entry| entry.descriptor_path != target_str);

        if settings.recent_projects.len() == before {
            return Err(LauncherError::RecentProjectNotFound(target));
        }

        Ok(())
    }

    pub fn list_recent(settings: &LauncherSettings) -> Vec<RecentEntryStatus> {
        settings
            .recent_projects
            .iter()
            .map(|entry| {
                let path = PathBuf::from(&entry.descriptor_path);
                RecentEntryStatus {
                    entry: entry.clone(),
                    exists: path.exists(),
                }
            })
            .collect()
    }
}

#[derive(Debug, Clone)]
pub struct RecentEntryStatus {
    pub entry: RecentProject,
    pub exists: bool,
}
