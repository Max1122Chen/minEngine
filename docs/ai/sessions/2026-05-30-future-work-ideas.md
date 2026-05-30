# Session 2026-05-30 — 未来工作想法清单

## Meta

- **Date:** 2026-05-30
- **Feature/Slice:** #3 已 promote → `RND-F01`（RenderGraph）；其余项未登记
- **Status:** **Reference** — 个人想法备忘；**不是** agent backlog
- **Promote to:** 选定某项后再登记 `FEATURE_REGISTRY` → Design Spec →（可选）写入 `ACTIVE_WORK.md`

> **Agent:** 勿从此文件推断 mandatory 待办。Tier B reference only（见 `.cursor/rules/docs-trust-tiers.mdc`）。

---

## TL;DR

用户在 2026-05-30 梳理了 10 条「可以做的事」；仅作 parking lot，未进入 `ACTIVE_WORK.md`。

---

## 背景

材质 Phase 0–5 与基建（CLI/Test/verify）已 largely 收口；用户开始思考下一批可选方向，希望先落盘以免遗忘，但**暂不排期**。

---

## 想法清单（用户原序）

| # | 主题 | 简要说明 | 已有参考文档 |
|---|------|----------|--------------|
| 0 | **编辑器创建引擎原生资产** | 在 Editor 侧创建 Material、Scene 等引擎原生资产类型（非仅导入外部文件） | [EDITOR_PLATFORM_PLAN](../Editor/EDITOR_PLATFORM_PLAN.md)、[ASSET_PIPELINE_DESIGN](../Platform/ContentBrowser/ASSET_PIPELINE_DESIGN.md) |
| 1 | **Lua 脚本接入** | `LuaComponent` + Script 资产 + 反射驱动游戏逻辑 | [LUA_SCRIPTING_DESIGN](../Platform/Scripting/LUA_SCRIPTING_DESIGN.md)（占位）；前置：函数反射 P4 |
| 2 | **委托系统** | 单播/多播、与 `MEFunction`/Lua 统一事件模型 | [REFLECTION_DELEGATES_DESIGN](../Platform/Reflection/REFLECTION_DELEGATES_DESIGN.md)（占位）；前置：函数反射 |
| 3 | **渲染系统** | 渲染管线重构 → **RenderGraph**（**`RND-F01`**）；渲染 bug 单独 `BUG-*` | [RENDER_GRAPH_DESIGN](../Render/RENDER_GRAPH_DESIGN.md)、[RENDER_REFACTOR_PLAN](../Render/RENDER_REFACTOR_PLAN.md) |
| 4 | **MaterialInstance + MaterialRenderProxy + MeshDrawCommand 排序** | 材质实例化、渲染代理、DrawCommand 排序（UE 式 draw 路径深化） | [MATERIAL_RUNTIME_BRIDGE_CHECKLIST](../Render/Material/MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md) §B/D；[RENDER_REFACTOR_PLAN](../Render/RENDER_REFACTOR_PLAN.md) |
| 5 | **2D 渲染** | Sprite、2D 动画 | —（尚无专项设计） |
| 6 | **骨骼网格体 + 骨骼动画** | Skeletal mesh、骨骼动画管线 | [RESOURCE_PIPELINE_PLAN](../Render/RESOURCE_PIPELINE_PLAN.md)（网格导入相关，非完整骨骼方案） |
| 7 | **粒子系统** | — | — |
| 8 | **音效系统** | — | — |
| 9 | **物理系统** | 碰撞、物理模拟等 | — |

---

## 依赖关系（粗粒度，非拍板）

```text
P4 函数反射 ──┬──► 委托 (#2)
              └──► Lua (#1)

渲染 Viewport 重构 (#3) ──► MaterialInstance / DrawCommand (#4) 更易落地

Editor 原生资产创建 (#0) ──► 与 Content Browser / 序列化 / Inspector 联动

#5–#9 相对独立，但均依赖较稳的 Scene/Component + 资产管线
```

与 [PROJECT_CONTEXT](../PROJECT_CONTEXT.md) §6 一致的部分：**Platform Core 主线**仍是 P4 反射 → 委托 → P5 Lua；渲染/Editor 产品化可并行。

---

## 未决问题

| 问题 | Owner | Next check |
|------|-------|------------|
| 10 条中的优先级与第一条开工项 | 用户 | 选定后 promote 到 `ACTIVE_WORK` + `FEATURE_REGISTRY` |
| ~~RenderGraph 是 #3 的子目标还是独立 Feature~~ | — | **已拍板：** 独立 `RND-F01` |
| #0 Material/Scene「创建」与现有 Import/模板流程边界 | 用户 | 开 ED Design 前拍板 |

---

## 下一步

无自动 next action。用户选定一项后：

1. 在 [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) 登记 `<DOMAIN>-Fnn`
2. 从 [templates/design-spec.template.md](../templates/design-spec.template.md) 开 Design
3. 若需 agent 排期，写入 [ACTIVE_WORK](../ACTIVE_WORK.md) 的 **In focus**（1–3 条）

---

## 链接

- [ACTIVE_WORK.md](../ACTIVE_WORK.md) — 刻意**未**修改
- [PLATFORM_ROADMAP.md](../Platform/PLATFORM_ROADMAP.md) §11 Core 切片（历史参考）
- [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md) — 下次开工前登记
