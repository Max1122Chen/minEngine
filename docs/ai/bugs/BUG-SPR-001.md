# BUG-SPR-001 — SpriteAtlas missing authored sampler fields

## Meta
- **ID:** BUG-SPR-001
- **Status:** Fixed（所有权模型另见 TD-055，不在本 bug 范围）
- **Severity:** S2
- **Owner:**
- **Found:** 2026-10-30
- **Last updated:** 2026-10-30
- **Affects:** SpriteAtlas Rebuild → generated Texture2D; Editor Sprite Atlas panel; Automation `sprite_atlas.*`
- **Related Feature/Slice:** SPR-F07; RND-F24 (Texture2D sampler author fields)

## TL;DR
SpriteAtlas had no Filter/Address author fields; Rebuild left generated pages on Texture2D CreateFromPixels defaults, so atlases could not match Texture2D sampling controls. Fixed by atlas-level fields applied on Rebuild + Editor/automation.

---

## 症状
- minCraft / tooling cannot author atlas page sampling (filter / address).
- Generated atlas textures do not follow atlas intent; no parity with Texture2D's `m_SamplerFilter` / `m_SamplerAddress`.
- Editor only exposed MaxSize / Padding / PaddingMode; Automation had `sprite_atlas.set_padding` but no sampler op.

## 期望
- SpriteAtlas exposes the same two authored axes as Texture2D (`RHISamplerFilter`, `RHISamplerAddress`), serializable and EditAnywhere.
- Rebuild (and live setters) apply those modes onto all generated pages via `Texture2D::SetSamplerFilter` / `SetSamplerAddress`.
- Editor combos + `sprite_atlas.set_sampler` automation configure them.

## 复现
1. Create SpriteAtlas, add members, Rebuild.
2. Inspect generated Texture2D PreferredSamplerDesc — no atlas-level way to set Nearest/Clamp (or Linear/Repeat) before/after Rebuild.
3. Compare with Texture2D asset which already exposes both fields.

## 环境
- Branch: `feat/render`
- OS: Windows; contract build `minEngine/build`

## 根因
Rebuild creates pages with `Texture2DLoader::CreateFromPixels`, which does not take sampler settings. SpriteAtlas never stored or applied filter/address, so generated pages stayed at Texture2D object defaults (Linear + Repeat) with no author path on the atlas asset.

## 修复
- Add `m_SamplerFilter` / `m_SamplerAddress` on SpriteAtlas (defaults Nearest + ClampToEdge for pixel-atlas authoring; API aligned with Texture2D).
- Apply to generated textures on Rebuild and when setters run; re-apply after load rebind.
- Editor: SamplerFilter / SamplerAddress combos; Services `SetAtlasSampler`; Automation `sprite_atlas.set_sampler`.
- Test: `sprite-atlas: rebuild applies authored sampler modes`.

## 回归验证

- [x] `minEngineTests.exe test sprite-atlas` (contract bin)
- [x] Editor panel shows Filter/Address; Automation capability registered

## 关联
- RND-F24 Texture2D sampler author fields
- FontGlyphAtlas already forces Linear + ClampToEdge on its own bake path (not SpriteAtlas)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-30 | Opened from minCraft feedback; Fixed on feat/render |
