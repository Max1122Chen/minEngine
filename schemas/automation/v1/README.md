# Automation Protocol v1

Shared wire contract between **minEngine MCP Server** and **Maximum** (Automation Host).

**Design:** [`docs/ai/Editor/MCP/MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md`](../../../docs/ai/Editor/MCP/MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md)

## Transport frame (not part of JSON Schema)

On Named Pipe (and any future local transport):

```text
[u32 little-endian byte length of body][UTF-8 JSON body]
```

- Length covers the JSON body only (not the 4-byte header).
- One request or one response per frame.
- `protocolVersion` inside JSON must be `1` for this directory.

## Files

| File | Role |
|------|------|
| `request.schema.json` | Automation request envelope |
| `response.schema.json` | Automation response envelope |
| `ERROR_CODES.md` | Stable error `code` strings |
| `examples/` | Golden JSON instances |

## Semantics (summary)

| Field | Rule |
|-------|------|
| `op` | **Primary key** — Host looks up CapabilityRegistry by `op` |
| `kind` | Must match the registered kind for that `op`, else `KindOpMismatch` |
| `sessionId` | Required when the capability has `sessionRequired: true` (all Scene edits) |
| `payload` / `result` | Op-specific; schemas allow additional properties so Host can evolve without schema churn. **Shape is pinned by examples.** |

Consumers: ED-F17 (C++), MCP-F03 (TypeScript). Do not invent a second copy of these rules in application code without updating this folder.
