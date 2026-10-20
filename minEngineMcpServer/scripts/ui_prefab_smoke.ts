/**
 * Prefab authoring smoke: MCP-authorable UI tree -> Prefab asset -> scene instance -> Stage edit loop.
 *
 *   cd minEngineMcpServer
 *   npx tsx scripts/ui_prefab_smoke.ts
 *
 * Drives the Automation Host pipe directly (same op strings the MCP adapter composes as
 * domain.action) so error payloads such as index/applied[] stay observable.
 * Leaves no scene changes on disk: the Editor is killed at the end without saving the scene.
 */
import fs from "node:fs";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

import { AutomationPipeClient } from "../dist/automation/pipeClient.js";
import type { AutomationResponse } from "../dist/automation/protocol.js";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const editorPath = path.join(root, "minEngine", "bin", "MaximumEditor.exe");
const projectPath = path.join(root, "minEngine", "MyMEProject", "MyMEProject.meproject");
const workingDirectory = path.join(root, "minEngine", "bin");
const openScene = "Assets/Scenes/test.mescene";
const pipeName = "minEngine.prefab.smoke";
const readyFilePath = path.join(workingDirectory, "_mcp_prefab_smoke_ready.txt");

const canvasName = "UI_Smoke_Canvas";
const widgetName = "UI_Smoke_Widget";
const imageName = "UI_Smoke_Image";
const stageChildName = "UI_Smoke_StageChild";
const probeName = "UI_Smoke_RefProbe";
const buttonName = "UI_Smoke_Button";

const prefabDirectory = "Prefabs";
const prefabName = "UI_PrefabSmoke";
const prefabPath = `${prefabDirectory}/${prefabName}.meprefab`;
const texturePath = "Textures/albedo.jpg";
const missingAssetPath = "Textures/does_not_exist.metex";
const missingSpritePath = "Sprites/does_not_exist.mesprite";

type Json = Record<string, unknown>;

let failures = 0;
let checks = 0;

function check(condition: boolean, label: string, detail?: unknown): void {
  checks += 1;
  if (condition) {
    console.log(`  PASS ${label}`);
    return;
  }
  failures += 1;
  console.log(`  FAIL ${label}${detail === undefined ? "" : " -> " + JSON.stringify(detail)}`);
}

function equalJson(actual: unknown, expected: unknown): boolean {
  return JSON.stringify(actual) === JSON.stringify(expected);
}

/** Split a "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" guid into exact decimal High/Low digits. */
function guidStringToParts(guid: string): { high: string; low: string } {
  const parts = guid.split("-");
  if (parts.length !== 5) throw new Error(`unexpected guid format: ${guid}`);
  const [a, b, c, d, e] = parts;
  const high = (BigInt("0x" + a) << 32n) | (BigInt("0x" + b) << 16n) | BigInt("0x" + c);
  const low = (BigInt("0x" + d) << 48n) | BigInt("0x" + e);
  return { high: high.toString(), low: low.toString() };
}

const projectAssetsDirectory = path.join(root, "minEngine", "MyMEProject", "Assets");

async function waitForReadyFile(filePath: string, timeoutMs: number): Promise<void> {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (fs.existsSync(filePath)) return;
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error(`Timed out waiting for ready file: ${filePath}`);
}

function spawnEditor(): number {
  try {
    fs.unlinkSync(readyFilePath);
  } catch {
    /* first run: no stale ready file */
  }

  const child = spawn(
    editorPath,
    [
      "--project",
      projectPath,
      "--automation-pipe",
      pipeName,
      "--automation-ready-file",
      readyFilePath,
      "--open-scene",
      openScene,
    ],
    {
      cwd: workingDirectory,
      stdio: "ignore",
      windowsHide: false,
      detached: true,
    },
  );
  child.unref();
  if (!child.pid) throw new Error("Failed to spawn MaximumEditor");
  console.log(`spawned MaximumEditor pid=${child.pid}`);
  return child.pid;
}

/**
 * The smoke Prefab is a test artifact. It is removed after the Editor is gone, because a Prefab
 * instance root cannot be deleted through automation and asset.delete refuses a referenced asset.
 */
function removeSmokeArtifactFiles(): void {
  for (const suffix of ["", ".meta"]) {
    const filePath = path.join(projectAssetsDirectory, ...`${prefabPath}${suffix}`.split("/"));
    try {
      fs.rmSync(filePath, { force: true });
    } catch (err) {
      console.log(`  warn: could not remove ${filePath}: ${String(err)}`);
    }
  }
}

function removeSmokeArtifacts(): void {
  console.log("\n[10] cleanup");
  removeSmokeArtifactFiles();
  const prefabFile = path.join(projectAssetsDirectory, ...prefabPath.split("/"));
  check(!fs.existsSync(prefabFile), "smoke Prefab artifact removed from the project", prefabFile);
}

function killEditor(pid: number): void {
  try {
    spawn("taskkill", ["/PID", String(pid), "/T", "/F"], { stdio: "ignore" });
  } catch {
    /* best effort */
  }
}

class Smoke {
  constructor(private readonly client: AutomationPipeClient) {}

  async raw(
    op: string,
    kind: "query" | "edit" | "verify" | "control" | "asset",
    payload: Json,
    sessionId?: string,
  ): Promise<AutomationResponse> {
    return this.client.request({
      kind,
      op,
      ...(sessionId === undefined ? {} : { sessionId }),
      payload,
    });
  }

  async ok(op: string, kind: "query" | "edit" | "verify" | "control" | "asset", payload: Json, sessionId?: string): Promise<Json> {
    const response = await this.raw(op, kind, payload, sessionId);
    if (!response.ok) {
      throw new Error(`${op} failed: ${JSON.stringify(response.errors)}`);
    }
    return (response.result ?? {}) as Json;
  }

  async error(
    op: string,
    kind: "query" | "edit" | "verify" | "control" | "asset",
    payload: Json,
    sessionId?: string,
  ): Promise<{ code: string; message: string; extra: Json }> {
    const response = await this.raw(op, kind, payload, sessionId);
    if (response.ok) {
      throw new Error(`${op} unexpectedly succeeded: ${JSON.stringify(response.result)}`);
    }
    const first = (response.errors?.[0] ?? {}) as Json;
    return {
      code: String(first.code ?? ""),
      message: String(first.message ?? ""),
      extra: first,
    };
  }
}

interface HierarchyNode {
  objectId: string;
  name: string;
  children: HierarchyNode[];
}

function findNodes(nodes: HierarchyNode[], name: string, out: HierarchyNode[] = []): HierarchyNode[] {
  for (const node of nodes) {
    if (node.name === name) out.push(node);
    findNodes(node.children ?? [], name, out);
  }
  return out;
}

function hasChildChain(node: HierarchyNode, names: string[]): boolean {
  let current: HierarchyNode | undefined = node;
  for (const name of names) {
    current = (current?.children ?? []).find((child) => child.name === name);
    if (!current) return false;
  }
  return true;
}

async function main(): Promise<void> {
  // A previous aborted run must not turn the first create into an AssetExists failure.
  removeSmokeArtifactFiles();
  const editorPid = spawnEditor();
  const client = new AutomationPipeClient(60_000);
  const smoke = new Smoke(client);

  try {
    await waitForReadyFile(readyFilePath, 120_000);
    await client.connect(pipeName);

    console.log("\n[1] capabilities");
    const capabilities = await smoke.ok("editor.get_capabilities", "query", {});
    const ops: string[] = [];
    for (const domain of (capabilities.domains as Array<{ name: string; actions: Array<{ op: string }> }>) ?? []) {
      for (const action of domain.actions ?? []) ops.push(action.op);
    }
    for (const op of [
      "prefab.create_from_game_object",
      "prefab.open",
      "prefab.save",
      "prefab.close",
    ]) {
      check(ops.includes(op), `capability registered: ${op}`, ops.filter((value) => value.startsWith("prefab.")));
    }

    const state = await smoke.ok("editor.get_state", "query", {});
    const sceneSessionId = String(state.defaultSessionId ?? "");
    check(sceneSessionId.length > 0, "scene session available", state);
    if (!sceneSessionId) throw new Error("missing defaultSessionId");

    console.log("\n[2] build UI tree");
    const canvas = await smoke.ok("scene.create_game_object", "edit", { name: canvasName }, sceneSessionId);
    const canvasId = String(canvas.objectId);
    await smoke.ok("scene.add_component", "edit", { objectId: canvasId, className: "CanvasComponent" }, sceneSessionId);

    const widget = await smoke.ok("scene.create_game_object", "edit", { name: widgetName }, sceneSessionId);
    const widgetId = String(widget.objectId);
    await smoke.ok(
      "scene.reparent",
      "edit",
      { objectId: widgetId, newParentObjectId: canvasId },
      sceneSessionId,
    );
    const widgetComp = await smoke.ok(
      "scene.add_component",
      "edit",
      { objectId: widgetId, className: "WidgetComponent" },
      sceneSessionId,
    );
    await smoke.ok(
      "scene.set_property",
      "edit",
      { objectId: String(widgetComp.componentObjectId), propertyPath: "m_Size", value: [480, 96] },
      sceneSessionId,
    );

    const image = await smoke.ok("scene.create_game_object", "edit", { name: imageName }, sceneSessionId);
    const imageId = String(image.objectId);
    await smoke.ok(
      "scene.reparent",
      "edit",
      { objectId: imageId, newParentObjectId: widgetId },
      sceneSessionId,
    );
    const imageComp = await smoke.ok(
      "scene.add_component",
      "edit",
      { objectId: imageId, className: "ImageComponent" },
      sceneSessionId,
    );
    const imageCompId = String(imageComp.componentObjectId);

    const textures = await smoke.ok("asset.list", "query", { directory: "Textures", assetType: "Texture2D" });
    const textureMeta = ((textures.assets as Json[]) ?? []).find((asset) => asset.path === texturePath);
    check(textureMeta !== undefined, `texture asset imported: ${texturePath}`, textures.assets);
    if (!textureMeta) throw new Error(`texture asset missing: ${texturePath}`);
    const textureGuidText = String(textureMeta.guid ?? "");
    const textureGuidParts = guidStringToParts(textureGuidText);

    console.log("\n[3] bind texture through \$assetRef");
    const bindResult = await smoke.ok(
      "scene.set_property",
      "edit",
      { objectId: imageCompId, propertyPath: "m_Texture", value: { $assetRef: texturePath } },
      sceneSessionId,
    );
    const boundGuid = (((bindResult.value as Json) ?? {}).$guid ?? null) as Json | null;
    check(
      boundGuid !== null && typeof boundGuid.high === "number" && typeof boundGuid.low === "number",
      "set_property wrote a resolved asset Guid",
      bindResult.value,
    );

    const readBack = await smoke.ok(
      "scene.get_property",
      "query",
      { objectId: imageCompId, propertyPath: "m_Texture" },
      sceneSessionId,
    );
    const readBackGuid = (((readBack.value as Json) ?? {}).$guid ?? null) as Json | null;
    check(equalJson(readBackGuid, boundGuid), "get_property reads the written Guid back", readBack.value);

    // A second texture must resolve to a different Guid, so the check above is asset-specific.
    const otherMeta = ((textures.assets as Json[]) ?? []).find(
      (asset) => asset.path === "Textures/marble_cliff_03_diff_1k.jpg",
    );
    const otherWrite = await smoke.ok(
      "scene.set_property",
      "edit",
      {
        objectId: imageCompId,
        propertyPath: "m_Texture",
        value: { $assetRef: "Textures/marble_cliff_03_diff_1k.jpg" },
      },
      sceneSessionId,
    );
    const otherGuid = (((otherWrite.value as Json) ?? {}).$guid ?? null) as Json | null;
    check(
      otherMeta !== undefined && otherGuid !== null && !equalJson(otherGuid, boundGuid),
      "a different asset resolves to a different Guid",
      otherWrite.value,
    );

    // Restore the first texture so later assertions and the saved Prefab keep it.
    await smoke.ok(
      "scene.set_property",
      "edit",
      { objectId: imageCompId, propertyPath: "m_Texture", value: { $assetRef: texturePath } },
      sceneSessionId,
    );

    console.log("\n[4] prefab.create_from_game_object");
    const created = await smoke.ok(
      "prefab.create_from_game_object",
      "asset",
      { objectId: canvasId, directory: prefabDirectory, name: prefabName },
      sceneSessionId,
    );
    const createdPath = String(created.prefabPath ?? "");
    const createdGuid = String(created.guid ?? "");
    check(createdPath === prefabPath, "create returns the expected prefabPath", created);
    check(createdGuid.length > 0, "create returns an asset guid", created);
    check(created.overwritten === false, "first create is not an overwrite", created);
    check(String(created.rootName ?? "") === canvasName, "rootName matches the source GameObject", created);

    const prefabAssetFile = path.join(projectAssetsDirectory, ...prefabPath.split("/"));
    const prefabText = fs.readFileSync(prefabAssetFile, "utf8");
    check(
      prefabText.includes(textureGuidParts.high) && prefabText.includes(textureGuidParts.low),
      "saved Prefab stores the exact texture High/Low digits",
      { high: textureGuidParts.high, low: textureGuidParts.low },
    );

    const prefabsAfterCreate = await smoke.ok("asset.list", "query", { directory: prefabDirectory });
    const listedPrefab = ((prefabsAfterCreate.assets as Json[]) ?? []).find(
      (asset) => asset.path === prefabPath,
    );
    check(listedPrefab !== undefined, "asset.list sees the new Prefab", prefabsAfterCreate.assets);
    check(String(listedPrefab?.guid ?? "") === createdGuid, "asset.list guid matches create response", listedPrefab);

    console.log("\n[5] scene untouched + instantiate round-trip");
    const hierarchyBeforeInstantiate = await smoke.ok("scene.get_hierarchy", "query", {}, sceneSessionId);
    const rootsBefore = (hierarchyBeforeInstantiate.roots as HierarchyNode[]) ?? [];
    const canvasRootsBefore = findNodes(rootsBefore, canvasName);
    check(canvasRootsBefore.length === 1, "source tree kept in the scene (single Canvas)", canvasRootsBefore.length);
    check(
      hasChildChain(canvasRootsBefore[0], [widgetName, imageName]),
      "source tree structure unchanged",
      canvasRootsBefore[0],
    );

    await smoke.ok(
      "scene.instantiate_prefab",
      "edit",
      { assetPath: prefabPath },
      sceneSessionId,
    );
    const hierarchyAfterInstantiate = await smoke.ok("scene.get_hierarchy", "query", {}, sceneSessionId);
    const rootsAfter = (hierarchyAfterInstantiate.roots as HierarchyNode[]) ?? [];
    const canvasRootsAfter = findNodes(rootsAfter, canvasName);
    check(canvasRootsAfter.length === 2, "instantiated Prefab added a second Canvas root", canvasRootsAfter.length);
    check(
      canvasRootsAfter.some((node) => hasChildChain(node, [widgetName, imageName])),
      "instantiated hierarchy names match the source tree",
      canvasRootsAfter,
    );

    console.log("\n[6] idempotent re-run keeps the asset Guid");
    const overwritten = await smoke.ok(
      "prefab.create_from_game_object",
      "asset",
      { objectId: canvasId, directory: prefabDirectory, name: prefabName, overwrite: true },
      sceneSessionId,
    );
    check(overwritten.overwritten === true, "second create reports overwritten", overwritten);
    check(String(overwritten.guid ?? "") === createdGuid, "overwrite keeps the asset Guid", overwritten);

    const notOverwrite = await smoke.error(
      "prefab.create_from_game_object",
      "asset",
      { objectId: canvasId, directory: prefabDirectory, name: prefabName },
      sceneSessionId,
    );
    check(notOverwrite.code === "AssetExists", "create without overwrite reports AssetExists", notOverwrite);

    console.log("\n[7] Prefab Stage loop");
    const saveWithoutStage = await smoke.error("prefab.save", "asset", { prefabPath });
    check(saveWithoutStage.code === "PrefabNotOpen", "prefab.save without a Stage reports PrefabNotOpen", saveWithoutStage);
    const closeWithoutStage = await smoke.error("prefab.close", "control", { prefabPath });
    check(closeWithoutStage.code === "PrefabNotOpen", "prefab.close without a Stage reports PrefabNotOpen", closeWithoutStage);

    const opened = await smoke.ok("prefab.open", "control", { prefabPath });
    const stageSessionId = String(opened.sessionId ?? "");
    const stageRootGuid = String(opened.rootGuid ?? "");
    check(stageSessionId.length > 0, "prefab.open returns a Stage sessionId", opened);
    check(stageRootGuid.length > 0, "prefab.open returns the Stage rootGuid", opened);
    check(String(opened.guid ?? "") === createdGuid, "prefab.open keeps the asset Guid", opened);
    if (!stageSessionId || !stageRootGuid) throw new Error("prefab.open did not return a usable stage");

    const reopened = await smoke.ok("prefab.open", "control", { prefabPath });
    check(String(reopened.rootGuid ?? "") === stageRootGuid, "prefab.open is idempotent", reopened);
    check(reopened.alreadyOpen === true, "second prefab.open reports alreadyOpen", reopened);

    const stageChild = await smoke.ok(
      "scene.create_game_object",
      "edit",
      { name: stageChildName, parentObjectId: stageRootGuid },
      stageSessionId,
    );
    check(String(stageChild.objectId ?? "").length > 0, "created a child inside the Stage", stageChild);

    const dirtyClose = await smoke.error("prefab.close", "control", { prefabPath });
    check(dirtyClose.code === "UnsavedChanges", "prefab.close refuses to drop unsaved Stage edits", dirtyClose);

    const saved = await smoke.ok("prefab.save", "asset", { prefabPath });
    check(saved.saved === true, "prefab.save reports saved", saved);
    check(String(saved.guid ?? "") === createdGuid, "prefab.save keeps the asset Guid", saved);

    const closed = await smoke.ok("prefab.close", "control", { prefabPath });
    check(closed.closed === true, "prefab.close reports closed", closed);

    const reopenedAfterClose = await smoke.ok("prefab.open", "control", { prefabPath });
    const stageSessionId2 = String(reopenedAfterClose.sessionId ?? "");
    check(String(reopenedAfterClose.guid ?? "") === createdGuid, "reopened Prefab keeps the asset Guid", reopenedAfterClose);
    const stageHierarchy = await smoke.ok("scene.get_hierarchy", "query", {}, stageSessionId2);
    const stageNodes = findNodes((stageHierarchy.roots as HierarchyNode[]) ?? [], stageChildName);
    check(stageNodes.length === 1, "Stage child persisted across save/reopen", stageNodes.length);

    await smoke.ok("prefab.close", "control", { prefabPath });

    console.log("\n[8] set_property reference failures");
    const probe = await smoke.ok("scene.create_game_object", "edit", { name: probeName }, sceneSessionId);
    const probeId = String(probe.objectId);
    const probeComp = await smoke.ok(
      "scene.add_component",
      "edit",
      { objectId: probeId, className: "ImageComponent" },
      sceneSessionId,
    );
    const probeCompId = String(probeComp.componentObjectId);

    const unresolved = await smoke.error(
      "scene.set_property",
      "edit",
      { objectId: probeCompId, propertyPath: "m_Texture", value: { $assetRef: missingAssetPath } },
      sceneSessionId,
    );
    check(unresolved.code === "UnresolvedObjectRef", "unresolved \$assetRef reports UnresolvedObjectRef", unresolved);
    const probeTextureAfterFailure = await smoke.ok(
      "scene.get_property",
      "query",
      { objectId: probeCompId, propertyPath: "m_Texture" },
      sceneSessionId,
    );
    check(probeTextureAfterFailure.value === null, "failed reference leaves the property untouched", probeTextureAfterFailure.value);

    const batchFailure = await smoke.error(
      "scene.set_property",
      "edit",
      {
        objectId: probeCompId,
        properties: [
          { propertyPath: "m_Texture", value: { $assetRef: texturePath } },
          { propertyPath: "m_Sprite", value: { $assetRef: missingSpritePath } },
        ],
      },
      sceneSessionId,
    );
    check(batchFailure.code === "UnresolvedObjectRef", "batch resolve failure reports UnresolvedObjectRef", batchFailure);
    check(Number(batchFailure.extra.index) === 1, "batch failure names the failing index", batchFailure.extra);
    check(String(batchFailure.extra.propertyPath ?? "") === "m_Sprite", "batch failure names the propertyPath", batchFailure.extra);
    const batchTexture = await smoke.ok(
      "scene.get_property",
      "query",
      { objectId: probeCompId, propertyPath: "m_Texture" },
      sceneSessionId,
    );
    const batchSprite = await smoke.ok(
      "scene.get_property",
      "query",
      { objectId: probeCompId, propertyPath: "m_Sprite" },
      sceneSessionId,
    );
    check(batchTexture.value === null, "one bad reference wrote neither property (m_Texture)", batchTexture.value);
    check(batchSprite.value === null, "one bad reference wrote neither property (m_Sprite)", batchSprite.value);

    const ambiguousRef = await smoke.error(
      "scene.set_property",
      "edit",
      {
        objectId: probeCompId,
        propertyPath: "m_Texture",
        value: { $assetRef: texturePath, $objectRef: imageCompId },
      },
      sceneSessionId,
    );
    check(ambiguousRef.code === "InvalidPayload", "two reference forms in one value report InvalidPayload", ambiguousRef);

    const badGuidShape = await smoke.error(
      "scene.set_property",
      "edit",
      { objectId: probeCompId, propertyPath: "m_Texture", value: { $guid: { high: "x", low: 1 } } },
      sceneSessionId,
    );
    check(badGuidShape.code === "InvalidPayload", "malformed \$guid reports InvalidPayload", badGuidShape);

    const arrayRef = await smoke.error(
      "scene.set_property",
      "edit",
      { objectId: probeCompId, propertyPath: "m_Size", value: [{ $assetRef: texturePath }, 96] },
      sceneSessionId,
    );
    check(arrayRef.code === "Unsupported", "references inside arrays report Unsupported", arrayRef);

    const duplicate = await smoke.error(
      "scene.set_property",
      "edit",
      {
        objectId: probeCompId,
        properties: [
          { propertyPath: "m_Color", value: [1, 1, 1, 1] },
          { propertyPath: "m_Color", value: [0, 0, 0, 1] },
        ],
      },
      sceneSessionId,
    );
    check(duplicate.code === "InvalidPayload", "duplicate propertyPath reports InvalidPayload", duplicate);

    const partial = await smoke.error(
      "scene.set_property",
      "edit",
      {
        objectId: probeCompId,
        properties: [
          { propertyPath: "m_Texture", value: { $assetRef: texturePath } },
          { propertyPath: "m_Sprite", value: 12345 },
        ],
      },
      sceneSessionId,
    );
    check(partial.code === "PartialWrite", "write-phase failure reports PartialWrite", partial);
    const appliedList = (partial.extra.applied as Json[]) ?? [];
    check(
      appliedList.length === 1 && String(appliedList[0]?.propertyPath ?? "") === "m_Texture",
      "PartialWrite reports the applied properties",
      partial.extra.applied,
    );
    check(
      Number(((partial.extra.failed as Json) ?? {}).index) === 1,
      "PartialWrite names the failing index",
      partial.extra.failed,
    );
    const partialTexture = await smoke.ok(
      "scene.get_property",
      "query",
      { objectId: probeCompId, propertyPath: "m_Texture" },
      sceneSessionId,
    );
    const partialAppliedGuid = ((((appliedList[0] as Json) ?? {}).value as Json) ?? {}).$guid ?? null;
    const partialReadGuid = (((partialTexture.value as Json) ?? {}).$guid ?? null) as Json | null;
    check(
      partialAppliedGuid !== null && equalJson(partialAppliedGuid, partialReadGuid),
      "PartialWrite applied[] matches the readback",
      { applied: partialAppliedGuid, readback: partialReadGuid },
    );

    console.log("\n[9] \$objectRef binds a scene component");
    const button = await smoke.ok("scene.create_game_object", "edit", { name: buttonName }, sceneSessionId);
    const buttonId = String(button.objectId);
    const buttonComp = await smoke.ok(
      "scene.add_component",
      "edit",
      { objectId: buttonId, className: "ButtonComponent" },
      sceneSessionId,
    );
    const buttonCompId = String(buttonComp.componentObjectId);
    const objectRefWrite = await smoke.ok(
      "scene.set_property",
      "edit",
      { objectId: buttonCompId, propertyPath: "m_TargetGraphic", value: { $objectRef: imageCompId } },
      sceneSessionId,
    );
    const objectRefRead = await smoke.ok(
      "scene.get_property",
      "query",
      { objectId: buttonCompId, propertyPath: "m_TargetGraphic" },
      sceneSessionId,
    );
    const targetGuid = (((objectRefRead.value as Json) ?? {}).$guid as Json) ?? null;
    check(targetGuid !== null, "\$objectRef resolved to a component Guid", objectRefRead.value);
    check(
      objectRefWrite.value !== undefined,
      "\$objectRef write reported a value",
      objectRefWrite,
    );

  } finally {
    try {
      await client.close();
    } catch {
      /* already closed */
    }
    killEditor(editorPid);
    await new Promise((r) => setTimeout(r, 1500));
    removeSmokeArtifacts();
  }

  console.log(`\n${checks - failures}/${checks} checks passed`);
  if (failures > 0) {
    console.error(`ui_prefab_smoke: ${failures} check(s) failed`);
    process.exitCode = 1;
  }
}

main().catch((err) => {
  console.error("ui_prefab_smoke failed:", err);
  process.exitCode = 1;
});
