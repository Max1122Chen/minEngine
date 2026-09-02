# ED-F03 — Editor Play Toolbar（Viewport 内三行布局）

## Meta
- **ID:** `ED-F03`
- **Type:** Design Spec
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-02
- **Related:** [EDITOR_SHELL_DESIGN](./EDITOR_SHELL_DESIGN.md) · [EDITOR_APPEARANCE](./EDITOR_APPEARANCE.md) · [CORE-F05 Play Mode](../Platform/Core/CORE-F05_PLAY_MODE_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md)

## TL;DR
Scene **Viewport 面板**垂直分三层：**① Dock Tab 行**（ImGui 原生）→ **② Toolbar 行**（面板内固定高度、Icon 居中）→ **③ 场景渲染主体**（剩余高度）。  
不再使用 Tab 行叠加、画面浮条或全宽全局 Chrome 条。

---

## 1) 变更动机

| 迭代 | 问题 | 结论 |
|------|------|------|
| v1 全局 Chrome 条 | Dock 四周蓝色内缩 | 废弃；`DockSpaceOverViewport` |
| v2 画面内浮条 | 挡场景、不像编辑器 | 废弃 |
| v2.1 Tab 行叠加 | `DockNode->TabBar` 叠加不可靠，**Toolbar 不可见** | 废弃 |
| **v2.2 面板内 Toolbar 行** | 稳定、可预期、与 Unity Game View 一致 | **采用** |

---

## 2) 目标与非目标

### 2.1 目标

| # | 目标 |
|---|------|
| G1 | Viewport 面板内：**Tab → Toolbar → 场景** 三行垂直结构 |
| G2 | Toolbar：**FA Solid Icon**、**水平居中**；Play / Stop / Pause / Step |
| G3 | 场景 RT 使用 Toolbar **之后**的 `GetContentRegionAvail()`，Gizmo 仍在画面上 |
| G4 | Dock 黑灰底色；无自定义 Dock Host 内缩 |
| G5 | `IPlayModeService` 绑定；F5 / Shift+F5 快捷键（`ToolbarModule`） |

### 2.2 非目标

- Material Viewport 的 Toolbar 行（仅 Scene Viewport）
- 全宽全局第二行 Chrome
- Tab 行内绘制控件（v2.1 已废弃）
- Pause/Step 行为（`CORE-F05-S05`）
- FPS / PlayState 指示（可后续加在角标）

---

## 3) 布局规格

### 3.1 Viewport 面板剖面

```text
┌──────────────────────────────────────┐
│ ▼ Viewport                    ① Tab │  ImGui Dock Tab（系统绘制）
├──────────────────────────────────────┤
│        ▶  ■  |  ⏸  ⏭           ② Bar │  固定高度 ~32px；Icon 居中
├──────────────────────────────────────┤
│                                      │
│         Scene Render Target      ③ │  剩余高度；letterbox 居中
│                                      │
└──────────────────────────────────────┘
```

### 3.2 编辑器整体

```text
┌─────────────────────────────────────────────────────────────┐
│ File  Edit  View  Window  Tools  Help          MainMenuBar │
├───────────────────┬──────────┬──────────────────────────────┤
│ Viewport (三行)   │ Hierarchy│ Inspector                    │
├───────────────────┴──────────┴──────────────────────────────┤
│ Console                      │ Content Browser              │
└─────────────────────────────────────────────────────────────┘
```

---

## 4) 架构

| 组件 | 职责 |
|------|------|
| `EditorChrome` | 仅 MainMenu |
| `EditorGUIManager` | `DockSpaceOverViewport` + `DrawWindows`（**不**再画 Toolbar） |
| `EditorViewportWindow` | `BeginPanel` → 可选 `DrawViewportToolbarRow()` → `DrawSceneColorImage` |
| `SceneEditingViewportWindow` | 启用 Toolbar 行；Gizmo 在 `OnPostSceneImageDraw` |
| `ViewportPlayToolbar` | `DrawToolbarRow(IEditorContext&)`：面板内一行 Icon |
| `ToolbarModule` | F5 / Shift+F5 |

### 4.1 绘制顺序（单帧、Viewport 窗内）

```text
1. EditorWindowTypography::BeginPanel("Viewport")
2. ViewportPlayToolbar::DrawToolbarRow()   // ② 占固定高度
3. DrawSceneColorImage()                   // ③ 用剩余 avail
4. OnPostSceneImageDraw() → Gizmo
5. ImGui::End()
```

### 4.2 扩展点

```cpp
// EditorViewportWindow — 默认无 Toolbar 行
virtual bool WantsViewportToolbarRow() const { return false; }
virtual void DrawViewportToolbarRow() {}
```

`SceneEditingViewportWindow` 覆写为 `true` 并调用 `ViewportPlayToolbar::DrawToolbarRow`。

---

## 5) 样式

| 项 | 规格 |
|----|------|
| 行高 | `按钮 30px` + 上下各 `6px` padding |
| 与视口分界 | **`ImGui::Separator()`**；无 Child 背景、无边框、无圆角块 |
| 字体 | `GetAssetIconSolidImFont()`，`17px` |
| 按钮 | `30×30` 方按钮；**间距 8px**；Play|Stop 与 Pause|Step 组间 `12px` + `\|` |
| Glyph | `ICON_FA_PLAY` / `STOP` / `PAUSE` / `FORWARD_STEP` |
| Stop | Playing 时偏红强调 |
| Pause/Step | `BeginDisabled(true)` |

---

## 6) 行为

| 控件 | 条件 | API |
|------|------|-----|
| Play | `!IsPlaying()` | `EnterPlay()` |
| Stop | `IsPlaying()` | `Stop()` |
| Pause / Step | 禁用 | S05 |
| F5 / Shift+F5 | 全局 | 同左 |

---

## 7) 文件

| 路径 | 说明 |
|------|------|
| `UI/Chrome/ViewportPlayToolbar.*` | `DrawToolbarRow`（移除 `DrawInDockTabBar`） |
| `UI/EditorWindows/EditorViewportWindow.*` | Toolbar 行 hook + 绘制顺序 |
| `UI/EditorWindows/SceneEditingViewportWindow.*` | 启用 Toolbar |
| `EditorGUIManager.cpp` | 移除 Tab 叠加调用 |
| `Shell/EditorChrome.*` | Menu only |
| `Services/ToolbarModule.*` | 快捷键 |

---

## 8) 验收（DoD）

- [x] Viewport 可见：**Tab 下**有独立 Toolbar 行，**其下**为场景
- [x] Icon 居中；Play/Stop 可用；Pause/Step 灰显
- [x] 无 Tab 叠加、无画面浮条、无全局 Chrome 条
- [x] Dock 无彩色内缩；Gizmo 正常
- [x] F5 / Shift+F5 可用

---

## 9) 决策记录

| ID | 决策 |
|----|------|
| D1 | **面板内 Toolbar 行**（Tab 与场景之间） |
| D2 | Icon 居中（FA Solid） |
| D3 | `DockSpaceOverViewport`，无自定义 Host |
| D4 | 仅 Scene Viewport |
| D5 | F5 / Shift+F5 |

---

## 10) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | v1：全局 Chrome Toolbar |
| 2026-09-02 | v2：画面内浮条；移除 Overlay / `ToolbarWindow` |
| 2026-09-02 | v2.1：Tab 行叠加（废弃，不可见） |
| 2026-09-02 | **v2.2**：Viewport 三行布局（Tab / Toolbar 行 / 主体） |
| 2026-09-02 | **v2.2.1**：Toolbar 行去掉 Child 背景框，仅用 `Separator` 与视口分界；按钮 30px、间距加大 |
