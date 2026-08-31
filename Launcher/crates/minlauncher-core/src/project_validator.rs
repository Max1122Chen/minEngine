use std::fs;
use std::path::{Path, PathBuf};

use crate::error::{LauncherError, LauncherResult};
use crate::settings::normalize_path;
use crate::types::{ProjectDescriptor, PROJECT_DESCRIPTOR_EXTENSION};

#[derive(Debug)]
pub struct ResolvedProject {
    pub descriptor_path: PathBuf,
    pub descriptor: ProjectDescriptor,
}

pub struct ProjectValidator;

impl ProjectValidator {
    pub fn resolve(input: impl AsRef<Path>) -> LauncherResult<ResolvedProject> {
        let input = input.as_ref();
        if !input.exists() {
            return Err(LauncherError::PathNotFound(input.to_path_buf()));
        }

        let descriptor_path = if input.is_dir() {
            Self::find_descriptor_in_directory(input)?
        } else if input
            .extension()
            .and_then(|ext| ext.to_str())
            .is_some_and(|ext| ext.eq_ignore_ascii_case(PROJECT_DESCRIPTOR_EXTENSION))
        {
            input.to_path_buf()
        } else {
            return Err(LauncherError::WrongDescriptorExtension(
                input.to_path_buf(),
            ));
        };

        let descriptor = Self::load_descriptor(&descriptor_path)?;
        Ok(ResolvedProject {
            descriptor_path: normalize_path(&descriptor_path),
            descriptor,
        })
    }

    fn find_descriptor_in_directory(dir: &Path) -> LauncherResult<PathBuf> {
        let mut matches = Vec::new();
        for entry in fs::read_dir(dir).map_err(|e| LauncherError::io(dir, e))? {
            let entry = entry.map_err(|e| LauncherError::io(dir, e))?;
            let path = entry.path();
            if path.is_file()
                && path
                    .extension()
                    .and_then(|ext| ext.to_str())
                    .is_some_and(|ext| ext.eq_ignore_ascii_case(PROJECT_DESCRIPTOR_EXTENSION))
            {
                matches.push(path);
            }
        }

        match matches.len() {
            0 => Err(LauncherError::DescriptorNotFound(dir.to_path_buf())),
            1 => Ok(matches.remove(0)),
            _ => Err(LauncherError::InvalidDescriptor {
                path: dir.to_path_buf(),
                message: "multiple .meproject files found in directory".into(),
            }),
        }
    }

    fn load_descriptor(path: &Path) -> LauncherResult<ProjectDescriptor> {
        let contents =
            fs::read_to_string(path).map_err(|e| LauncherError::io(path, e))?;
        let descriptor: ProjectDescriptor = serde_json::from_str(&contents).map_err(|e| {
            LauncherError::InvalidDescriptor {
                path: path.to_path_buf(),
                message: e.to_string(),
            }
        })?;

        if descriptor.project_name.trim().is_empty() {
            return Err(LauncherError::InvalidDescriptor {
                path: path.to_path_buf(),
                message: "ProjectName is empty".into(),
            });
        }

        if descriptor.project_id.is_zero() {
            return Err(LauncherError::InvalidDescriptor {
                path: path.to_path_buf(),
                message: "ProjectId must be non-zero".into(),
            });
        }

        Ok(descriptor)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::tempdir;

    fn write_descriptor(dir: &Path, name: &str, json: &str) -> PathBuf {
        let path = dir.join(name);
        let mut file = fs::File::create(&path).unwrap();
        file.write_all(json.as_bytes()).unwrap();
        path
    }

    #[test]
    fn resolves_descriptor_file() {
        let dir = tempdir().unwrap();
        let json = r#"{
            "ProjectName": "Demo",
            "ProjectId": { "High": 1, "Low": 2 },
            "ProjectRoot": "D:/Demo"
        }"#;
        let path = write_descriptor(dir.path(), "Demo.meproject", json);
        let resolved = ProjectValidator::resolve(&path).unwrap();
        assert_eq!(resolved.descriptor.project_name, "Demo");
    }

    #[test]
    fn resolves_descriptor_directory() {
        let dir = tempdir().unwrap();
        let json = r#"{
            "ProjectName": "Demo",
            "ProjectId": { "High": 1, "Low": 2 },
            "ProjectRoot": "D:/Demo"
        }"#;
        write_descriptor(dir.path(), "Demo.meproject", json);
        let resolved = ProjectValidator::resolve(dir.path()).unwrap();
        assert_eq!(resolved.descriptor.project_name, "Demo");
    }

    #[test]
    fn rejects_zero_guid() {
        let dir = tempdir().unwrap();
        let json = r#"{
            "ProjectName": "Demo",
            "ProjectId": { "High": 0, "Low": 0 },
            "ProjectRoot": "D:/Demo"
        }"#;
        let path = write_descriptor(dir.path(), "Demo.meproject", json);
        let err = ProjectValidator::resolve(&path).unwrap_err();
        assert!(matches!(err, LauncherError::InvalidDescriptor { .. }));
    }
}
