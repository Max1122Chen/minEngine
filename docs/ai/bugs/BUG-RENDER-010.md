# BUG-RENDER-010 — Vulkan directional shadow false self-shadow on large plane

## Meta
- **ID:** BUG-RENDER-010
- **Status:** Open
- **Severity:** S1
- **Owner:**
- **Found:** 2026-08-28
- **Last updated:** 2026-08-30
- **Affects:** Vulkan Editor / ForwardRenderer CSM, `test` scene plane receiver, ED-F01-S06
- **Related Feature/Slice:** ED-F01-S06 · TD-025

## TL;DR
Vulkan Editor: directional shadow on 100×100 plane appeared as huge false self-shadow; three light types incorrect on VK. **Root cause (revised):** fragmented clip/viewport/cull policy (`IsVulkan()` branches), not PCF. Plane cast off removed blob → caster/winding path. **Fix:** TD-025 `RHIClipSpaceCapabilities` + Shadow scheme A (`flipY=false`, Front cull). Prior ZO-only patch was ortho no-op.

---

## 症状
- `test` scene, `--rhi vulkan`: plane huge false shadow; cube shadow may slide with camera.
- All three shadow types wrong on VK; GL OK.
- Disabling plane Cast Shadow removes large blob.

## 期望
- No plane false self-shadow when plane casts.
- Cube shadow tracks transform; stable on orbit camera.
- Point/spot/dir shadows match GL.

## 修复状态（TD-025 分层回退后）

**基础设施（保留）：** `RHIClipSpaceCapabilities` / `RHIClipSpace` / `RHIViewportConvention`；ShadowPass convention + Front cull；ForwardRenderer ZO 矩阵；`EngineSceneBindingSets` 槽清空；dir shadow index 门控。

**当前（2026-08-30）：** ZO depth read + scheme A + slot gate。VK Spot ~OK。**Dir：仅方向光 shadow 预算下 VK 已正常**（BUG-RENDER-013 隔离实验）→ 全类型并存时异常 **主因修订为 RDG 调度/资源生命周期**（见 BUG-RENDER-013），非 Dir shader 坐标或 TD-025 convention 主链错误。

**实验结论（2026-08-30）：**
- `FORCE_CASCADE=0`：多影→单影；GL/VK 强制0 仍不对 → 级联混用放大问题，单级仍错（点/聚光恢复后需再验）。
- 固定光空间 ortho 盒子（cascade 0）：**GL/VK 均不见 Dir 影**（盒子参数/光空间 near-far 未闭合，实验无效）；CSM frustum→AABB 路径对可见性必要。
- **Dir-only shadow maps (`MAX_*_SHADOW_MAPS=0`)：VK Dir 视觉正常** → BUG-RENDER-013：RDG 主因假设；关闭本 bug Dir 主项待 RDG 修复后回归。

## 回归验证
- [ ] VK `test` scene visual parity
- [ ] GL regression
- [x] Build `minEngine` + `Editor`

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-28 | Filed; initial ZO patch (ineffective) |
| 2026-08-28 | TD-025 caps + shadow scheme A |
| 2026-08-29 | 分层回退 shader/flip；BUG 重开 Open；见 sessions handoff |
