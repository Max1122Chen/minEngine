# SPR-F02 — Sprite / SpriteAtlas Asset Truth — Design Spec

## Meta
- **ID:** SPR-F02
- **Type:** Feature
- **Complexity:** L3
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - Roadmap：[SPR-F01](./SPR-F01_SPRITE_SYSTEM_ROADMAP.md)
  - 消费：[SPR-F03](./SPR-F03_SPRITE_RUNTIME_CONSUME_DESIGN.md)
  - 命令面：[SPR-F04](./SPR-F04_SPRITE_AUTHORING_SERVICES_DESIGN.md)
  - 底座：[RND-F16](../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)
  - 身份：[ASSET-F03](../Asset/ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)
  - UE：`UPaperSprite` / `UPaperSpriteAtlas`（Plugins/2D/Paper2D）

## TL;DR

**问题：** 引擎只有 `Texture2D` + `SpriteComponent` 裸字段，无 Sprite/Atlas 资产，无法「保留源图、运行时共享打包图」。

**方案：** 引入 `Sprite` / `SpriteAtlas` 资产类型；Source/Baked 双轨；`Rebuild` 写 Baked；Runtime 查询 API。本 Feature **不含** Editor UI（F05/F06）与 Component 接线细节（F03），但定义它们依赖的契约。

**状态：** Draft — 待审阅。

## Scope
- **In:**
  - 类型、字段、序列化、扩展名、AssetRegistry 登记
  - `GetRuntimeTexture` / `GetUVRectNormalized` / `GetSource*`
  - `SpriteAtlas::Rebuild`（CPU blit + 更新成员 Baked）
  - 单测：回退、Pack、Source 不变
- **Out:**
  - Sprite Editor / CB UX / EditorServices（F04–F06）
  - `SpriteComponent` / shader UV 接线（F03）
  - Flipbook、trim、旋转、多页 atlas、压缩格式策略

## Reader quick start
1. §3 数据模型与不变量  
2. §4 Rebuild 契约  
3. §6 API  
4. §11 验收  

---

## Problem

RND-F16 交付了可画的世界 Sprite，但资产层停在「整张 Texture + 手填 UVRect」。缺少：

1. 可命名、可引用的 **一帧 Sprite**  
2. **图集打包**后仍可编辑源图的双轨模型  
3. 游戏/网格/组件共用的 **Runtime 解析 API**

## Requirements

- 可从 Texture（全图或像素 rect）创建 `Sprite` 资产并落盘  
- `Sprite` 始终保留 Source；Rebuild 后增加/更新 Baked  
- `SpriteAtlas` 持有成员列表与 GeneratedTextures；`Rebuild` 幂等或失败可恢复（见 Failure）  
- 未入 Atlas 或 Baked 为空时，Runtime API **回退 Source**  
- 像素 rect 为存盘真源；提供 normalized UV 查询  
- 符合 `ASSET-F03`：Create 不换身、meta 同步  

## Constraints

- 不引入玩法类型（Block/Item）  
- Pack 在编辑期/命令期执行，**不**在每帧 Runtime  
- 依赖现有 `Texture2D` / AssetManager / 序列化栈  

## Preferences

- 对齐 Paper2D：Baked 写在 Sprite 上  
- 扩展名：`.mesprite` / `.mespriteatlas`  
- MVP 单页 atlas（一张 GeneratedTexture）；超限则失败并报错  

## Assumptions

- CPU 可读源纹理像素（或经现有导入路径得到 `Image`/CPU 副本）可在 Editor/工具进程完成 blit  
- 若某后端 Texture 仅有 GPU 侧数据，Rebuild 需走已有读回或强制源为可导入图像——**Open Question**  

## Non-Goals

- 在线可视化排版、自动 trim、旋转装箱  
- 运行时 Late-binding 换 atlas（Unity late bind）——后置  
- 替换 `Texture2D` 资产系统  

## Current Architecture

- `Texture2D` 资产可用；`SpriteComponent` 持 `shared_ptr<Texture2D>` + `m_UVRect`（GPU remap **未落地**，F03）  
- `AssetManager::CreateAsset<T>` / Registry / meta 已有 Scene/Material/MIC 先例  
- 无 Sprite 类型  

## Proposed Architecture

```text
Sprite : Asset (ME_CLASS)
  SourceTexture  (shared_ptr / soft ref — 实现选一，见 Alternatives)
  SourceRectPx   (x,y,w,h) 全图时 = texture size
  BakedTexture   nullable
  BakedRectPx    valid iff BakedTexture
  Atlas          soft/weak ref optional
  Pivot / PixelsPerUnit optional (MVP 可默认中心 / 1)

SpriteAtlas : Asset
  Members[]      ordered Sprite refs
  MaxSize, Padding, PaddingMode
  GeneratedTextures[]   (MVP: 0 or 1)
  Slots[]        { SpriteRef, x,y,w,h, page }
  Rebuild()
```

### Responsibilities

| Unit | Responsible for | Not responsible for |
|------|-----------------|---------------------|
| `Sprite` | Source/Baked 字段；Runtime 解析 | Pack；UI |
| `SpriteAtlas` | 成员；生成纹理；Rebuild；slot 表 | 场景绘制 |
| `SpritePacker`（内部 helper） | 布局 + blit | 资产身份 |
| AssetManager | Create/Load/Save 身份 | Pack 语义 |

### Boundaries

- **Allowed：** Runtime / Editor / 游戏代码调用 Sprite 查询 API；Atlas.Rebuild 由 Command/Service 触发  
- **Forbidden：** 游戏 tick 内隐式 Rebuild；Core 出现 BlockId；渲染 Pass 直接改 Sprite 资产  

## Dependencies

- Depends on: Texture2D、AssetManager、Serialization、ASSET-F03  
- Depended on by: F03–F06、未来 minCraft mesher  
- New direction: Sprite → Texture（读）；Atlas → Sprite（成员）；Rebuild 写回 Sprite Baked（**显式 mutual update**，需文档化，避免循环 Load）  

## Ownership & Lifetime

- Sprite / Atlas 由 AssetManager 拥有（与 Material 同类）  
- GeneratedTextures：由 Atlas **拥有**（子资产或内嵌贴图对象——Open：内嵌 vs 旁路 `.png`）  
- SourceTexture：Sprite **引用**既有 Texture 资产，不拷贝像素到 Sprite 文件（除非选择嵌入——默认引用）  
- Rebuild 更新：Atlas.Generated* + 各 Member.Baked*；不删除 SourceTexture 资产  

## Data Flow

```text
Authoring:
  Texture2D → CreateSprite → Sprite{Source*} → Save
  Sprites[] → CreateAtlas / AddMembers → SpriteAtlas → Rebuild
       → blit GeneratedTexture
       → write each Sprite.Baked* + Atlas.Slots
       → Save Atlas + dirty Sprites

Runtime resolve:
  Sprite.GetRuntimeTexture() =
      BakedTexture ? BakedTexture : SourceTexture
  Sprite.GetUVRectNormalized() =
      rect_to_uv(Baked? BakedRect : SourceRect, runtimeTexSize)
```

## Control Flow

- Create/Save：同步，主线程（对齐现有资产）  
- Rebuild：同步 Editor/Command 线程；失败不提交半写入（见 Failure）  
- 无异步 MVP  

## State & Invariants

1. **Source 永在：** `SourceTexture != null` 且 `SourceRectPx` 在源尺寸内（允许全图）  
2. **Baked 一致：** `BakedTexture != null` ⟺ `BakedRectPx` 有效且落在 Baked 尺寸内  
3. **Atlas 成员：** 每个 Member 的 `Atlas` 指向本 atlas（或 null 表示未打包）；Rebuild 后必须非 null  
4. **移除成员：** 清除该 Sprite 的 Baked* 与 Atlas 引用（或标记 dirty 待 Rebuild——推荐 **立即 ClearBaked**）  
5. **非法：** 仅有 Baked 无 Source；Baked 指向已销毁纹理却未 Clear  

## API / Interface Semantics

### `Sprite::GetRuntimeTexture() -> Texture2D*`
- 非拥有指针；调用方不释放  
- null 仅当 Source 也缺失（应视为资产损坏）  

### `Sprite::GetUVRectNormalized() -> Vector4 (u0,v0,u1,v1)`
- 相对 **Runtime** 纹理尺寸  
- 约定：原点与引擎纹理 UV 一致（与 RND-F16 / 现有采样一致；实现钉死 V 是否翻转）  

### `Sprite::GetSourceRectPx() / GetBakedRectPx()`
- 编辑器与调试用  

### `SpriteAtlas::Rebuild() -> Result`
- 输入：当前 Members + 各自 Source 像素  
- 输出：更新 GeneratedTextures、Slots、各 Sprite Baked  
- 错误：成员空、源不可读、超 MaxSize、重叠失败  

### `SpriteAtlas::AddMember / RemoveMember`
- 改成员表；Remove 时 Clear 该 Sprite Baked；**不**自动 Rebuild（调用方显式 Rebuild）  

## Failure Model

| 失败 | 行为 |
|------|------|
| Rebuild 中途 blit 失败 | **不**写回任何 Member Baked；保留重建前状态（事务式：先算到临时，成功再 swap） |
| 成员 Source 丢失 | 跳过该成员并 Error 日志，或整次失败——**推荐整次失败** |
| 超出 MaxSize | 失败；提示增大 MaxSize 或减少成员（多页后置） |

## Alternatives & Trade-offs

### Option A（推荐）— Baked 写在 Sprite（Paper2D）
- Pros：查询只碰 Sprite；Component/mesher 简单  
- Cons：Rebuild 要脏多资产；Save 范围大  

### Option B — Sprite 只存 Atlas+SlotId
- Pros：单真源在 Atlas  
- Cons：每次查询间接；卸载 Atlas 则 Sprite 无法画；与「保留独立源图编辑」仍要 Source 字段，模型并不更简  

**选择 A。**

### GeneratedTexture 存储
- **A1 推荐：** Atlas 资产内嵌或旁路生成 `Texture2D` 子对象并登记  
- A2：仅导出 PNG 路径——编辑迭代差  

## Integration Impact

- AssetTypeRegistry 新类型；CB 图标后置可暂用通用  
- 序列化 schema：新类型，走 CORE-F18 schema 门闸惯例  
- 不影响现有 Scene/Material 加载  

## Migration / Compatibility

- 无旧 Sprite 资产  
- `SpriteComponent` 裸 Texture 路径保留至 F03 迁移策略  

## Verification Strategy

- 单测：`CreateSprite` 全图；Slice rect；未 Rebuild 时 Runtime==Source  
- 单测：两 Sprite → Atlas → Rebuild → Runtime 同 Texture、UV 不重叠、Source 未改  
- 单测：RemoveMember → Baked cleared  
- 单测：Rebuild 失败事务（可 mock 超限）  

## Open Questions

1. SourceTexture 用 `shared_ptr<Texture2D>` 还是 soft path + Guid？— **blocking?** 否（实现期对齐 Material 引用惯例）  
2. CPU 像素可读性 / V 翻转约定 — **blocking?** 是（F02 实现前钉死）  
3. GeneratedTexture 是否独立 `.metex` 文件 — **blocking?** 否（默认内嵌/旁路子对象）  
4. 扩展名最终拼写 — **blocking?** 否  

## Design Review

- **Verdict:** Pending  
- **Reviewer / date:**  
- **Link or summary:**  

## Implementation Readiness

- [ ] Requirements understood  
- [ ] Constraints identified  
- [ ] Relevant existing architecture inspected  
- [x] Responsibilities and boundaries defined  
- [x] Key dependencies understood  
- [x] Ownership/lifetime resolved or N/A  
- [x] Major state/invariants resolved or N/A  
- [x] API semantics sufficiently defined  
- [x] Important failure modes considered  
- [x] Alternatives considered where meaningful  
- [x] Integration impact understood  
- [x] Verification strategy exists  
- [ ] Unresolved questions resolved or explicitly deferred/accepted（Q2 待钉）  

**Ready for implementation?** `no` — 待维护者 Review；钉死 UV 原点/V 翻转与 CPU 读源策略。

## Acceptance Checklist

- [ ] Design review complete  
- [ ] Implementation plan traces to design（L2+）  
- [ ] Progress log updated when status changes  
- [ ] Feature registry status synced  

---

## Changelog

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
