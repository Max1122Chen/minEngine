#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";

import { ConnectionManager } from "./connection/manager.js";
import { registerTools } from "./tools/register.js";

async function main(): Promise<void> {
  const connection = new ConnectionManager();
  const server = new McpServer({
    name: "minEngine MCP Server",
    version: "0.1.0",
  });

  registerTools(server, connection);

  const transport = new StdioServerTransport();
  await server.connect(transport);

  const shutdown = async () => {
    await connection.disconnect({ killSpawned: true });
    process.exit(0);
  };
  process.on("SIGINT", () => void shutdown());
  process.on("SIGTERM", () => void shutdown());
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
