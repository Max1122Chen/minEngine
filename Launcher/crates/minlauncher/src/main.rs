mod cli;

use std::path::Path;
use std::process::ExitCode;

use anyhow::Context;
use clap::Parser;
use minlauncher_core::engine_locator::EngineLocator;
use minlauncher_core::process_launcher::ProcessLauncher;
use minlauncher_core::project_catalog::ProjectCatalog;
use minlauncher_core::project_factory::ProjectFactory;
use minlauncher_core::project_validator::ProjectValidator;
use minlauncher_core::settings::LauncherSettings;
use minlauncher_core::templates;
use minlauncher_core::LauncherError;

use crate::cli::{
    Cli, Commands, ConfigCommands, ConfigSetKey, RecentCommands,
};

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("error: {error:#}");
            ExitCode::from(map_exit_code(&error))
        }
    }
}

fn run() -> anyhow::Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Commands::Open { path, editor } => cmd_open(&path, editor.as_deref()),
        Commands::Create { name, parent, template } => cmd_create(&name, &parent, &template),
        Commands::Recent { command } => match command {
            RecentCommands::List => cmd_recent_list(),
            RecentCommands::Remove { path } => cmd_recent_remove(&path),
        },
        Commands::Config { command } => match command {
            ConfigCommands::Show => cmd_config_show(),
            ConfigCommands::Set { key } => cmd_config_set(key),
        },
    }
}

fn cmd_open(path: &Path, editor_override: Option<&Path>) -> anyhow::Result<()> {
    let mut settings = LauncherSettings::load()?;
    let resolved = ProjectValidator::resolve(path)?;
    let editor = EngineLocator::resolve_editor(editor_override, &settings)?;

    let pid = ProcessLauncher::launch_editor(&editor, &resolved.descriptor_path)?;
    ProjectCatalog::add_recent(&mut settings, &resolved.descriptor_path)?;
    settings.save()?;

    println!(
        "launched Editor (pid {pid}) for project {}",
        resolved.descriptor.project_name
    );
    println!("descriptor: {}", resolved.descriptor_path.display());
    Ok(())
}

fn cmd_create(name: &str, parent: &Path, template: &str) -> anyhow::Result<()> {
    let mut settings = LauncherSettings::load()?;
    let templates_root = templates::resolve_templates_root()?;
    let descriptor_path =
        ProjectFactory::create(&mut settings, name, parent, template, &templates_root)?;
    settings.save()?;

    println!("created project: {}", name);
    println!("descriptor: {}", descriptor_path.display());
    Ok(())
}

fn cmd_recent_list() -> anyhow::Result<()> {
    let settings = LauncherSettings::load()?;
    let entries = ProjectCatalog::list_recent(&settings);

    if entries.is_empty() {
        println!("no recent projects");
        return Ok(());
    }

    for entry in entries {
        let status = if entry.exists { "ok" } else { "missing" };
        println!(
            "[{status}] {} | {} | {}",
            entry.entry.project_name,
            entry.entry.descriptor_path,
            entry.entry.last_opened_utc
        );
    }

    Ok(())
}

fn cmd_recent_remove(path: &Path) -> anyhow::Result<()> {
    let mut settings = LauncherSettings::load()?;
    ProjectCatalog::remove_recent(&mut settings, path)?;
    settings.save()?;
    println!("removed from recent list: {}", path.display());
    Ok(())
}

fn cmd_config_show() -> anyhow::Result<()> {
    let settings = LauncherSettings::load()?;
    let path = LauncherSettings::settings_path()?;

    println!("settings file: {}", path.display());
    println!(
        "editor: {}",
        settings
            .editor_executable_path
            .as_deref()
            .unwrap_or("<not set>")
    );
    println!(
        "engine root: {}",
        settings.engine_root.as_deref().unwrap_or("<not set>")
    );
    println!(
        "projects dir: {}",
        settings
            .default_projects_directory
            .as_deref()
            .unwrap_or("<not set>")
    );
    println!("recent count: {}", settings.recent_projects.len());
    Ok(())
}

fn cmd_config_set(key: crate::cli::ConfigSetKey) -> anyhow::Result<()> {
    let mut settings = LauncherSettings::load()?;

    match key {
        ConfigSetKey::Editor { path } => {
            let validated = EngineLocator::validate_editor(&path)
                .with_context(|| format!("invalid editor path: {}", path.display()))?;
            settings.set_editor_executable_path(validated);
        }
        ConfigSetKey::ProjectsDir { path } => {
            let path = path
                .canonicalize()
                .with_context(|| format!("invalid projects directory: {}", path.display()))?;
            settings.set_default_projects_directory(path);
        }
    }

    settings.save()?;
    println!("settings updated");
    Ok(())
}

fn map_exit_code(error: &anyhow::Error) -> u8 {
    if error
        .chain()
        .find_map(|cause| cause.downcast_ref::<LauncherError>())
        .is_some()
    {
        return 1;
    }

    1
}
