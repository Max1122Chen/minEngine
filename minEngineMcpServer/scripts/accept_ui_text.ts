/**
 * Detached Editor + MCP attach scaffold for UI-F04 Text visual accept.
 *
 *   cd minEngineMcpServer
 *   npx tsx scripts/accept_ui_text.ts
 *
 * Leaves Editor running after MCP disconnect.
 */
import fs from "node:fs";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const serverEntry = path.join(root, "minEngineMcpServer", "dist", "index.js");
const editorPath = path.join(root, "minEngine", "bin", "MaximumEditor.exe");
const projectPath = path.join(root, "minEngine", "MyMEProject", "MyMEProject.meproject");
const workingDirectory = path.join(root, "minEngine", "bin");
const pipeName = "minEngine.mcp.ui.text";
const readyFilePath = path.join(workingDirectory, "_mcp_ui_text_ready.txt");

function textFromToolResult(result: {
  content?: Array<{ type: string; text?: string }>;
  isError?: boolean;
}): string {
  return (result.content ?? [])
    .filter((c) => c.type === "text" && typeof c.text === "string")
    .map((c) => c.text as string)
    .join("\n");
}

async function callTool(client: Client, name: string, args: Record<string, unknown>) {
  console.log(`\n=== tools/call ${name} ===`);
  console.log(JSON.stringify(args, null, 2));
  const result = await client.callTool({ name, arguments: args });
  const text = textFromToolResult(
    result as { content?: Array<{ type: string; text?: string }>; isError?: boolean },
  );
  console.log(text);
  if ((result as { isError?: boolean }).isError) {
    throw new Error(`Tool ${name} returned isError: ${text}`);
  }
  return JSON.parse(text) as Record<string, unknown>;
}

async function waitForReadyFile(filePath: string, timeoutMs: number): Promise<void> {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (fs.existsSync(filePath)) return;
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error(`Timed out waiting for ready file: ${filePath}`);
}

async function spawnEditorDetached(): Promise<number> {
  try {
    fs.unlinkSync(readyFilePath);
  } catch {
    // ignore
  }
  const child = spawn(
    editorPath,
    [
      "--project",
      projectPath,
      "--automation-pipe",
      pipeName,
      "--automation-ready-file",
      readyFilePath,
      "--open-scene",
      "Assets/Scenes/test.mescene",
    ],
    {
      cwd: workingDirectory,
      stdio: "ignore",
      windowsHide: false,
      detached: true,
    },
  );
  child.unref();
  if (!child.pid) throw new Error("Failed to spawn MaximumEditor");
  console.log(`Spawned MaximumEditor pid=${child.pid} (detached)`);
  await waitForReadyFile(readyFilePath, 90_000);
  return child.pid;
}

async function main(): Promise<void> {
  const editorPid = await spawnEditorDetached();
  const transport = new StdioClientTransport({ command: "node", args: [serverEntry] });
  const client = new Client({ name: "minengine-ui-text-accept", version: "0.1.0" });
  await client.connect(transport);

  const connected = await callTool(client, "editor_connect", {
    mode: "attach",
    pipeName,
  });
  if (!connected.ready) throw new Error("editor_connect not ready");

  const state = await callTool(client, "editor_get_state", {});
  const sessionId = String(state.defaultSessionId ?? "");
  if (!sessionId) throw new Error("missing defaultSessionId");

  const canvasCreated = await callTool(client, "editor_edit", {
    domain: "scene",
    action: "create_game_object",
    sessionId,
    payload: { name: "UI_F04_Canvas" },
  });
  const canvasId = String(canvasCreated.objectId ?? "");
  if (!canvasId) throw new Error("create canvas failed");

  await callTool(client, "editor_edit", {
    domain: "scene",
    action: "add_component",
    sessionId,
    payload: { objectId: canvasId, className: "CanvasComponent" },
  });

  const textCreated = await callTool(client, "editor_edit", {
    domain: "scene",
    action: "create_game_object",
    sessionId,
    payload: { name: "UI_F04_Text" },
  });
  const textGoId = String(textCreated.objectId ?? "");
  if (!textGoId) throw new Error("create text GO failed");

  await callTool(client, "editor_edit", {
    domain: "scene",
    action: "reparent",
    sessionId,
    payload: { objectId: textGoId, newParentObjectId: canvasId },
  });

  const widgetAdd = await callTool(client, "editor_edit", {
    domain: "scene",
    action: "add_component",
    sessionId,
    payload: { objectId: textGoId, className: "WidgetComponent" },
  });
  const textAdd = await callTool(client, "editor_edit", {
    domain: "scene",
    action: "add_component",
    sessionId,
    payload: { objectId: textGoId, className: "TextComponent" },
  });

  const widgetId = String(widgetAdd.componentObjectId ?? "");
  const textCompId = String(textAdd.componentObjectId ?? "");

  if (widgetId) {
    await callTool(client, "editor_edit", {
      domain: "scene",
      action: "set_property",
      sessionId,
      payload: { objectId: widgetId, propertyPath: "m_Size", value: [480, 96] },
    });
  }
  if (textCompId) {
    await callTool(client, "editor_edit", {
      domain: "scene",
      action: "set_property",
      sessionId,
      payload: { objectId: textCompId, propertyPath: "m_FontSize", value: 48 },
    });
  }

  await callTool(client, "editor_control", {
    domain: "scene",
    action: "select_object",
    sessionId,
    payload: { objectId: textGoId },
  });

  await callTool(client, "editor_query", {
    domain: "scene",
    action: "get_hierarchy",
    sessionId,
    payload: { maxDepth: 3 },
  });

  console.log(
    "\nUI-F04 TEXT ACCEPT READY\n" +
      `Editor pid=${editorPid} left running (detached).\n` +
      "Hierarchy: UI_F04_Canvas / UI_F04_Text (Widget + Text)\n" +
      "1) UI_F04_Text selected; Content defaults to Hello UI-F04\n" +
      "2) Inspector -> Font = Assets/Fonts/Inter_18pt-Regular.ttf\n" +
      "3) Check Viewport for glyphs\n",
  );

  await client.close();
  console.log("MCP client closed; Editor should remain open.");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});