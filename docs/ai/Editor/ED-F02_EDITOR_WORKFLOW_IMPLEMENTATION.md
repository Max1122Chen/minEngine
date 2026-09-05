# ED-F02 — Implementation Plan

## Meta
- **ID:** `ED-F02`
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-03
- **Branch:** `master`（原 `feat/editor` 已合入）
- **Related:** [Design](./ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)

## TL;DR

工作流主路径（S00–S02）与视口局部导航（S04）已在 `master`。剩余：**S03** Preview SkyBox 实体；**S05** 给不可实例化 Component 补 `ME_CLASS(Abstract)`（过滤代码已有）。

## Scope
- **In:** Design §Scope In；切片 S00–S05。
- **Out:** Component 图标、全资产类型创建、Play Mode、资产重命名引用更新。

## Reader quick start
1. [Design](./ED-F02_EDITOR_WORKFLOW_DESIGN.md) — 模块边界与 dirty 策略。
2. 下表 — 切片状态（以代码为准）。
3. 剩余工作在 `master` 小 PR 即可，不必再开 `feat/editor`。

---

## 1) 切片总览

| Slice ID | 内容 | 优先级 | 状态 | 验证 |
|----------|------|--------|------|------|
| `ED-F02-S00` | Content Browser 双击 → `TryOpenAsset` | 高 | **Done** | 双击 Scene / Material 进入对应模式 |
| `ED-F02-S01` | Scene `OpenAsset` + File Open + dirty 对话框 | 高 | **Done** | 打开、切换、Cancel 保留 dirty |
| `ED-F02-S02` | `CreateAsset` Scene/Material + New/Create UI | 高 | **Done** | 创建落盘、registry、可打开 |
| `ED-F02-S03` | Material Preview SkyBox | 中 | Planned | Material 视口可见天空（需 Preview 加 SkyBox） |
| `ED-F02-S04` | Scene 视口局部 RMB 鼠标捕获 | 中 | **Done** | hover/focus 门控；释放恢复光标 |
| `ED-F02-S05` | Abstract Component 过滤 | 低 | Partial | 过滤已实现；补 `PrimitiveComponent` 等 Abstract 标注 |

**Merge gate：** S01 + S02 — **已满足**（合入 `master`）。

---

## 2) 切片详情

### ED-F02-S00 — Content Browser 打开接线 — **Done**

- **Goal:** 双击资产调用统一打开 API。
- **Landed:** `ContentBrowserWindow::ActivateAssetFromBrowser` → `TryOpenAsset`。
- **DoD:**
  - [x] 双击 Material 打开 Material Editor
  - [x] 双击 Scene 可加载（随 S01）
  - [x] 打开失败有可读 log

### ED-F02-S01 — 打开 Scene 与 dirty 确认 — **Done**

- **Goal:** File Open Scene、Scene `OpenAsset`、切换前确认。
- **Landed:** `SceneEditor::OpenAsset` / `OpenSceneByPath`；`EditorUnsavedChangesDialog`；File Open；exit 确认路径。
- **DoD:**
  - [x] File → Open Scene
  - [x] dirty 三选项
  - [x] 加载成功清 CommandStack / dirty
  - [x] `OpenAsset` 对合法 Scene 不再 stub false

### ED-F02-S02 — 创建 Scene / Material — **Done**

- **Goal:** Runtime 创建 API + Editor 入口。
- **Landed:** `AssetManager::CreateAsset<Scene/Material>`；`TryCreateSceneInDirectory` / Material；File New Scene；CB Create；Save As。
- **DoD:**
  - [x] 磁盘 + meta
  - [x] Scene 可 Save
  - [x] Material 可打开
  - [x] Content Browser 可见（刷新后）

### ED-F02-S03 — Material Preview SkyBox — Remaining

- **Goal:** Material 编辑器预览与 Scene 视口一样有天空。
- **Touch:**
  - `PreviewScene::BuildDefaultSphereScene` — 添加 `SkyBoxComponent`（资源 GUID 与 EngineDefault 对齐）
  - `MaterialEditorViewportClient` — **已设** `EnableSkyBox | EnablePostProcess`（仅缺场景内容）
- **DoD:**
  - [ ] Material Editor 视口可见 SkyBox
  - [ ] Scene Editor 视口无回归
- **Verify:** 人工目视 Material Editor GL；`verify.ps1`

### ED-F02-S04 — Viewport 局部鼠标捕获 — **Done**

- **Goal:** RMB 导航仅作用于聚焦中的 Scene 视口。
- **Landed:** `IsHovered()` / `IsFocused()` 门控 begin navigate / selection / gizmo；`SetCursorVisible(!navigating)`。
- **DoD:**
  - [x] 视口内 RMB 进入导航
  - [x] 释放退出并恢复光标
  - [x] 面板上 RMB 不抢全局光标（门控）

### ED-F02-S05 — Abstract Component 过滤 — Partial

- **Goal:** 仅列出可实例化 Component。
- **Landed:** `InitializeComponentTypeNames` 跳过 `ClassSpecifier::Abstract`。
- **Remaining:** 给 `PrimitiveComponent` 等基类加 `ME_CLASS(Abstract)` 并 regen；核对右键 Add Component 同源列表。
- **DoD:**
  - [ ] Inspector Add Component 无抽象基类
  - [ ] 右键 Add Component 同步
  - [x] 过滤钩子存在
- **Verify:** 人工 Inspector 下拉；`verify.ps1`

---

## 3) 建议 PR / 提交顺序

```text
S00–S02 + S04 — Done on master
--- remaining ---
S03（Preview SkyBox 实体）— 独立小 PR
S05（Abstract 标注 + regen）— 独立小 PR
```

---

## 4) 测试与记录

| 检查 | 命令 / 步骤 |
|------|-------------|
| Smoke | `.\scripts\verify.ps1` |
| Editor | `Editor.exe --rhi opengl --project <path.meproject>` |
| 记录 | 每切片完成后 `PROGRESS_LOG.md` 一条 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 初版 Implementation Plan（S00–S05） |
| 2026-09-03 | 对照代码收口：S00–S02/S04 Done；S03/S05 余量；Branch=`master` |
