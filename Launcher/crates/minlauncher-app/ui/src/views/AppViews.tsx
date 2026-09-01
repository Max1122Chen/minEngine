import { useCallback, useEffect, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import {
  clearRecent,
  getSettings,
  openProject,
  resolveEditorStatus,
  saveSettings,
  setEditorPath as persistEditorPath,
  setProjectsDir as persistProjectsDir,
  type EditorStatus,
  type LauncherSettings,
  type RecentEntryStatus,
  listRecent,
} from "../api/launcher";
import { NewProjectModal } from "../components/NewProjectModal";
import { RecentList } from "../components/RecentList";

interface ProjectsViewProps {
  onError: (message: string) => void;
  onEditorStatus: (status: EditorStatus) => void;
}

export function ProjectsView({ onError, onEditorStatus }: ProjectsViewProps) {
  const [entries, setEntries] = useState<RecentEntryStatus[]>([]);
  const [selectedPath, setSelectedPath] = useState<string | null>(null);
  const [showNewProject, setShowNewProject] = useState(false);
  const [defaultParent, setDefaultParent] = useState("");

  const refresh = useCallback(async () => {
    try {
      const [recent, settings, editor] = await Promise.all([
        listRecent(),
        getSettings(),
        resolveEditorStatus(),
      ]);
      setEntries(recent);
      onEditorStatus(editor);
      setDefaultParent(settings.DefaultProjectsDirectory ?? "");
    } catch (error) {
      onError(String(error));
    }
  }, [onError, onEditorStatus]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const handleOpenDialog = async () => {
    const selected = await open({
      multiple: false,
      title: "Open project",
      filters: [{ name: "minEngine Project", extensions: ["meproject"] }],
    });

    if (typeof selected === "string") {
      try {
        await openProject(selected);
        await refresh();
      } catch (error) {
        onError(String(error));
      }
    }
  };

  return (
    <>
      <div className="content-toolbar">
        <h2>Recent Projects</h2>
        <button type="button" className="primary" onClick={() => setShowNewProject(true)}>
          + New Project
        </button>
      </div>

      <div className="content-main">
        <RecentList
          entries={entries}
          selectedPath={selectedPath}
          onSelect={setSelectedPath}
          onChanged={refresh}
          onError={onError}
        />
      </div>

      <div style={{ padding: "0 20px 12px" }}>
        <button type="button" onClick={handleOpenDialog}>
          Open Project...
        </button>
      </div>

      {showNewProject && (
        <NewProjectModal
          defaultParent={defaultParent}
          onClose={() => setShowNewProject(false)}
          onCreated={refresh}
          onError={onError}
        />
      )}
    </>
  );
}

interface SettingsViewProps {
  onError: (message: string) => void;
  onEditorStatus: (status: EditorStatus) => void;
  onSaved: () => void;
}

export function SettingsView({ onError, onEditorStatus, onSaved }: SettingsViewProps) {
  const [settings, setSettings] = useState<LauncherSettings | null>(null);
  const [editorPath, setEditorPath] = useState("");
  const [projectsDir, setProjectsDir] = useState("");
  const [maxRecent, setMaxRecent] = useState(20);

  const load = useCallback(async () => {
    try {
      const loaded = await getSettings();
      setSettings(loaded);
      setEditorPath(loaded.EditorExecutablePath ?? "");
      setProjectsDir(loaded.DefaultProjectsDirectory ?? "");
      setMaxRecent(loaded.MaxRecentProjects);
      onEditorStatus(await resolveEditorStatus());
    } catch (error) {
      onError(String(error));
    }
  }, [onError, onEditorStatus]);

  useEffect(() => {
    void load();
  }, [load]);

  const browseEditor = async () => {
    const selected = await open({
      multiple: false,
      title: "Select Editor executable",
      filters: [{ name: "Editor", extensions: ["exe"] }],
    });
    if (typeof selected === "string") {
      setEditorPath(selected);
    }
  };

  const browseProjectsDir = async () => {
    const selected = await open({ directory: true, multiple: false, title: "Default projects directory" });
    if (typeof selected === "string") {
      setProjectsDir(selected);
    }
  };

  const handleSave = async () => {
    try {
      if (editorPath.trim()) {
        await persistEditorPath(editorPath.trim());
      }
      if (projectsDir.trim()) {
        await persistProjectsDir(projectsDir.trim());
      }
      if (settings) {
        const updated = { ...settings, MaxRecentProjects: maxRecent };
        await saveSettings(updated);
      }
      await load();
      onSaved();
    } catch (error) {
      onError(String(error));
    }
  };

  const handleClearRecent = async () => {
    try {
      await clearRecent();
      await load();
      onSaved();
    } catch (error) {
      onError(String(error));
    }
  };

  return (
    <>
      <div className="content-toolbar">
        <h2>Settings</h2>
      </div>

      <div className="content-main">
        <div className="settings-form">
          <div className="field-group">
            <label htmlFor="editor-path">Editor executable</label>
            <div className="field-row">
              <input id="editor-path" value={editorPath} onChange={(e) => setEditorPath(e.target.value)} placeholder="Path to Editor.exe" />
              <button type="button" onClick={browseEditor}>
                Browse...
              </button>
            </div>
          </div>

          <div className="field-group">
            <label htmlFor="projects-dir">Default projects directory</label>
            <div className="field-row">
              <input id="projects-dir" value={projectsDir} onChange={(e) => setProjectsDir(e.target.value)} />
              <button type="button" onClick={browseProjectsDir}>
                Browse...
              </button>
            </div>
          </div>

          <div className="field-group">
            <label htmlFor="max-recent">Recent projects — max entries</label>
            <select
              id="max-recent"
              value={maxRecent}
              onChange={(e) => setMaxRecent(Number(e.target.value))}
              style={{ width: 120 }}
            >
              {[5, 10, 20, 50].map((n) => (
                <option key={n} value={n}>
                  {n}
                </option>
              ))}
            </select>
          </div>

          <div className="settings-actions">
            <button type="button" onClick={handleClearRecent}>
              Clear all recent
            </button>
            <button type="button" className="primary" onClick={handleSave}>
              Save
            </button>
          </div>
        </div>
      </div>
    </>
  );
}
