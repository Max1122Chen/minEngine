/**
 * Smoke: attach to an already-running Maximum with --automation-pipe and ping.
 *
 * Usage:
 *   npx tsx scripts/smoke_ping.ts <pipeName>
 */
import { AutomationPipeClient } from "../src/automation/pipeClient.js";

async function main(): Promise<void> {
  const pipeName = process.argv[2];
  if (!pipeName) {
    console.error("Usage: tsx scripts/smoke_ping.ts <pipeName>");
    process.exit(2);
  }

  const client = new AutomationPipeClient();
  await client.connect(pipeName);
  const response = await client.request({ kind: "query", op: "editor.ping" });
  console.log(JSON.stringify(response, null, 2));
  await client.close();
  if (!response.ok) {
    process.exit(1);
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
