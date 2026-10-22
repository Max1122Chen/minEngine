# BUG-EDITOR-009 — scene.set_property 写空串到 string 属性崩溃

## Meta
- **ID:** BUG-EDITOR-009
- **Status:** Fixed
- **Severity:** S2（MCP/自动化改内容时必现崩溃）
- **Owner:** Editor Automation / Scene property write
- **Found:** 2026-10-22（minCraft 实践反馈）
- **Last updated:** 2026-10-22
- **Affects:** `EditorAutomationSurface::WritePropertyValue`；`scene.set_property` 对 `std::string` 属性赋值 `""`
- **Related Feature/Slice:** [MCP-F02](../Editor/MCP/MCP-F02_AUTOMATION_PROTOCOL_DESIGN.md) / [MCP-F05](../Editor/MCP/MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_DESIGN.md)

## TL;DR

MCP `scene.set_property` 把 string 属性写成空串 `""` 时编辑器崩溃。根因是 Scene 写路径用未构造的 raw `uint8_t` buffer 做 `GetProperty`/`SetProperty`，对非平凡 `std::string` 触发 copy-assign UB。已改为与 Material/AnimGraph 一致的 blob capture + `CommitObjectPropertyBlobs`。

---

## 症状

- 通过 MCP / Automation 调用 `scene.set_property`，`value` 为 JSON 空串 `""`，目标为 `std::string` 反射属性（如 `m_Name`）→ 进程崩溃（访问违例 / abort）。
- 非空串赋值可能偶发「看起来成功」但仍是同一 UB 路径。

## 期望

- 空串与非空串均可安全写入；未变更时返回 `unchanged`。
- 写入走序列化 blob + undo 命令，不对未构造存储做 `std::string` assign。

## 复现

1. 打开带可编辑 Scene 会话的 Editor（或 minCraft 产品 Editor）。
2. 对某 GameObject：`scene.set_property`，`propertyPath: "m_Name"`，`value: ""`（原名非空更易复现）。
3. 观察崩溃。

## 环境

- Windows；Debug 合同构建（`minEngine/build` → `minEngine/bin`）。
- 消费方：minCraft 经 MCP 改游戏内容。

## 根因

`WritePropertyValue` 在 `TryWriteJsonToLive` 之后：

1. `std::vector<uint8_t> afterBuffer(storageSize)`（零填充，**未** `ConstructValue`）
2. `GetProperty` → `CopyPropertyStorage` → 对 `std::string` 调用 `operator=` 到未构造对象 → UB
3. 再对 `beforeBuffer` 重复，并 `SetProperty(afterBuffer)`

Inspector（`ConstructValue`/`DestructValue`）与 `HandleGraphSetProperty`（blob）已正确；仅 Scene 自动化写路径遗漏。

零填充内存 ≠ 合法空 `std::string`（合法空串 data 指向 SSO 本地缓冲）。空串赋值更容易在 `operator=` 释放「假旧状态」时立刻崩。

## 修复

`EditorAutomationSurface::WritePropertyValue` 对齐 `HandleGraphSetProperty`：

1. `TryWriteJsonToLive`
2. `TryCaptureBlob` → `afterBlob`
3. `TryRestoreBlob(beforeBlob)`
4. `beforeBlob == afterBlob` → `unchanged`
5. `SceneEditorServices::CommitObjectPropertyBlobs(..., applyOnFirstExecute=true)`

删除 raw `GetProperty`/`SetProperty` 环路。

## 回归验证

- [x] `minEngine/bin/minEngineTests.exe test scene-property-string` — 3 passed / 36 assert（空串 blob 写、empty→empty unchanged、constructed temp assign）
- [ ] 可选：MCP 手验 `m_Name` ← `""`

## 关联

- 对照正确实现：`EditorAutomationSurfaceGraph.cpp` `HandleGraphSetProperty`
- Inspector 对照：`SceneEditorInspectorSource::DrawLeafPropertyViaAssign`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-22 | 开单；根因与修复方案；实现 + `scene-property-string` 回归；Status → Fixed |
