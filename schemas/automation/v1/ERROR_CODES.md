# Automation Protocol v1 — Error codes

Stable machine-readable `errors[].code` / `warnings[].code` strings for `protocolVersion=1`.

**Design:** [`MCP-F02` §3.1.2](../../../docs/ai/Editor/MCP/MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md)

Host (ED-F17) and MCP Server (MCP-F03) **must** emit these exact PascalCase codes. Do not invent synonyms (`invalid_object`, `KIND_MISMATCH`, etc.).

## Protocol / client (fix the request and retry)

| Code | When |
|------|------|
| `ProtocolVersionMismatch` | `protocolVersion` ≠ Host-supported version (this folder = `1`) |
| `KindOpMismatch` | Request `kind` ≠ CapabilityRegistry kind for `op` |
| `UnknownOp` | `op` not registered |
| `InvalidPayload` | Payload missing required fields, wrong types, or fails op-specific validation |

## Session / target (change session or identity)

| Code | When |
|------|------|
| `SessionNotFound` | `sessionId` missing when required, or unknown / closed |
| `InvalidObjectId` | GameObject (or other object) GUID not in the session document |
| `InvalidPropertyPath` | `propertyPath` not resolvable on the target |

## Internal (do not blindly retry the same write)

| Code | When |
|------|------|
| `InternalError` | Unexpected Host failure |
| `EditorShuttingDown` | Host rejecting work during teardown |
| `Timeout` | Client-side RPC timeout (MCP default **30s**); Host may also emit if it aborts long work |

## Error object shape

Required: `code`, `message`. Optional context (when applicable): `objectId`, `propertyPath`, `op`.

See `response.schema.json` `$defs.automationError` and `examples/*fail*` / `kind_op_mismatch.response.json`.
