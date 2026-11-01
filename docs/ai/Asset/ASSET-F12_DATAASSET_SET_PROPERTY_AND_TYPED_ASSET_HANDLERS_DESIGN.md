# ASSET-F12 — DataAsset set_property + typed asset mutate handlers

## Meta
- **ID:** `ASSET-F12`
- **Type:** `Feature`
- **Complexity:** `L1`
- **Status:** `Done`
- **Owner:** project maintainer
- **Last updated:** `2026-11-01`
- **Related:**
  - [ASSET-F10](./ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md) / [S03](./ASSET-F10_S03_DATAASSET_DERIVED_CREATE_DESIGN.md)（多态 Load/Create）
  - [ASSET-F07](./ASSET-F07_DATA_ASSET_DESIGN.md)（DataAsset 编辑）
  - [ASSET-F05](./ASSET-F05_TEXTURE_COOKED_ASSET_DESIGN.md)（Texture2D Save = settings + cooked）
  - [BUG-SPR-002](../bugs/BUG-SPR-002.md)（weak cache + mutate-without-save）
  - ED-F18（AES 原语；Automation 只解析 JSON）
- **Depends on:** F10 S01–S03；`ScenePropertyValueCodec`；AssetManager weak load cache
- **Blocks:** Agent/minCraft 改派生 DataAsset 字段并可靠落盘；Texture/Sprite 专用 mutate MCP
- **Branch:** `feat/asset`（或现有 asset 区分支；勿用 ID 命名）

## TL;DR

引擎内置资产继续走 **`specific.set_xxx` + 同调用立即 Save**；游戏派生 **`DataAsset`** 走通用 **`dataasset.set_property`**（Load → 反射写 → Save，类由实例 `GetClass()` 决定）。  
不做万物 `asset.set_property`。所有 mutate op **必须在仍持有 `shared_ptr` 时 Save**，否则 weak cache 释放后改动永远无法落盘（已由 BUG-SPR-002 证实）。顺带修 `Texture2D` SaveHandler 写回 sampler，并补 `texture.set_sampler` / `sprite.set_source_rect`。

## Intent playback

Agent 需要改表型 DataAsset 字段，以及 Texture sampler / Sprite sourceRect。内置类型靠专用 op 钉死合法性；DataAsset 字段是游戏自定义的，适合反射通用写。成功标准：一次 MCP 调用后磁盘已更新；Evict + Reload 可见。不引入跨 RPC Document pin。

## Problem

| 缺口 | 结果 |
|------|------|
| 无 `dataasset.set_property` | 只能 GUI Inspector 改 DataAsset，Agent 卡死 |
| 部分 mutate 不 Save | weak_ptr 缓存：本地 `shared_ptr` 释放 → 实例销毁 → 下次 Load 从磁盘旧状态重建 |
| `sprite` 仅 create/save | 有 `SetSpriteSourceRect` 服务但无 MCP |
| `texture` 无 domain op | 无法改 sampler；且 `Texture2DLoader::SaveHandler` 今日只确认 `.metex` 存在，**不写回** 内存中的 filter/address |

## Requirements

### Must

1. **`dataasset.set_property`：** path 或 guid 定位 → 必须是 `DataAsset`（含派生）→ `propertyPath` + JSON `value` → 反射写入实例真实类 → **立即 `SaveAsset`** → 响应含 `saved` / path / guid / `runtimeClassName`。
2. **拒绝非 DataAsset** 与非法 path/value（失败，不静默）。
3. **内置 mutate 立即 Save：** 本 Feature 新增/接线的 `texture.set_sampler`、`sprite.set_source_rect`（及已有 atlas mutate）均在同调用内 Save。
4. **`Texture2D` SaveHandler** 把当前 sampler（及从现有 cooked 读出的其余 cook 设置）写回 `.metex` 像素不变。
5. **验证：** DataAsset 写字段后 Evict+Reload 一致；Texture sampler 经 Set+Save 后 Reload 一致；Sprite sourceRect 同理。

### Should

6. 整表 `m_Rows`（JSON 数组）可写；单元格 `m_Rows[i].field` **本期不强制**（codec 尚无数组下标段）。
7. CapabilityRegistry 登记新 op。

### Must not

- 统一 `asset.set_property` 按 AssetType 大 switch。
- 用通用 set_property 改 Texture/Sprite 替代专用 op。
- 跨 RPC pin / Document session 作为默认持久化手段。

## Constraints

- Capabilities not Opinions：DataAsset 字段语义属游戏；引擎只提供反射写 + Save。
- Mechanism：专用 op 保内置合法性；通用入口仅 DataAsset 树。
- Editor 原语在 AES / SpriteEditorServices；Automation 只解析 payload。
- 源码注释无 Feature/Slice/Bug/ADR ID。

## Assumptions

- `ScenePropertyValueCodec::TryWriteJsonToLive` + `EditorObjectPropertyApply::MakeDefaultOptions` 足以写 DataAsset 反射字段（含 `std::vector` 整表）。
- 产品 Editor / 测试进程在改游戏派生类前已加载对应模块。

## Non-Goals

- `dataasset.get_property` / 批量 set（可后置）。
- 数组下标路径、嵌套 ref 数组。
- Material / AnimGraph 改道（已有 `*.set_property` + 文档会话）。
- 修复所有历史「仅内存 mutate」路径的审计大扫除（本 Feature 覆盖新增 op + Texture Save；其余遇一件修一件）。

## Current Architecture

```text
AssetManager::m_LoadedAssetCache : path → weak_ptr<MEObject>
Load → shared_ptr；无其它持有者时析构，磁盘未改则改动丢失

DataAsset: Create/Load 多态 OK；Inspector 手动 Save；无 MCP set
Sprite: SetSpriteSourceRect 服务有；MCP 仅 create_from_texture / save
SpriteAtlas: mutate ops 已立即 Save（BUG-SPR-002）
Texture2D SaveHandler: ReadCookedFile 成功即 return true（不写 sampler）
```

## Proposed Architecture

```text
AES::SetDataAssetProperty(path|guid, propertyPath, Json)
  → Load → Cast<DataAsset> → GetClass()
  → TryNormalizeValueReferences + TryWriteJsonToLive
  → SaveAsset while shared_ptr held

AES::SetTextureSampler(path, filter, address)
  → Load Texture2D → setters → SaveAsset（依赖修好的 SaveHandler）

MCP sprite.set_source_rect
  → SetSpriteSourceRect(local stack) → SaveSprite

MCP dataasset.set_property / texture.set_sampler → 上列 AES
```

## Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `AssetEditorServices` | DataAsset 定位/写/Save；Texture sampler+Save | Automation JSON 别名；玩法字段语义 |
| `SpriteEditorServices` | SourceRect mutate（既有）+ Save | 通用反射写 |
| `Texture2DLoader::SaveHandler` | 把当前 sampler 写回 `.metex` | 改像素（仍 Import/Recook） |
| `EditorAutomationSurface` | payload 解析、capability 名 | 业务校验细节 |

## Boundaries

- Allowed: Automation → AES / SpriteEditorServices；codec 写 live 对象。
- Forbidden: Runtime 依赖 Editor；Core 内置游戏表类型；`asset.set_property` 统一入口。

## Ownership & Lifetime

- 每次 mutate op 在栈上持有 `shared_ptr` 直至 Save 返回。
- 不引入跨调用 pin。Invariant：**mutate 成功 ⇒ 磁盘已更新或返回错误**。

## API / Interface Semantics

### `dataasset.set_property`

- Inputs: `path` **或** `guid`；`propertyPath`；`value`（JSON）
- Behavior: 立即 Save
- Errors: 缺定位、非 DataAsset、路径/值非法、Save 失败
- Out: `saved`, `path`, `guid`, `runtimeClassName`, `propertyPath`

### `texture.set_sampler`

- Inputs: `path`；可选 `filter` / `address`（同 atlas 字符串约定；省略的轴保持现值）
- Behavior: 立即 Save（写 metex header）
- Out: `saved`, `filter`, `address`

### `sprite.set_source_rect`

- Inputs: `path`；`sourceRect` `[x,y,w,h]`
- Behavior: 立即 Save
- Out: `saved`, `path`

## Failure Model

| Failure | Behavior |
|---------|----------|
| 路径/guid 无效 | InvalidPayload；不写盘 |
| 类型不是 DataAsset / Texture2D / Sprite | InvalidPayload |
| 反射写失败 | 不 Save；返回错误 |
| Save 失败 | 内存可能已改；返回错误（与现有 atlas 一致） |

## Alternatives & Trade-offs

### Option A（采用）— DataAsset 通用 + 内置专用 + 立即 Save

- Pros: 合法性别钉死；与 BUG-SPR-002 一致；Agent 可发现 capability
- Cons: 多几个 op 名（可接受）

### Option B — 统一 `asset.set_property`

- Why not: 内置类型合法性/副作用（rebuild、sampler 下推、metex 像素）易漏；与用户决策相反

### Option C — Document pin 跨 RPC

- Why not: path-keyed MCP 宜无会话；用户已选立即 Save

## Verification Strategy

- Suite：DataAsset AES set_property → Evict → Reload 字段一致（含 `m_Rows` 整表）
- Suite：Texture SetSampler + Save → Evict → Reload filter/address
- Suite：Sprite set source rect + Save → Evict → Reload（可挂 sprite-authoring）
- Build：`verify.ps1` / 至少 `minEngineTests` 相关 suite + Editor

## Open Questions

- 数组下标路径 — **Deferred**（非阻塞）
- `dataasset.get_property` — **Deferred**

## Design Review

- **Verdict:** `Ready with deferred items`（数组下标 / get_property）
- **Reviewer / date:** maintainer direction 2026-11-01；user authorized implement
- **Summary:** 分界与立即 Save 已确认；Texture SaveHandler 缺口纳入本期

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions deferred

**Ready for implementation?** `yes`

## Acceptance Checklist

- [x] `dataasset.set_property` MCP + AES；立即 Save
- [x] `texture.set_sampler` + SaveHandler 写回 sampler
- [x] `sprite.set_source_rect` MCP + 立即 Save
- [x] Tests 覆盖 Evict/Reload（DataAsset AES；Sprite rect 落盘；Texture SaveHandler）
- [x] Registry / ACTIVE_WORK / PROGRESS_LOG 更新
