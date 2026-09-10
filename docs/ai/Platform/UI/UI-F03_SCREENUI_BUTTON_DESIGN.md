# UI-F03 — ScreenUI Button

## Meta
- **ID:** UI-F03
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Branch:** feat/ui
- **Related:**
  - [UI-F01](./UI-F01_UI_SYSTEM_DESIGN.md)（Canvas / Widget / Image）
  - [UI-F02](./UI-F02_SCREENUI_HITTEST_DESIGN.md)（Hit / Pointer / Click 边沿）
  - [CORE-F04](../Core/CORE-F04_NATIVE_MULTICAST_DELEGATES_DESIGN.md)（Native Multicast Delegate）
  - [CORE-F01](../Scripting/LUA_SCRIPTING_DESIGN.md) / [CORE-F02](../Scripting/LUA_SCRIPT_BINDING_DESIGN.md)（Lua — **本 Feature 不接入**；待专用分支设计 C++→Lua 调用后再挂）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** UI-F01 Done；UI-F02 Done；CORE-F04 Done
- **Blocks:** 可交互 HUD / 菜单雏形；不挡 Text / Focus / Lua 事件桥（另 Feature / 分支）
- **UE 对照源码根：** `D:/Dev/GitRepo/UnrealEngine/Engine/Source/Runtime/`（UMG `UButton` / Slate `SButton`）

## TL;DR

在 UI-F02 的 **Widget 命中 + Click 边沿** 之上，增加薄 **`ButtonComponent`**：同一 GO 上复合 Widget（几何/命中）+ Image（皮）+ Button（政策/订阅）。  
对齐 **Unity uGUI**；**不**克隆 UE ContentWidget / `FReply`。  
`OnClicked` = Native Multicast（C++）；**可指定 `TargetGraphic`（Image）做 tint**。  
**本期不做任何 Lua 回调**——C++ 调 Lua 函数应先在专用分支把机制设计妥当，再接入 Button。

## Scope

### In（MVP）
- `ButtonComponent`（同一 UISystem 伞；**不成**新 System）
- UISystem 在 Click 边沿通知归属 Widget 的 Button → `OnClicked.Broadcast()`（**无参**）
- 同 GO 需 `WidgetComponent`（命中）；缺则无效
- **`TargetGraphic`**：可选 `ImageComponent` 引用（序列化）；空则回退同 GO sibling Image；**只影响 tint，不影响 Hit**
- Interactable + 四色 tint（可关）
- C++ 订阅 `OnClicked`；单测 + Play 目视
- Play/PIE 才路由（继承 F02）

### Out（后置）
- **Lua / 脚本：** `on_clicked` 薄桥、ScriptCallable 表面、Multicast↔Lua AddListener — **一律后置**；依赖「C++ 调用 Lua 函数」专用设计落地后再开切片或另 Feature
- Focus / Tab / 键盘·手柄激活
- Toggle / Checkbox / Radio
- UnityEvent 式 Inspector 绑任意方法名；Dynamic/反射委托
- 九宫格 / Button Style 资产 / 声音
- 透明像素精确命中；Button 自建第二套 Hit
- 新建 `ButtonSystem` / `UIEventSystem`

## Reader quick start
1. §3.0 对标  
2. §3.1 组件 + TargetGraphic  
3. §3.2 Click → OnClicked（仅 C++）  
4. §5 切片 · §9 已决

---

## Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 前置 | F01/F02/F04 **sound** |
| 债风险 | **low–medium**：TargetGraphic 引用序列化需对齐现有 Component 指针属性习惯 |
| Philosophy | 机制出口进 UI；Hit 仍 Widget；脚本桥不绑死在 Button 上抢跑 |
| 建议 | **Go**（§9 已决；Lua 明确后置） |

---

## 1) 背景与目标

**Pain：** 能 Hover/Click，但无法用 C++ 订阅「这个控件被点了」；也不能指定变色用的 Image。

**成功（MVP）：**
- GO = Widget + Image + Button；Play 点击 → C++ `OnClicked`
- tint 打在 `TargetGraphic`（或 sibling Image）；命中仍只走 Widget
- 无新 UI System；无 Lua 耦合

---

## 2) 现状（2026-09-10）

| 能力 | 状态 |
|------|------|
| Widget / Image / Canvas | **有** |
| Hit + PointerState + Click | **有** |
| Multicast Delegate | **有** |
| LuaComponent | **有**（仅 `tick` 等；**本 Feature 不调用**） |
| Button | **无** |

---

## 3) 方案

### 3.0 对标（Unity 优先）

| | Unity | UE | minEngine |
|--|-------|-----|-----------|
| 组合 | GO 多 Component | UButton + Content | **GO 多 Component** |
| Hit | Graphic | SButton 几何 | **WidgetComponent** |
| 变色目标 | `targetGraphic` | Style Brush | **`TargetGraphic` → Image** |
| Click 出口 | UnityEvent | OnClicked | **Native Multicast（C++）** |

Hit **不**改到 Image；与 F02 一致。

### 3.1 组件职责

```text
GameObject (under Canvas)
 ├── WidgetComponent     ← 几何 / 命中
 ├── ImageComponent      ← 皮（常作 TargetGraphic 默认）
 └── ButtonComponent     ← OnClicked / tint
```

| 组件 | 做 | 不做 |
|------|----|------|
| Widget | 命中 | 回调、tint |
| Image | 绘制 | Hit、OnClicked |
| Button | 派发、TargetGraphic tint | 第二套 Hit、Focus、Lua、新 System |

**Widget：** 同 Owner 查找；缺失 → 不派发。

**TargetGraphic（进 MVP）：**
- `ME_PROPERTY`：`std::shared_ptr<ImageComponent> m_TargetGraphic`
- `ResolveTargetGraphic()`：非空用引用；否则同 GO sibling Image；仍无则跳过 tint
- **语义对齐 Unity `targetGraphic`：视觉状态目标，不是 hit 代理**
- 允许 Image 在子 GO：引用可指向其它对象；Hit 仍是 Button 所在 GO 的 Widget

### 3.2 Click → OnClicked（仅 C++）

```text
UISystem 确认 bClickThisFrame 且 Pressed = Widget W：
  → Owner 上可交互 Button B
  → B.NotifyClicked():
       OnClicked.Broadcast()           // C++ only
```

**派发：** UISystem 边沿（已决）。Capture 归属 Pressed（对齐 F02）。

**可交互：** Active + Widget `IsHitTestVisible` + `m_bInteractable`。

**OnClicked：** `DECLARE_MULTICAST_DELEGATE(FOnButtonClicked)`，**无参**。

**Lua（后置说明）：**  
需求方向是「C++ 能调用 Lua 中的函数」。当前 `LuaComponent` 只有环境 + `tick`，缺少稳定、可复用的 **C++→Lua 调用** 机制（寿命、错误、签名、与 Delegate 关系）。在 Button 上先塞 `on_clicked` 约定会变成特例债。  
**决议：** 先在 **专用分支** 设计并落地通用能力，再以薄适配挂回 Button（或另开 UI/Script Feature）。本 MVP **零** Lua 代码路径。

### 3.3 视觉反馈（进 MVP）

| 状态 | 行为 |
|------|------|
| Normal | 恢复缓存色 / `m_NormalColor` |
| Hovered | → Highlighted |
| Pressed | → Pressed |
| Disabled | → Disabled；不派发 |

`m_bColorTintTransition` 默认 true。作用对象 = `ResolveTargetGraphic()`。

### 3.4 UISystem

禁止 `ButtonSystem`；非 Play 不误触。

### 3.5 测试

- 单测：Broadcast 计数；Disabled / 无 Widget；TargetGraphic 显式引用 vs 回退 sibling
- Play：点击变色 + C++ 订阅可观察（日志即可）

---

## 4) UE / Unity 备忘

- Unity：`targetGraphic` 主攻变色；raycast 另算 — 我们 Hit=Widget、tint=TargetGraphic
- UE：`SButton` + Reply — 不抄；挡住世界点用已有 `ShouldBlockWorldPointer`

---

## 5) 切片

| 切片 | 内容 | 验收 |
|------|------|------|
| **S0** | Button + OnClicked + UISystem 派发 + TargetGraphic 解析 + 单测 | Click → Broadcast；引用/回退正确 |
| **S1** | Interactable + 四色 tint | Play 悬停/按下变色；Disabled 不派发 |
| **后置** | Lua（待 C++→Lua 专用设计）、Focus、Style 资产、声音 | — |

---

## 6) 备选方案

| 选项 | 结论 |
|------|------|
| A. Unity 式 GO 复合 + Widget 命中 | **选用** |
| B. UE ContentWidget | 不用 |
| C. 无 Button 只读 PointerState | 不用 |
| D. Button 自建 Hit | **禁止** |

---

## 7) 风险与缓解

| 风险 | 缓解 |
|------|------|
| Component 引用序列化 | 对齐现有 `shared_ptr` 属性；单测显式引用 |
| 误把 TargetGraphic 当 Hit | 注释写死；Hit 测试钉 Widget |
| Tick 丢 Click | UISystem 边沿派发 |
| 日后 Lua 接入改 API | `OnClicked` 保持 C++ Multicast；Lua 只作额外订阅/桥，不替换出口 |

---

## 8) 验收标准

- [x] S0–S1 落地
- [x] 无新 UI System
- [x] 单测：OnClicked、Disabled、无 Widget、TargetGraphic（`screen-ui-button` 4/4）
- [x] Play：tint + C++ 回调可观察（HUD 第二行 Button / OnClickedEdge / Total；ship 确认）
- [x] 命中仅经 Widget + HitTester
- [x] **无** Lua 调用路径进入本 Feature 代码

---

## 9) 岔路口决议（2026-09-10）

| # | 问题 | 决议 |
|---|------|------|
| 1 | OnClicked 无参 vs sender？ | **无参** |
| 2 | 派发位置 | **UISystem 边沿** |
| 3 | tint 进 MVP？ | **是** |
| 4 | Lua？ | **后置**（专用分支先设计 C++→Lua 调用，再接入；本 Feature 不做薄桥） |
| 5 | TargetGraphic？ | **进 MVP**（tint 目标引用；不改 Hit） |

---

## 10) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done**（MVP） |
| Blocked by | — |
| Next | —；后置见 §Out（Lua / Focus / Text 等另 Feature） |

### Play 目视步骤（简）

1. 场景：Canvas → 子 GO 加 `WidgetComponent` + `ImageComponent` + `ButtonComponent`（可设醒目 Pressed/Highlighted 色）
2. Play；viewport 左上角 ScreenUI HUD 第二行应出现 `Button Hover:yes`
3. 悬停/按下看 Image tint；松开看 `OnClickedEdge:YES` 且 `Total` +1

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-09 | Draft：Unity 式复合；Widget 命中 + OnClicked |
| 2026-09-10 | **Planned**：TargetGraphic + tint 进 MVP；曾拟 Lua 薄桥进 MVP |
| 2026-09-10 | **修订：** Lua 全部后置；待专用分支设计「C++ 调用 Lua」后再挂；切片改回 S0–S1 |
| 2026-09-10 | **In Progress**：S0/S1 代码 + `screen-ui-button` 绿；修 `GetComponentsOfType` 销毁期空洞；待 Play 目视 |
| 2026-09-10 | Play HUD：`Clicked` / `bButtonOnClickedThisFrame` + 第二行 Button 调试；Editor 已编过 |
| 2026-09-10 | **Done**（MVP）：用户确认提交；Lua/Focus/Text 等仍后置 |
