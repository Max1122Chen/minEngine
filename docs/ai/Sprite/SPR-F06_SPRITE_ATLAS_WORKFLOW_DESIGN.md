# SPR-F06 — Sprite Atlas Workflow UX — Design Spec

## Meta
- **ID:** SPR-F06
- **Type:** Feature
- **Complexity:** L2
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - 硬前置：[SPR-F04](./SPR-F04_SPRITE_AUTHORING_SERVICES_DESIGN.md) · [SPR-F02](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)
  - 并行：[SPR-F05](./SPR-F05_SPRITE_EDITOR_DESIGN.md)
  - Roadmap：[SPR-F01](./SPR-F01_SPRITE_SYSTEM_ROADMAP.md)

## TL;DR

**问题：** 用户期望「多选 Sprite → 创建 Atlas → Rebuild → 运行时共享一张图」，需要 CB/编辑器工作流，而不仅是 API。

**方案：** Atlas 文档编辑器 + CB 多选创建；成员列表；Rebuild 按钮与打包预览（只读）；全部经 Services。不做手动拖拽排版。

**状态：** Draft。

## Scope
- **In:**
  - CB：多选 Sprite → Create Sprite Atlas（调用 F04）  
  - 打开 `.mespriteatlas`：成员列表、Add/Remove、Padding/MaxSize、Rebuild、Save  
  - Rebuild 后只读预览 GeneratedTexture（ImGui）  
  - 成员 Sprite 显示 Baked 状态  
- **Out:**
  - 手动拖拽改 slot 位置  
  - 多页 atlas UI  
  - 自动从文件夹监视热更新  
  - Flipbook  

## Reader quick start
1. §3 工作流  
2. §4 Atlas Editor  
3. §8 验收  

---

## Problem

F02/F04 已能 Pack；缺人类可发现的工作流。这是 Sprite 系统 MVP 的「最后一公里」产品面。

## Requirements

- 多选 ≥1 Sprite 可创建 Atlas 并加入成员  
- Rebuild 成功后：各 Sprite Runtime 指向同一 GeneratedTexture；Source 仍在  
- Rebuild 失败：UI 显示错误；资产不损坏  
- 打开 Atlas 可再次 Rebuild  

## Constraints

- UI → Services only  
- 不在 UI 线程发明第二套 packer  

## Preferences

- 预览只读即可（对齐 Unity Pack Preview 的「看结果」而非「手摆」）  
- Create 后可选自动打开 Atlas 编辑器  

## Assumptions

- F04 `AddAtlasMembers` / `RebuildAtlas` 已可用  

## Non-Goals

- 美术向的纹理合并工具链替代（外部 Aseprite 等）  
- 运行时换皮 late-bind UI  

## Current Architecture

- CB 多选与 Context Menu 已有模式（Create Material 等）  
- 无 Atlas 文档  

## Proposed Architecture

```text
CB multi-select Sprites
  → ContextMenu "Create Sprite Atlas"
  → SpriteEditorServices::CreateAtlas + AddAtlasMembers
  → optional TryOpenAsset(Atlas)

AtlasEditorWindow
  → Members list (add via picker / remove)
  → Properties: MaxSize, Padding
  → Rebuild → preview image
  → Save
```

### Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| Atlas Editor UI | 列表、参数、预览、调用 Rebuild | blit 实现 |
| CB 菜单 | 发现性入口 | 领域规则 |

## Boundaries

- 不在 CB 内隐式 Rebuild 无确认  
- 不把「方块图集」向导放进引擎  

## Dependencies

- F02、F04；建议 F03 已有便于场景验证，**非**代码硬前置  
- F05 并行，不互相阻塞  

## Ownership & Lifetime

- Atlas Session 拥有编辑态；Rebuild 可能脏多个 Sprite → Save 策略：  

**锁定草案：** Rebuild 成功后标记 Atlas + 所有受影响 Sprite dirty；Save Atlas 时 **级联 Save 脏 Sprite**（或提示用户 Save All）。Open Question 可改为「仅 Save Atlas，Sprite 自动写盘」。

## Data Flow

`选成员 → Add → Rebuild(临时→swap) → 预览 ← GeneratedTexture → Save`

## Control Flow

- Rebuild 同步；大图可显示 busy 光标（无异步 MVP）  

## State & Invariants

- 与 F02 一致  
- 预览纹理来自 GeneratedTextures[0]，只读  

## Failure Model

- Rebuild 失败：Toast/日志 + 保留旧 Generated（F02 事务）  
- 空成员 Rebuild：Error  

## Alternatives & Trade-offs

### 级联 Save
- **A 推荐：** Rebuild 后级联脏 Sprite，Save Atlas 写齐  
- B：只写 Atlas，Sprite Baked 仅内存——重启丢失，不可接受  

### 多选 Create 是否立即 Rebuild
- **推荐：** Create+Add 后 **询问或自动 Rebuild 一次**（减少空 Atlas 困惑）  

## Integration Impact

- CB 菜单；AssetWorkflow 打开 Atlas  
- Thumbnail：可用 Generated 后置  

## Migration / Compatibility

- 无  

## Verification Strategy

- 手验：3 个 Sprite → Create Atlas → Rebuild → 预览有图 → 场景/组件（若 F03）同纹理不同 UV  
- Source 纹理文件仍在工程且 Sprite Source 引用不变  
- Agent：同流程纯 Service  

## Open Questions

1. Save 级联策略最终式 — **blocking?** 轻微（实现前钉 A）  
2. Create 后是否默认自动 Rebuild — **blocking?** 否（默认自动）  
3. 从 Texture 多选直接「Extract Sprites + Atlas」一键 — **blocking?** 否（后置）  

## Design Review

- **Verdict:** Pending  

## Implementation Readiness

**Ready for implementation?** `no` — 依赖 F04。

## Acceptance Checklist

- [ ] Design review complete  
- [ ] Registry / Progress synced  

---

## Changelog

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
