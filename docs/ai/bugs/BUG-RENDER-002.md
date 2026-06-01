# BUG-RENDER-002 — Scene3D Preview Uses Wrong Frame Before Render Tick

## Meta
- **ID:** BUG-RENDER-002
- **Status:** Open
- **Severity:** S2
- **Owner:**
- **Found:** 2026-06-01
- **Last updated:** 2026-06-01
- **Affects:** AssetWorkflow inspector preview, Content Browser Scene3D thumbnails, Material/StaticMesh preview flow
- **Related Feature/Slice:** Editor AssetThumbnailService Scene3D preview

## TL;DR
Scene3D preview path submits draw requests during UI phase and marks preview ready immediately, but the actual render executes later in renderer tick, causing stale/wrong-frame texture caching.

---

## 症状
- Inspector 3D preview (Material/StaticMesh) can show stale image, incorrect image, black image, or one-frame lag.
- Content Browser Scene3D thumbnails may appear inconsistent when switching assets quickly.
- Texture2D preview is usually correct because it uses direct texture display and does not go through Scene3D submit path.

## 期望
- Scene3D preview should only mark `Ready` after the submitted draw has actually been rendered.
- Asset switch should not show stale previous-asset image as final cached result.

## 复现
1. Open Editor and select Material/StaticMesh assets in Inspector.
2. Observe 3D preview immediately after selection, especially during quick asset switching.
3. Compare with Texture2D preview behavior (usually stable).
4. In affected cases, preview can look stale until another interaction re-dirties the entry.

## 环境
- OS: Windows (reported)
- Editor runtime with ImGui-driven UI frame + deferred renderer tick pipeline
- Branch: master (reported during 2026-06-01 debugging session)

## 根因
- Suspected root cause:
  - Scene3D preview flow calls `SubmitSceneDraw(desc)` during UI frame.
  - It then immediately reads render target texture and marks cache as `Ready` / `m_bDirty=false`.
  - Actual rendering executes in later renderer tick, so sampled texture can still be previous frame content.
- This is a frame-order/state bug (timing-sensitive), so it may look fine on some machines/workflows and fail on others.

## 修复
- Pending.
- Recommended direction:
  - Introduce explicit preview state machine: Submitted -> Pending -> Ready.
  - Only promote to `Ready` after at least one renderer tick completes for that submission.
  - Avoid final cache update from pre-render frame data.

## 回归验证
- [ ] Selecting Material/StaticMesh never leaves stale previous-asset image as final preview.
- [ ] First preview frame may be `Pending`, but final `Ready` image must match current asset.
- [ ] Content Browser Scene3D thumbnails remain consistent under rapid scrolling/switching.
- [ ] Texture2D direct preview behavior remains unchanged.

## 关联
- BUG-RENDER-001 (post-process feedback loop instability)
- AssetThumbnailService Scene3D preview code path

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | Bug created from preview rendering behavior analysis after cross-machine report. |
