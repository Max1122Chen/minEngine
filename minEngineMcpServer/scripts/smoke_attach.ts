/**
 * Smoke: ConnectionManager attach → get_state + capabilities, then a scene query.
 * Usage: tsx scripts/smoke_attach.ts <pipeName>
 */
import { ConnectionManager } from "../src/connection/manager.js";
import { composeOp } from "../src/automation/protocol.js";

async function main(): Promise<void> {
  const pipeName = process.argv[2];
  if (!pipeName) {
    console.error("Usage: tsx scripts/smoke_attach.ts <pipeName>");
    process.exit(2);
  }

  const connection = new ConnectionManager();
  const snap = await connection.attach({ pipeName });
  console.log("attached:", JSON.stringify({ mode: snap.mode, ready: snap.ready, defaultSessionId: snap.state?.defaultSessionId }, null, 2));

  const domains = (snap.capabilities as { domains?: unknown[] } | null)?.domains;
  console.log("capabilityDomains:", Array.isArray(domains) ? domains.length : 0);

  const sessionId = String(snap.state?.defaultSessionId ?? "");
  const response = await connection.getClient().request({
    kind: "query",
    op: composeOp("scene", "get_hierarchy"),
    sessionId,
    payload: { maxDepth: 1 },
  });
  console.log("get_hierarchy ok=", response.ok, "roots=", Array.isArray((response.result as { roots?: unknown[] })?.roots)
    ? (response.result as { roots: unknown[] }).roots.length
    : "?");

  await connection.disconnect({ killSpawned: false });
  if (!response.ok) {
    process.exit(1);
  }
  console.log("ATTACH SMOKE PASS");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
