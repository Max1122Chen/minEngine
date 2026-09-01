# BUG-RENDER-014 — Point light shadow beyond influence radius

## Meta
- **ID:** BUG-RENDER-014
- **Status:** Open
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-09-01
- **Last updated:** 2026-09-01
- **Affects:** Point light shadows, GL/VK forward path
- **Related Feature/Slice:** `master` 小修复（非独立 Feature ID）

## TL;DR
点光源阴影在超出影响半径后应衰减/消失，当前表现为**全屏阴影**；需补 **attenuation + radius** 配置并与 shader 一致。

---

## 症状
点光影响范围外仍出现明显阴影或错误衰减。

## 期望
超出 `radius` / 衰减曲线后阴影贡献趋近 0；Inspector 可配置 attenuation 与 radius。

## 复现
1. 场景中放置带点光阴影的 Point Light。
2. 将几何体移到影响半径外或增大 radius 对比。

## 环境
`master` post-merge；Editor GL 为主验证路径。

## 根因
<!-- TBD -->

## 修复
<!-- TBD -->

## 回归验证
- [ ] Editor 点光场景目视
- [ ] `verify.ps1` / 相关 render 测试（如有）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 登记于 ACTIVE_WORK master 小修复轨 |
