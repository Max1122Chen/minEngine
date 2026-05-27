# Editor 任务推进安排（2026-05-27）

Last updated: 2026-05-27  
Status: **rolling**（S3 M3 Done；S4 M4 Done；新增 **S4.1 稳定性修订**）  
父文档：[`PLATFORM_ROADMAP.md`](../Platform/PLATFORM_ROADMAP.md) §10

---

## 0) 定位

推进安排文档；设计细节见 [`EDITOR_CONTEXT_MENU_DESIGN.md`](./EDITOR_CONTEXT_MENU_DESIGN.md) **§6.1、§15.4**。

---

## 1) 今日主线（可增量）

> 今日 Editor 侧主要目标：**Inspector 场景右键 / Texture2D 预览 / CB Asset Icon**

### A. Inspector 右键菜单（SceneEditing）

- 目标：在 **SceneEditing** 语境下补齐 Inspector 右键菜单  
  - 命中 **Component 行** 时：弹出菜单，提供 **Remove Component**（通过右键菜单 Action，而非本地按钮）  
  - 命中 **GO Inspector 区域空白处** 时：弹出菜单，提供 **Add Component**，并通过 **ActionProvider** 构建组件列表
- 路线图对应：P2/E1（Inspector 统一化）+ P7（菜单/产品集成）

#### A 子任务勾选

#### A 子任务勾选

| 子项 | 状态 |
|------|------|
| M0 骨架 + `GetContextMenu()` | [x] |
| M1 Context + CB 右键接入 | [x] |
| M1 Delete / Import / Refresh | [x] |
| M1 代码清理：移除 Reveal + Rename 临时实现 | [x] |
| M2 Hierarchy + Inspector | [x] |
| M3 移除 Tools FileDialog | [x] |
| M4 Provider + Section 子菜单 | [x] |
| M4.1 稳定性修订（崩溃/范围收口） | [ ] |

### B. Texture2D 预览图

- 目标：为 **Texture2D 资产** 提供可视预览  
  - 在 Inspector / Previewer 中选中 Texture2D 时，展示贴图内容（E2.2b）  
  - 优先实现单张 2D 纹理的基础预览，不引入复杂缩略图缓存策略
- 路线图对应：P2/E2（Previewer）中的 **Texture2D Inspector 预览（E2.2b）**

### C. Content Browser Asset Icon

- 目标：为 Content Browser Tile 引入 **静态 Asset Icon**，替换当前统一的占位方框  
  - 按 `AssetType`（Scene / Material / Texture / Mesh / Font …）绘制不同 Icon  
  - 暂不实现缩略图；缩略图仍归 E2.3b（后续）
- 路线图对应：P2/P6.1（CB UI 抛光）+ `EDITOR_CONTEXT_MENU_DESIGN` §11.7 关于 Icon/缩略图预留接口

---

## 2) 执行切片

| Slice | 内容 | 状态 |
|-------|------|------|
| S0 | M0 骨架 | **Done** |
| S1 | M1 CB（Delete/Import/Refresh） | **Done**（收口） |
| S1b | 删除 Reveal/Rename 临时代码 | **Done** |
| S2 | M2 Hierarchy / Inspector | **Done** |
| S3 | M3 Tools FileDialog | **Done** |
| S4 | M4 Provider + Section 子菜单 | **Done** |
| S4.1 | M4 稳定性修订（Section 策略 + 作用域收口） | **Planned** |

---

## 4) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-27 | 初版；S0；方案 A；S1 |
| 2026-05-27 | **M1 收口**：Reveal/Rename 暂缓；S1b 代码清理；对齐设计 §15.4 |
| 2026-05-27 | **M2**：Hierarchy/Inspector 右键；`EditorEditActions` 统一 Delete；Duplicate/Focus 占位 |
| 2026-05-27 | **M4**：`SceneAddComponentMenuProvider`；Section `BeginMenu`；Create Empty / Remove Component |
| 2026-05-27 | **S4.1 计划**：修复 Edit 悬停崩溃；仅 Create 使用折叠子菜单；Hierarchy GO 菜单移除 Add Component；Inspector 右键临时下线 |
