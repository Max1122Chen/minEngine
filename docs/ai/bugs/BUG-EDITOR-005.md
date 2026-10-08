# BUG-EDITOR-005 — PIE SceneViewport 近平面未从 Main Camera 同步

## Meta
- **ID:** BUG-EDITOR-005
- **Status:** Fixed
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-10-07
- **Last updated:** 2026-10-08
- **Affects:** PIE SceneEditingViewport；`SyncPlayModeCamera`；`CameraComponent` 镜头参数
- **Related Feature/Slice:** CORE-F30（Play Main Camera）；minCraft feedback `ENGINE_FEEDBACK_PROMPT_PIE_VIEWPORT_NEAR_PLANE.md`

## TL;DR
PIE 绘制用 `SceneViewport` 自有 `RenderCamera`（默认 near=1），`SyncPlayModeCamera` 只拷位姿；`CameraComponent` 的 near/FOV 未推到内部相机。已同步镜头参数并在组件侧 ApplyLens。

---

## 症状
PIE 贴墙近处面被 near-clip；游戏把 Main 相机 near 设为 0.05 无效。

## 期望
Play 视口使用 Main `CameraComponent` 的 FOV/near/far；Edit 飞行仍用 viewport 自有镜头（Stop 后不残留 Play near）。

## 复现
1. PIE 场景中 Main Camera near=0.05（或体素玩家贴墙）
2. 观察近处方块面被整面裁掉

## 环境
- Windows；Debug MaximumEditor；minCraft-gameplay 反哺 2026-10-07

## 根因
1. `BuildDrawDesc` → viewport `m_Camera`（`RenderCamera::m_zNear` 默认 1.0）
2. `SyncPlayModeCamera` 仅 position/rotation
3. `CameraComponent::DoEndOfFrameUpdate` 不写 projection；组件字段与内部 `RenderCamera` 脱节

## 修复
- `CameraComponent::ApplyLensToRenderCamera` + SetNear/FOV/Far；ctor / EOF 推送
- `SyncPlayModeCamera` 拷贝 FOV/near/far 并更新投影（aspect 仍由 `SyncSceneViewportCameraAspect`）

## 回归验证
- [x] `play-main-camera`：`RunLensParamsPushToRenderCamera`
- [ ] 手动：minCraft PIE 贴墙；删掉 `EnsureVoxelNearPlane` 后仍正常

## 关联
- [CORE-F30](../Platform/Core/CORE-F30_PLAY_MAIN_CAMERA_BINDING_DESIGN.md)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-08 | 记录 + 修复落地 |
