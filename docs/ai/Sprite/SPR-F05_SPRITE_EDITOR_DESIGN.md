# SPR-F05 — Sprite Editor MVP — Design Spec

## Meta
- **ID:** SPR-F05
- **Type:** Feature
- **Complexity:** L2
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - 硬前置：[SPR-F04](./SPR-F04_SPRITE_AUTHORING_SERVICES_DESIGN.md)（Services）· [SPR-F02](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)
  - Roadmap：[SPR-F01](./SPR-F01_SPRITE_SYSTEM_ROADMAP.md)
  - 工作流：[ED-F02](../Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) · Tab：[ED-F11](../Editor/ED-F11_MULTI_DOCUMENT_TAB_HOST_DESIGN.md)
  - 并行：[SPR-F06](./SPR-F06_SPRITE_ATLAS_WORKFLOW_DESIGN.md)

## TL;DR

**问题：** 人需要从 Texture **可视化切分** Sprite，而不是只改数字字段。

**方案：** Sprite 文档编辑器：显示 Source 纹理，拖拽/输入像素矩形，调用 F04 Services 写回；Save；双击 CB 打开。不做动画帧时间轴。

**状态：** Draft。

## Scope
- **In:**
  - 打开/保存 `.mesprite`  
  - Source 纹理预览 + region 矩形（拖拽与数值）  
  - 显示 Runtime 提示（是否已 Baked / 所属 Atlas 名）  
  - 所有写入走 `SpriteEditorServices`  
  - `TryOpenAsset` / Create 接线  
- **Out:**
  - Flipbook 时间轴、多帧编辑  
  - 自动 trim / 魔法棒选区  
  - Atlas 排版画布（F06）  
  - 完整 Undo 图形抛光（依赖 F04 Command）  

## Reader quick start
1. §3 UI 结构  
2. §4 与 Services 边界  
3. §8 验收  

---

## Problem

F04 提供 API；人仍需要「看见纹理再切」。这是引擎内容管线的标准缺口，不是 minCraft 特例。

## Requirements

- CB 双击 Sprite → 进入编辑器  
- 可见 Source 图像与当前 SourceRect 叠加  
- 修改 rect → dirty → Save  
- 若已 Baked：UI 警告「需 Atlas Rebuild」且不直接改 Baked  

## Constraints

- GUI 不直接写资产字段绕过 Service  
- 复用现有 Document Session / dirty / Tab 模式  

## Preferences

- 布局参考 Texture Region 类编辑器（Godot `texture_region_editor` 思路）：左预览、右属性  
- Pivot 编辑可后置；MVP 只切 rect  

## Assumptions

- SourceTexture 可在 Editor 取得 CPU/预览图（与 Material 预览同类能力）  

## Non-Goals

- 像素级绘制；图集打包预览（F06）  

## Current Architecture

- Material / Anim Graph / MIC 已有 SubEditor + TryOpenAsset  
- 无 Sprite 文档模式  

## Proposed Architecture

```text
ContentBrowser / File
  → AssetWorkflowModule::TryOpenAsset(Sprite)
  → SpriteEditorModule / DocumentSession
  → SpriteEditorWindow (ImGui)
       → SpriteEditorServices::SetSpriteSourceRect / Save
```

### UI 草案

| 区域 | 内容 |
|------|------|
| 预览 | Source 纹理；半透明遮罩外区；可拖矩形柄 |
| 属性 | SourceTexture 引用；Rect x,y,w,h；只读 Baked / Atlas |
| 工具条 | Save；必要时「Clear Baked」经 Service |

### Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| SpriteEditor UI | 交互、预览 | Pack |
| Services | 突变、Undo | 布局像素算法以外的业务 |

## Boundaries

- 禁止 UI 直接 `sprite->BakedTexture = ...`  
- 禁止在 Sprite Editor 内触发整 Atlas Rebuild（可按钮跳转/提示去 F06）  

## Dependencies

- F02、F04；ED-F02/F11 模式  
- F03 非硬前置（预览可用 2D ImGui 图像，不必场景 Proxy）  

## Ownership & Lifetime

- Session 拥有打开中的 Sprite 编辑态；关闭确认 dirty  

## Data Flow

`用户拖拽 → Service.SetRect → 资产 SourceRect → 预览刷新`

## Control Flow

- 输入在 Editor Tick / ImGui  
- Save 同步  

## State & Invariants

- 编辑的是 SourceRect；不编辑 BakedRect  
- dirty 与 Session 一致  

## Failure Model

- SourceTexture 丢失：显示占位 + 错误，禁 Save 覆盖坏引用（或允许清引用——推荐禁 Save 直至修复）  

## Alternatives & Trade-offs

### 预览实现
- **A：** ImGui 图像 + 矩形（推荐 MVP）  
- B：隔离 Scene 放一张 SpriteComponent——重，依赖 F03  

## Integration Impact

- AssetWorkflow 路由；CB Create Sprite  
- 可选 Thumbnail 后置  

## Migration / Compatibility

- 无  

## Verification Strategy

- 手验：Create from Texture → 打开 → 改 rect → Save → 再开仍正确  
- Agent：同流程纯 Service（F04）已覆盖逻辑；本 Feature 验 UI 接线  

## Open Questions

1. 是否支持「从 Texture 右键 Extract Sprite」快捷入口 — **blocking?** 否（可进 F06/CB）  
2. 多 Sprite 从同一 Texture 切片工作流是否进本 Feature — **blocking?** 否（MVP 单资产编辑；批量 Extract 可 F06 或后置）  

## Design Review

- **Verdict:** Pending  

## Implementation Readiness

**Ready for implementation?** `no` — 依赖 F04 Planned。

## Acceptance Checklist

- [ ] Design review complete  
- [ ] Registry / Progress synced  

---

## Changelog

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
