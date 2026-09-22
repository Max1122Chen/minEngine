# SPR-F04 — Sprite Authoring Services（Human / Agent）— Design Spec

## Meta
- **ID:** SPR-F04
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - 硬前置：[SPR-F02](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)
  - UI 消费者：[SPR-F05](./SPR-F05_SPRITE_EDITOR_DESIGN.md) · [SPR-F06](./SPR-F06_SPRITE_ATLAS_WORKFLOW_DESIGN.md)
  - 模式：[ED-F18](../Editor/ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_DESIGN.md) · [ED-F19](../Editor/ED-F19_MATERIAL_ANIM_EDITOR_SERVICES_DESIGN.md)
  - 哲学：ENGINE_DESIGN_PHILOSOPHY §5（Editor 域服务与 Client）

## TL;DR

**问题：** 即使有 Sprite/Atlas 资产，人和 Agent 仍缺**同一套**可撤销/可脚本的创建、切分、组 Atlas、Rebuild 原语。

**方案：** 新增 `SpriteEditorServices`：只暴露域操作；GUI 与 Automation Surface 薄封装；不新增「Agent 专用」API。Undo 走显式 `EditorCommandStack`（F05 Tab Session 接入时直接传 session 栈）。

**状态：** Done。

## Scope
- **In:**
  - Create Sprite from Texture（全图 / 像素 rect）  
  - Set/Clear Sprite source rect  
  - Create Atlas；Add/Remove members  
  - Rebuild Atlas  
  - Save Sprite / Atlas 文档  
  - 与 Document Session / Command 栈集成（对齐 Material）  
- **Out:**
  - ImGui 画布细节（F05/F06）  
  - 新 MCP Tool / 新 kind（用现有 `editor_edit` / `asset.*`）  
  - 为 Client 拼整图 JSON 的服务端 API  

## Reader quick start
1. §3 操作表  
2. §4 Session / Undo  
3. §7 验收  

---

## Problem

F02 定义数据；F05/F06 需要 UI。中间必须有 **可编程原语**，否则 Agent 无法验收管线，GUI 也会直接改字段造成无 Undo。

## Requirements

- 下列操作均可仅通过 Services 完成（无 GUI）  
- 可变操作进入对应文档 Session 的 Command 栈（可 Undo）  
- Rebuild / Compile 类：可不进 Undo，但必须可重复调用且事务式（F02）  
- Create 走 `ASSET-F03` 身份契约  

## Constraints

- 服务不解析 Automation JSON 业务结构（Surface 拼）  
- 服务不依赖「当前 UI 选中」作为唯一目标——**显式目标**（ED-F13）：路径 / 资产指针 / session id  

## Preferences

- 类比 `MaterialEditorServices` 的命名与生命周期挂接  
- CB「Create Sprite / Atlas」菜单调用同一 Service  

## Assumptions

- Tab Host 可为 Sprite/Atlas 开 Session（F05/F06）；F04 可先支持「无 UI Session」的资产级 Command 或临时 session——Open  

## Non-Goals

- list_all_sprites_in_project 类聚合查询进 Service（可用 Registry / TypeQuery）  
- Pack 算法参数的炫酷预设库  

## Current Architecture

- Material/Anim：`*EditorServices` + Command + Surface op  
- AssetWorkflow：`TryOpenAsset` / `CreateAsset`  
- 无 Sprite 服务  

## Proposed Architecture

```text
GUI / Automation Surface
  → SpriteEditorServices
      → EditorCommand (mutate) | direct Rebuild (non-undo)
      → Sprite / SpriteAtlas assets
```

### 操作表（锁定草案）

| API | 可变？ | Undo | 说明 |
|-----|--------|------|------|
| `CreateSpriteFromTexture(tex, rectPx?, dir, name)` | 创建 | N/A 或 Create cmd | 落盘 + registry |
| `SetSpriteSourceRect(sprite, rectPx)` | 是 | 是 | 清 Baked 或标需 Rebuild |
| `CreateAtlas(dir, name)` | 创建 | N/A | 空成员 |
| `AddAtlasMembers(atlas, sprites[])` | 是 | 是 | 不自动 Rebuild |
| `RemoveAtlasMember(atlas, sprite)` | 是 | 是 | ClearBaked |
| `RebuildAtlas(atlas)` | 写多资产 | **否**（或单次复合 cmd） | 事务式 |
| `SaveSprite` / `SaveAtlas` | 落盘 | 否 | |

**推荐：** `SetSpriteSourceRect` 若 Sprite 已在 Atlas 中 → ClearBaked + 日志提示需 Rebuild（不隐式 Rebuild）。

### Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `SpriteEditorServices` | 原语、入栈、调用 F02 Rebuild | ImGui；MCP schema |
| Surface | JSON ↔ 参数 | 领域规则 |
| GUI | 选区、预览 | 重复实现 Pack |

## Boundaries

- Allowed：Services → AssetManager / Commands / SpriteAtlas::Rebuild  
- Forbidden：Services 内 `dynamic_cast` 到具体 Window；为 Agent 返回「整个项目 Sprite 树」大包  

## Dependencies

- F02；ED-F18 模式；AssetWorkflow Create  
- F05/F06 依赖本 Feature  

## Ownership & Lifetime

- Services 生命周期对齐 Editor 模块（与 Material 相同）  
- Command 拥有 before/after 快照或字段级 undo——实现选简单路径（整对象属性 blob 可接受 MVP）  

## Data Flow

`Request → Service → Command.Apply → Asset fields → optional Save`

## Control Flow

同步；主线程。

## State & Invariants

- 与 F02 不变量一致；Service 不得留下「半 Rebuild」  
- Create 后 Guid/meta 与 ASSET-F03 一致  

## Failure Model

| 失败 | 行为 |
|------|------|
| Create 路径冲突 | Error，不覆盖（或遵循现有 Create 策略） |
| Rebuild 失败 | 返回错误码/消息；资产保持重建前 |
| Add 已是其他 Atlas 成员 | Error 或先 Remove——**推荐 Error，显式** |

## Alternatives & Trade-offs

### Rebuild Undo
- **A：** 不入 Undo（推荐 MVP）  
- B：复合 Command 存旧 Baked 快照——内存大  

### Session 模型
- 有文档 Session 时改动入该栈；无 Session 时允许「资产级」全局栈或仅允许 Save 前直接改——Open，实现期对齐 Material  

## Integration Impact

- Automation Surface 增加 `sprite.*` ops（编辑期）  
- CB Create 菜单项  

## Migration / Compatibility

- 无旧 API  

## Verification Strategy

- 测试或 Editor 自动化：CreateSprite → CreateAtlas → Add → Rebuild → 查询 UV  
- Undo：SetSourceRect 一次可撤销  
- 失败：重叠 MaxSize Rebuild 不损坏 Source  

## Open Questions

1. 无打开文档时 Command 栈落点 — **blocking?** 否（可先要求 open session）  
2. `sprite.*` Surface 命名空间最终名 — **blocking?** 否  
3. Rebuild 是否允许在无 Session 下调用 — **blocking?** 否（允许）  

## Design Review

- **Verdict:** Accepted（实现合入）  

## Implementation Readiness

**Ready for implementation?** `done`

## Acceptance Checklist

- [x] Design / code review complete  
- [x] Registry / Progress synced  

---

## Changelog

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
| 2026-09-22 | Done：`SpriteEditorServices` + Commands；Asset create 白名单 Sprite/SpriteAtlas；suite `sprite-authoring` |
