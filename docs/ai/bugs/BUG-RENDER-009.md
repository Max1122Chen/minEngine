# BUG-RENDER-009 — Mesh Hot-Swap Causes VK_ERROR_DEVICE_LOST

## Meta
- **ID:** BUG-RENDER-009
- **Status:** Verified
- **Severity:** S0
- **Owner:**
- **Found:** 2026-08-26
- **Last updated:** 2026-08-26
- **Affects:** Vulkan `VulkanRHIBuffer` lifetime, StaticMesh Inspector swap
- **Related Feature/Slice:** ED-F01 BF-S05

## TL;DR
Changing a StaticMesh asset in Inspector could `DEVICE_LOST` because `VkBuffer` was destroyed while still referenced by in-flight command buffers. **Fixed** with deferred buffer destruction after frame fences.

---

## 症状
- Inspector: swap mesh on a component → crash / device lost on Vulkan.

## 期望
- Mesh swap safe; old GPU buffers retire after GPU finished prior frames.

## 复现
1. Vulkan Editor; select mesh component; change Mesh asset repeatedly.

## 根因
`VulkanRHIBuffer` destructor called `vkDestroyBuffer` immediately. Weak asset cache can drop the old `StaticMesh` while CB still binds its VB/IB.

## 修复
Retire queue on `VulkanRHI`: destroy buffers only after `vkWaitForFences` at frame begin (all in-flight frames idle).

## 回归验证
- [x] VK: swap meshes in Inspector without DEVICE_LOST (user 2026-08-26)
- [x] GL: unchanged

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-26 | Open → Fixed (deferred destroy) |
| 2026-08-26 | User verified → Verified |
