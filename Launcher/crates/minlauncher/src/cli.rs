use std::path::PathBuf;

use clap::{Parser, Subcommand};

#[derive(Parser)]
#[command(name = "minlauncher", version, about = "minEngine project launcher")]
pub struct Cli {
    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand)]
pub enum Commands {
    /// Open a project in Editor.
    Open {
        /// Path to a .meproject file or project directory.
        path: PathBuf,
        /// Override editor executable path.
        #[arg(long)]
        editor: Option<PathBuf>,
    },
    /// Create a new project from a template.
    Create {
        /// Project name (also used as directory name).
        name: String,
        /// Parent directory for the new project.
        #[arg(long)]
        parent: PathBuf,
        /// Template name under Launcher/Templates.
        #[arg(long, default_value = "Empty")]
        template: String,
    },
    /// Manage recent projects.
    Recent {
        #[command(subcommand)]
        command: RecentCommands,
    },
    /// View or update launcher settings.
    Config {
        #[command(subcommand)]
        command: ConfigCommands,
    },
}

#[derive(Subcommand)]
pub enum RecentCommands {
    /// List recent projects.
    List,
    /// Remove a project from the recent list (does not delete files).
    Remove {
        /// Descriptor path or project directory.
        path: PathBuf,
    },
}

#[derive(Subcommand)]
pub enum ConfigCommands {
    /// Show current settings.
    Show,
    /// Set a configuration value.
    Set {
        #[command(subcommand)]
        key: ConfigSetKey,
    },
}

#[derive(Subcommand)]
pub enum ConfigSetKey {
    /// Path to Editor executable.
    Editor { path: PathBuf },
    /// Default parent directory for new projects.
    #[command(name = "projects-dir")]
    ProjectsDir { path: PathBuf },
}
