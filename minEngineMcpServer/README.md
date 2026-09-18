# minEngine MCP Server

Thin **stdio MCP** adapter that talks to Maximum via Automation Protocol (Named Pipe, u32 LE + UTF-8 JSON).

**No editor business logic** — only connect, translate `domain`+`action` → `kind`+`op`, and map errors. Capability catalog comes from Host `editor.get_capabilities`.

## Setup

```bash
cd minEngineMcpServer
npm install
npm run build
```

## Cursor / MCP config (example)

```json
{
  "mcpServers": {
    "minengine": {
      "command": "node",
      "args": ["D:/Dev/GitRepo/minEngine-mcp/minEngineMcpServer/dist/index.js"]
    }
  }
}
```

Then call `editor_connect` with `mode: "spawn"` or `"attach"`.

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
