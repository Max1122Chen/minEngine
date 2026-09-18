/**
 * Smoke: ConnectionManager.spawn → Ready → disconnect (kills Editor).
 */
import path from "node:path";
import { fileURLToPath } from "node:url";

import { ConnectionManager } from "../src/connection/manager.js";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const editorPath = path.join(root, "minEngine", "bin", "MaximumEditor.exe");
const projectPath = path.join(root, "minEngine", "MyMEProject", "MyMEProject.meproject");
const binDir = path.join(root, "minEngine", "bin");
const readyFilePath = path.join(binDir, "_mcp_f03_spawn_ready.txt");

async function main(): Promise<void> {
  const connection = new ConnectionManager();
  const snap = await connection.spawn({
    editorPath,
    projectPath,
    openScene: "Assets/Scenes/test.mescene",
    workingDirectory: binDir,
    pipeName: "minEngine.mcp.f03.spawn",
    readyFilePath,
  });
  console.log(
    JSON.stringify(
      {
        ready: snap.ready,
        mode: snap.mode,
        defaultSessionId: snap.state?.defaultSessionId,
        editorPid: snap.editorPid,
      },
      null,
      2,
    ),
  );
  await connection.disconnect({ killSpawned: true });
  console.log("SPAWN SMOKE PASS");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
