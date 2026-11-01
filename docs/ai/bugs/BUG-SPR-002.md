# BUG-SPR-002 — sprite_atlas MCP mutations do not persist across save

## Meta
- **ID:** BUG-SPR-002
- **Status:** Fixed
- **Severity:** S2
- **Owner:**
- **Found:** 2026-11-01
- **Last updated:** 2026-11-01
- **Affects:** Editor Automation `sprite_atlas.add_members` / `set_padding` / `set_sampler` / `rebuild` / `save`; AssetManager loaded-asset weak cache
- **Related Feature/Slice:** ASSET-F04; SPR-F07; minCraft feedback `ENGINE_FEEDBACK_PROMPT_SPRITE_ATLAS_MUTATIONS_NOT_PERSISTED.md`

## TL;DR
MCP mutate ops reported success then dropped the only strong reference; a later `save` reloaded from disk and wrote the old asset. Fix: each mutate op saves in the same call while still holding the `shared_ptr` (same shape as `create_from_selection`).

---

## 症状
```
editor_asset { domain: "sprite_atlas", action: "add_members", ... } => { "added": 5 }
editor_asset { domain: "sprite_atlas", action: "save", ... } => { "saved": true }
disk: m_Members unchanged
```
Same for `set_padding` / `set_sampler`. `create_from_selection` worked (mutate + save in one call while holding `shared_ptr`).

## 期望
- Mutate ops persist members / padding / sampler (no separate save required for durability).
- Fail with an error when sprite paths cannot load; report the real number of members added, not the request array length.

## 复现
1. Editor with MCP; existing `blocks.mespriteatlas` (or any SpriteAtlas).
2. `add_members` with new sprite paths → success.
3. `save` → success.
4. Read the `.mespriteatlas` file: member list unchanged.

## 环境
- Engine: `D:/Dev/GitRepo/minEngine` master (post RND-F31 asset chain)
- Consumer: minCraft-ui feedback 2026-11-01
- OS: Windows

## 根因
`HandleSpriteAtlas*` loaded via `AssetManager::LoadAssetByPath`, mutated, then released the local `shared_ptr`. The load cache stores `weak_ptr` only. Nothing pinned the instance across RPC calls, so a later `save` constructed a fresh object from disk and serialised the pre-mutation state while returning `saved: true`.

This is the same class of bug as any “mutate RPC → separate save RPC” path without a strong pin. SpriteAtlas automation used that split; one-shot create ops did not.

Secondary: `{"added": sprites.size()}` echoed the request size; failed sprite loads were skipped with `continue`.

## 修复
- `add_members` / `set_padding` / `set_sampler` / `rebuild`: mutate then `SaveAtlas` in the same handler while the `shared_ptr` is still live; response includes `"saved": true`.
- `rebuild` also cascade-saves members (matches editor save after rebuild).
- Hard-fail unloadable sprite paths; report member-count delta as `added`.
- Chose immediate save over Document pin so path-keyed MCP stays session-less (aligned with create ops). Explicit `sprite_atlas.save` remains for manual flush.

## 回归验证

- [x] `minEngine/bin/minEngineTests.exe test sprite-authoring` — 4/4 PASS（含 `mutate then save while held persists across reload`）
- [x] `MaximumEditorLib` / `Editor` / `minEngineTests` 构建 exit 0
- [ ] 真机 MCP（minCraft）：`add_members` 单独调用后磁盘 `m_Members` 增长；`set_padding` / `set_sampler` 单独调用后字段落盘

## 关联
- minCraft-ui `docs/ai/ASSET/ENGINE_FEEDBACK_PROMPT_SPRITE_ATLAS_MUTATIONS_NOT_PERSISTED.md`
- BUG-SPR-001 (sampler authoring; fixed)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-11-01 | Opened from minCraft feedback |
| 2026-11-01 | Fix: mutate-then-immediate-save (not Document pin) |
