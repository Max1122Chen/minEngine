import { useEffect, useRef, useState } from "react";
import type { RecentEntryStatus } from "../api/launcher";
import { openProject, removeRecent, revealInExplorer } from "../api/launcher";
import { formatRelativeTime } from "../utils/format";

interface RecentListProps {
  entries: RecentEntryStatus[];
  selectedPath: string | null;
  onSelect: (path: string) => void;
  onChanged: () => void;
  onError: (message: string) => void;
}

interface ContextMenuState {
  x: number;
  y: number;
  entry: RecentEntryStatus;
}

export function RecentList({
  entries,
  selectedPath,
  onSelect,
  onChanged,
  onError,
}: RecentListProps) {
  const [contextMenu, setContextMenu] = useState<ContextMenuState | null>(null);
  const listRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const close = () => setContextMenu(null);
    window.addEventListener("click", close);
    return () => window.removeEventListener("click", close);
  }, []);

  const handleOpen = async (path: string) => {
    try {
      await openProject(path);
      onChanged();
    } catch (error) {
      onError(String(error));
    }
  };

  const handleRemove = async (path: string) => {
    try {
      await removeRecent(path);
      onChanged();
    } catch (error) {
      onError(String(error));
    }
  };

  const handleReveal = async (path: string) => {
    try {
      await revealInExplorer(path);
    } catch (error) {
      onError(String(error));
    }
  };

  if (entries.length === 0) {
    return (
      <div className="empty-state">
        No recent projects — create a new project or open an existing one.
      </div>
    );
  }

  return (
    <>
      <div className="recent-list" ref={listRef}>
        {entries.map((item) => {
          const path = item.entry.DescriptorPath;
          const isSelected = selectedPath === path;
          return (
            <div
              key={path}
              className={`recent-item ${isSelected ? "selected" : ""} ${item.exists ? "" : "missing"}`}
              onClick={() => onSelect(path)}
              onDoubleClick={() => handleOpen(path)}
              onContextMenu={(event) => {
                event.preventDefault();
                setContextMenu({ x: event.clientX, y: event.clientY, entry: item });
              }}
            >
              <div className="recent-item-row">
                <span className="recent-name">
                  {isSelected ? "▌ " : ""}
                  {item.entry.ProjectName}
                  {!item.exists ? " (missing)" : ""}
                </span>
                <span className="recent-time">{formatRelativeTime(item.entry.LastOpenedUtc)}</span>
              </div>
              <div className="recent-path">{path}</div>
            </div>
          );
        })}
      </div>

      {contextMenu && (
        <div className="context-menu" style={{ left: contextMenu.x, top: contextMenu.y }} onClick={(e) => e.stopPropagation()}>
          <button type="button" onClick={() => handleOpen(contextMenu.entry.entry.DescriptorPath)}>
            Open
          </button>
          <button type="button" onClick={() => handleReveal(contextMenu.entry.entry.DescriptorPath)}>
            Reveal in Explorer
          </button>
          <button type="button" onClick={() => handleRemove(contextMenu.entry.entry.DescriptorPath)}>
            Remove from list
          </button>
        </div>
      )}
    </>
  );
}
