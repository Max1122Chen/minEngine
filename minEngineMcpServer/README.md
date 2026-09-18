# minEngine MCP Server

Thin **stdio MCP** adapter that talks to Maximum via Automation Protocol (Named Pipe, u32 LE + UTF-8 JSON).

**No editor business logic** — only connect, translate `domain`+`action` → `kind`+`op`, and map errors. Capability catalog comes from Host `editor.get_capabilities`.

## Setup

```bash
cd minEngineMcpServer
npm install
npm run build
```

## Cursor / MCP config

Project config is committed at [`.cursor/mcp.json`](../.cursor/mcp.json):

```json
{
  "mcpServers": {
    "minengine": {
      "command": "node",
      "args": ["${workspaceFolder}/minEngineMcpServer/dist/index.js"]
    }
  }
}
```

1. `cd minEngineMcpServer && npm install && npm run build`
2. Cursor: **Settings → MCP** — confirm `minengine` is listed (reload window if needed)
3. In chat, tools `editor_connect` / `editor_query` / … should appear under that server

### Full MCP-path acceptance (no Cursor UI required)

Simulates Cursor: MCP Client SDK → stdio → this server → Pipe → Maximum:

```bash
npm run build
npx tsx scripts/accept_mcp_path.ts --hold 30
```

Expect: `tools/list` shows 5 tools; spawn Editor; create `McpAcceptDemo`; hold so you can see Hierarchy; then delete.

## Tools

| Tool | Role |
|------|------|
| `editor_connect` | spawn \| attach |
| `editor_get_state` | sessions / project |
| `editor_query` | domain + action → query |
| `editor_edit` | domain + action + sessionId → edit |
| `editor_verify` | domain + action → verify |

## Smoke (Host already listening)

```bash
# Terminal A: MaximumEditor with --automation-pipe minEngine.mcp.smoke ...
npm run smoke:ping -- minEngine.mcp.smoke
```

## Docs

- Design: `docs/ai/Editor/MCP/MCP-F03_MCP_SERVER_DESIGN.md`
- Protocol: `schemas/automation/v1/`
