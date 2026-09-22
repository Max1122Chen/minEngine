# SPR-F01 — Sprite System MVP Roadmap（Asset + Runtime + Editor）

## Meta
- **ID:** SPR-F01（本文件为 **Capability 路线图**；落地见 SPR-F02+）
- **Type:** Roadmap
- **Complexity:** L3（多 Feature 族）
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ENGINE_DESIGN_PHILOSOPHY](../ENGINE_DESIGN_PHILOSOPHY.md)
  - 绘制底座（Done）：[RND-F16](../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)
  - 子 Feature：[F02](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md) · [F03](./SPR-F03_SPRITE_RUNTIME_CONSUME_DESIGN.md) · [F04](./SPR-F04_SPRITE_AUTHORING_SERVICES_DESIGN.md) · [F05](./SPR-F05_SPRITE_EDITOR_DESIGN.md) · [F06](./SPR-F06_SPRITE_ATLAS_WORKFLOW_DESIGN.md)
- **Trust:** Tier A（与 ACTIVE_WORK 共同指导 Sprite 轨）
- **Driver note:** minCraft 图集需求是**催化剂**，不是本期验收闸门；目标是引擎 Sprite 能力面。

## TL;DR

构建 **Sprite 资产管线 + Sprite Editor + Sprite Atlas** 的引擎 MVP：从 raw Texture 创建/切分 Sprite，多选组成 Atlas 并 Rebuild，运行时走 Baked 纹理+UV，同时**永久保留 Source**。对齐 Unity SpriteAtlas / UE Paper2D Source–Baked 双轨。人与 Agent 共用同一套 EditorServices / Command。

## Scope
- **In:** `Sprite` / `SpriteAtlas` 资产；Source vs Baked；Create / Slice / Pack / Rebuild；Runtime 查询 API；`SpriteComponent` 消费；Sprite Editor（切分）；Atlas 工作流 UX；Agent 同面
- **Out:** Flipbook / 动画表；九宫格；运行时动态 pack；体素/Block API；完整 pack 可视化编辑器；Billboard；Sprite 阴影；每实例 mega VB 合批

## Reader quick start
1. §1 原则 + §2 类型地图  
2. §3 Feature 表（**F02 数据模型先于 Editor**）  
3. 首刀 Design：[SPR-F02](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)

---

## 1) 方向与原则

| 原则 | 含义 |
|------|------|
| **Capabilities, not Opinions** | Sprite / Atlas / UV 查询 / Pack 是机制；方块 ID、物品栏等留在游戏仓 |
| **Source / Baked 双轨** | Authoring 真源永不因 Pack 删除；Runtime 解析优先 Baked |
| **Asset truth before UI** | 先可序列化、可查询、可 Rebuild，再做漂亮 Editor |
| **Human = Agent surface** | GUI 是薄壳；`SpriteEditorServices` / Command 是真源（对齐 ED-F18/F19） |
| **对标职责，不克隆体积** | 学 UE `UPaperSprite` Source/Baked + Unity Atlas 工作流；不搬 Paper2D 碰撞/Socket/Diced |
| **可增量验收** | 每 Feature 竖切 DoD；不绑定 minCraft chunk 上线 |

---

## 2) 类型地图（产品目标态）

```text
Texture2D (raw)                    ← 源图像资产（始终可编辑）
    │ CreateSpriteFromTexture / Slice
    ▼
Sprite                             ← 一帧矩形语义
    ├── SourceTexture + SourceRectPx   (authoring)
    ├── BakedTexture  + BakedRectPx    (after Atlas Rebuild; may be null)
    ├── Atlas soft ref (optional)
    └── GetRuntimeTexture() / GetUVRectNormalized()

SpriteAtlas                        ← 成员表 + PackedTexture(s) + slots
    ├── Members[] → Sprite
    ├── GeneratedTextures[]
    └── Rebuild() → blit + write Sprite.Baked*

SpriteComponent                    ← 场景消费（F03）
    └── prefers Sprite asset; Texture+UVRect legacy path retained
```

| 类型 | 职责 | 非职责 |
|------|------|--------|
| `Sprite` | 一帧 Source/Baked、pivot/size 可选、Runtime 解析 | Pack 算法；编辑器 UI |
| `SpriteAtlas` | 成员、打包参数、GeneratedTextures、Rebuild | 玩法命名空间；Block 注册表 |
| `SpriteComponent` | 世界面片绘制消费 Runtime API | 拥有图集；切分 UI |
| `SpriteEditorServices` | Create/Slice/Atlas/Rebuild 原语 + Undo | 协议 JSON 拼装（Surface 层） |

---

## 3) Feature 优先级

| Order | ID | 名称 | Status | Design |
|-------|-----|------|--------|--------|
| 0 | **SPR-F01** | 本路线图 | **Draft** | 本文 |
| 1 | **SPR-F02** | Sprite / Atlas **资产真源** + 序列化 + Runtime 解析 + Rebuild | **Done** | [Design](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md) |
| 2 | **SPR-F03** | 运行时消费（Component + UVRect→GPU） | **Draft** | [Design](./SPR-F03_SPRITE_RUNTIME_CONSUME_DESIGN.md) |
| 3 | **SPR-F04** | Authoring 命令面（人/Agent 同 API） | **Draft** | [Design](./SPR-F04_SPRITE_AUTHORING_SERVICES_DESIGN.md) |
| 4 | **SPR-F05** | Sprite Editor MVP（纹理预览 + 矩形切分） | **Draft** | [Design](./SPR-F05_SPRITE_EDITOR_DESIGN.md) |
| 5 | **SPR-F06** | Atlas 工作流 UX（多选创建、成员、Rebuild、预览） | **Draft** | [Design](./SPR-F06_SPRITE_ATLAS_WORKFLOW_DESIGN.md) |

**硬依赖：** **F02 → F03 / F04**；**F05 / F06 依赖 F04**（可先并行 Design，实现不得早于 F02/F04 契约）。  
**建议实现序：** F02 → F03（可与 F04 小并行）→ F04 → F05 ∥ F06。

---

## 4) 里程碑

| Milestone | 交付 | 验收（引擎内） |
|-----------|------|----------------|
| **M0** | F01–F06 Draft 审阅 → Planned | 维护者逐篇 Review |
| **M1** | F02：磁盘 Sprite/Atlas；Rebuild 写 Baked；查询 API | 单测：Source 保留；Baked UV；未入 Atlas 回退 Source |
| **M2** | F03：场景两个 Sprite 同 Atlas 不同 UV | Editor 手验 + UV 单测 |
| **M3** | F04：无 GUI 走完 Create→Slice→Atlas→Rebuild | Automation / 单测调用 Services |
| **M4** | F05+F06：人可切分、可组 Atlas | CB 打开/创建；Rebuild 后场景正确 |

---

## 5) 已拍板 / 默认（讨论结论，待 Review 确认）

| # | 议题 | 默认 |
|---|------|------|
| 1 | Pack 时机 | **Editor/Command 显式 Rebuild**；运行时只读 Baked |
| 2 | Baked 落点 | **写在每个 Sprite**（对齐 Paper2D）；Atlas 另持 GeneratedTextures + slots |
| 3 | UV 单位 | 资产存 **像素 rect**；对外提供 **normalized 0–1** |
| 4 | Pack 算法 MVP | 最简 shelf / row pack；padding 可配；trim/旋转后置 |
| 5 | 扩展名（草案） | `.mesprite` / `.mespriteatlas`（Review 可改） |
| 6 | minCraft | **非本期 DoD**；日后用同一 Runtime API |

---

## 6) 依赖与风险

| 项 | 说明 |
|----|------|
| 依赖 | `RND-F16` Done；`ASSET-F03` Create 身份；Tab Host / `TryOpenAsset` 模式（ED-F02/F11） |
| 风险 | UVRect GPU 债拖延 → F03 画面错；Rebuild 不幂等 → 资产损坏；Editor 先于 F02 → 双真源 |
| 缓解 | F02 单测锁契约；Rebuild 失败不写半套 Baked（或明确回滚策略，见 F02） |

---

## 7) 延后索引

| 项 | 原因 | Unblock |
|----|------|---------|
| Flipbook / SpriteFrames | 动画表另 Feature | 生物外观压力足够 |
| 运行时动态 Pack | 复杂度高 | 流式 DLC 需求 |
| 多页 Atlas / 压缩格式专项 | MVP 单页够用 | 内容量爆炸 |
| Block/物品专用 API | 玩法语义 | 永不进 Core |
| 完整 pack 可视化拖拽排版 | UX 抛光 | F06 MVP 后 |

---

## 8) 参考锚点（只读）

| 引擎 | 路径 / 概念 |
|------|-------------|
| UE Paper2D | `PaperSprite.h`：`SourceTexture`/`SourceUV` vs `BakedSourceTexture`/`BakedSourceUV`；`GetBakedTexture()`；`AtlasGroup` |
| UE | `PaperSpriteAtlas.h`：slots、GeneratedTextures、Rebuild |
| Godot | `AtlasTexture`（region 视图）；`EditorAtlasPacker`（import 打包） |
| Unity | Sprite + Sprite Atlas workflow（Objects for Packing → Pack） |

---

## 9) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft：登记 SPR 族；升格为 Sprite 系统 MVP（非仅 minCraft atlas） |
