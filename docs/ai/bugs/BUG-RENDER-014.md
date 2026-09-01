# BUG-RENDER-014 — Point light shadow beyond influence radius

## Meta
- **ID:** BUG-RENDER-014
- **Status:** Fixed (pending commit + Editor visual verify)
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-09-01
- **Last updated:** 2026-09-01
- **Affects:** Point light shadows, GL/VK forward path
- **Related Feature/Slice:** `master` 小修复
- **Design:** [BUG-RENDER-014_POINT_LIGHT_RADIUS_ATTENUATION_DESIGN.md](../Render/BUG-RENDER-014_POINT_LIGHT_RADIUS_ATTENUATION_DESIGN.md)

## TL;DR
点光源无半径/衰减；阴影在超出有效范围后仍采样 → 画面像全屏落影。按 Design 补 `AttenuationRadius` + shader 衰减 + shadow far 对齐。

---

## 症状
点光影响范围外仍出现明显阴影或错误衰减。

## 期望
超出 `AttenuationRadius` 后光照与阴影贡献 → 0；Inspector 可配置半径与 falloff。

## 复现
1. 场景中放置带点光阴影的 Point Light。
2. 将几何体移到影响半径外或对比默认无半径行为。

## 环境
`master` post-merge；Editor GL 为主验证路径。

## 根因
1. **CPU** 未向 UBO 写入 radius（`Position.w = 1.0`）；`LightComponent` 无半径属性。
2. **光照 shader** 无距离衰减（`CalcPointLightGraph` / `CalcPointLightPBR`）。
3. **阴影 shader** 无 radius mask；`currentDepth > 1` 仍 PCF → 超 far 易全阴影。
4. Shadow pass 固定 `kPointShadowFar = 50`，与 per-light 影响范围无关。

详见 Design §2。

## 修复
按 [Design Spec](../Render/BUG-RENDER-014_POINT_LIGHT_RADIUS_ATTENUATION_DESIGN.md) 落地：
- `PointLightComponent`：`m_AttenuationRadius` / `m_AttenuationFalloff`
- UBO + shadow pass far 与 radius 对齐
- `PointLightAttenuation` / `PointLightShadowFactor` shader 辅助函数

## 回归验证
- [ ] Editor 点光场景目视：半径外无全屏落影（待你确认）
- [x] `minEngineTests.exe test shader-compiler` pass
- [x] `physics-shapes` / `physics-smoke` / `physics-sync` pass（无回归）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 登记于 ACTIVE_WORK |
| 2026-09-01 | 根因 + Design Spec 链接 |
