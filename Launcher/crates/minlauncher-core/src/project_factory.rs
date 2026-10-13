use std::fs;
use std::path::{Path, PathBuf};

use crate::engine_locator::EngineLocator;
use crate::error::{LauncherError, LauncherResult};
use crate::guid::generate_guid;
use crate::project_catalog::ProjectCatalog;
use crate::settings::{normalize_path, LauncherSettings};
use crate::types::{
    ProjectDescriptor, ProjectSettings, DISK_SCHEMA_VERSION, ENGINE_VERSION_STAMP,
};

const DEFAULT_TEMPLATE: &str = "Empty";
const DEFAULT_SCENE_NAME: &str = "default";

struct TemplateVars {
    project_name: String,
    project_editor_name: String,
    game_module_name: String,
    engine_root: String,
}

impl TemplateVars {
    fn pairs(&self) -> [(&str, &str); 4] {
        [
            ("{{PROJECT_NAME}}", self.project_name.as_str()),
            ("{{PROJECT_EDITOR_NAME}}", self.project_editor_name.as_str()),
            ("{{GAME_MODULE_NAME}}", self.game_module_name.as_str()),
            ("{{ENGINE_ROOT}}", self.engine_root.as_str()),
        ]
    }

    fn apply(&self, input: &str) -> String {
        let mut out = input.to_owned();
        for (from, to) in self.pairs() {
            out = out.replace(from, to);
        }
        out
    }
}

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

        let needs_engine = Self::template_needs_engine_root(&template_dir);
        if needs_engine {
            Self::validate_code_identifier_name(name)?;
        }

        let vars = if needs_engine {
            Some(Self::build_template_vars(name, settings)?)
        } else {
            None
        };

        fs::create_dir_all(&project_dir).map_err(|e| LauncherError::io(&project_dir, e))?;
        Self::copy_dir_recursive(&template_dir, &project_dir)?;

        if let Some(ref vars) = vars {
            Self::apply_placeholders_in_tree(&project_dir, vars)?;
            Self::rename_placeholder_paths(&project_dir, vars)?;
        }

        let project_root = normalize_path(&project_dir);
        let descriptor_path = project_dir.join(format!("{name}.meproject"));
        let settings_path = project_dir.join(format!("{name}Settings.mesettings"));

        let descriptor = ProjectDescriptor {
            schema_version: DISK_SCHEMA_VERSION,
            engine_version: ENGINE_VERSION_STAMP.to_owned(),
            project_name: name.to_owned(),
            project_id: generate_guid(),
            project_root: project_root.to_string_lossy().into_owned(),
        };

        let mut project_settings =
            Self::load_partial_settings(&template_dir).unwrap_or(ProjectSettings {
                schema_version: 0,
                engine_version: String::new(),
                editor_default_scene_name: DEFAULT_SCENE_NAME.to_owned(),
            });
        project_settings.schema_version = DISK_SCHEMA_VERSION;
        project_settings.engine_version = ENGINE_VERSION_STAMP.to_owned();

        Self::write_json(&descriptor_path, &descriptor)?;
        Self::write_json(&settings_path, &project_settings)?;

        ProjectCatalog::add_recent(settings, &descriptor_path)?;
        Ok(descriptor_path)
    }

    fn template_needs_engine_root(template_dir: &Path) -> bool {
        let cmake = template_dir.join("CMakeLists.txt");
        fs::read_to_string(cmake)
            .map(|text| text.contains("{{ENGINE_ROOT}}"))
            .unwrap_or(false)
    }

    fn build_template_vars(name: &str, settings: &LauncherSettings) -> LauncherResult<TemplateVars> {
        let engine_root = Self::resolve_engine_root(settings)?;
        Ok(TemplateVars {
            project_name: name.to_owned(),
            project_editor_name: format!("{name}Editor"),
            game_module_name: format!("{name}Game"),
            engine_root,
        })
    }

    fn resolve_engine_root(settings: &LauncherSettings) -> LauncherResult<String> {
        if let Some(root) = settings.engine_root.as_deref() {
            let trimmed = root.trim();
            if !trimmed.is_empty() {
                let path = PathBuf::from(trimmed);
                return Self::validate_engine_root(&path);
            }
        }

        if let Ok(editor) = EngineLocator::resolve_editor(None, settings) {
            if let Some(root) = Self::engine_root_from_editor(&editor) {
                return Self::validate_engine_root(&root);
            }
        }

        if let Some(root) = Self::discover_engine_root_from_cwd()? {
            return Self::validate_engine_root(&root);
        }

        Err(LauncherError::Settings(
            "GameProject template requires engine_root in Launcher settings (config set engine-root), \
             a discoverable MaximumEditor under <repo>/minEngine/bin, \
             or running from inside the minEngine repository tree"
                .into(),
        ))
    }

    fn discover_engine_root_from_cwd() -> LauncherResult<Option<PathBuf>> {
        let mut roots = Vec::new();
        if let Ok(cwd) = std::env::current_dir() {
            roots.push(cwd);
        }
        if let Ok(exe) = std::env::current_exe() {
            if let Some(parent) = exe.parent() {
                roots.push(parent.to_path_buf());
            }
        }

        for start in roots {
            let mut current = start;
            for _ in 0..16 {
                if current.join("minEngine").join("CMakeLists.txt").is_file() {
                    return Ok(Some(current));
                }
                if !current.pop() {
                    break;
                }
            }
        }
        Ok(None)
    }

    fn engine_root_from_editor(editor: &Path) -> Option<PathBuf> {
        // <repo>/minEngine/bin/MaximumEditor.exe → <repo>
        let bin = editor.parent()?;
        let min_engine = bin.parent()?;
        if min_engine
            .file_name()
            .and_then(|n| n.to_str())
            .is_some_and(|n| n.eq_ignore_ascii_case("minEngine"))
        {
            return min_engine.parent().map(Path::to_path_buf);
        }
        // <repo>/bin/MaximumEditor.exe → <repo> if minEngine/CMakeLists exists beside bin
        if bin
            .parent()
            .map(|repo| repo.join("minEngine").join("CMakeLists.txt").is_file())
            .unwrap_or(false)
        {
            return bin.parent().map(Path::to_path_buf);
        }
        None
    }

    fn validate_engine_root(path: &Path) -> LauncherResult<String> {
        let cmake = path.join("minEngine").join("CMakeLists.txt");
        if !cmake.is_file() {
            return Err(LauncherError::Settings(format!(
                "engine_root '{}' is missing minEngine/CMakeLists.txt",
                path.display()
            )));
        }
        let normalized = normalize_path(path);
        Ok(normalized.to_string_lossy().replace('\\', "/"))
    }

    fn apply_placeholders_in_tree(root: &Path, vars: &TemplateVars) -> LauncherResult<()> {
        let mut stack = vec![root.to_path_buf()];
        while let Some(dir) = stack.pop() {
            for entry in fs::read_dir(&dir).map_err(|e| LauncherError::io(&dir, e))? {
                let entry = entry.map_err(|e| LauncherError::io(&dir, e))?;
                let path = entry.path();
                let file_type = entry.file_type().map_err(|e| LauncherError::io(&path, e))?;
                if file_type.is_dir() {
                    stack.push(path);
                    continue;
                }
                if !file_type.is_file() {
                    continue;
                }
                if !Self::is_text_template_file(&path) {
                    continue;
                }
                let contents = fs::read_to_string(&path).map_err(|e| LauncherError::io(&path, e))?;
                if !contents.contains("{{") {
                    continue;
                }
                let replaced = vars.apply(&contents);
                fs::write(&path, replaced).map_err(|e| LauncherError::io(&path, e))?;
            }
        }
        Ok(())
    }

    fn is_text_template_file(path: &Path) -> bool {
        matches!(
            path.extension().and_then(|e| e.to_str()).unwrap_or(""),
            "txt" | "md" | "cpp" | "h" | "hpp" | "cmake" | "json" | "meproject" | "mesettings"
                | "mescene" | "partial"
        ) || path
            .file_name()
            .and_then(|n| n.to_str())
            .is_some_and(|n| n.eq_ignore_ascii_case("CMakeLists.txt") || n.eq_ignore_ascii_case("README.md"))
    }

    fn rename_placeholder_paths(root: &Path, vars: &TemplateVars) -> LauncherResult<()> {
        loop {
            let mut renamed = false;
            let mut candidates = Vec::new();
            Self::collect_paths(root, &mut candidates)?;
            candidates.sort_by_key(|p| std::cmp::Reverse(p.components().count()));

            for path in candidates {
                let name = match path.file_name().and_then(|n| n.to_str()) {
                    Some(n) => n.to_owned(),
                    None => continue,
                };
                if !name.contains("{{") {
                    continue;
                }
                let new_name = vars.apply(&name);
                if new_name == name {
                    continue;
                }
                let dest = path
                    .parent()
                    .map(|p| p.join(&new_name))
                    .ok_or_else(|| LauncherError::Settings("invalid template path".into()))?;
                if dest.exists() {
                    return Err(LauncherError::Settings(format!(
                        "cannot rename '{}' → '{}' (destination exists)",
                        path.display(),
                        dest.display()
                    )));
                }
                fs::rename(&path, &dest).map_err(|e| LauncherError::io(&path, e))?;
                renamed = true;
                break;
            }

            if !renamed {
                break;
            }
        }
        Ok(())
    }

    fn collect_paths(root: &Path, out: &mut Vec<PathBuf>) -> LauncherResult<()> {
        for entry in fs::read_dir(root).map_err(|e| LauncherError::io(root, e))? {
            let entry = entry.map_err(|e| LauncherError::io(root, e))?;
            let path = entry.path();
            let file_type = entry.file_type().map_err(|e| LauncherError::io(&path, e))?;
            out.push(path.clone());
            if file_type.is_dir() {
                Self::collect_paths(&path, out)?;
            }
        }
        Ok(())
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

    fn validate_code_identifier_name(name: &str) -> LauncherResult<()> {
        if !name
            .chars()
            .next()
            .is_some_and(|c| c.is_ascii_alphabetic() || c == '_')
        {
            return Err(LauncherError::InvalidProjectName(
                "GameProject name must start with a letter or underscore".into(),
            ));
        }
        if !name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
            return Err(LauncherError::InvalidProjectName(
                "GameProject name may only contain ASCII letters, digits, and underscore".into(),
            ));
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::project_validator::ProjectValidator;
    use crate::settings::LauncherSettings;
    use std::fs;
    use std::path::PathBuf;

    fn templates_root() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../Templates")
    }

    fn repo_engine_root() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..")
    }

    #[test]
    fn create_stamps_disk_version_and_roundtrips() {
        let parent = tempfile::tempdir().unwrap();
        let mut settings = LauncherSettings::default();

        let descriptor_path = ProjectFactory::create(
            &mut settings,
            "SmokeProj",
            parent.path(),
            ProjectFactory::default_template_name(),
            &templates_root(),
        )
        .unwrap();

        let descriptor_text = fs::read_to_string(&descriptor_path).unwrap();
        assert!(descriptor_text.contains("\"$schemaVersion\": 1"));
        assert!(descriptor_text.contains("\"$engineVersion\": \"0.0.9\""));

        let settings_path = parent.path().join("SmokeProj").join("SmokeProjSettings.mesettings");
        let settings_text = fs::read_to_string(&settings_path).unwrap();
        assert!(settings_text.contains("\"$schemaVersion\": 1"));
        assert!(settings_text.contains("\"$engineVersion\": \"0.0.9\""));
        assert!(settings_text.contains("\"EditorDefaultSceneName\": \"default\""));

        let scene = parent
            .path()
            .join("SmokeProj")
            .join("Assets")
            .join("Scenes")
            .join("default.mescene");
        assert!(scene.is_file());

        let resolved = ProjectValidator::resolve(&descriptor_path).unwrap();
        assert_eq!(resolved.descriptor.project_name, "SmokeProj");
        assert_eq!(resolved.descriptor.schema_version, DISK_SCHEMA_VERSION);
        assert!(!resolved.descriptor.project_id.is_zero());
        assert_eq!(settings.recent_projects.len(), 1);
    }

    #[test]
    fn create_game_project_fills_placeholders() {
        let parent = tempfile::tempdir().unwrap();
        let mut settings = LauncherSettings {
            engine_root: Some(
                normalize_path(&repo_engine_root())
                    .to_string_lossy()
                    .into_owned(),
            ),
            ..LauncherSettings::default()
        };

        let descriptor_path = ProjectFactory::create(
            &mut settings,
            "DemoGame",
            parent.path(),
            "GameProject",
            &templates_root(),
        )
        .unwrap();

        let project_dir = descriptor_path.parent().unwrap();
        let cmake = fs::read_to_string(project_dir.join("CMakeLists.txt")).unwrap();
        assert!(cmake.contains("project(DemoGame "));
        assert!(cmake.contains("DemoGameEditor"));
        assert!(cmake.contains("DemoGameGame"));
        assert!(!cmake.contains("{{PROJECT_NAME}}"));
        assert!(!cmake.contains("{{ENGINE_ROOT}}"));

        let module = project_dir.join("Source").join("DemoGameGame").join("Module.cpp");
        assert!(module.is_file());
        let module_text = fs::read_to_string(&module).unwrap();
        assert!(module_text.contains("DemoGameGame_ModuleMarker"));
        assert!(!module_text.contains("{{GAME_MODULE_NAME}}"));

        let game_main = fs::read_to_string(project_dir.join("Source/Programs/GameMain.cpp")).unwrap();
        assert!(game_main.contains("DemoGameGameApplication"));
    }
}
