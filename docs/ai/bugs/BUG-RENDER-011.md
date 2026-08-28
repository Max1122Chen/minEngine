# BUG-RENDER-011 — Crash when disabling point/spot light Cast Shadow

## Meta
- **ID:** BUG-RENDER-011
- **Status:** Fixed (pending verify)
- **Severity:** S1
- **Found:** 2026-08-28
- **Related:** TD-025 · `EngineSceneBindingSets.cpp`

## TL;DR
Runtime toggle of point/spot `Cast Shadow` off left stale SRV bindings in scene set 1 (loops only iterated active handle count, never cleared cached slots). **Fixed:** always iterate max shadow map slots and null cached textures when handles absent.

## 回归验证
- [ ] Editor VK：关 PointLight / SpotLight Cast Shadow 不崩溃
- [ ] 方向光 shadow 仍正常
