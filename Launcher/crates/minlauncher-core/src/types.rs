use serde::{Deserialize, Serialize};

pub const PROJECT_DESCRIPTOR_EXTENSION: &str = "meproject";
pub const PROJECT_SETTINGS_EXTENSION: &str = "mesettings";

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
    #[serde(rename = "ProjectName")]
    pub project_name: String,
    #[serde(rename = "ProjectId")]
    pub project_id: Guid,
    #[serde(rename = "ProjectRoot")]
    pub project_root: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ProjectSettings {
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
