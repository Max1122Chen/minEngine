# Lua 脚本 — 占位说明（P5）

Last updated: 2026-05-27  
Status: **占位，仅记录模块目标与依赖；详细设计待函数反射方案稳定后再写**  
父文档：[Platform 路线图](../PLATFORM_ROADMAP.md) §2 P5、§11  
前置：[函数反射现状](../Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md)

---

## 0) 模块目标（概念级）

- 为引擎提供一条 **轻量级脚本通路**，用于编写 Gameplay/Editor 级逻辑。  
- 通过 **现有反射系统**（属性 + 未来的函数反射）访问引擎对象，而不是直接暴露大量手写 C API。  
- 不在首版追求「全引擎 API 覆盖」或复杂的运行时管理功能。

---

## 1) 与 P4 的依赖关系

- Lua 层理想的调用方式是：`obj:Foo(args)` → 通过 `MEFunction` / `ProcessEvent` 进入 C++，而不是每个函数绑一份独立的 C 接口。  
- 因此，Lua 的详细设计需要在 **函数反射的调用约定、参数封送方案** 拍板之后再展开。  
- 本文档在那之前只保留高层目标和依赖说明，不规定具体 API（如是否使用 sol2、脚本组件形态等）。

---

## 2) 未来需要讨论的点（备忘）

- 选用哪一层 Lua 绑定方式（纯 C API / sol2 / 其它库）。  
- 脚本生命周期如何与 `ObjectManager` / Scene 卸载协同。  
- Gameplay 脚本是挂在通用 `ScriptComponent` 上，还是按系统拆分（输入、AI 等）。  
- 是否需要 Editor 一侧的脚本调试/重载支持。

这些内容暂时**不具约束力**，仅作为后续设计讨论的起点。

