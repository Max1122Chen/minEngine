use serde::{Deserialize, Serialize};

pub const PROJECT_DESCRIPTOR_EXTENSION: &str = "meproject";
pub const PROJECT_SETTINGS_EXTENSION: &str = "mesettings";

/// Matches `kDiskSchemaVersion` in the engine. Missing or 0 is treated as legacy.
pub const DISK_SCHEMA_VERSION: u32 = 1;
/// Matches `kEngineVersion` in `EngineVersion.h`. Bump both together (0.1.0 is deferred).
pub const ENGINE_VERSION_STAMP: &str = "0.0.9";

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Guid {
    #[serde(rename = "High")]
    pub high: u64,
    #[serde(rename = "Low")]
    pub low: u64,
}

impl Guid {
    pub fn is_zero(&self) -> bool {
        self.high == 0 && self.low == 0
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProjectDescriptor {
    #[serde(rename = "$schemaVersion", default)]
    pub schema_version: u32,
    #[serde(rename = "$engineVersion", default, skip_serializing_if = "String::is_empty")]
    pub engine_version: String,
    #[serde(rename = "ProjectName")]
    pub project_name: String,
    #[serde(rename = "ProjectId")]
    pub project_id: Guid,
    #[serde(rename = "ProjectRoot")]
    pub project_root: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProjectSettings {
    #[serde(rename = "$schemaVersion", default)]
    pub schema_version: u32,
    #[serde(rename = "$engineVersion", default, skip_serializing_if = "String::is_empty")]
    pub engine_version: String,
    #[serde(rename = "EditorDefaultSceneName")]
    pub editor_default_scene_name: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RecentProject {
    #[serde(rename = "ProjectName")]
    pub project_name: String,
    #[serde(rename = "DescriptorPath")]
    pub descriptor_path: String,
    #[serde(rename = "LastOpenedUtc")]
    pub last_opened_utc: String,
}
