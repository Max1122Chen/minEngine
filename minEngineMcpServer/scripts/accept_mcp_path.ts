/**
 * Full-stack acceptance over MCP (stdio), not raw Named Pipe.
 *
 * Cursor Agent → (same as) MCP Client SDK → minEngine MCP Server → Pipe → Maximum
 *
 * Usage:
 *   npx tsx scripts/accept_mcp_path.ts
 *   npx tsx scripts/accept_mcp_path.ts --hold 45
 */
import path from "node:path";
import { fileURLToPath } from "node:url";

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const serverEntry = path.join(root, "minEngineMcpServer", "dist", "index.js");
const editorPath = path.join(root, "minEngine", "bin", "MaximumEditor.exe");
const projectPath = path.join(root, "minEngine", "MyMEProject", "MyMEProject.meproject");
const workingDirectory = path.join(root, "minEngine", "bin");

function holdSecondsFromArgs(): number {
  const idx = process.argv.indexOf("--hold");
  if (idx >= 0 && process.argv[idx + 1]) {
    return Math.max(0, Number(process.argv[idx + 1]) || 0);
  }
  return 20;
}

function textFromToolResult(result: {
  content?: Array<{ type: string; text?: string }>;
  isError?: boolean;
}): string {
  const parts = (result.content ?? [])
    .filter((c) => c.type === "text" && typeof c.text === "string")
    .map((c) => c.text as string);
  return parts.join("\n");
}

async function callTool(client: Client, name: string, args: Record<string, unknown>) {
  console.log(`\n=== tools/call ${name} ===`);
  console.log(JSON.stringify(args, null, 2));
  const result = await client.callTool({ name, arguments: args });
  const text = textFromToolResult(result as { content?: Array<{ type: string; text?: string }>; isError?: boolean });
  console.log(text);
  if ((result as { isError?: boolean }).isError) {
    throw new Error(`Tool ${name} returned isError`);
  }
  return JSON.parse(text) as Record<string, unknown>;
}

async function main(): Promise<void> {
  const holdSeconds = holdSecondsFromArgs();

  const transport = new StdioClientTransport({
    command: "node",
    args: [serverEntry],
  });

  const client = new Client({ name: "minengine-accept", version: "0.1.0" });
  await client.connect(transport);

  const listed = await client.listTools();
  const toolNames = listed.tools.map((t) => t.name).sort();
  console.log("=== tools/list ===");
  console.log(toolNames.join(", "));
  const expected = [
    "editor_connect",
    "editor_get_state",
    "editor_query",
    "editor_edit",
    "editor_verify",
  ];
  for (const name of expected) {
    if (!toolNames.includes(name)) {
      throw new Error(`Missing tool: ${name}`);
    }
  }

  const connected = await callTool(client, "editor_connect", {
    mode: "spawn",
    editorPath,
    projectPath,
    openScene: "Assets/Scenes/test.mescene",
    workingDirectory,
    pipeName: "minEngine.mcp.accept",
    readyFilePath: path.join(workingDirectory, "_mcp_accept_ready.txt"),
  });

  if (!connected.ready) {
    throw new Error("editor_connect did not report ready");
  }

  const state = await callTool(client, "editor_get_state", {});
  const sessionId = String(state.defaultSessionId ?? "");
  if (!sessionId) {
    throw new Error("missing defaultSessionId");
  }

  await callTool(client, "editor_query", {
    domain: "scene",
    action: "get_hierarchy",
    sessionId,
    payload: { maxDepth: 1 },
  });

  const created = await callTool(client, "editor_edit", {
    domain: "scene",
    action: "create_game_object",
    sessionId,
    payload: { name: "McpAcceptDemo" },
  });
  const objectId = String(created.objectId ?? "");
  if (!objectId) {
    throw new Error("create_game_object missing objectId");
  }

  await callTool(client, "editor_edit", {
    domain: "scene",
    action: "add_component",
    sessionId,
    payload: { objectId, className: "StaticMeshComponent" },
  });

  await callTool(client, "editor_verify", {
    domain: "scene",
    action: "validate",
    sessionId,
    payload: {},
  });

  console.log(
    `\nMCP PATH ACCEPT PASS — look in Editor Hierarchy for "McpAcceptDemo" (holding ${holdSeconds}s)…`,
  );
  await new Promise((r) => setTimeout(r, holdSeconds * 1000));

  await callTool(client, "editor_edit", {
    domain: "scene",
    action: "delete_game_object",
    sessionId,
    payload: { objectId },
  });

  await client.close();
  console.log("Cleaned up demo object; MCP client closed (spawned Editor may exit with server).");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
