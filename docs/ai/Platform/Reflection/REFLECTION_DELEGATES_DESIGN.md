# 委托系统 — 占位说明（已移交）

Last updated: 2026-08-04  
Status: **Archived / 移交** — 实施约束以 **CORE-F04** 为准  

**请改读：** [CORE-F04 Native Multicast Delegates Design](../Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)

---

## 历史说明

- 本文原为 P4-Delegate 占位，强调「须先完成 ProcessEvent / 反射函数再设计委托」。
- **CORE-F04** 将委托拆为：**Native multicast（本期）** vs **Dynamic/反射（远期）**。玩法/物理所需为前者，不再被反射路径阻塞。
- 动态委托与 Lua 一等事件仍可在将来另开 Feature；概念备忘见 CORE-F04 Design §3.4。
