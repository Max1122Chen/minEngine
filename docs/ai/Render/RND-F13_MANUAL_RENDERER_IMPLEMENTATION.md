# ManualRenderer — Implementation Plan

## Meta
- **ID:** `RND-F13`
- **Type:** Implementation Plan
- **Status:** **In Progress**
- **Owner:** project maintainer
- **Last updated:** 2026-08-30
- **Design:** [RND-F13_MANUAL_RENDERER_DESIGN.md](./RND-F13_MANUAL_RENDERER_DESIGN.md)

## Slices

### S01 — Skeleton + CLI switch (Done)
- [x] `ManualRenderer` subclasses `ForwardRenderer`; protected frame helpers
- [x] Manual frame RT pool (SceneColor, SceneDepth, DirShadowAtlas; spot/point slots when `MAX_* > 0`)
- [x] `ShadowPass → BasePass → PresentPass` manual enqueue
- [x] `--renderer manual` (`handpass` alias) via `ApplicationCommandLine` → `RenderSystem`
- [x] `PresentPass::RunWithInputTexture` for non-RDG present path

### S02 — Experiment matrix + parity run (Done for diagnosis)
- [x] Document isolation matrix + dir-only results in [BUG-RENDER-013](../bugs/BUG-RENDER-013.md)
- [x] Restore production shadow constants (`MAX_*=2`, `MAX_CASCADES=4`, `FORCE_CASCADE=-1`)
- [x] Full-map Manual vs Forward: **same wrong shadows** (user 2026-08-31) → RDG demoted
- [x] Record conclusion in BUG-RENDER-013

**Dir-only interim:** Manual ≈ Forward; faint shadow shared.
**Full-map:** Manual == RDG wrong → shared pipeline / VK resources.

### S03 — Fix on Manual (Next; see execution plan)
- [ ] **[BUG-RENDER-013 VK Shadow Fix Execution Plan](./BUG-RENDER-013_VK_SHADOW_FIX_EXECUTION_PLAN.md)** — S00 定界 → S01 矩阵单源 → …
- [ ] ShadowPass / cascade layer / clear / transition
- [ ] set1 + LightUBO indices under full maps
- [ ] VK Texture2DArray / cube depth create & SRV
- [ ] After Manual fix: regress Forward+RDG

### S04 — Optional polish
- [x] SkyBox manual clear/draw path (viewport fix)
- [ ] Translucent / PostProcess (still Out for diagnostic)
- [ ] Translucent / PostProcess (explicit non-goals for v1)

## DoD (Feature)
- ManualRenderer switchable; default remains ForwardRenderer
- No `RenderGraph` include in `ManualRenderer.cpp` (grep)
- BUG-013 RDG hypothesis documented after S02

## 变更记录
| 日期 | 说明 |
|------|------|
| 2026-08-30 | S01 landed: ManualRenderer + `--renderer manual` |
| 2026-08-30 | Dir-only: Manual≈Forward; sky clear fix; restore full map for S02 |
