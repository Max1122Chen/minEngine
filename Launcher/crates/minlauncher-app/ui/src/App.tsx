import { useCallback, useState } from "react";
import type { EditorStatus } from "./api/launcher";
import { resolveEditorStatus } from "./api/launcher";
import { StatusBar } from "./components/StatusBar";
import { ProjectsView, SettingsView } from "./views/AppViews";

type ViewId = "projects" | "settings";

export default function App() {
  const [view, setView] = useState<ViewId>("projects");
  const [error, setError] = useState<string | null>(null);
  const [editorStatus, setEditorStatus] = useState<EditorStatus | null>(null);

  const onError = useCallback((message: string) => {
    setError(message);
  }, []);

  const onEditorStatus = useCallback((status: EditorStatus) => {
    setEditorStatus(status);
  }, []);

  const refreshEditorStatus = useCallback(async () => {
    try {
      setEditorStatus(await resolveEditorStatus());
    } catch (err) {
      onError(String(err));
    }
  }, [onError]);

  return (
    <div className="app-shell">
      <header className="app-header">
        <h1>minEngine Launcher</h1>
      </header>

      <div className="app-body">
        <nav className="sidebar">
          <button
            type="button"
            className={`nav-item ${view === "projects" ? "active" : ""}`}
            onClick={() => {
              setView("projects");
              setError(null);
            }}
          >
            Projects
          </button>
          <button
            type="button"
            className={`nav-item ${view === "settings" ? "active" : ""}`}
            onClick={() => {
              setView("settings");
              setError(null);
            }}
          >
            Settings
          </button>
        </nav>

        <main className="content">
          {error && <div className="error-banner">{error}</div>}

          {view === "projects" ? (
            <ProjectsView onError={onError} onEditorStatus={onEditorStatus} />
          ) : (
            <SettingsView
              onError={onError}
              onEditorStatus={onEditorStatus}
              onSaved={() => {
                setError(null);
                void refreshEditorStatus();
              }}
            />
          )}
        </main>
      </div>

      <StatusBar editorStatus={editorStatus} onOpenSettings={() => setView("settings")} />
    </div>
  );
}
