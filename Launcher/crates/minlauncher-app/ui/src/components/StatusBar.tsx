import type { EditorStatus } from "../api/launcher";

interface StatusBarProps {
  editorStatus: EditorStatus | null;
  onOpenSettings: () => void;
}

export function StatusBar({ editorStatus, onOpenSettings }: StatusBarProps) {
  let editorLabel = "Editor: not configured";
  let statusClass = "status-warn";

  if (editorStatus?.ok && editorStatus.path) {
    editorLabel = `Editor: ${editorStatus.path.split(/[/\\]/).pop()} ✓`;
    statusClass = "status-ok";
  } else if (editorStatus?.path) {
    editorLabel = "Editor: missing libminEngine.dll / libminEngined.dll ⚠";
    statusClass = "status-warn";
  }

  return (
    <footer className="status-bar">
      <span>Open Project launches Editor with --project</span>
      <span>
        <button type="button" className="nav-item" style={{ padding: "2px 8px", fontSize: 12 }} onClick={onOpenSettings}>
          Settings
        </button>
        {" · "}
        <span className={statusClass} onClick={onOpenSettings} style={{ cursor: "pointer" }} title={editorStatus?.path ?? undefined}>
          {editorLabel}
        </span>
      </span>
    </footer>
  );
}
