import { invoke } from "@tauri-apps/api/core";

export interface RecentProject {
  ProjectName: string;
  DescriptorPath: string;
  LastOpenedUtc: string;
}

export interface RecentEntryStatus {
  entry: RecentProject;
  exists: boolean;
}

export interface LauncherSettings {
  EditorExecutablePath?: string | null;
  EngineRoot?: string | null;
  DefaultProjectsDirectory?: string | null;
  RecentProjects: RecentProject[];
  MaxRecentProjects: number;
}

export interface EditorStatus {
  path: string | null;
  source: string;
  ok: boolean;
}

export async function listRecent(): Promise<RecentEntryStatus[]> {
  return invoke("list_recent");
}

export async function removeRecent(path: string): Promise<void> {
  return invoke("remove_recent", { path });
}

export async function clearRecent(): Promise<void> {
  return invoke("clear_recent");
}

export async function openProject(path: string): Promise<void> {
  return invoke("open_project", { path });
}

export async function createProject(
  name: string,
  parent: string,
  template?: string,
): Promise<string> {
  return invoke("create_project", { name, parent, template });
}

export async function getSettings(): Promise<LauncherSettings> {
  return invoke("get_settings");
}

export async function saveSettings(settings: LauncherSettings): Promise<void> {
  return invoke("save_settings", { settings });
}

export async function setEditorPath(path: string): Promise<void> {
  return invoke("set_editor_path", { path });
}

export async function setProjectsDir(path: string): Promise<void> {
  return invoke("set_projects_dir", { path });
}

export async function resolveEditorStatus(): Promise<EditorStatus> {
  return invoke("resolve_editor_status");
}

export async function revealInExplorer(path: string): Promise<void> {
  return invoke("reveal_in_explorer", { path });
}
