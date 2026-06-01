# BUG-RENDER-001 — Editor 3D Viewport Flicker From Post-Process Feedback Loop

## Meta
- **ID:** BUG-RENDER-001
- **Status:** Open
- **Severity:** S1
- **Owner:**
- **Found:** 2026-06-01
- **Last updated:** 2026-06-01
- **Affects:** Editor 3D viewports (Scene viewport, Material preview viewport, Scene3D thumbnails), OpenGL backend
- **Related Feature/Slice:** Render post-process pipeline (FXAA / Sharpen)

## TL;DR
Editor 3D viewport rendering can flicker or show unstable artifacts on some machines because post-process passes sample and render to the same scene color target in one pass chain, which is undefined behavior in OpenGL.

---

## 症状
- Main editor Scene viewport flickers or shows unstable frame artifacts on some machines.
- Material editor 3D preview can show similar instability.
- Issue is machine/driver dependent: may appear stable on one machine and fail on another.
- Disabling post-process passes (FXAA/Sharpen) restores stable rendering in the main viewport.

## 期望
- 3D viewports remain stable with post-process enabled across different GPUs/drivers.
- Post-process should never rely on undefined read/write feedback behavior.

## 复现
1. Launch Editor with a valid project and open Scene viewport or Material preview viewport.
2. Keep post-process passes enabled (FXAA/Sharpen path active).
3. Observe viewport on affected machine/GPU driver.
4. Disable post-process passes and compare behavior.

## 环境
- OS: Windows (reported cross-machine difference)
- Render backend: OpenGL (GLFW + GLAD)
- Build mode: Debug/Release both potentially affected
- Branch: master (reported during 2026-06-01 debugging session)

## 根因
- Suspected root cause: post-process pass chain reads from `m_SceneColorTexture` while rendering full-screen output back into the same scene framebuffer attachment.
- This creates an OpenGL feedback loop (sampling from a texture that is simultaneously bound as render target), which is undefined behavior and driver-dependent.

## 修复
- Pending.
- Recommended direction:
  - Introduce ping-pong post-process render targets (A->B->A...) or dedicated intermediate targets per pass.
  - Ensure no pass samples from the same texture currently attached for write.

## 回归验证
- [ ] Enable post-process and verify Scene viewport remains stable on at least two GPU/driver configurations.
- [ ] Verify Material preview viewport remains stable with post-process enabled.
- [ ] Verify disabling/enabling post-process only affects visual style, not frame stability.

## 关联
- BUG-RENDER-002 (preview wrong-frame caching)
- Render pipeline post-process implementation in `RenderPipeline` / `PostProcessPass`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | Bug created from cross-machine rendering issue investigation. |
