import { z } from "zod";
import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";

import { composeOp, kindForTool } from "../automation/protocol.js";
import { assertOk, type ConnectionManager } from "../connection/manager.js";

function textResult(data: unknown) {
  return {
    content: [{ type: "text" as const, text: JSON.stringify(data, null, 2) }],
  };
}

function errorResult(err: unknown) {
  const message = err instanceof Error ? err.message : String(err);
  return {
    isError: true as const,
    content: [{ type: "text" as const, text: message }],
  };
}

function describeCapabilities(capabilities: unknown): string {
  if (!capabilities || typeof capabilities !== "object") {
    return "Connect first; fine-grained actions come from Host editor.get_capabilities.";
  }
  const domains = (capabilities as { domains?: Array<{ name?: string; actions?: Array<{ name?: string; kind?: string; op?: string }> }> })
    .domains;
  if (!Array.isArray(domains)) {
    return JSON.stringify(capabilities);
  }
  const lines: string[] = [];
  for (const domain of domains) {
    const actions = domain.actions ?? [];
    for (const action of actions) {
      lines.push(`- ${action.op ?? `${domain.name}.${action.name}`} (${action.kind})`);
    }
  }
  return lines.length > 0 ? lines.join("\n") : "No actions reported by Host.";
}

export function registerTools(server: McpServer, connection: ConnectionManager): void {
  server.registerTool(
    "editor_connect",
    {
      title: "Connect to Maximum",
      description:
        "Connect to Maximum Automation Host via spawn (launch Editor with CLI-F02 flags) or attach (existing pipe).",
      inputSchema: {
        mode: z.enum(["spawn", "attach"]),
        pipeName: z.string().optional().describe("Required for attach; optional for spawn (auto-generated)."),
        editorPath: z.string().optional().describe("Absolute path to MaximumEditor.exe (spawn)."),
        projectPath: z.string().optional().describe("Absolute path to .meproject (spawn)."),
        openScene: z.string().optional().describe("Optional --open-scene path (spawn)."),
        workingDirectory: z.string().optional().describe("Working directory for Editor (spawn; default: editor dir)."),
        readyFilePath: z.string().optional(),
      },
    },
    async (args) => {
      try {
        if (args.mode === "attach") {
          if (!args.pipeName) {
            throw new Error("pipeName is required for attach.");
          }
          const snap = await connection.attach({ pipeName: args.pipeName });
          return textResult(snap);
        }

        if (!args.editorPath || !args.projectPath) {
          throw new Error("editorPath and projectPath are required for spawn.");
        }
        const snap = await connection.spawn({
          editorPath: args.editorPath,
          projectPath: args.projectPath,
          openScene: args.openScene,
          pipeName: args.pipeName,
          readyFilePath: args.readyFilePath,
          workingDirectory: args.workingDirectory,
        });
        return textResult(snap);
      } catch (err) {
        return errorResult(err);
      }
    },
  );

  server.registerTool(
    "editor_get_state",
    {
      title: "Editor state",
      description: "Return project path and open document sessions (from Host editor.get_state).",
      inputSchema: {},
    },
    async () => {
      try {
        connection.ensureReady();
        const state = await connection.refreshState();
        return textResult(state);
      } catch (err) {
        return errorResult(err);
      }
    },
  );

  server.registerTool(
    "editor_query",
    {
      title: "Editor query",
      description:
        "Run a query op on Maximum (Automation Protocol). Pass domain + action (op = domain.action). " +
        "Fine-grained actions come from Host editor.get_capabilities after editor_connect — not a TS-owned catalog.",
      inputSchema: {
        domain: z.string().describe("e.g. scene or editor"),
        action: z.string().describe("e.g. get_hierarchy or get_property"),
        sessionId: z.string().optional(),
        payload: z.record(z.unknown()).optional(),
      },
    },
    async (args) => {
      try {
        connection.ensureReady();
        const op = composeOp(args.domain, args.action);
        const response = await connection.getClient().request({
          kind: kindForTool("query"),
          op,
          sessionId: args.sessionId,
          payload: args.payload ?? {},
        });
        assertOk(response, op);
        const snap = connection.getSnapshot();
        return textResult({
          result: response.result ?? null,
          availableActionsHint: describeCapabilities(snap.capabilities),
        });
      } catch (err) {
        return errorResult(err);
      }
    },
  );

  server.registerTool(
    "editor_edit",
    {
      title: "Editor edit",
      description:
        "Run an edit op on Maximum (session Command stack / Undo). sessionId is required. " +
        "op = domain.action. Actions are validated by Host CapabilityRegistry.",
      inputSchema: {
        domain: z.string(),
        action: z.string(),
        sessionId: z.string().describe("Required for edits."),
        payload: z.record(z.unknown()).optional(),
      },
    },
    async (args) => {
      try {
        connection.ensureReady();
        if (!args.sessionId) {
          throw new Error("sessionId is required for editor_edit.");
        }
        const op = composeOp(args.domain, args.action);
        const response = await connection.getClient().request({
          kind: kindForTool("edit"),
          op,
          sessionId: args.sessionId,
          payload: args.payload ?? {},
        });
        assertOk(response, op);
        return textResult(response.result ?? null);
      } catch (err) {
        return errorResult(err);
      }
    },
  );

  server.registerTool(
    "editor_verify",
    {
      title: "Editor verify",
      description:
        "Run a verify op on Maximum. op = domain.action. Host is the source of truth for allowed actions.",
      inputSchema: {
        domain: z.string(),
        action: z.string(),
        sessionId: z.string().optional(),
        payload: z.record(z.unknown()).optional(),
      },
    },
    async (args) => {
      try {
        connection.ensureReady();
        const op = composeOp(args.domain, args.action);
        const response = await connection.getClient().request({
          kind: kindForTool("verify"),
          op,
          sessionId: args.sessionId,
          payload: args.payload ?? {},
        });
        assertOk(response, op);
        return textResult(response.result ?? null);
      } catch (err) {
        return errorResult(err);
      }
    },
  );

  server.registerTool(
    "editor_control",
    {
      title: "Editor control",
      description:
        "Change what the user is looking at (open asset, focus session, select object, reveal in Content Browser). " +
        "Does not push a Scene undo command. op = domain.action.",
      inputSchema: {
        domain: z.string(),
        action: z.string(),
        sessionId: z.string().optional(),
        payload: z.record(z.unknown()).optional(),
      },
    },
    async (args) => {
      try {
        connection.ensureReady();
        const op = composeOp(args.domain, args.action);
        const response = await connection.getClient().request({
          kind: kindForTool("control"),
          op,
          sessionId: args.sessionId,
          payload: args.payload ?? {},
        });
        assertOk(response, op);
        return textResult(response.result ?? null);
      } catch (err) {
        return errorResult(err);
      }
    },
  );

  server.registerTool(
    "editor_asset",
    {
      title: "Editor asset",
      description:
        "Create or delete an asset. Not a Scene undo unit. Listing assets uses editor_query asset.list. op = domain.action.",
      inputSchema: {
        domain: z.string(),
        action: z.string(),
        payload: z.record(z.unknown()).optional(),
      },
    },
    async (args) => {
      try {
        connection.ensureReady();
        const op = composeOp(args.domain, args.action);
        const response = await connection.getClient().request({
          kind: kindForTool("asset"),
          op,
          payload: args.payload ?? {},
        });
        assertOk(response, op);
        return textResult(response.result ?? null);
      } catch (err) {
        return errorResult(err);
      }
    },
  );
}
