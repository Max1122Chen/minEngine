# SPR-F07 — SpriteAtlas DilateBorder 真正边渗

## Meta
- **ID:** SPR-F07
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-23
- **Related:**
  - 硬前置：[SPR-F02](./SPR-F02_SPRITE_ATLAS_ASSET_DESIGN.md)
  - 正交：[RND-F22](../Render/RND-F22_TEXTURE_SAMPLER_SEPARATION_DESIGN.md)
  - UX：[SPR-F06](./SPR-F06_SPRITE_ATLAS_WORKFLOW_DESIGN.md)
  - 代码：`SpriteAtlasPacker::PackShelf`（inset）· `DilateBorderRing` · `SpriteAtlas::Rebuild`
  - Editor：`SpriteAtlasEditorWindow` PaddingMode Combo

## TL;DR

**问题：** DilateBorder stub；PackShelf padding 是槽间 gap，无法正确双边渗。

**方案：** **所有 PaddingMode** 改为 inset 独占环布局；DilateBorder 边/角复制填环；PadWithZero 环保持 0；BakedRect/UV = **内容矩形**。Editor 补 PaddingMode 控件。

**状态：** Done — inset PackShelf + DilateBorderRing + Editor PaddingMode；`sprite-atlas`/`smoke` PASS。

## Problem

1. DilateBorder 空 stub。
2. PackShelf：`cursor += width + pad` → 共享 gap，无法无歧义 dilate。
3. Editor 无可选 PaddingMode（仅 Padding 数值）——消费/手验缺口。

## Requirements

- R1：DilateBorder + padding≥1 → 每 sprite **独占**环为边缘外推色（整环，含 padding>1）。
- R2：Slot / BakedRect / UV = 内容矩形（不含环）。
- R3：PadWithZero：同 inset；环清零。
- R4：padding==0：紧挨，两 mode 等价。
- R5：Editor 可选 PaddingMode（Combo）+ 现有 Padding；Rebuild 后可预览。
- R6：文档：Nearest（F22）+ Dilate 推荐像素风组合。
- R7：**接受**同等 MaxSize 下可装数量下降（inset 代价）；旧 atlas 需 Rebuild。

## Constraints

- Rebuild 事务性保持 F02。
- 单页 MVP。
- Dilate/Pack 像素运算在 **atlas 内存空间**（与 Blit 一致：row 0 = bottom；`placed.Y` 即 dstRow 原点）。

## Preferences

- Option A inset（锁定）。
- 算法：边复制 + 角复制；`SpriteAtlasPacker::DilateBorderRing`。

## Assumptions

- A1：forward-only；旧资产 Rebuild。
- A2：手验可用 Editor PaddingMode（本 Feature 补 UI）或 Services。

## Non-Goals

- 多页大改；运行时动态 pack；替换 minCraft 条带图集；拖拽排版；Rebuild 后自动 Nearest（F22）。

## Current Architecture

| 组件 | 行为 |
|------|------|
| PackShelf | 内容紧排 + 槽间 gap |
| Blit | destX/Y 为 flipped 内存坐标；内容写入 |
| Dilate | stub |
| Editor | MaxSize/Padding；无 PaddingMode |

## Proposed Architecture

### Pack（锁定）

输入仍为 contentW/H。对每个 member：

```
outerW = contentW + 2*padding
outerH = contentH + 2*padding
shelf places outer boxes (no extra gap between outers)
contentX = outerX + padding
contentY = outerY + padding
```

- `PackShelf` 返回的 `Entries[].X/Y/Width/Height` = **内容**矩形（供 Blit / Slot / Baked）。
- `AtlasWidth/Height` = 所有 outer 的包围盒（含右侧/上侧环，勿漏 trailing padding）。
- padding==0：outer==content，行为同今日紧排。

### Dilate（内存空间）

在 Blit 内容之后，对内容矩形 `(contentX, contentY, w, h)` 在 atlas U8 缓冲：

- 左/右/「内存 +Y」/「内存 -Y」各 padding 列/行：复制邻接内容边 texel 外推。
- 四角：`padding×padding` 填角 texel。
- **勿**按「屏幕顶/底」命名实现；以 dstRow 增减为准，与 `BlitTopLeftRectToAtlas` 一致。
- BakedRect 仍用现有 top-left 转换（Rebuild 末尾），不采进环。

### PadWithZero

同 inset；memset 0 即可。

### Editor

`SpriteAtlasEditorWindow`：PaddingMode Combo（PadWithZero / DilateBorder）→ `SetPaddingMode`；Dirty + 可提示 Rebuild。

## Responsibilities

| Unit | For | Not |
|------|-----|-----|
| SpriteAtlasPacker | inset Pack；DilateBorderRing | Editor；filter |
| SpriteAtlas::Rebuild | 模式分支；事务 | |
| SpriteAtlasEditor | PaddingMode UI | 像素算法 |

## State & Invariants

- I1：BakedRect = 内容。
- I2：Dilate 不写入邻内容矩形。
- I3：PadWithZero 环为 0。
- I4：padding==0 两 mode 像素布局一致。

## API

- 公共 API 不变；**布局语义变更**（PadWithZero 也 inset）须在 Progress 注明。

## Failure Model

- outer 超 MaxSize：Pack 失败 Message（与今日同类）。

## Alternatives

- A inset（选） / B 共享 gap（否） / C 缩 UV（否）

## Verification

- 单测：更新 PackShelf 坐标期望；Dilate 环色==边色；PadWithZero 环==0；padding0 两 mode 同。
- 手验：Editor 选 DilateBorder → Rebuild → 预览环。

## Open Questions（非阻塞）

1. Dilate 后默认 Nearest — Defer（F22）。
2. 环外再加 1px 安全缝 — Defer（MVP 不加）。

## Design Review

- **Mode:** A
- **Verdict:** Ready with deferred items
- **Date:** 2026-09-22
- **Findings absorbed：**
  - inset 密度下降 **Accepted**（原 blocking Q1）
  - Pack 返回内容原点；Atlas 尺寸含 outer
  - 像素空间 = Blit flipped memory
  - **PadWithZero 也改 inset**（全 mode 布局变更）
  - Editor 缺 PaddingMode → 纳入本 Feature
- **Ready for implementation?** yes

## Suggested Slices

| Slice | 内容 |
|-------|------|
| S01 | PackShelf inset + 单测布局 |
| S02 | DilateBorderRing + Rebuild + 颜色单测 |
| S03 | Editor PaddingMode UI + 手验 |
| S04 | 短文档 / Progress |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | Draft |
| 2026-09-22 | design-review：锁定 inset 接受；内存空间；PadWithZero 同布局；Editor PaddingMode；Status→Review |