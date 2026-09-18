import fs from "node:fs/promises";
import path from "node:path";
import { spawn, type ChildProcess } from "node:child_process";
import { randomBytes } from "node:crypto";

import { AutomationPipeClient } from "../automation/pipeClient.js";
import type { AutomationResponse } from "../automation/protocol.js";

export type ConnectionMode = "spawn" | "attach" | "disconnected";

export interface EditorStateSummary {
  projectPath?: string;
  ready?: boolean;
  sessions?: unknown[];
  defaultSessionId?: string;
  [key: string]: unknown;
}

export interface ConnectionSnapshot {
  mode: ConnectionMode;
  pipeName: string | null;
  ready: boolean;
  capabilities: unknown | null;
  state: EditorStateSummary | null;
  editorPid: number | null;
}

export interface AttachOptions {
  pipeName: string;
}

export interface SpawnOptions {
  editorPath: string;
  projectPath: string;
  openScene?: string;
  /** Optional; generated if omitted. */
  pipeName?: string;
  /** Optional; generated under os.tmpdir if omitted. */
  readyFilePath?: string;
  workingDirectory?: string;
  extraArgs?: string[];
  readyTimeoutMs?: number;
}

export class ConnectionManager {
  private client = new AutomationPipeClient();
  private mode: ConnectionMode = "disconnected";
  private pipeName: string | null = null;
  private capabilities: unknown | null = null;
  private state: EditorStateSummary | null = null;
  private child: ChildProcess | null = null;
  private readyFilePath: string | null = null;

  getClient(): AutomationPipeClient {
    return this.client;
  }

  getSnapshot(): ConnectionSnapshot {
    return {
      mode: this.mode,
      pipeName: this.pipeName,
      ready: this.mode !== "disconnected" && this.capabilities !== null,
      capabilities: this.capabilities,
      state: this.state,
      editorPid: this.child?.pid ?? null,
    };
  }

  async attach(options: AttachOptions): Promise<ConnectionSnapshot> {
    await this.disconnect({ killSpawned: false });
    this.pipeName = options.pipeName;
    await this.client.connect(options.pipeName);
    this.mode = "attach";
    await this.bootstrapAfterConnect();
    return this.getSnapshot();
  }

  async spawn(options: SpawnOptions): Promise<ConnectionSnapshot> {
    await this.disconnect({ killSpawned: true });

    const pipeName = options.pipeName ?? `minEngine.mcp.${process.pid}.${randomBytes(4).toString("hex")}`;
    const readyFilePath =
      options.readyFilePath ??
      path.join(process.env.TEMP || process.env.TMP || ".", `minengine-mcp-ready-${process.pid}.txt`);

    try {
      await fs.unlink(readyFilePath);
    } catch {
      // ignore missing
    }

    const args = [
      "--project",
      options.projectPath,
      "--automation-pipe",
      pipeName,
      "--automation-ready-file",
      readyFilePath,
      ...(options.openScene ? ["--open-scene", options.openScene] : []),
      ...(options.extraArgs ?? []),
    ];

    const cwd = options.workingDirectory ?? path.dirname(options.editorPath);
    this.child = spawn(options.editorPath, args, {
      cwd,
      stdio: "ignore",
      windowsHide: false,
      detached: false,
    });
    this.child.on("exit", () => {
      if (this.mode === "spawn") {
        void this.disconnect({ killSpawned: false });
      }
    });

    this.pipeName = pipeName;
    this.readyFilePath = readyFilePath;
    this.mode = "spawn";

    await waitForFile(readyFilePath, options.readyTimeoutMs ?? 90_000);
    await this.client.connect(pipeName);
    await this.bootstrapAfterConnect();
    return this.getSnapshot();
  }

  async refreshState(): Promise<EditorStateSummary> {
    this.ensureReady();
    const response = await this.client.request({
      kind: "query",
      op: "editor.get_state",
    });
    assertOk(response, "editor.get_state");
    this.state = (response.result ?? {}) as EditorStateSummary;
    return this.state;
  }

  async disconnect(options: { killSpawned: boolean }): Promise<void> {
    await this.client.close();
    this.capabilities = null;
    this.state = null;
    this.pipeName = null;
    this.mode = "disconnected";

    if (options.killSpawned && this.child && this.child.pid) {
      try {
        this.child.kill();
      } catch {
        // best-effort
      }
    }
    this.child = null;

    if (this.readyFilePath) {
      try {
        await fs.unlink(this.readyFilePath);
      } catch {
        // ignore
      }
      this.readyFilePath = null;
    }
  }

  ensureReady(): void {
    if (this.mode === "disconnected" || !this.client.connected) {
      throw new Error("Not connected to Maximum Automation Host. Call editor_connect first.");
    }
    if (this.capabilities === null) {
      throw new Error("Connected but capabilities were not loaded from Host.");
    }
  }

  private async bootstrapAfterConnect(): Promise<void> {
    const caps = await this.client.request({
      kind: "query",
      op: "editor.get_capabilities",
    });
    assertOk(caps, "editor.get_capabilities");
    this.capabilities = caps.result ?? null;

    const state = await this.client.request({
      kind: "query",
      op: "editor.get_state",
    });
    assertOk(state, "editor.get_state");
    this.state = (state.result ?? {}) as EditorStateSummary;
  }
}

export function assertOk(response: AutomationResponse, op: string): void {
  if (response.ok) {
    return;
  }
  const first = response.errors?.[0];
  const code = first?.code ?? "HostError";
  const message = first?.message ?? "Automation request failed.";
  throw new Error(`[${code}] ${op}: ${message}`);
}

async function waitForFile(filePath: string, timeoutMs: number): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      await fs.access(filePath);
      return;
    } catch {
      await new Promise((r) => setTimeout(r, 200));
    }
  }
  throw new Error(`Timed out waiting for automation ready-file: ${filePath}`);
}
