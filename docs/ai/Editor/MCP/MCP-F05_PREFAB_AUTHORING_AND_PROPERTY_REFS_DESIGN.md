# MCP-F05 — Prefab 作者化自动化 + 属性引用解析 — Design Spec

## Meta
- **ID:** `MCP-F05`
- **Type:** Feature
- **Status:** In Progress（实现已落地并验证；Stage 循环稳定性待修）
- **Owner:** project maintainer
- **Last updated:** 2026-10-19
- **Branch:** `feat/automation-prefab-ops`
- **Depends on:** `ED-F16`（Prefab 文档 Mode / Stage）· `ED-F17`（Automation Host）· `ED-F18`（*EditorServices）· `MCP-F02`（协议信封）· `MCP-F04`（kind 扩展）
- **Related:**
  - [Implementation](./MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_IMPLEMENTATION.md)
  - [ED-F16 Prefab Editor](../ED-F16_PREFAB_EDITOR_DESIGN.md)（Stage / WriteStageTreeToPrefab）
  - [BUG-EDITOR-007](../../bugs/BUG-EDITOR-007.md)（Stage save+close 偶发 AV）
  - 消费方：minCraft `GP-F02`（HUD/容器屏资产化）
- **Blocks:** 依赖「界面作者化 → 落盘 → 运行时只加载」的任何消费方

## TL;DR

自动化面能建结构、不能建内容，也不能把结构变成资产。本 Feature 补两块：

1. **`prefab.*` 四个 op**（`create_from_game_object` / `open` / `save` / `close`），让「场景树 → Prefab 资产」可程序化完成；
2. **`scene.set_property` 支持对象引用**（`$assetRef` / `$guid` / `$objectRef` + `properties[]` 批量），让「绑贴图/字体/组件」不再报 unresolved。

两块合起来才是一条闭环：**MCP 建树 → 绑资源 → 存 Prefab → 运行时只 Load**。只做任一块，消费方仍要要么手点菜单、要么在运行时拼装并写盘（后者是已发生过的事故模式）。

## Problem

| 事实 | 证据 |
|------|------|
| 从 GameObject 生成 Prefab 的能力只挂在人类菜单 | `Editor/src/ContextMenu/Actions/PrefabBuiltInActions.cpp:26-70` |
| 底层 API 已存在 | `PrefabUtility::CreatePrefabFromGameObject` / `SavePrefabAsset` / `WriteStageTreeToPrefab` / `RestoreTemplateObjects` |
| 自动化面没有任何 `prefab.*` op | `CapabilityRegistry.cpp`（61 op）与 `EditorAutomationSurface.cpp:66-366` 逐条核对 |
| `set_property` 遇对象引用即失败 | `ScenePropertyValueCodec.cpp:237-241`；人类路径 `EditorObjectPropertyApply.cpp:64` 会解析 |

后果：任何要求「界面/场景元素以 Prefab 资产形态交付」的消费方，要么人工点菜单（agent/CI 不可复现），要么在运行时拼装再写盘（破坏「Play 只读」边界）。

## Scope

**In**
- `prefab.create_from_game_object` / `prefab.open` / `prefab.save` / `prefab.close`
- `scene.set_property`：`$assetRef` / `$guid` / `$objectRef` 三种引用形态；`properties[]` 批量写与 `PartialWrite`
- 必要接线：`ResolveSession` / `SceneEditorServices` 接受 Prefab 文档会话并映射到 Stage 场景；`PrefabStageController::SaveStage(key)` 从 `SaveActive` 抽出

**Out / Non-goals**
- 不变更序列化格式；不新增第二套 Prefab 实现；不动 `minEngine/src/Runtime/**`
- 不做嵌套 Prefab / Prefab 变体的引用重写（Deferred）
- 不做数组/集合内引用（显式 `Unsupported`）
- 不引入独立 Prefab 编辑会话类型（复用 ED-F16 Stage）

## Op 契约

### prefab.create_from_game_object（kind=asset，sessionRequired=true）
```json
{ "objectId": "<go guid>", "directory": "Prefabs", "name": "UI_Screens", "overwrite": false }
→ { "prefabPath": "Prefabs/UI_Screens.meprefab", "guid": "<asset guid>", "rootName": "UI_Screens", "overwritten": false }
```
- 序列化该 GO 及全部子节点；**不修改场景**（引擎路径会为源 GO 推入 PrefabInstanceRecord，本 op 再把它丢掉）。
- `overwrite=false` 且已存在 → `AssetExists`。
- `overwrite=true`：加载既有 Prefab，把新建模板树搬进去再保存 → **资产 GUID 不变**；保存前备份旧字节，失败则还原。
- 注意：覆盖会**重生成模板对象 GUID**，已在场景里的该 Prefab 实例会持有过期映射（后续 Stage 保存报 missing template）。约定：可重建的 Prefab 不要摆进场景，改运行时 Instantiate。

### prefab.open（kind=control）
走 `PrefabStageController::OpenFromAsset` + `EditorDocumentHost::OpenOrFocus`（即人类双击资源路径），返回 `sessionId`（供 `scene.*` 对 Stage 寻址）与 `rootGuid`；同一 Prefab 幂等；已开其他干净 Stage 先关，脏 Stage 保留并标记。

### prefab.save（kind=asset）
`PrefabStageController::SaveStage(key)`：`WriteStageTreeToPrefab` → `SavePrefabAsset`，失败走 `RestoreTemplateObjects` + edit-clone 回滚，并还原旧文件字节；无 Stage → `PrefabNotOpen`。

### prefab.close（kind=control）
走人类关标签页路径（`RequestClose` → `ExitPrefabStage`）；`discard=false` 且脏 → `UnsavedChanges`（不销毁）。关后把 inspect 目标重置回文档场景。

### scene.set_property 引用与批量
```json
{ "objectId": "<guid>", "propertyPath": "m_Sprite", "value": { "$assetRef": "Textures/Gui/inventory.metex" } }
{ "objectId": "<guid>", "properties": [
    { "propertyPath": "m_Sprite", "value": {"$assetRef": "Textures/Gui/inventory.metex"} },
    { "propertyPath": "m_Font",   "value": {"$guid": {"high":1,"low":2}} } ] }
```
- 解析（phase A）在任何写入之前：任一失败 → `UnresolvedObjectRef`（带 index / propertyPath），**零写入**。
- 写入（phase B）顺序执行；某项失败 → `PartialWrite` + `applied[]`（**不**声称回滚）。
- 同一 `propertyPath` 重复 → `InvalidPayload`；数组内引用 → `Unsupported`；三种形态互斥。
- 顺带修复：`TryRestoreBlob` 也走解析 → 指针类属性的 undo/restore 不再丢引用。

## How（实现落点）

| 位置 | 改动 |
|------|------|
| `Editor/src/Automation/CapabilityRegistry.cpp` | 四条能力表项 |
| `Editor/src/Automation/EditorAutomationSurface.cpp/.h` | 四个 handler + 分发表 + `properties[]`（phase A/B） |
| `Editor/src/Services/Scene/ScenePropertyValueCodec.cpp/.h` | `TryNormalizeValueReferences`；去掉「未解析即失败」分支 |
| `Editor/src/Services/Scene/SceneEditorServices.cpp` | 会话类型扩到 Scene / Prefab，Prefab 会话映射到 Stage 场景 |
| `Editor/src/SubEditor/Prefab/PrefabStageController.cpp/.h` | `SaveStage(key)` 从 `SaveActive` 抽出；`SaveActive` 改为委托 |
| `minEngineMcpServer/scripts/ui_prefab_smoke.ts` | 端到端 smoke（55 检查） |

复用而非重写：所有序列化/落盘/回滚均走 ED-F16 已有路径。

## State & Invariants

- `scene.*` 可寻址的会话类型：Scene、Prefab（后者映射到 Stage 场景）。
- 同一时刻最多一个可操作 Stage；开新 Stage 前必须处理旧 Stage（干净则关，脏则报）。
- `overwrite=true` 后资产 GUID 不变；失败后磁盘仍为旧内容。
- `set_property` 批量写的解析阶段是全有或全无；写入阶段不保证回滚，但必须如实报告。
- 自动化 op 只在编辑器面暴露；游戏模块不得调用（防「Play 写盘」复发）。

## Failure Model

| 失败 | 行为 |
|------|------|
| 无会话内 GO / 目录不存在 / 已存在且未指 overwrite | `ObjectNotFound` / `DirectoryNotFound` / `AssetExists` |
| 保存失败 | 模板回滚 + 文件字节还原，`SaveFailed` |
| 引用解析失败 | `UnresolvedObjectRef`（带 index/propertyPath），零写入 |
| 写入阶段部分失败 | `PartialWrite` + `applied[]`，不谎称回滚 |
| Stage 循环后偶发 AV | 见 [BUG-EDITOR-007](../../bugs/BUG-EDITOR-007.md)；主路径改用 `create_from_game_object` 规避 |

## Verification

- `scripts/verify.ps1` → `VERIFY_EXIT=0`，13 套 PASSED
- `minEngineMcpServer/scripts/ui_prefab_smoke.ts` → **55/55 checks passed**（含：建树→绑图→存 Prefab→再实例化名一致；overwrite 保 GUID；幂等重跑；Stage 循环；三种引用与两类失败面）
- 合入前要求：`ui_prefab_smoke.ts` 连跑 N 次不崩（受 BUG-EDITOR-007 阻塞）

## Deferred

1. 嵌套 Prefab / Prefab 变体的引用重写（现在覆盖会重生成模板 GUID）。
2. 数组/集合内的对象引用。
3. 真正的原子替换（临时文件 + rename）：现为备份+还原，进程被硬杀仍可能截断。
4. 自动化 op 层面的 Prefab 单元测试（当前 Editor automation surface 无独立测试目标，靠 smoke 覆盖）。
