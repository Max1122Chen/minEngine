# Content Browser — 产品意图（实现后置）

Last updated: 2026-05-26  
Status: **P6 基础设施已合入；P6.1 窗口 UI 为当前重点**  
父文档：[Platform 路线图](../PLATFORM_ROADMAP.md)、[Editor 平台化规划](../../Editor/EDITOR_PLATFORM_PLAN.md)

> **说明：** P6 数据层与窗口框架见 [ASSET_PIPELINE_P6_API.md](./ASSET_PIPELINE_P6_API.md)。**窗口视觉与交互**见 [CONTENT_BROWSER_UI_DESIGN.md](./CONTENT_BROWSER_UI_DESIGN.md)（依赖 Appearance M0–M6b，故意在合并后再做）。

---

## 产品意图（保留）

1. **Shared 窗口**；**Scene Editing** 下默认 dock **右下角**；**Material Editor 默认不显示**（Window 菜单后置）。
2. 展示 Project **`Assets/`** 树；条目为引擎识别资产类型（v0 无缩略图）。
3. **Import** 外部文件 → 项目目录 + meta → Registry 刷新。
4. **交互**：选中 → Inspector（meta + 预览）；右键 Delete/Move（文件 + meta + Registry）。
5. **双击** → 预留接口 + Log（P6.1）；打开 Asset Editor **Editor 路由设计后再接**。
6. **可扩展**：数据源、过滤、动作、预览 Provider 插件化。

---

## 依赖的基础板块

| 板块 | 文档 |
|------|------|
| Inspector 门面 + Drawer | [EDITOR_PLATFORM_PLAN.md § E1](../../Editor/EDITOR_PLATFORM_PLAN.md) |
| Previewer 统一化 | [EDITOR_PLATFORM_PLAN.md § E2](../../Editor/EDITOR_PLATFORM_PLAN.md) |
| AssetManager 基础设施 | [EDITOR_PLATFORM_PLAN.md § E3](../../Editor/EDITOR_PLATFORM_PLAN.md) |
| 跨平台 FileDialog | [EDITOR_PLATFORM_PLAN.md § E4](../../Editor/EDITOR_PLATFORM_PLAN.md) |

---

## 实施阶段

| 阶段 | 文档 | 状态 |
|------|------|------|
| P6 基础设施 | [ASSET_PIPELINE_P6_API.md](./ASSET_PIPELINE_P6_API.md) | **已合入** |
| P6.1 窗口 UI | [CONTENT_BROWSER_UI_DESIGN.md](./CONTENT_BROWSER_UI_DESIGN.md) | **当前** |
| E1 资产 Inspector | [EDITOR_PLATFORM_PLAN.md § E1](../../Editor/EDITOR_PLATFORM_PLAN.md) | 下一 |
| E2 Preview | [EDITOR_PLATFORM_PLAN.md § E2](../../Editor/EDITOR_PLATFORM_PLAN.md) | E1 后 |

## 下一步

- **P6.1：** Content Browser 接 `EditorAppearance` / 排版 / 主题 scope（见 UI 设计案验收表）。
- **E1：** Asset Meta 用 PropertyWidgets；`InspectorTarget` 统一路由。
- **E2：** 选中资产预览（与 Inspector 边界拍板后）。
