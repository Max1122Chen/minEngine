# ED-F05 — Implementation Plan

## Meta
- **ID:** `ED-F05`
- **Status:** Done（S00–S04；S05 Deferred）
- **Owner:** project maintainer
- **Last updated:** 2026-09-09
- **Related:** [Design Spec](./ED-F05_INSPECTOR_COMPONENT_UX_DESIGN.md)（**§10 实现设计**）

## TL;DR

S00–S04 **Done**（`feat/editor`，Editor 已编过）；S05 Deferred。实现细节以 Design **§10** 为准。

## Scope
- **In:** Design §Scope In；S00–S04。
- **Out:** Design §Out；S05 Deferred。

## Reader quick start
1. [Design §10](./ED-F05_INSPECTOR_COMPONENT_UX_DESIGN.md) — API / 图标表 / 数据流  
2. 下表 — 状态  
3. `PROGRESS_LOG.md`  

---

## 1) 切片总览

| Slice ID | 内容 | 优先级 | 状态 | 验证 |
|----------|------|--------|------|------|
| `ED-F05-S00` | `InlineRenameField`：Esc/点空白取消；Enter 提交 | 高 | **Done** | Hierarchy / Inspector / CB |
| `ED-F05-S01` | Catalog + Inspector Add + **右键 Add Component ▶ 子菜单** | 高 | **Done** | 面板与右键同源 Submit |
| `ED-F05-S02` | 头 icon+name+TypeDisplay；`RenameComponentCommand` | 高 | **Done** | 改名 Undo |
| `ED-F05-S03` | `GameObject::MoveComponent` + ↑↓ + Undo；Root 置顶 | 中 | **Done** | Root API+UI 禁移 |
| `ED-F05-S04` | CB + `AssetManager::RenameAsset` + InlineRename | 高 | **Done** | F2 / 树 / 瓦片 |
| `ED-F05-S05` | 视口图标 | 中 | **Deferred** | — |

---

## 2) 切片详情

### ED-F05-S00 — Rename dismiss
- **Goal:** Design §10.1  
- **Touch:** 新建 `Editor/src/UI/Widgets/InlineRenameField.*`；改 `HierarchyWindow.*`  
- **DoD:**
  - [x] Esc / 点空白 → Cancel，不 Dirty
  - [x] Enter → Commit → 现有 `SubmitRenameGameObject`
  - [x] 空名 → 按确认 A 关框不提交空串
- **Verify:** Hierarchy 三种退出路径。

### ED-F05-S01 — Add Component catalog + context submenu
- **Goal:** Design §10.2 + **§10.9**  
- **Touch:** `ComponentTypeUiCatalog.*`；`AddComponentPicker.*`；`SceneEditorInspectorSource`；`SceneContextMenuProviders.*`；Abstract 标注；`InitializeComponentTypeNames`  
- **DoD:**
  - [x] 静态表 + 基类回退 + 默认 glyph
  - [x] Inspector：Filter + 图标行
  - [x] Abstract 靠 specifier
  - [x] 右键根级 **Add Component ▶** 次级菜单（无 Create 外壳）；**含子菜单搜索**
  - [x] 可见：**仅 Inspector** GO Header / Components 空白；Hierarchy 暂不含；同构 Submit
- **Verify:** Inspector 右键加组件 + 子菜单搜 `point`；面板搜过滤；图标目视。

### ED-F05-S02 — Component header
- **Goal:** Design §10.3  
- **Touch:** `SceneEditorInspectorSource` header；`RenameComponentCommand.*`；Add 后 `Rename()`；切词 API  
- **DoD:**
  - [x] `[icon] name TypeDisplay`；名不切词；类型切词去后缀
  - [x] 内联改名用 S00 helper
  - [x] Undo 往返；PropertyPath 仍按类型
- **Verify:** 两灯改名；`get GO@PointLightComponent...` 仍可用。

### ED-F05-S03 — Reorder
- **Goal:** Design §10.4  
- **Touch:** `GameObject::MoveComponent`；`MoveComponentCommand.*`；Inspector ↑↓  
- **DoD:**
  - [x] Root 不可移（API + UI；不可落到 Root 及之前）
  - [x] Save/Load 顺序；Undo（`MoveComponentCommand`）
- **Verify:** 调序保存重开；可选单测 Move。

### ED-F05-S04 — CB inline rename
- **Goal:** Design §10.5  
- **Touch:** `ContentBrowserWindow.*`；`ContentBrowserModule` pending rename；`EditorEditActions` Rename；`AssetManager::RenameAsset`；`InlineRenameField`  
- **DoD:**
  - [x] Enter 成功；Esc/空白取消
  - [x] 同目录重名拒绝（`RenameAsset`/`MoveAsset`）；扩展名保留
  - [x] 无 Undo（确认 O）；刷新树/选中
  - [x] **右键 Rename**（TreeAsset / TileAsset）与 F2 同源进入内联改名
- **Verify:** 改名后双击仍打开；meta/registry 路径一致。

### ED-F05-S05 — Viewport icons（Deferred）
- 见 Design §10.6。

---

## 3) 依赖顺序

```text
S00 (InlineRenameField)
  ├─→ S02 / S04 复用
S01 (Catalog)
  └─→ S02 复用图标/TypeDisplay
S03 相对独立（可与 S02 后）
S05 Deferred
```

---

## 4) 延后

| Slice ID | Reason | Unblock |
|----------|--------|---------|
| `ED-F05-S05` | 维护者后置 | 显式提优 |

---

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-09 | Draft / Review 切片表 |
| 2026-09-09 | S01 含右键根级 Add Component；对齐 Design §10.9 |
| 2026-09-09 | S00–S04 落地；Editor 构建通过；S05 仍 Deferred |
| 2026-09-09 | S04 补：CB 右键 Rename（与 F2 同源 pending） |
