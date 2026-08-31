mod commands;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            commands::list_recent,
            commands::remove_recent,
            commands::clear_recent,
            commands::open_project,
            commands::create_project,
            commands::get_settings,
            commands::save_settings,
            commands::set_editor_path,
            commands::set_projects_dir,
            commands::resolve_editor_status,
            commands::reveal_in_explorer,
        ])
        .run(tauri::generate_context!())
        .expect("error while running minEngine Launcher");
}
