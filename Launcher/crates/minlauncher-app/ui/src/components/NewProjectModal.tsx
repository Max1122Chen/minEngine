import { useEffect, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import { createProject, openProject } from "../api/launcher";

interface NewProjectModalProps {
  defaultParent: string;
  onClose: () => void;
  onCreated: () => void;
  onError: (message: string) => void;
}

export function NewProjectModal({ defaultParent, onClose, onCreated, onError }: NewProjectModalProps) {
  const [name, setName] = useState("");
  const [parent, setParent] = useState(defaultParent);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    setParent(defaultParent);
  }, [defaultParent]);

  const preview =
    name.trim().length > 0
      ? `${parent.replace(/[/\\]+$/, "")}\\${name.trim()}\\${name.trim()}.meproject`
      : "";

  const browseParent = async () => {
    const selected = await open({ directory: true, multiple: false, title: "Select parent directory" });
    if (typeof selected === "string") {
      setParent(selected);
    }
  };

  const handleCreate = async (andOpen: boolean) => {
    const trimmed = name.trim();
    if (!trimmed) {
      onError("Project name cannot be empty");
      return;
    }
    if (!parent.trim()) {
      onError("Parent directory is required");
      return;
    }

    setBusy(true);
    try {
      const descriptorPath = await createProject(trimmed, parent, "Empty");
      if (andOpen) {
        await openProject(descriptorPath);
      }
      onCreated();
      onClose();
    } catch (error) {
      onError(String(error));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()} role="dialog" aria-modal="true">
        <div className="modal-header">
          <h3>New Project</h3>
          <button type="button" className="modal-close" onClick={onClose} aria-label="Close">
            ×
          </button>
        </div>

        <div className="field-group">
          <label htmlFor="project-name">Project name</label>
          <input
            id="project-name"
            value={name}
            onChange={(e) => setName(e.target.value)}
            placeholder="MyGame"
            autoFocus
          />
        </div>

        <div className="field-group">
          <label htmlFor="project-parent">Location</label>
          <div className="field-row">
            <input id="project-parent" value={parent} onChange={(e) => setParent(e.target.value)} />
            <button type="button" onClick={browseParent}>
              Browse...
            </button>
          </div>
          {preview && <span style={{ fontSize: 11, color: "var(--me-text-muted)" }}>Preview: {preview}</span>}
        </div>

        <div className="field-group">
          <label>Template</label>
          <div className="template-grid">
            <div className="template-card active">Empty</div>
            <div className="template-card disabled">(future)</div>
          </div>
        </div>

        <div className="modal-actions">
          <button type="button" onClick={onClose} disabled={busy}>
            Cancel
          </button>
          <button type="button" onClick={() => handleCreate(false)} disabled={busy}>
            Create
          </button>
          <button type="button" className="primary" onClick={() => handleCreate(true)} disabled={busy}>
            Create &amp; Open
          </button>
        </div>
      </div>
    </div>
  );
}
