import net from "node:net";
import { EventEmitter } from "node:events";

import {
  type AutomationRequest,
  type AutomationResponse,
  makeRequestId,
  PROTOCOL_VERSION,
} from "./protocol.js";

export class PipeClosedError extends Error {
  constructor(message = "Automation pipe closed.") {
    super(message);
    this.name = "PipeClosedError";
  }
}

export class AutomationTimeoutError extends Error {
  constructor(public readonly requestId: string, timeoutMs: number) {
    super(`Automation request '${requestId}' timed out after ${timeoutMs}ms.`);
    this.name = "AutomationTimeoutError";
  }
}

/**
 * Windows Named Pipe client: [u32 LE length][UTF-8 JSON].
 * Single in-flight RPC matching Host MVP (serial).
 */
export class AutomationPipeClient extends EventEmitter {
  private socket: net.Socket | null = null;
  private readBuffer = Buffer.alloc(0);
  private pending:
    | {
        requestId: string;
        resolve: (value: AutomationResponse) => void;
        reject: (reason: unknown) => void;
        timer: NodeJS.Timeout;
      }
    | null = null;

  constructor(private readonly defaultTimeoutMs = 30_000) {
    super();
  }

  get connected(): boolean {
    return this.socket !== null && !this.socket.destroyed;
  }

  async connect(pipeName: string): Promise<void> {
    await this.close();

    const path = normalizePipePath(pipeName);
    this.socket = net.createConnection(path);

    await new Promise<void>((resolve, reject) => {
      const socket = this.socket!;
      const onError = (err: Error) => {
        cleanup();
        reject(err);
      };
      const onConnect = () => {
        cleanup();
        resolve();
      };
      const cleanup = () => {
        socket.off("error", onError);
        socket.off("connect", onConnect);
      };
      socket.once("error", onError);
      socket.once("connect", onConnect);
    });

    this.socket.on("data", (chunk) => this.onData(chunk));
    this.socket.on("error", (err) => this.emit("error", err));
    this.socket.on("close", () => {
      this.failPending(new PipeClosedError());
      this.socket = null;
      this.emit("close");
    });
  }

  async close(): Promise<void> {
    this.failPending(new PipeClosedError("Automation pipe closing."));
    const socket = this.socket;
    this.socket = null;
    this.readBuffer = Buffer.alloc(0);
    if (socket && !socket.destroyed) {
      await new Promise<void>((resolve) => {
        socket.end(() => resolve());
        socket.destroy();
        resolve();
      });
    }
  }

  async request(
    partial: Omit<AutomationRequest, "protocolVersion" | "requestId"> & {
      requestId?: string;
    },
    timeoutMs = this.defaultTimeoutMs,
  ): Promise<AutomationResponse> {
    if (!this.socket || this.socket.destroyed) {
      throw new PipeClosedError("Not connected.");
    }
    if (this.pending) {
      throw new Error("AutomationPipeClient: only one in-flight request is allowed (Host single-flight).");
    }

    const request: AutomationRequest = {
      protocolVersion: PROTOCOL_VERSION,
      requestId: partial.requestId ?? makeRequestId(),
      kind: partial.kind,
      op: partial.op,
      ...(partial.sessionId !== undefined ? { sessionId: partial.sessionId } : {}),
      payload: partial.payload ?? {},
    };

    const body = Buffer.from(JSON.stringify(request), "utf8");
    const header = Buffer.alloc(4);
    header.writeUInt32LE(body.length, 0);

    const responsePromise = new Promise<AutomationResponse>((resolve, reject) => {
      const timer = setTimeout(() => {
        if (this.pending?.requestId === request.requestId) {
          this.pending = null;
          reject(new AutomationTimeoutError(request.requestId, timeoutMs));
        }
      }, timeoutMs);
      this.pending = { requestId: request.requestId, resolve, reject, timer };
    });

    this.socket.write(Buffer.concat([header, body]));
    return responsePromise;
  }

  private onData(chunk: Buffer): void {
    this.readBuffer = Buffer.concat([this.readBuffer, chunk]);
    while (this.readBuffer.length >= 4) {
      const length = this.readBuffer.readUInt32LE(0);
      if (this.readBuffer.length < 4 + length) {
        return;
      }
      const body = this.readBuffer.subarray(4, 4 + length);
      this.readBuffer = this.readBuffer.subarray(4 + length);
      this.dispatchFrame(body.toString("utf8"));
    }
  }

  private dispatchFrame(jsonText: string): void {
    let response: AutomationResponse;
    try {
      response = JSON.parse(jsonText) as AutomationResponse;
    } catch (err) {
      this.failPending(err);
      return;
    }

    const pending = this.pending;
    if (!pending) {
      return;
    }
    if (response.requestId && response.requestId !== pending.requestId) {
      pending.reject(
        new Error(
          `Mismatched requestId: expected ${pending.requestId}, got ${response.requestId}`,
        ),
      );
      clearTimeout(pending.timer);
      this.pending = null;
      return;
    }

    clearTimeout(pending.timer);
    this.pending = null;
    pending.resolve(response);
  }

  private failPending(reason: unknown): void {
    if (!this.pending) {
      return;
    }
    clearTimeout(this.pending.timer);
    const pending = this.pending;
    this.pending = null;
    pending.reject(reason);
  }
}

export function normalizePipePath(pipeName: string): string {
  const trimmed = pipeName.trim();
  if (trimmed.startsWith("\\\\.\\pipe\\") || trimmed.startsWith("//./pipe/")) {
    return trimmed;
  }
  return `\\\\.\\pipe\\${trimmed}`;
}
