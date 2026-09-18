/** Automation Protocol v1 — wire types (see schemas/automation/v1/). */

export const PROTOCOL_VERSION = 1 as const;

export type AutomationKind = "query" | "edit" | "verify" | "control" | "asset";

export interface AutomationRequest {
  protocolVersion: typeof PROTOCOL_VERSION;
  requestId: string;
  kind: AutomationKind;
  op: string;
  sessionId?: string;
  payload?: Record<string, unknown>;
}

export interface AutomationError {
  code: string;
  message: string;
  op?: string;
  [key: string]: unknown;
}

export interface AutomationResponse {
  protocolVersion: number;
  requestId: string;
  ok: boolean;
  result?: unknown;
  errors?: AutomationError[];
  warnings?: unknown[];
}

export function makeRequestId(prefix = "mcp"): string {
  return `${prefix}-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 8)}`;
}

/** domain + action → op primary key (string translate only). */
export function composeOp(domain: string, action: string): string {
  return `${domain}.${action}`;
}

export function kindForTool(tool: AutomationKind): AutomationKind {
  return tool;
}
