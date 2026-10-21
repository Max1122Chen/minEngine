# CORE-F43 GO/Component 的 active 与 visible 语义及级联 — Design Spec

## Meta
- **ID:** `CORE-F43`
- **Type:** Feature（域语义补齐 + 级联传播 + 编辑器表面）
- **Complexity:** `L2`（跨 Framework 基类 / UI 布局 / UI 命中 / 渲染收集 / 物理 / 序列化 / Editor Inspector 七个消费面；含状态机、缓存失效不变量、调用点分类审计）
- **Status:** `Implemented` —— S01–S07 全部落地（`FEATURE_REGISTRY.md:165` 仍登记为 Draft，需要单独一行改动；本 Feature 不改该文件）。实现记录：S01/S02 → `CORE-F43_S01_S02_IMPLEMENTATION.md`，S03–S05 → `CORE-F43_S03_S05_IMPLEMENTATION.md`，S06/S07 → `CORE-F43_S06_S07_IMPLEMENTATION.md`
- **Owner:** project maintainer
- **Last updated:** 2026-02-14 **v2**（修订：①Inspector 两轴做成 header 特殊开关 + `Invisible` specifier；②D2 裁定为祖先链项，active 与 visible 同理；③D5 改写成「查询 / 状态变化 / 稳态」三层；④**D6 反转**——`IsActive()` / `IsVisible()` 返回层级结果，自身 flag 另用 `IsActiveSelf()` / `IsVisibleSelf()`，并新增 §H8 调用点分类审计）
- **Related:**
  - [CORE-F06 Component Activate](./CORE-F06_COMPONENT_ENABLE_DESIGN.md) —— **本 Feature 的直接前作**；其 §2.6（:284）明确写着「单轴 `m_bActive` / 将来若需要：拆 `bVisible`、`bSimulatePhysics`」，F43 就是那个「将来」里的**可见性轴 + 层级级联**部分
  - [CORE-F08 GameObject Hierarchy](./CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md) —— 提供 `m_Parent` / `m_Children` 的上溯与下溯链
  - [CORE-F25 Owning Scene RenderScene](./CORE-F25_OWNING_SCENE_RENDERSCENE_DESIGN.md) —— 代理注册 / 摘除的宿主
  - [CORE-F31 Begin/End Play Lifecycle](./CORE-F31_BEGIN_END_PLAY_LIFECYCLE_DESIGN.md) —— `BeginPlay` 的 active 门
  - [BUG-CORE-003 Prefab ReflProp Propagate](./BUG-CORE-003_PREFAB_PROPAGATE_REFLECTED_PROPERTIES_FIX_DESIGN.md) —— prefab override 传播的反射路径（注意：`Invisible` 属性**不**传播，见 §D10）
  - `FEATURE_REGISTRY.md:165`（已登记，本 Feature **不改**该文件）
- **Depends on:** `GameObject::GetParent/GetChildren`（`GameObject.h:123-124`）、`Component::m_Owner`（`Component.h:86-87`）、`Component` 激活机制（`Component.cpp:77-146`）、`SceneComponent::MarkRenderStateDirty`（`SceneComponent.h:28`）、`SceneManager::ResolvePendingActivationsForScene`（`SceneManager.cpp:660-691`）、Editor 组件 header 挂点（`SceneEditorInspectorSource.cpp:273-323`）
- **Blocks:** 无（但 UI 侧「面板整体隐藏 / 整块禁用」类需求会持续以 workaround 形式堆积，直到本 Feature 落地）
- **Does not block:** 任何进行中 Feature

## TL;DR

本仓已经有**每组件**的 active 开关（`Component.h:38-41, 90`），但 **`GameObject` 完全没有任何 active / visible 字段**（`GameObject.h` 全文无此二者），并且**没有任何级联**：UI 布局、UI 命中、渲染收集三处消费点在遍历时都**只查自身**却在**无条件递归子树**（`UILayoutPass.cpp:33` + `:41-44`、`ScreenUIHitTester.cpp:148` + `:160-163`），渲染侧则完全依据每组件自己算出的位（`WidgetComponent.cpp:348-351, 386-389` → `ForwardRenderer.cpp:1232`）。

于是「把一个 UI 面板整体关掉」这件事在数据模型里**无法表达**——只能靠调用方手动遍历子树逐个 `SetActive(false)`，而那会**破坏每个组件原本的作者化状态**（关一次再开一次，组件被写回成统一的 `true`，用户之前精心关掉的按钮、隐藏的图标全部回魂）。

本 Feature 补的是 **GameObject–Component 这个域的语义**，不是某个 widget 的补丁：

1. `GameObject` 增加 `m_bActive` / `m_bVisible`（作者化、序列化），**两者都按子树递归生效**（active 亦然，不只 visible——见 §D2 裁定）。
2. `SceneComponent` 增加 `m_bVisible`（**不是 `Component` 基类**——理由见 §H2）。
3. 对外暴露**有效态**：**`IsActive()` / `IsVisible()` 直接返回层级计算结果**（v2 裁定，符合用户心智）；**自身 flag 另用 `IsActiveSelf()` / `IsVisibleSelf()`**。
4. **级联靠逻辑与，永不写回字段**。父 GO 切换只做两件事：把子树的有效态缓存失效、按「已应用态 vs 新有效态」的差量调 `Apply/RemoveActivationToSystems`。**组件自己的 `m_bActive` / `m_bVisible` 一个比特都不动**——这是本设计的核心不变量（I1），也是「component 自己维护自己的 activation 状态」的唯一可行实现。
5. 性能上界：稳态查询 **O(1)**（一次缓存读，与树深无关），失效只在状态切换 / 挂接 / 反序列化时发生，**每帧不做任何父链上溯**。
6. **Inspector 单点编辑（v2）**：两轴在 GO header 与 Component header 上是**特殊开关**，字段本体用 `ME_PROPERTY(Invisible)` 从通用字段列表移除，杜绝「多处可改」。
7. **命名改动就是语义改动（v2）**：`IsActive()` 从「自身 flag」变成「层级结果」，因此必须逐处分类现有 58 个调用点（56 在 Runtime、2 在 Editor），见 §H8。

业界同构做法：Unity `activeSelf`（自身）× `activeInHierarchy`（祖先链求与，从不写回）、UE `SetActorHiddenInGame` / `SetActorTickEnabled` 分轴、Godot `process_mode` × `CanvasItem.visible`（`is_visible_in_tree()` 是祖先链求与）。三者共同点：**节点局部开关是唯一真源，有效态是派生量**。本 v2 的命名取舍（`IsActive()` = 有效态、`IsActiveSelf()` = 自身）正是 Unity `activeSelf` 的同构。

## Scope

### In
- `GameObject` 的作者化 `m_bActive` / `m_bVisible` 与子树递归语义（两轴同构）。
- `SceneComponent` 的作者化 `m_bVisible`（可见性轴的**唯一**落点基类）。
- 有效态契约与访问器命名（`IsActive` / `IsVisible` = 有效态；`IsActiveSelf` / `IsVisibleSelf` = 自身 flag）。
- 失效与重算（reconcile）传播：`SetActive` / `SetVisible` / `SetOwner` / 挂接变更 / 反序列化后。
- 消费点改造清单（UI 布局、UI 命中、渲染代理收集、3D 网格与阴影、物理 / 碰撞、tick、BeginPlay、相机选择、Lua、音频门）。
- **调用点分类审计**（58 处：行为门 / 激活机制本体 / 纯诊断），含「会改变语义」清单与自激防护。
- **Inspector 契约**：两轴作为 GO header 与 Component header 的特殊开关；字段本体 `Invisible`；禁用态与只读有效态的显示规则。
- 与既有 `m_bActive` 激活状态机（Applied / Pending）的对接方式。
- 序列化缺省值、旧场景 / prefab 行为、需直接改动的旧文件清单。
- 可观察 oracle 与最小自动化验证路径。

### Non-goals（明确不做，见 §Deferred）
- **不引入**多轴模拟开关（`bSimulatePhysics` / `bAffectsNavigation` / `bTickEnabled` 独立轴）—— F06 §2.6 提到的其余轴留待将来。
- 不做遮挡剔除 / 距离剔除 / 逐视口可见性（那是渲染器的 culling，不是作者化语义）。
- 不做 UI CanvasGroup 式聚合 alpha / 交互屏蔽。
- 不重构 `m_bHitTestVisible`（保持正交，见 §A1）。
- 不改 `FEATURE_REGISTRY.md`、不改 `bugs/*`、不动 `minCraft*`。
- 不做「延迟到帧首批量 flush」的队列化传播（S02 同步实现，队列化见 Deferred）。
- **不新增反射 specifier**（v2：现成的 `Invisible` 已够用，理由与核实见 §H9）；不重做 Inspector 通用字段列表框架。

## Reader quick start
1. 先读 **Problem P2/P3**（级联缺失的三处证据）与 **§H2**（visible 为什么落在 `SceneComponent`）。
2. 契约只读四处：**§H3 有效态契约原文**、**§H4 三层模型**、**§H8 调用点分类审计**、**§S1–S3 状态表 + 不变量 I1–I8**。
3. 编辑器面看 **§H9 Inspector 契约**；落地顺序看 **§切片**；争议看 **§待主人审批的决策清单**（D1/D2/D3/D4/D5/D7/D8/D9 已批准，D6 已裁定反转，D10 待定）。

## Pre-flight

| 项 | 结论 |
|----|------|
| **Complexity** | **L2**（基类字段 + 七类消费面 + 缓存失效不变量 + 命名语义反转的逐处审计；每片可独立回退） |
| **Prerequisites（语义层）** | `Component` 激活机制（`Component.cpp:77-146`，含 Applied/Pending 两位）**sound**；`GameObject` 父子链（`GameObject.h:123-124`）**sound**；`SceneComponent::MarkRenderStateDirty`（`SceneComponent.h:28`）**sound**；反序列化统一重算点（`SceneManager.cpp:660-691`）**已存在**，扩成两阶段 |
| **Prerequisites（编辑器层，v2）** | 组件 header 已有 active 复选框 + 禁用灰显 + undo 路径（`SceneEditorInspectorSource.cpp:273-323`）**sound，可直接扩展**；GO header 行存在（同文件 128–141 的 `Selectable`）但**无 header 控件 helper**，需新增绘制函数；`Invisible` specifier 已能隐藏字段并挡住 PropertyPath 写入（`PropertyEditPolicy.cpp:34-40`、`PropertyPath.cpp:693-700`） |
| **Tech-debt risk** | **medium**——① 新增「有效态」概念轴，漏改消费点会留半级联；② v2 命名反转使 58 个调用点必须逐处分类，漏分类会让激活机制自身被级联影响（§F11）。缓解：S01/S02 先钉死契约与缓存，S05 前审计清零，差分测试兜底 |
| **WIP** | 与任何进行中 Feature 无文件重叠；本设计**只落一个 .md** |
| **Philosophy** | 「节点自己维护自己的开关」是引擎机制 → 进 Framework 基类；「关掉整块」是作者意图 → 用祖先链求与表达，**不靠写回**；「开关只有一个编辑点」是编辑器纪律 → 靠 specifier + header 控件强制，而不是靠注释提醒 |
| **Recommendation** | S01（字段 + 契约 + 缓存，不改行为）→ S02（reconcile）→ S03/S04/S05（消费面清零）→ **S06（Inspector 契约）** → S07（收尾）。S01/S02 之后设计的真伪已被证明，后续各片可独立验收 |

---

## Problem

### P1 — 域语义缺失：活跃性只有「每组件」一档，`GameObject` 连字段都没有

- `Component::m_bActive{ true }`（`Component.h:90`）+ `SetActive` / `IsActive`（`Component.h:38-41`）是**全仓唯一**的活跃性开关。
- `GameObject.h` 全文（163 行）**没有任何 active / visible 字段**：`GameObject` 有 `m_Parent`（`GameObject.h:145-146`）、`m_Children`（`:148`）、`m_RootComponent`（`:150-151`）、`m_Components`（`:153-154`）、`m_bHasBegunPlay` / `m_bPendingDestroy` / `m_FixedTickComponentCount`（`:156-158`）——**没有开关**。
- 因此「这棵子树整体停用」在数据模型里没有表达方式。

### P2 — 级联缺失的直接后果：三处消费点只查自身，却无条件向下递归

| 消费点 | 证据 | 现状 | 后果 |
|--------|------|------|------|
| UI 布局 | `UILayoutPass.cpp:33`（`widgets[0]->IsActive()`）+ `:41-44`（无条件 `LayoutGameObject(child, ...)`） | 父节点不活跃时仍递归进子树；`:30` / `:38` 的 `childParentRect` 回退到祖父矩形 | 父 GO 关掉后子 widget 的 `m_ComputedRect` **照旧被写入**（只是父矩形错位），布局与作者意图不一致 |
| UI 命中 | `ScreenUIHitTester.cpp:148`（`widget->IsActive() && widget->IsHitTestVisible()`）+ `:160-163`（无条件递归） | 父不活跃，子仍可被命中 | 「隐藏的面板还能点到」——最容易被用户当成 bug 的一类 |
| 渲染收集 | `ForwardRenderer.cpp:1232`（`proxy->m_bVisible`）+ `:1244`（`canvas->IsActive()`）；`m_bVisible` 由 `WidgetComponent.cpp:348-351`（文字）/ `:386-389`（图片）**只由自身状态算出** | 与父链无关 | 父 GO 关掉后 widget 依然进 `ScreenUIQueue` 被画出来 |

注意 `WidgetSceneProxy.h:42` 的 `m_bVisible = false` 是**每帧重算的渲染代理缓存位**（由 `FillSceneProxy` 写、由 `ForwardRenderer.cpp:1232` 读），**不是作者化标志**；把它当作者化字段用是本设计要避免的第一类错误。

### P3 — 可见性轴根本不存在

- `WidgetComponent` **只有** `m_bHitTestVisible`（接口 `WidgetComponent.h:70-72`、字段 `:116`），语义是「命中测试是否跳过」（`:70` 注释原文：*"When false, skipped by ScreenUI hit-test (still may draw)"*）——**它明确不控制绘制**。
- 全仓 `m_bVisible` 仅 3 处出现（`WidgetSceneProxy.h:42` 缓存位、`WidgetComponent.cpp:348/386` 写、`ForwardRenderer.cpp:1232` 读），**没有任何作者化可见字段**。
- 结论：「不画出来但还在」同样无法表达。今天只能把组件 `SetActive(false)`——顺带 tick / 脚本 / 物理一起停，语义过载。

### P4 — 消费面散落 20+ 处，语义靠口头约定；v2 的命名反转把它升级为**审计问题**

全仓（Runtime）`IsActive()|SetActive(|m_bActive` 共 **56 处命中**，Editor 侧另有 **2 处**（`SceneEditorInspectorSource.cpp:273`、`SceneEditingViewportClient.cpp:252`）→ **合计 58 处审计面**（逐处分类见 §H8）。每一处都是「组件自己说了算」的判断点：漏一处就等于留一条半级联通道；而在 v2 的命名反转下，**漏分类还会让激活机制自身被级联影响**（§F11）。

### P5 — 唯一能补救的土办法会破坏 per-component 状态

若让 `GameObject::SetActive(false)` 遍历组件写 `component.m_bActive = false`（最容易想到、也是本设计**明令禁止**的实现）：

   初始：GO.active = true;  Btn.active = false（作者刻意关掉）;  Icon.active = true
   GO.SetActive(false) → 写回：Btn.active = false, Icon.active = false   ← 看起来「关对了」
   GO.SetActive(true)  → 写回：Btn.active = true,  Icon.active = true    ← Btn 的作者意图被永久破坏

一次「关一下看看、再打开」就丢掉了用户在 Inspector 里逐组件调过的全部状态。这是 §I1 要挡死的东西，也是本设计**只做逻辑与、绝不写回**的唯一动机。行业同解：Unity 的 `activeInHierarchy`、Godot 的 `is_visible_in_tree()` 都是纯派生查询，从不把祖先状态写进子节点。

### P6 — 编辑器面：开关散落 / 无可视入口的风险（v2 新增）

- 组件 active 目前**已有** header 复选框（`SceneEditorInspectorSource.cpp:273-315`），而 `m_bActive` 本身是 `Invisible`（`Component.gen.cpp:9`）——**这是正确形态**：列表里没有、header 里唯一。
- GO 侧**什么都没有**：`DrawGameObjectDetails` 只有标题行（`:128-141` 的 `Selectable`）、`Selected ID`（`:145`）、然后直接进入 Add Component / Root Transform / Components。
- 若新字段只是普通 `ME_PROPERTY(EditAnywhere)`，它们会同时出现在**通用字段列表**与**header 开关**两处 → 正是主人要求避免的「多处可改」。

---

## How（所有权与数据流）

### H1 数据模型：字段、真源、落点、Inspector 呈现

| 类 | 字段 | 序列化 / specifier | 缺省 | Inspector 呈现（v2） | 真源角色 |
|----|------|--------------------|------|----------------------|----------|
| `GameObject` | `m_bActive` | 是（`ME_PROPERTY(Invisible)`） | `true` | **GO header 上的特殊开关**（§H9） | **子树的活跃性祖先项**（本节点 + 全体后代 GO + 其全部组件） |
| `GameObject` | `m_bVisible` | 是（`ME_PROPERTY(Invisible)`） | `true` | **GO header 上的特殊开关**（§H9） | **子树的呈现祖先项**（「方便用的 flag」：一次隐藏整块） |
| `Component` | `m_bActive`（**已存在**） | 是（现状已是 `Invisible`，`Component.h:89-90`） | `true` | **Component header 复选框**（现状 `SceneEditorInspectorSource.cpp:305`） | 组件自身的活跃性开关 |
| `SceneComponent` | `m_bVisible`（**新增**） | 是（`ME_PROPERTY(Invisible)`） | `true` | **Component header 复选框**（v2 新增第二个，§H9） | 组件的呈现开关（可见性轴的**唯一**作者化字段） |

> **v2 关键改动**：四个字段全部带 `Invisible` specifier —— 通用字段列表里**一个都看不到**，编辑入口只在 header 控件上（契约见 §H9，副作用见 §D10 / §F12）。

派生量（**不序列化、不作者化、不可直写**）：

| 派生量 | 位置 | 含义 |
|--------|------|------|
| `GameObject::IsActive()` / `IsVisible()` | `GameObject` | **有效态**：自身 flag ∧ 祖先 GO 链（缓存，O(1)） |
| `GameObject::IsActiveSelf()` / `IsVisibleSelf()` | `GameObject` | **自身 flag**（直读 `m_bActive` / `m_bVisible`） |
| `Component::IsActive()` | `Component` | **有效态**：`IsActiveSelf() && m_Owner->IsActive()`（缓存） |
| `Component::IsActiveSelf()` | `Component` | **自身 flag**（直读 `m_bActive`） |
| `SceneComponent::IsVisible()` | `SceneComponent` | **有效态**：`IsVisibleSelf() && m_Owner->IsVisible()`（缓存） |
| `SceneComponent::IsVisibleSelf()` | `SceneComponent` | **自身 flag**（直读 `m_bVisible`） |
| `SceneComponent::IsActiveAndVisible()` | `SceneComponent` | `IsActive() && IsVisible()` —— **绘制 / 命中的门** |
| `m_bAppliedEffective` | `Component`（private，不序列化） | 上一次 reconcile 时系统侧实际是否 applied（差量检测） |

**关键点：`Component` 基类上不出现任何 visible 概念。** `Component` 只有 active 两问；一个 `LuaComponent` 或 `InputComponent` 在类型层面就**没有**「我是否可见」这个可以问错的接口（I8）。

### H2 为什么 `m_bVisible` 落在 `SceneComponent`，而不是 `Component`

**继承层次事实（逐类实测）：**

   MEObject
   └── Component (abstract)                              Components/Component.h:23
       ├── SceneComponent (abstract)                     Components/SceneComponent.h:19   ← 提议落点
       │   ├── PrimitiveComponent (abstract)             Components/PrimitiveComponent.h:12
       │   │   ├── StaticMeshComponent                   Components/StaticMeshComponent.h:14
       │   │   ├── SkeletalMeshComponent                 Components/SkeletalMeshComponent.h:23
       │   │   └── SpriteComponent                       Components/SpriteComponent.h:18
       │   ├── LightComponent                            Components/LightComponent.h:20  (→ Directional/Point/Spot)
       │   ├── CameraComponent                           Components/CameraComponent.h:11
       │   ├── SkyBoxComponent                           Components/SkyBoxComponent.h:12
       │   ├── WidgetComponent                           Components/WidgetComponent.h:24
       │   ├── AudioComponent                            Audio/Components/AudioComponent.h:13
       │   └── AudioListenerComponent                    Audio/Components/AudioListenerComponent.h:8
       └── 直接继承 Component（**无空间变换 / 无自身渲染**）
           CanvasComponent        Components/CanvasComponent.h:14
           TextComponent          Components/TextComponent.h:25
           ImageComponent         Components/ImageComponent.h:22
           ButtonComponent        Components/ButtonComponent.h:24
           DragSourceComponent    Components/DragSourceComponent.h:21
           DropTargetComponent    Components/DropTargetComponent.h:19
           InputComponent         Components/InputComponent.h:17
           LuaComponent           Components/LuaComponent.h:21
           MovementComponent      Components/MovementComponent.h:10
           RigidBodyComponent     Physics/RigidBodyComponent.h:14
           ColliderComponent      Physics/ColliderComponent.h:10
           GameplayEventSystemComponent  GameplayFramework/Events/GameplayEventSystemComponent.h:29

**四条论证：**

1. **渲染管线的入口只有 `SceneComponent`。** `RenderScene` 只接受四类组件注册代理：`UpdatePrimitive`（`RenderScene.cpp:24`）、`UpdateLight`（`:79`）、`UpdateSkyBox`（`:207`）、`UpdateWidget`（`:253`）——**四个形参类型全是 `SceneComponent` 派生类**。可见性作为「是否产出渲染代理输出」的开关，落在绘制单元的公共基类上才是同层次的概念。
2. **代理失效的原语已长在 `SceneComponent` 上。** `MarkRenderStateDirty()`（`SceneComponent.h:28`）与 `m_bRenderStateDirty`（`:107`）都在 `SceneComponent`，注释明确 *"does not cascade"*。`visible` 翻转后要做的正是这件事——同一语义、同一层级、同一失效通道。
3. **`Component` 基类里一半是纯逻辑组件。** `InputComponent` / `LuaComponent` / `MovementComponent` / `RigidBodyComponent` / `ColliderComponent` / `GameplayEventSystemComponent` / `CanvasComponent` 都没有实体呈现。把 `m_bVisible` 放在 `Component` 上会造出 13 个直系子类里至少 7 个「永远为 true 且永远不该被读」的字段——典型的「会撒谎的字段」：新人在 `LuaComponent` 上勾掉 visible 会以为脚本停了，实际什么也没发生。**「不是所有 Component 都需要可见性」这条要求，用类型系统表达比用注释表达强。**
4. **UI 的视觉节点不会因此漏掉。** `TextComponent` / `ImageComponent` 是 `Component` 直系子类，但**它们不自己渲染**：视觉样式由兄弟 `WidgetComponent` 组装（`WidgetComponent.h:20-21` 注释原文 *"Visual style comes from a sibling ImageComponent"*；实现在 `WidgetComponent.cpp:334-357` 读 sibling text/image 填 proxy）。而 `WidgetComponent : SceneComponent`（`WidgetComponent.h:24`）。所以 UI 侧 100% 的**可绘制节点**都拿到了 `m_bVisible`，一个不漏。

**因此：** 落点是 `SceneComponent::m_bVisible`。`GameObject::m_bVisible` 则是「作者一次隐藏整棵子树」的便利 flag，它**不是一个独立的语义轴**，而是祖先链上的一项（见 H3）。

### H3 有效态契约（v2 原文，可直接引用）

> **组件的有效态由且仅由自身的作者化 flag 与祖先链上 `GameObject` 的对应 flag 相与得出：`IsActive() = IsActiveSelf() && Owner->IsActive()`，`IsVisible() = IsVisibleSelf() && Owner->IsVisible()`；两个轴都沿祖先链上溯（active 亦然，不只是 visible）。任何祖先的状态切换绝不写入（既不写入子 GO、也不写入任何组件）这些作者化 flag。**

形式化（`P(g)` = g 的父 GO，`U(c)` = c 的 owner GO）：

   GO::IsActive(g)              = g.IsActiveSelf()  ∧ (g.m_Parent ? GO::IsActive(g.m_Parent)  : true)
   GO::IsVisible(g)             = g.IsVisibleSelf() ∧ (g.m_Parent ? GO::IsVisible(g.m_Parent) : true)
   Component::IsActive(c)       = c.IsActiveSelf()  ∧ (c.m_Owner  ? GO::IsActive(c.m_Owner)   : true)
   SceneComponent::IsVisible(s) = s.IsVisibleSelf() ∧ (s.m_Owner  ? GO::IsVisible(s.m_Owner)  : true)
   SceneComponent::IsActiveAndVisible(s) = IsActive(s) ∧ IsVisible(s)

**命名（v2 裁定）**：`IsActive` / `IsVisible` = 有效态（默认给人用，符合心智）；`IsActiveSelf` / `IsVisibleSelf` = 自身 flag（**只有激活机制与 Inspector 复选框可用**，见 §H8）。`IsActiveInHierarchy()` / `IsVisibleInHierarchy()` 保留为**同义显式别名**（inline 转发到 `IsActive()` / `IsVisible()`），用于让「这里关心的是级联」的代码自解释——§H6 的行为门统一写别名。

**两轴的职责分工（正交，且 `visible` 在输出上从属 `active`）：**

- **`active` = 模拟轴**：决定是否 tick、是否参与物理 / 碰撞、是否注册到系统（渲染代理**生存期**、物理 body、音频 voice、Lua 脚本、相机候选、BeginPlay）。
- **`visible` = 呈现轴**：**只**决定是否产出可见输出（绘制、阴影、UI 命中）。**不**影响 tick、**不**影响物理与碰撞。
- **组合规则（明确回答「hidden 但 active 的组件是否仍 tick / 仍参与物理」）：**
  - `active ∧ visible` → 正常：tick + 物理 + 注册 + 绘制 + 可命中。
  - `active ∧ ¬visible` → **tick 继续、物理 / 碰撞继续、音频 / 脚本继续、代理保留在 `RenderScene` 但绘制位为假**；UI **不可命中**；不画、不投影。理由：作者关掉可见性通常是想「暂时不显示」而不是「把它从世界里删掉」——UE `SetActorHiddenInGame` 同款取舍（hidden actor 仍 tick、仍碰撞）。
  - `¬active` → 全部停：组件被 `Deactivate()`，代理从 `RenderScene` 摘除（复用 `WidgetComponent.cpp:270-289` 现有路径），tick / 物理 / 脚本 / 命中 / 绘制一律不发生。此时 `visible` 取任何值都没有额外效果（I4）。

### H4 缓存与失效：三层模型（v2 措辞加固，与 §D5 一字不差对齐）

> ⚠ 本节的三层是本设计的性能契约，**三层缺一不可**，也是对外解释「是不是每次都往上查一轮」的正式答案。

**第一层 · 查询 —— 每次查询 O(1)，不遍历父链**

- `IsActive()` / `IsVisible()` 读的是**已缓存好的派生位**（`uint8` 三态：Unknown / false / true）。命中缓存就是**一次内存读 + 一次分支**返回，**不遍历父链**。
- 只有缓存处于 `Unknown`（刚被失效、尚无人读过）时，才**上溯一次**并回填沿途所有节点（路径压缩）；此后该节点回到 O(1)。这一次上溯是「失效后的首次访问」，**不是每次查询**。

**第二层 · 状态变化 —— 上溯 / 下推只发生在这里**

- 触发点只有 §H4 末尾列的 7 类：`SetActive` / `SetVisible`、挂接变更（Attach / Detach / SetRootComponent）、`AddComponent`、`SetOwner`、反序列化（`ResolvePendingActivationsForScene`）、反射直写兜底、prefab 传播。
- 那一刻做两件事：沿**子树下推失效**（把缓存位置 `Unknown`，**不碰任何 flag**）+ 对每个受影响组件做**一次差量 reconcile**（比对 `m_bAppliedEffective` 与 `IsActive()`，必要时调 `TryActivate` / `Deactivate`）。

**第三层 · 稳态每帧 —— 0 次失效、0 次上溯**

- 没有状态变化就没有失效遍历、没有下推、没有 reconcile。每个消费点每帧只做有限次**缓存读**。

> **明确禁止**：「每次查询都向上查一轮」是本设计**显式禁止**的实现（§F2）。任何在消费点写父链循环（例如 `while (parent) { if (!parent->IsActiveSelf()) return false; }`）的代码都违反 I7，无论写起来多顺手。

**方案细节**

   // GameObject（private，不序列化）
   mutable uint8_t m_EffectiveActiveCache  { kUnknown };   // 0=Unknown 1=false 2=true
   mutable uint8_t m_EffectiveVisibleCache { kUnknown };

   // Component（private，不序列化）
   mutable uint8_t m_EffectiveActiveCache  { kUnknown };
   bool            m_bAppliedEffective     { false };      // 已应用的运行时态（差量检测）

   // SceneComponent（private，不序列化）
   mutable uint8_t m_EffectiveVisibleCache { kUnknown };

- **失效**：值真的变化时，对该 GO **子树**做一次 DFS，把每个 GO 的两个缓存位、每个组件的 active 缓存、每个 `SceneComponent` 的 visible 缓存置 `Unknown`；**不触碰任何作者化字段**。
- **重算（reconcile）**：同一次 DFS 里逐组件差量：

   const bool want = component->IsActive();                // 有效态；顺带热缓存（O(depth) 一次）
   if (want != component->m_bAppliedEffective) {
       want ? component->TryActivate() : component->Deactivate();   // 复用 Component.cpp:110-146
       component->m_bAppliedEffective = want;
   }
   // visible 翻转：MarkRenderStateDirty()（SceneComponent.h:28），逐节点标子树，不依赖它级联

- **Pending 语义保留且更准确**：`m_bPendingActivation`（`Component.h:102`）继续表示「自身想开但还开不了（不在 Scene 内）」。父 GO 不活跃时组件是 **applied=false / pending=false**（有意关着，不是在等），与「等 Scene」区分开——见 §S1。

**性能上界（写进验收）：**

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| 稳态查询（任意消费点、任意树深） | **O(1)**：1 次 `uint8` 读 + 1 次分支，**不遍历父链** | 每帧 500 widget × 2 位 ≈ 1000 次读，与深度无关 |
| 失效后首次查询 | O(depth) 一次，并回填整条路径 | 之后回到 O(1) |
| `SetActive` / `SetVisible` 切换 | **O(k)**，k = 子树 GO 数 + 组件数；无堆分配、无虚调用 | 用户 / 脚本动作，不是每帧路径 |
| 稳态每帧失效遍历 / 上溯 | **0 / 0** | 没有状态变化就什么都不做 |
| 反序列化 / 挂接变更 | O(k) 一次 | 复用 `SceneManager.cpp:660-691` 的统一重算点，扩成两阶段 |

对比：朴素「每次查询沿父链上溯」在 500 widget / 深度 5 场景 ≈ 2500–4000 次指针追逐/帧（且与层级线性恶化）；本方案稳态 ≈ 1000 次缓存读/帧。切换瞬间付出一次 O(k)，**与帧率无关**。

**失效触发点全集（漏一个就是 F1 陈旧态）：**

1. `GameObject::SetActive` / `SetVisible`
2. `GameObject::AttachToParent` / `DetachFromParent` / `SetRootComponent`（`GameObject.h:121-122, 73`）——两侧子树都要失效
3. `GameObject::AddComponent_Internal`（`GameObject.h:134`）——新组件继承当前链上的有效态
4. `Component::SetOwner`（`Component.cpp:197-211`）——换 owner 即换祖先链
5. `SceneManager::ResolvePendingActivationsForScene`（`SceneManager.cpp:660-691`）——**反序列化后唯一统一口**，改两阶段：先对全场景失效（避免子先于父计算时拿到旧祖先值），再逐组件 reconcile
6. 反射直写路径：`ColliderComponent.cpp:37-38` 这类在 `PostEditChangeProperty` 里按字段名 `"m_bActive"` 兜底的地方，直写后必须触发 reconcile
7. Prefab 实例化 / 属性传播（`PrefabUtilityDetail.cpp:158` → 已有统一口）

### H5 命名与语义（v2 裁定：`IsActive()` 返回层级结果）

| 项 | 现状 / v1 | **v2 设计** | 理由 |
|----|-----------|-------------|------|
| 序列化字段名 `m_bActive` / `m_bVisible` | 唯一真源 | **完全不变**（只改访问器语义，不改字段名 / 不改序列化） | 改名会牵动 `Component.gen.cpp:9` 访问器、全部旧存档、`ColliderComponent.cpp:38` 的字符串比较；旧场景零迁移 |
| `Component::IsActive()` | 自身 flag（`Component.h:39` 直接 `return m_bActive`） | **有效态**（层级结果，缓存，O(1)） | 主人裁定：行为门占 33/58，符合「我是不是在工作」的用户心智；默认读法就是正确读法 |
| `Component::IsActiveSelf()` | 无 | **新增**：自身 flag（直读 `m_bActive`） | 「想不想开」与「现在能不能开」必须可分；**只有激活机制本体与 Inspector 复选框允许用它**（§H8） |
| `Component::IsActiveInHierarchy()` | 无 | **新增同义别名**（inline 转发 `IsActive()`） | 让关心级联的调用点自解释；§H6 行为门统一写别名，代码评审一眼可辨 |
| `SceneComponent::IsVisible()` / `IsVisibleSelf()` / `IsVisibleInHierarchy()` | 无 | 同上三件套（有效态 / 自身 / 别名） | 两轴命名对称，避免「active 一套、visible 另一套」 |
| `SetActive(bool)` / `SetVisible(bool)` | 写自身字段并触发对账（`Component.cpp:77-86`） | **语义不变**：只写自身 flag，然后触发失效 + reconcile | 写入口永远只写自己那一格（I1） |
| `SyncActivationWithActiveFlag()`（`Component.h:41-42`） | 直写 `m_bActive` 后对账 | **更名为 `ReconcileEffectiveActivation()`**，按**有效态**对账 | 名字带 ActiveFlag 会诱导人以为它读字段；只有 3 处调用（`Component.cpp:85`、`SceneManager.cpp:687`、`SceneManager.h:94`） |
| `m_bPendingActivation` / `m_bActivationApplied`（`Component.h:102-103`） | Applied / Pending | 保留，语义收窄为「自身想开但无法应用」；新增 `m_bAppliedEffective` | 见 §S1 |
| `m_bHitTestVisible`（`WidgetComponent.h:70-72,116`） | 命中测试跳过 | **保持正交**：命中门 = `IsActiveAndVisible() && IsHitTestVisible()` | 二者回答不同问题：「呈现了吗」vs「响应指针吗」 |
| Editor 组件 header 复选框（`SceneEditorInspectorSource.cpp:273`） | 读 `component->IsActive()` | **必须改读 `IsActiveSelf()`** | 否则「被祖先关掉时点一下」会把复选框显示成有效态再写回，语义错位（§F11 的首个真实受害者） |

**为什么自身 flag 用 `...Self` 后缀（而不是 `GetLocalActive()` 之类）：**

1. **业界先例唯一且贴切**：Unity 的自身开关就叫 `activeSelf`，层级结果是 `activeInHierarchy`。既然主人把「层级结果」给了 `IsActive()`，自身就只能用 `Self` 后缀——它**不需要文档就能读懂**。
2. **避开 local/world 歧义**：`GetLocalActive()` / `IsActiveLocally()` 在 `SceneComponent` 上与 Transform 的 local/world 词汇撞车（`GetWorldTransform` / `SetTransformFromSimulation` 满屏都是），会被误读成「局部空间里的 active」。
3. **与既有命名形状一致**：本仓已有 `IsHitTestVisible` / `IsFixedTickEnabled` / `HasBegunPlay` 这类「Is/Has + 形容词」读法，`IsActiveSelf` 不引入新形状。

### H6 消费点清单（逐项：位置 / 现在读什么 / 改成读什么）

> 行为门统一写**显式别名** `IsActiveInHierarchy()` / `IsVisibleInHierarchy()`（等价于 v2 的 `IsActive()` / `IsVisible()`），让「此处关心级联」在代码里自证。**任何消费点都不许写 `IsActiveSelf()`**——那是 §H8 的 (ii) 类专属。

**UI 布局**

| 位置 | 现在 | 改为 |
|------|------|------|
| `UILayoutPass.cpp:14` | `canvas.IsActive()` | `canvas.IsActiveInHierarchy()` |
| `UILayoutPass.cpp:33` | `widgets[0]->IsActive()` | `widgets[0]->IsActiveInHierarchy()` |
| `UILayoutPass.cpp:41-44` | 无条件递归子 GO | **非活跃 GO 子树直接跳过**（几何无需维护：布局由 `CanvasComponent::Tick`（`CanvasComponent.cpp:13-22`）每帧驱动，重新激活后下一帧即重建） |
| `CanvasComponent.cpp:16` | `IsActive()` | `IsActiveInHierarchy()` |

**UI 命中**

| 位置 | 现在 | 改为 |
|------|------|------|
| `ScreenUIHitTester.cpp:36` | `canvasPtr->IsActive()` | `IsActiveInHierarchy()` |
| `ScreenUIHitTester.cpp:148` | `widget->IsActive() && widget->IsHitTestVisible()` | `widget->IsActiveAndVisibleInHierarchy() && widget->IsHitTestVisible()` |
| `ScreenUIHitTester.cpp:160-163` | 无条件递归 | 非活跃 GO 子树跳过（¬active ⇒ ¬可见 ⇒ ¬命中，跳过是精确的） |
| `ButtonComponent.cpp:128` | `widget->IsActive() && widget->IsHitTestVisible()` | 同上组合 |
| `ButtonComponent.cpp:18, 122, 173` | `IsActive()` | `IsActiveInHierarchy()` |
| `DropTargetComponent.cpp:38` / `DragSourceComponent.cpp:48` | `IsActive()` | `IsActiveInHierarchy()` |
| `UISystem.cpp:410` | `target->IsActive()` | `IsActiveInHierarchy()` |

**渲染：UI 代理**

| 位置 | 现在 | 改为 |
|------|------|------|
| `WidgetComponent.cpp:348-351`（文字路径 `proxy.m_bVisible`） | 只由自身可绘性算出 | 追加 `&& IsActiveAndVisibleInHierarchy()` |
| `WidgetComponent.cpp:386-389`（图片路径） | 同上 | 同上 |
| `WidgetComponent.cpp:249-254`（`DoEndOfFrameUpdate` 早退） | `!IsActive()` | `!IsActiveInHierarchy()` |
| `WidgetComponent.cpp:265-289`（`Apply/RemoveActivationToSystems`） | 现有摘代理路径 | **保持不变**，改由 reconcile 驱动（这是级联摘除的落点） |
| `ForwardRenderer.cpp:1232` | `widgetProxy->m_bVisible` | 保留该位（快），**并加 O(1) 保险**：`widget->IsActiveAndVisibleInHierarchy()` |
| `ForwardRenderer.cpp:1244` | `canvas->IsActive()` | `IsActiveInHierarchy()` |

**渲染：3D 网格 / 光照 / 天空盒 / 阴影**

| 位置 | 现在 | 改为 |
|------|------|------|
| `RenderScene.cpp:26`（`UpdatePrimitive` 门） | `!IsActive()` | `!IsActiveInHierarchy()`（¬active ⇒ 不注册代理） |
| `RenderScene.cpp:79` / `:207` / `:255` | `!IsActive()` | `!IsActiveInHierarchy()` |
| `RenderScene.cpp:230` | `proxy->m_Enabled = skyBoxComponent->IsActive()` | `IsActiveInHierarchy()` |
| `PrimitiveComponent.cpp:36`、`LightComponent.cpp:88`、`SkyBoxComponent.cpp:53, 98` | `!IsActive()` / `IsActive()` | `IsActiveInHierarchy()` |
| `ForwardRenderer.cpp:1164-1193`（网格收集：主视 + 阴影共用同一循环） | 只查 `m_PrimitiveComponent` 非空 + 视锥 + `m_CastShadow` | **加两行守卫**（代码见下）——`mainCollector` 与 `shadowOnlyCollector` 都在这循环里喂（`:1185-1192`），**一处同时覆盖绘制与阴影** |

`ForwardRenderer.cpp:1164` 的守卫（后向指针由 `PrimitiveComponent.cpp:81` / `SpriteComponent.cpp:148` 设置）：

   for (PrimitiveSceneProxy* primitiveProxy : renderScene->m_PrimitiveSceneProxies)
   {
       if (!primitiveProxy || !primitiveProxy->m_PrimitiveComponent) { continue; }

       const PrimitiveComponent* source = primitiveProxy->m_PrimitiveComponent;
       if (!source->IsActiveInHierarchy() || !source->IsVisibleInHierarchy()) { continue; }   // ← 新增（O(1) 缓存读）

       ...  // 既有视锥 / m_CastShadow / GetMeshBatches 逻辑不变
   }

**模拟：tick / 物理 / 相机 / 脚本 / 音频**

| 位置 | 现在 | 改为 |
|------|------|------|
| `GameObject.cpp:464-473`（`Tick`） | `component->IsActive()` | `IsActiveInHierarchy()` |
| `GameObject.cpp:475-488`（`FixedTick`） | `!component->IsActive()` | `IsActiveInHierarchy()` |
| `GameObject.cpp:63-85`（`BeginPlay`） | `!component->IsActive()` | `IsActiveInHierarchy()` |
| `Scene.cpp:278-292`（`Scene::Tick` 扁平遍历全部 GO） | 只查 `IsPendingDestroy()` | 加 `!gameObject->IsActiveInHierarchy()` 子树跳过（精确：¬active 的子树必然全 ¬active），见 D8 |
| `RigidBodyComponent.cpp:37, 122`、`PhysicsSystem.cpp:161-171` | `IsActive()` | `IsActiveInHierarchy()`（**只看 active，不看 visible**） |
| `CameraComponent.cpp:101`、**`SceneEditingViewportClient.cpp:252`（Editor）** | `camera->IsActive()` | `IsActiveInHierarchy()`（未激活子树里的相机不得成为主相机 / 视口相机） |
| `LuaComponent.cpp:61` | `!IsActive()` | `!IsActiveInHierarchy()` |
| Audio（`AudioComponent` 经 `Apply/RemoveActivationToSystems` 注册 voice） | 由激活机制驱动 | **不动**——reconcile 走后同一扇门，自动跟随 active（`AudioVoice::IsActive()` 是另一个类，域外，见 §H8） |

### H7 数据流（一次父 GO 关断的完整路径）

   作者/脚本: parentGO.SetActive(false)
      │  ① m_bActive = false（只改 GO 自己的字段）
      ├─② 子树 DFS：GO 的 active/visible 缓存 = Unknown
      │             每个 Component 的 active 缓存 = Unknown
      │             每个 SceneComponent 的 visible 缓存 = Unknown
      │  ③ 同一次 DFS 逐组件差量：
      │     want = c->IsActive()                        // 有效态；上溯一次并回填，此后 O(1)
      │     want(false) != c->m_bAppliedEffective(true)
      │        → c->Deactivate()
      │             → OnDeactivate()                    （组件自己的收尾）
      │             → RemoveActivationFromSystems()     （WidgetComponent 摘代理 / 物理 body 退场 / voice 停）
      │        → c->m_bAppliedEffective = false
      └─④ visible 翻转的 SceneComponent：MarkRenderStateDirty()
           （下一帧 DoEndOfFrameUpdate 重填代理位；ForwardRenderer 侧另有 O(1) 保险）

   全过程：c->m_bActive 与 c->m_bVisible 均未被读写 ← 不变量 I1
   下一帧：所有消费点读 IsActive() / IsVisible() → 一次缓存读，O(1)，0 次上溯

---

### H8 调用点分类审计（v2 新增 · 命名反转的必付成本）

**为什么必须做**：v2 把 `IsActive()` 从「自身 flag」改成「层级结果」。凡是**不该**被级联影响的地方（激活机制本体、Inspector 复选框绑定）必须改读 `IsActiveSelf()`；凡是**应该**被级联影响的地方（行为门）保持读 `IsActive()` 即自动正确。**漏分类的两个方向都有害**：把机制改错 → 自激 / 不收敛（§F11）；把行为门漏掉 → 半级联（§F8）。

**三类定义**

| 类 | 定义 | 用什么 | 判据 |
|----|------|--------|------|
| **(i) 行为门** | 决定「要不要 tick / 画 / 跑物理 / 跑脚本 / 可命中 / 当主相机」 | **有效态**（`IsActive()` / 显式别名） | 问的是「它现在到底在不在工作」 |
| **(ii) 激活机制本体** | 决定「要不要 Apply/Remove 系统注册、是否 pending、是否记差量、复选框显示什么」 | **自身 flag**（`IsActiveSelf()`），**例外见 R-D** | 问的是「作者想不想开」 |
| **(iii) 纯诊断 / 日志** | 打印 / 报表 / 调试命令 | 保持**有效态** | 人想看的是「实际在不在跑」 |

**R-D 细分规则（机制内部再分两类，这是本审计最容易做错的地方）**

- **意图判断**（「作者想开吗」）→ 自身 flag：`SetActive` 的写入与去重比较、`ResolvePendingActivation` 的前置判断。
- **许可判断**（「现在能不能真的注册进系统」）→ **有效态**：`TryActivate` 的门、`SetOwner` 后的重试判断、reconcile 的差量基准、`RefreshPhysicsBody` 的建体前检查。
- 一句话记法：**想不想开看自己，能不能开看整棵树。**

**统计（合计 58 = Runtime 56 + Editor 2）**

| 类别 | 数量 | 明细 |
|------|------|------|
| 域外（`AudioVoice` 自身 API，与 Component 无关） | **7** | `AudioSystem.cpp:490/623/684/793/846`、`AudioVoice.cpp:127`、`AudioVoice.h:23` —— **不动** |
| 声明 / 注释 | **6** | `Component.h:38/39/41/90`、`SceneManager.cpp:685`、`ColliderComponent.cpp:37` —— 随命名改签名与注释 |
| 生成代码 | **2** | `Component.gen.h:10`、`Component.gen.cpp:9` —— 由 reflection_codegen 重新生成 |
| **(ii) 激活机制本体** | **9** | 见下表 |
| **(i) 行为门** | **32** | 见下表 |
| Editor：(ii) 1 + (i) 1 | **2** | `SceneEditorInspectorSource.cpp:273`（ii）、`SceneEditingViewportClient.cpp:252`（i） |
| **合计** | **58** | 与 §P4 一致 |

**(ii) 激活机制本体 9 处（Runtime）**

| 位置 | 判断性质 | 用 | 说明 |
|------|----------|----|------|
| `Component.cpp:77`（`SetActive` 函数体） | 写入 | Self | 只写自己那一格 |
| `Component.cpp:79`（`m_bActive == active` 去重） | 写入 | Self | 值未变直接返回（F9） |
| `Component.cpp:84`（`m_bActive = active`） | 写入 | Self | 唯一写点 |
| `Component.cpp:102`（`ResolvePendingActivation`） | **意图** | Self | 「自身想开」+ pending |
| `Component.cpp:90`（`SyncActivationWithActiveFlag` → 更名后） | **许可** | Effective | reconcile 的基准必须是有效态 |
| `Component.cpp:112`（`TryActivate` 门） | **许可** | Effective | 祖先关着时绝不 ApplyActivation |
| `Component.cpp:207`（`SetOwner` 重试） | **许可** | Effective | 新 owner 可能整棵子树关着 |
| `RigidBodyComponent.cpp:110`（`RefreshPhysicsBody` 门） | **许可** | Effective | 建物理体是系统注册行为 |
| `ColliderComponent.cpp:38`（`PostEditChangeProperty` 兜底，字符串 `"m_bActive"`） | 纯管道 | —— | 直写后触发 reconcile；字段名不变，字符串照旧 |

→ 其中**必须显式写 `IsActiveSelf()` 的 4 处**：`Component.cpp:77/79/84/102`，外加 Editor 的 `SceneEditorInspectorSource.cpp:273`（共 **5 处**）。

**(i) 行为门 32 处（Runtime）**

| 域 | 位置 | 数量 |
|----|------|------|
| UI 布局 | `UILayoutPass.cpp:14, 33` | 2 |
| UI 命中 | `ScreenUIHitTester.cpp:36, 148` | 2 |
| UI 交互 | `UISystem.cpp:410`、`ButtonComponent.cpp:18, 122, 128, 173`、`DropTargetComponent.cpp:38`、`DragSourceComponent.cpp:48` | 7 |
| UI tick | `CanvasComponent.cpp:16` | 1 |
| GO 生命周期 | `GameObject.cpp:72, 468, 479`（BeginPlay / Tick / FixedTick） | 3 |
| 渲染代理 | `RenderScene.cpp:26, 79, 207, 230, 255` | 5 |
| 渲染组件 | `PrimitiveComponent.cpp:36`、`LightComponent.cpp:88`、`SkyBoxComponent.cpp:53, 98`、`WidgetComponent.cpp:251` | 5 |
| 渲染收集 | `ForwardRenderer.cpp:1244` | 1 |
| 物理 | `PhysicsSystem.cpp:162, 168`、`RigidBodyComponent.cpp:37, 122` | 4 |
| 相机 | `CameraComponent.cpp:101` | 1 |
| 脚本 | `LuaComponent.cpp:61` | 1 |
| **小计** | | **32** |

**「会改变语义」的三张清单（这是主人要的结论）**

1. **必须改写成 `IsActiveSelf()`，否则语义被改写（5 处）**：`Component.cpp:77`、`:79`、`:84`、`:102`、`SceneEditorInspectorSource.cpp:273`。
2. **含义发生变化、且必须接受（33 处）**：上表 32 处 Runtime 行为门 + `SceneEditingViewportClient.cpp:252`。它们的**代码文本可能不变**（仍读 `IsActive()`），但含义从「自身 flag」变成「有效态」——这正是本 Feature 的目的；在**没有任何 GO 祖先被关**的世界里行为与今天完全一致（根 GO 默认全开 ⇒ 有效态 ≡ 自身 flag），所以 **S01 仍可宣称零行为变化**。
3. **新增有效态输入、无祖先关闭时输出不变（4 处）**：`Component.cpp:90/112/207`、`RigidBodyComponent.cpp:110`。

**审计做法（S05 的验收动作）**：对 Runtime 全树用 grep 比对 §H8 两张表；行为门处 `IsActive()` / 别名出现 33 次、机制处 `IsActiveSelf()` 出现 5 次；**任何第 6 处 `IsActiveSelf()` 都需要在代码评审里给出理由**。

### H9 Inspector 契约（v2 新增）

#### H9.1 specifier 核实结论：`Invisible` 够用，不新增 specifier

| 核实点 | 证据 | 结论 |
|--------|------|------|
| 能否把字段从**通用字段列表**移除 | `PropertyEditPolicy::ShouldShow`（`Editor/src/UI/Property/PropertyEditPolicy.cpp:34-40`）**首行** `if (property.HasSpecifier(PropertySpecifier::Invisible)) return false;` | ✅ 就是主人要的 |
| 移除后是否也**不可编辑** | `PropertyEditPolicy::CanEdit`（同文件 `:55-60`）直接复用 `ShouldShow` | ✅ 双重保险 |
| 能否挡住经属性路径的写入 | `PropertyPath::IsPropertyVisible` / `IsPropertyWritable`（`Editor/src/PropertyPath/PropertyPath.cpp:693-700`）同样以 `Invisible` 为否 | ✅ **单一编辑点是强制的**，不靠自觉 |
| 会不会破坏序列化 | `m_Name` / `m_Guid`（`MEObject.gen.cpp:6-7`）、`Component::m_Owner` / `m_bActive`（`Component.gen.cpp:8-9`）、`RigidBodyComponent::m_PhysicsBodyId`（`RigidBodyComponent.gen.cpp:10`）都是 `Invisible` 却照常存盘 | ✅ **不影响序列化**（全仓已有 4 处成熟用例） |
| 有没有副作用必须知道 | ① `PrefabOverrideUtility.cpp:242-258` 的 `ShouldCopyReflectedProperty` 把 `Invisible` 排除在 prefab override 传播外（仅 `m_Name` 例外，见 `:254-255` 注释与判定）；② `DebugCommandSceneUtils.cpp:113/145/402/439` 的场景 dump 会跳过 `Invisible` 字段 | ⚠ 见 §D10（推荐接受，与 `Component::m_bActive` 现状一致）；S06 若需要 dump，用显式打印补 |

**结论**：用现成的 `Invisible`，**不新增反射 specifier**（Non-goal）。若将来需要「列表里隐藏但可经 PropertyPath 写」的第三态，再议（Deferred）。

#### H9.2 呈现位置契约

| 对象 | 位置（file:line） | 控件 | 绑定 | 可写？ | 备注 |
|------|------------------|------|------|--------|------|
| **GameObject** | `DrawGameObjectDetails` 的 header 之后：现 `SceneEditorInspectorSource.cpp:141`（`PopStyleVar` 之后）/ `:145`（Selected ID 之前）之间，**新增一行** | 两个小复选框：**Active** / **Visible** | `SetActive` / `SetVisible`（写自身 flag）；显示值读 `IsActiveSelf` / `IsVisibleSelf` | 是 | **需要新增绘制函数**（见 H9.4） |
| **Component（active）** | 组件 header，`SceneEditorInspectorSource.cpp:273`（读）/ `:305-315`（复选框 + `SetActive` + `MarkSceneDirty` + undo blob，属性路径 `"m_bActive"` 在 `:278`） | 复选框 | 同上（读改成 `IsActiveSelf()`） | 是 | **已有实现，只需换读取口** |
| **Component（visible）** | 同 header 行，紧接 active 复选框之后（`:315` 之后、`:321` 的 `SameLine` 之前） | 复选框 | `SetVisible` / `IsVisibleSelf`；undo 路径 `"m_bVisible"` | 是 | **仅当 `component->IsA(SceneComponent::StaticClass())` 才画**（无实体组件没有 visible，I8 在 UI 上也成立） |
| 字段本体（4 个） | 任何通用属性列表 | **不出现** | — | 否（`Invisible`） | 单点编辑的强制手段 |
| 有效态 | header 行右侧 | **只读文本 / 徽标** | `IsActive()` / `IsVisible()` | 否 | 见 H9.3 的显示矩阵 |

#### H9.3 禁用态 / 从属态显示矩阵（F6 的落地）

| 自身 flag | 有效态 | header 复选框 | 该行样式 | 追加只读徽标 | 说明 |
|----------|--------|---------------|----------|--------------|------|
| true | true | ✅ 勾选，**可写** | 正常 | 无 | 正常工作 |
| false | false | ⬜ 未勾，**可写** | **Alpha ≈ 0.55**（复用 `:274` / `:284-287` 的既有手法） | 无 | 用户自己关的 → 保持现有观感 |
| true | false | ✅ **仍显示勾选且可写** | 正常 Alpha + 头像区左侧描边/淡色 | **`Inactive in Hierarchy`**（灰） | 被祖先关掉：**不能吞掉用户的勾选状态**（否则用户以为自己关的），且必须能一键恢复 |
| false | false | ⬜ 未勾，可写 | Alpha 0.55 | 若有祖先关闭则同样追加徽标 | 两重原因，徽标揭示「不止你自己关」 |
| — | — | PIE 中沿用现有只读行为（`:76-79` 的 "Inspecting: PIE" 提示） | — | — | 不额外引入规则 |

**R1–R7 控件规则**

- **R1 单一编辑点**：两轴只在 header 出现一次；字段本体 `Invisible`（H9.1）。任何「再在列表里露出一个」的改动都视为破坏契约。
- **R2 复选框只绑自身 flag**：读 `IsActiveSelf()`、写 `SetActive()`。**永远不绑有效态**——否则「被祖先关掉时点一下」会把「想开」写反（§F11）。
- **R3 有效态只用只读方式展示**（文本 / 徽标 / 行底色），**绝不给第二个可写控件**。
- **R4 祖先禁用不锁子开关**：父关掉时子树仍可编辑自身 flag（否则无法预置层级打开后的状态）。
- **R5 undo 语义沿用现成 blob 机制**（`SceneEditorInspectorSource.cpp:277-314`）：active 用 `"m_bActive"`、visible 用 `"m_bVisible"`；GO 侧的路径挂在 GO 对象上，SceneComponent 侧挂在组件上，二者不会互相污染。
- **R6 标脏**：任何一次切换都调 `m_SceneEditor.MarkSceneDirty()`（沿用 `:308`）。
- **R7 灰显只影响观感、不影响可写性**：Alpha 0.55 是视觉提示（同 `:286`），**不得**用 `BeginDisabled` 包住复选框。

#### H9.4 GO 侧需要新增的绘制点（已核实无现成 helper）

- 现成 helper `SceneEditorInspectorSource::DrawComponentHeaderRow`（声明与实现 `:1153-1158`）**签名收 `Component&`**，无法复用到 GO。
- GO header 目前是纯 `ImGui::Selectable(headerLabel)`（`:129-141`），带双击重命名与右键菜单——**不应把复选框塞进 Selectable**（会抢走点击 / 双击判定）。
- 因此新增 `void DrawGameObjectStatusSwitches(GameObject& gameObject);`（声明进 `SceneEditorInspectorSource` 头，实现进同 .cpp），调用点固定在 `:141` 之后、`:145` 之前；内部只画两个小复选框 + 有效态徽标，**不持有状态**、**不改文档其它部分**。
- 归属切片：**S06**（见 §切片）。

---

## 状态机或契约

### S1 状态表：GO 祖先链 × 组件自身 → 有效态

| 祖先链 GO（全部 active?） | 组件 `m_bActive` | `IsActive()`（有效态） | `m_bAppliedEffective` | `m_bPendingActivation` | tick | 代理注册 | BeginPlay |
|---|---|---|---|---|---|---|---|
| 全部 `true` | `true` | **true** | `true` | `false` | ✔ | 已注册 | ✔（若 Scene 已 BeginPlay） |
| 全部 `true` | `false` | **false** | `false` | `false` | ✘ | 已摘除 | ✘ |
| 任一 `false` | `true` | **false** | `false`（被 `Deactivate`） | **`false`（不是 pending）** | ✘ | 已摘除 | ✘ |
| 任一 `false` | `false` | **false** | `false` | `false` | ✘ | 已摘除 | ✘ |
| 返回到全部 `true` | `true`（**未被改写**） | **true** | `true`（重新应用） | `false` | ✔ | 重新注册 | ✔ |

`m_bPendingActivation` 仍为 `true` 的唯一场景：**自身 `IsActiveSelf() == true` 且有效态为 `true`，但组件还不在可应用的 Scene 内**（`Component.cpp:117-121` 的 `CanApplyActivation()` 分支，F06 §4.1 语义原样保留）。

### S2 轴组合表：`active` × `visible` → 各子系统

| active（有效） | visible（有效） | tick | 物理 / 碰撞 | 音频 / Lua | 场景代理 | UI 布局 | UI 命中 | 绘制 | 阴影 |
|---|---|---|---|---|---|---|---|---|---|
| ✔ | ✔ | ✔ | ✔ | ✔ | 注册且绘制位真 | 计算 | ✔ | ✔ | ✔（若 `m_CastShadow`） |
| ✔ | ✘ | ✔ | **✔** | ✔ | **注册但绘制位假** | 仍计算 | **✘** | ✘ | **✘** |
| ✘ | ✔ | ✘ | ✘ | ✘ | **摘除** | 子树跳过 | ✘ | ✘ | ✘ |
| ✘ | ✘ | ✘ | ✘ | ✘ | 摘除 | 子树跳过 | ✘ | ✘ | ✘ |

**「active 决定代理生存期，visible 决定代理绘制位」** 是本设计的硬规则：频繁切 visible 不产生注册 / 摘除 churn（只翻一个 bool），而切 active 一定摘代理（释放配额、退出物理与阴影）。

### S3 不变量

- **I1（核心·不写回）** 对任意组件 `c`、任意祖先 `GO` 的任意 `SetActive` / `SetVisible` 调用序列，`c.m_bActive` 与 `c.m_bVisible` 的值**不变**。形式化：对 `g` 的子树内每个组件 `c`，调用前后 `c.m_bActive` 与 `c.m_bVisible` 相等。**这是必须成为断言的唯一一条。**
- **I2（缓存可信）** 任意时刻 `IsActive()` / `IsVisible()` 的返回值 == 用 §H3 公式现场重算（`pureRecompute`）的值。
- **I3（子树必杀）** 若 `GO.IsActiveSelf() == false`，则其子树内**每个**组件的 `IsActive() == false`，且**没有任何**该子树的场景代理存在于 `RenderScene`。
- **I4（visible 从属于 active）** ¬`IsActive()` 蕴含 ¬`IsActiveAndVisible()`，此时 visible 取任何值都无额外效果。
- **I5（可逆恢复）** `GO.SetActive(false)` 后再 `SetActive(true)`，有效态恢复到切换前精确值（由 I1 保证）。
- **I6（唯一真源）** 有效态只由「自身字段 ∧ 祖先链字段」决定；`m_bAppliedEffective` 只用于差量，不参与求值。
- **I7（三层性能契约）** 稳态查询 O(1) 且不遍历父链；上溯 / 下推只发生在 §H4 的 7 个触发点；稳态每帧 0 次失效、0 次上溯。
- **I8（无实体即无 visible）** `Component` 基类不提供任何 visible 查询；`IsVisible()` / `IsVisibleSelf()` 仅存在于 `SceneComponent`（Editor 只对 `SceneComponent` 画 visible 复选框）。
- **I9（机制自洽 · v2 新增）** 激活机制本体（`TryActivate` / `Deactivate` / reconcile 差量 / pending 判定）不得因自身产生的副作用再次触发自身的状态翻转——即 reconcile 是**幂等收敛**的：连续两次 `ReconcileEffectiveActivation()`，第二次不产生任何 Apply/Remove 调用。

---

## Failure Model

| ID | 失效 | 触发 | 症状 | 检测 | 缓解 / 设计对策 |
|----|------|------|------|------|-----------------|
| **F1** | **漏失效 → 状态陈旧** | 新增改祖先状态的路径没调失效（直写 `m_Parent`、prefab 换挂、编辑期直改字段） | 父已关，子仍 tick / 仍被画 / 仍可命中；或反之 | 差分测试（I2）；「代理清单 == 有效活跃组件集合」集合断言（I3） | 失效触发点收敛到 §H4 的 7 个；`m_Parent` / `m_Owner` **只**经 `AttachToParent` / `SetOwner` 修改；反序列化走两阶段 |
| **F2** | **递归上溯的性能悬崖** | 有人在消费点写父链循环逐个查自身 flag | UI 树一深帧时间线性涨 | 评审规则：`ForwardRenderer` / `UILayoutPass` / `ScreenUIHitTester` / `PhysicsSystem` 内不得出现父链循环（I7） | 契约层只允许调 `IsActive()`（缓存）；公式实现**唯一**放在 `GameObject` / `Component` |
| **F3** | **隐藏子树后代理未摘除** | 只改 visible 忘改 active；或 reconcile 漏某类代理 | 隐藏面板仍占绘制队列、隐形物件仍投影 | I3 集合断言；`ScreenUIQueue` 中不出现该子树 `StableOrder` | 摘除复用 `RemoveActivationFromSystems`（`WidgetComponent.cpp:270-289` 是模板），四类代理逐类补门（`RenderScene.cpp:26/79/207/255`） |
| **F4** | **序列化缺省值导致旧场景行为变化** | 新字段默认值取错 | 旧场景加载后一片空白 | 加载既有示例场景 + 往返测试（O6） | 三个新字段**全部默认 `true`**；`WidgetComponent` 不加字段；存档无需重写 |
| **F5** | **反序列化时机错位 → 祖先链还是空的** | 组件先于 GO 父子链重建被 reconcile | 「本该关着」的子树活跃 | S02 断言：载入「父 GO active=false」场景后立刻查子组件 | 两阶段：先全场景失效（不求值）→ 再逐组件 reconcile；复用 `SceneManager.cpp:660-691` |
| **F6** | **Inspector 显示歧义** | 组件面板 `m_bActive` 勾着却不工作（父关着） | 用户以为是 bug，反复点复选框 | 目视（O9）+ S06 | §H9.3 显示矩阵：复选框绑自身 flag + 只读徽标 `Inactive in Hierarchy`；灰显不用 BeginDisabled |
| **F7** | **visible 被误当 active 用** | 想用 `SetVisible(false)`「停掉」逻辑组件 | 逻辑继续跑；或编译不过（更好） | 编译期 | visible 只在 `SceneComponent`（§H2） |
| **F8** | **半级联：行为门漏改** | 迁移不彻底 | 「父关了，但 X 还在跑」逐个出现 | S05 逐项核对 §H6 + §H8（行为门 33 处） | 按 S03/S04/S05 分批清零 |
| **F9** | **重复切换开销** | 脚本每帧 `SetActive(true)`（值没变） | 若仍下推失效 → 每帧 O(k) | 性能测试 | `SetActive` / `SetVisible` 首行值比较（同 `Component.cpp:79-82`），零成本返回 |
| **F10** | **`Deactivate` 重入 / 析构期 reconcile** | 子树在 reconcile 中被销毁（`GameObject.cpp:55-59`） | 悬垂指针 / 崩溃 | ASAN（`scripts/debug/build_asan.ps1`） | 遍历先取快照或持 `shared_ptr`；`Deactivate()` 已幂等（`Component.cpp:136-146`） |
| **F11** | **漏分类导致激活机制自身被级联影响（自激 / 不收敛）** — **v2 新增，D6 反转的直接风险** | ① 把 `TryActivate` / `SetOwner` / reconcile 基准误写成 `IsActiveSelf()`（祖先关着仍注册进系统，随后又被 reconcile 拆除 → 抖动）；② 把 `ResolvePendingActivation` / Inspector 复选框误写成 `IsActive()`（父关时去激活，父开时丢掉用户意图 → 状态漂移）；③ 在 `OnActivate` / `ApplyActivationToSystems` 里改 flag 并再次触发 reconcile（递归） | 组件在「已应用 / 未应用」间每帧抖动；日志刷屏；物理体反复创建销毁；Inspector 勾选跳变；极端情况栈溢出 | O8（收敛测试）：连续两次 reconcile 第二次零 Apply/Remove（I9）；计数 `ApplyActivationToSystems` 调用次数在稳态为 0 | §H8 分类表就是防线：**(i)=有效态、(ii) 意图=Self、(ii) 许可=有效态**；机制内**禁止**递归触发 reconcile（改为置脏，由当前 DFS 统一处理）；O8 常驻 |
| **F12** | **`Invisible` 的副作用被忽略** — **v2 新增** | 以为「Invisible 只是不看」，忘了它同时影响 prefab override 传播与 debug dump | ① prefab 改了 GO 开关不推给实例（验收方以为 bug）；② 场景 dump 里看不到两轴（排查时以为字段没存） | `PrefabOverrideUtility.cpp:242-258` 的判定；dump 命令输出比对 | §D10 明确接受（与 `Component::m_bActive` 现状一致）；S06 在 dump 里显式补打印两轴；文档写清 |

---

## Verification

### V1 可观察 oracle（每条可脚本化，不需要编辑器；O9 为目视项）

| ID | 场景 | 断言（精确到字段值） |
|----|------|--------------------------|
| **O1** | **核心不变量 I1**：某子 widget 作者化 `SetActive(false)`，再 root `SetActive(false)` → `SetActive(true)` | 全程 `widget.IsActiveSelf() == false`（**自身字段未变**）；另一 `IsActiveSelf() == true` 的兄弟组件三个时刻恒为 `true`；其 `IsActive()` 只在中间时刻为 `false` |
| **O2** | **级联必杀 I3**：三层 GO（root→mid→leaf）各挂 `WidgetComponent`，mid `SetActive(false)` | mid / leaf 的 widget `IsActive() == false`；`RenderScene` widget 代理数 3→1；两者 `Tick` 计数在后续 N 帧**增量为 0**；命中测试在该 widget `GetComputedRect()` 中心点返回 `nullptr` |
| **O3** | **恢复保真 I5**：接 O2，mid `SetActive(true)` | 三者 `IsActive() == true`；代理数恢复 3；mid 的 widget `IsActiveSelf()` 仍为 `true`（未被写过） |
| **O4** | **缓存可信 I2（差分）**：固定种子随机操作序列（SetActive / SetVisible / AttachToParent / DetachFromParent / AddComponent 各 100 次） | 每步后对**每个节点**断言 `IsActive() == pureRecompute(c)`（`pureRecompute` 是 §H3 公式的朴素递归实现，作为 spec） |
| **O5** | **visible 轴**：`SetVisible(false)`（GO 级 / 组件级各一次） | `IsActiveAndVisible() == false`；`ScreenUIQueue` 中该 `StableOrder` 绘制命令数 **0**；同组件 `Tick` 计数**继续增长**（visible 不影响模拟）；命中不返回该 widget；`IsVisible() == false` 时 `mainCollector` / `shadowOnlyCollector` 均收不到该 proxy 的 mesh batch（阴影一并消失） |
| **O6** | **序列化 / prefab**：含「根 GO active=false」的场景存盘→读盘；同 GO 做 prefab 实例化 | 读盘后 `root.IsActiveSelf() == false` 且子组件 `IsActive() == false`（F5）；`m_bVisible` 往返保真；prefab 实例化按存盘值生效（override 传播见 D10，不作断言） |
| **O7** | **无实体即无 visible**：`LuaComponent` / `InputComponent` / `RigidBodyComponent` 上不存在 `SetVisible` / `IsVisible` | 代码评审 + 接口清单断言（不引入编译失败测试） |
| **O8** | **机制收敛 / 自激（I9、F11）**：随机 toggle 序列后在**静止状态**连续调两次 `ReconcileEffectiveActivation()`；再跑 60 帧 | 第二次调用的 `ApplyActivationToSystems` / `RemoveActivationFromSystems` 调用计数 **0**；60 帧内（无人改状态）上述计数 **0**；每个组件的 `m_bAppliedEffective` 与 `IsActive()` 始终一致 |
| **O9** | **Inspector 契约（目视，§H9）** | ① GO header 有 Active / Visible 两个开关；复选框显示的是**自身 flag**（祖先关掉时仍保持勾选）+ 只读徽标 `Inactive in Hierarchy`；② Component header 有 Active / Visible 两个开关，且 **Visible 只在 SceneComponent 上出现**；③ 通用字段列表里**看不到**这四个字段（`Invisible` 生效）；④ 复选框灰显时不使用 BeginDisabled（被祖先关掉仍可点，点了立刻生效）；⑤ undo 一次能正确回退（R5） |

### V2 最小自动化路径

- **单元层**：新增 doctest 套件 `minEngine/Tests/Suites/HierarchyActivityTest`（配套 .h），注册进 `minEngine/Tests/TestSuiteRegistration.cpp`（照当前注册块 12–45 行的模式）。
  - 夹具照抄 `GameObjectHierarchyTest.cpp:19-40` 的 `GameObjectHierarchyTestScope`（`Testing::TestAccess<ObjectManager>` / `Testing::TestAccess<SceneManager>` 注入 + Initialize/Shutdown）。
  - O1 / O2 / O3 / O5 / O8 各一个 `TEST_CASE`；O4 用固定种子 `std::mt19937` 驱动，`pureRecompute` 与缓存查询各写一份（**差分测试是本设计最重要的自动化产出**）。
- **回归层**：必须保持绿的既有套件——`UILayoutTest`、`ScreenUIHitTest`、`ScreenUIButtonTest`、`ScreenUIPointerTest`、`BeginPlayTest`、`GameObjectHierarchyTest`、`SceneCloneTest`、`PrefabOverridesTest`、`PhysicsSmokeTest`、`AudioSmokeTest`、`PlayMainCameraTest`、`LuaScriptMvpTest`。
- **全量入口**：`scripts/verify.ps1`（本仓全量 smoke 入口；脚本第 6–8 行把 BuildDir / BinDir 指向 minEngine/build 与 minEngine/bin）。每个切片结束整包绿。
- **审计层（v2 新增）**：S05 结束时用 §H8 的两张表核对 grep 结果（行为门 33、机制 Self 5），把差异贴进 Implementation 文档。
- **明确不做**：不在本 Feature 里启动编辑器做自动化目视（会触发 .meta 迁移噪声）；O9 属**主人目视确认项**（与 F06 DESIGN 第 476 行同等性质）。

### V3 为什么「自身字段未变」必须是断言

I1 是本设计唯一不可让渡的语义。只写在文档里，实现者随时会用「顺手写回一下更快」绕过它（§P5 描述的破坏路径）。O1 用**三时刻字段值比较**把它钉住：任何写回实现立刻红。同理 O8 钉住 I9：任何让机制自激的实现都会在「稳态 0 次 Apply/Remove」上立刻红。

---

## 兼容与迁移

### C1 缺省值、specifier 与旧场景行为

| 字段 | specifier | 缺省 | 旧场景（无此字段）读入后 | 行为变化 |
|------|-----------|------|--------------------------|----------|
| `GameObject::m_bActive` | `Invisible` | `true` | `true` | **无**（今天 GO 没有活跃性门，等价于恒 true） |
| `GameObject::m_bVisible` | `Invisible` | `true` | `true` | **无** |
| `SceneComponent::m_bVisible` | `Invisible` | `true` | `true` | **无**（`proxy.m_bVisible` 计算里多一个恒真项） |
| `Component::m_bActive` | `Invisible`（现状） | `true` | 现状 | **无**（字段名与序列化名不变，只有访问器语义变） |

**结论**：对既有场景 / prefab / 示例资产**零行为变化**；不改任何存档数据，不引入迁移层。

**唯一需要主动知道的兼容面（v2）**：`Invisible` 使这两轴**不参与 prefab override 传播**（`PrefabOverrideUtility.cpp:242-258`）——与 `Component::m_bActive` 今天的行为一致，见 §D10。

### C2 需要直接改动的旧文件清单

**Runtime（语义层 + 消费面）**

| 文件 | 改什么 | 切片 |
|------|--------|------|
| `Runtime/Function/Framework/GameObject/GameObject.h` | 新增 `m_bActive` / `m_bVisible`（`ME_PROPERTY(Invisible)`）、`SetActive/SetVisible`、`IsActive` / `IsVisible`（有效态）、`IsActiveSelf` / `IsVisibleSelf`（自身）、别名、缓存位、失效与 reconcile 声明 | S01 |
| `.../GameObject/GameObject.cpp` | 实现；挂接变更处失效；`Tick/FixedTick/BeginPlay` 改有效态 | S01 / S05 |
| `.../Components/Component.h` | `IsActive()` 语义改为有效态；新增 `IsActiveSelf()`、`IsActiveInHierarchy()` 别名、缓存位、`m_bAppliedEffective`；`SyncActivationWithActiveFlag → ReconcileEffectiveActivation` | S01 / S02 |
| `.../Components/Component.cpp` | 按 §H8 逐处分类：77/79/84/102 → Self；90/112/207 → Effective | S02 |
| `.../Components/SceneComponent.h/.cpp` | `+m_bVisible`（`Invisible`）与 `IsVisible/IsVisibleSelf/IsVisibleInHierarchy/IsActiveAndVisible`；visible 翻转 → `MarkRenderStateDirty` | S01 / S02 |
| `.../Components/WidgetComponent.h` | **不加字段**；注释说明 visible 继承自 SceneComponent、`m_bHitTestVisible` 正交 | S01 |
| `.../Components/WidgetComponent.cpp` | `FillSceneProxy` 两处 `m_bVisible`（348–351 / 386–389）；`DoEndOfFrameUpdate`（251） | S04 |
| `Runtime/Function/Render/RenderScene.cpp` | 26 / 79 / 207 / 230 / 255 | S04 |
| `.../RenderPipeline/ForwardRenderer.cpp` | 1164–1193 网格+阴影守卫；1232 widget O(1) 守卫；1244 canvas | S04 |
| `.../Components/PrimitiveComponent.cpp` / `LightComponent.cpp` / `SkyBoxComponent.cpp` | 36 / 88 / 53、98 | S04 |
| `Runtime/Function/UI/UILayoutPass.cpp` | 14 / 33 / 41–44 | S03 |
| `Runtime/Function/UI/ScreenUIHitTester.cpp` | 36 / 148 / 160–163 | S03 |
| `.../Components/ButtonComponent.cpp` | 18 / 122 / 128 / 173 | S03 |
| `Runtime/Function/UI/UISystem.cpp` | 410 | S03 |
| `.../Components/DropTargetComponent.cpp` / `DragSourceComponent.cpp` | 38 / 48 | S03 |
| `.../Scene/Scene.cpp` | 278–292 子树跳过 | S05 |
| `.../Components/CanvasComponent.cpp` / `CameraComponent.cpp` / `LuaComponent.cpp` | 16 / 101 / 61 | S05 |
| `.../Physics/RigidBodyComponent.cpp` / `PhysicsSystem.cpp` | 37、110(Self/Effective 见 §H8)、122 / 161–171 | S05 |
| `.../Scene/SceneManager.h/.cpp` | 660–691 两阶段；94 声明更名 | S02 |
| `.../Physics/ColliderComponent.cpp` | 37–38：直写 `"m_bActive"` 后补 reconcile（字符串名不变） | S02 |
| `Generated/Reflection` 下 `GameObject.gen.*` / `SceneComponent.gen.*` / `Component.gen.*` | 由 CMake `reflection_codegen` 重新生成（不手改） | S01 |

**Editor（v2 新增一列）**

| 文件 | 改什么 | 切片 |
|------|--------|------|
| `Editor/src/SubEditor/Scene/SceneEditorInspectorSource.cpp` | ① `:273` 改读 `IsActiveSelf()`；② `:305-315` 之后新增 SceneComponent 的 visible 复选框（undo 路径 `"m_bVisible"`）；③ 新增 `DrawGameObjectStatusSwitches(GameObject&)` 并在 `:141` 后调用；④ 徽标 `Inactive in Hierarchy` / `Hidden in Hierarchy` | S06 |
| `Editor/src/SubEditor/Scene/SceneEditorInspectorSource.h` | 声明 `DrawGameObjectStatusSwitches` | S06 |
| `Editor/src/SubEditor/Scene/SceneEditingViewportClient.cpp` | `:252` 视口相机判定改有效态（代码文本可不变，含义随 `IsActive()` 改变） | S05 |
| `Editor/src/DebugCommand/DebugCommandSceneUtils.cpp`（可选） | 若需在 dump 里看到两轴，显式打印（`Invisible` 会让通用 dump 跳过它们，见 §F12） | S06 |
| `minEngine/src/Runtime/Function/Framework/Prefab/PrefabOverrideUtility.cpp` | **仅当 D10 选 (b)**：把两字段加入 `:255` 的例外名单（1 行，同 `m_Name` 先例） | S06（视裁定） |
| `minEngine/Tests/TestSuiteRegistration.cpp` + 新增 `Tests/Suites/HierarchyActivityTest` | 注册新套件 | S01 起 |

**不需要改**：任何场景 / prefab / 资产数据；`FEATURE_REGISTRY.md`（协调者维护）；`bugs/*`；其它 Feature 的设计文档。

---

## 切片

每片结束构建与测试必须绿，可单独回退。

| 片 | 内容 | 验收 | 可独立验收 |
|----|------|------|-----------|
| **S01 字段与契约** | 四个字段（三个新增，全部 `Invisible`）、`IsActive/IsVisible/IsActiveSelf/IsVisibleSelf/IsActiveInHierarchy/IsVisibleInHierarchy/IsActiveAndVisible`、缓存位与失效骨架、reconcile 调用点先接空；反射重新生成 | 编译绿；O4 纯函数部分绿；O7 成立；**无消费点被改 → 行为零变化**（verify.ps1 全绿） | ✅ |
| **S02 reconcile 传播** | `TryActivate/Deactivate` 许可门改有效态；`ReconcileEffectiveActivation` 更名与实现；挂接 / `SetOwner` / `AddComponent` / 反序列化两阶段；`ColliderComponent` 直写兜底；**§H8 的 5 处 Self 落位** | O1（**I1 字段不变**）、O2 代理数、O3、O5 visible 位、O6、**O8（收敛）** 绿 | ✅（设计证明点：不写回也能级联） |
| **S03 UI 消费面** | `UILayoutPass`、`ScreenUIHitTester`、`ButtonComponent`、`UISystem`、`DragSource/DropTarget` | O2 的「命中归零 / 子树布局跳过」；4 个 UI 套件绿 | ✅ |
| **S04 渲染消费面** | `WidgetComponent::FillSceneProxy`、`RenderScene` 五门、`ForwardRenderer` 两处守卫、3D 三组件 | O5 的「ScreenUIQueue 无该命令 / 网格与阴影 batch 为 0」；`MeshDrawSortTest` / Sprite 相关套件绿 | ✅ |
| **S05 模拟消费面 + 审计** | `GameObject::Tick/FixedTick/BeginPlay`、`Scene::Tick` 子树跳过、Canvas / Camera / Lua / RigidBody / PhysicsSystem、`SceneEditingViewportClient.cpp:252`；**执行 §H8 审计并把结果贴进 Implementation 文档** | O2 的「tick 增量为 0」；BeginPlay / PhysicsSmoke / AudioSmoke / PlayMainCamera / LuaScriptMvp 绿；**审计数字与 §H8 表一致（行为门 33、机制 Self 5）** | ✅ |
| **S06 Inspector 契约（v2 扩写）** | **S06a** GO header 两开关：新增 `DrawGameObjectStatusSwitches` + 调用点（`:141` 后），Active/Visible 绑自身 flag、标脏、undo；**S06b** Component header 扩写：`:273` 改 `IsActiveSelf()`、新增 SceneComponent 专属 visible 复选框、只读徽标 `Inactive in Hierarchy` / `Hidden in Hierarchy`、§H9.3 显示矩阵（灰显不用 BeginDisabled）；**S06c** 一致性验证：确认四字段不出现在通用列表（`Invisible` 生效）、单一编辑点成立、必要时补 dump 打印；若 D10 选 (b) 则同片改 `PrefabOverrideUtility.cpp:255` | **O9 目视五项**（主人确认）+ 现有 Inspector/Property 相关套件绿 + 无编译期需要新增 specifier | ✅（目视项） |
| **S07 收尾** | O4 / O8 差分与收敛测试转常驻；审计清单入 Implementation 文档；F06 交叉引用更新；Registry 状态由协调者改 Done | verify.ps1 全绿 + 文档更新 | ✅ |

**顺序理由**：S01 不改行为 → 零风险；S02 是设计真伪判定（I1 + 级联 + 收敛同时成立）；S03–S05 按「用户可见 → 底层」推进且每片有独立 oracle；**S06 单列成编辑器契约片**（v2）：它是本次修订的新增要求，且完全不碰运行时语义，可独立评审 / 独立回退。

---

## Deferred

| 项 | 为什么现在不做 | 什么情况下重新考虑 |
|----|----------------|--------------------|
| 多轴模拟开关（`bSimulatePhysics` / `bAffectsNavigation` / `bTickEnabled`） | F06 §2.6 已列为「将来」；再多一轴状态表组合爆炸 | 出现「隐藏但仍要停物理」的真实需求 |
| 延迟 / 帧首批量 flush 的传播队列 | 同步 push-down 已够（§H4 成本表）；队列化引入一帧延迟的语义模糊 | 剖面显示短间隔大量 SetActive 成为热点 |
| UI CanvasGroup 式聚合 alpha / 交互屏蔽 | 属呈现聚合，不是 active/visible 语义 | 需要淡入淡出整块 UI |
| 逐视口 / 逐相机可见性、距离剔除、遮挡剔除 | 属 culling，不属作者化语义 | split-screen / 大世界 |
| 组件级 visible 影响碰撞（`bVisibleAffectsCollision`） | 会把两轴重新纠缠（D3） | 出现「隐形但可穿过」的冲突需求 |
| `Component` 基类的 visible | 呈现由兄弟 WidgetComponent 组装，字段会撒谎 | TextComponent 改为独立渲染单元 |
| 网络 / 存档对**有效态**的复制 | 派生量，复制会造出两个真源 | 确定性回放需求（届时复制节点字段） |
| 「隐藏于列表但可经 PropertyPath 写」的新 specifier | 本 Feature 不需要（`Invisible` 够用，H9.1） | 出现「调试命令要能改、面板不该显示」的字段 |
| GO 两轴进场景 debug dump | `Invisible` 会让通用 dump 跳过；S06 只做必要补丁 | 排查流程要求 dump 完整 |

---

## 假设清单（无需审批，直接按此执行）

- **A1** `m_bHitTestVisible` 保持正交、不改序列化：命中门 = `IsActiveAndVisible() && IsHitTestVisible()`。无真实权衡。
- **A2（v2 修订）** 新字段 specifier 用 `Invisible` + header 控件，**不用** `EditAnywhere`（那会同时出现在列表里 → 违反单点编辑）。**其后果**（prefab override 不传播）由 §D10 裁定。
- **A3** 新字段缺省全 `true` ⇒ 旧场景零行为变化（§C1）。
- **A4（v2 修订）** `IsActive()` 语义改为有效态（D6 裁定）；`IsActiveSelf()` 为自身 flag 的唯一入口，**全仓只允许 5 处使用**（§H8 清单），第 6 处需评审理由。
- **A5** `Generated/Reflection` 下 .gen 文件由 CMake reflection_codegen 重新生成，不手改。
- **A6** 本 Feature 不做编辑器自动化验证；O9 的目视确认由主人执行。
- **A7（v2 新增）** GO header 的两个开关**新增绘制函数**（无现成 helper，H9.4）；不复用 `DrawComponentHeaderRow`（签名不符），不改 `Selectable` 的命中逻辑。

---

## 决策清单（最终状态）

> 状态图例：**已批准** = 主人已裁定且与本人推荐一致；**已裁定** = 主人裁定的结果（可能与 v1 推荐不同）。

### D1 · `m_bVisible` 的落点基类 —— ✅ **主人已批准：(a) `SceneComponent`**
- 选项：(a) `SceneComponent`；(b) `Component` 基类；(c) 只给 WidgetComponent；(d) 每类自加。
- 理由：`RenderScene` 四个代理入口（26/79/207/253）形参全是 `SceneComponent` 派生类；失效原语 `MarkRenderStateDirty`（`SceneComponent.h:28`）就在这一层；`Component` 直系 13 类至少 7 类无实体。
- 推翻条件：要求非空间组件也能被「隐藏」——那需要另一条轴，不是 `m_bVisible`。

### D2 · GO 的两轴都作为祖先链项 —— ✅ **主人已批准（原推荐 (a)）**
- **明确无歧义的表述**：`GameObject::m_bActive` 与 `m_bVisible` **两者都**沿祖先链求与并作用于**整棵子树**——
  - `GO::IsActive = IsActiveSelf ∧ Parent->IsActive`（active 同样上溯，不只 visible）
  - `GO::IsVisible = IsVisibleSelf ∧ Parent->IsVisible`
  - 组件侧再由自身 flag 与之相与（§H3）。
- 理由：主人约束「GO 上的 visible 是方便用的 flag」，只有下传整棵子树才名副其实；active 与之同构，避免「父关 active 只停了自身」的荒谬语义。
- 推翻条件：要求「父停用但子单独启用」（非单调语义）——那会推翻 I3，需重新设计。

### D3 · `visible` 是否影响模拟 —— ✅ **主人已批准：(a) 纯呈现轴**
- hidden 但 active 仍 tick、仍物理、仍音频 / 脚本（UE `SetActorHiddenInGame` 同款）。
- 推翻条件：产品要「隐藏即彻底休眠」→ (b)，须接受隐形穿模，并改 §S2 与物理侧说明。

### D4 · UI 隐藏节点的布局与命中 —— ✅ **主人已批准：(a) 布局仍算、命中断**
- 并额外规定 ¬active 的 GO 子树连布局都跳过（§H6）。
- 推翻条件：性能验收显示隐藏面板布局成本显著 → (b)，接受取消隐藏后一帧锚点跳变。

### D5 · 级联传播时机 —— ✅ **主人已批准：(a) 同步 push-down + 差量 reconcile**（措辞已按 v2 加固为三层，见 §H4 与本题）
- **本决策的正文（三层，与 §H4 逐字对齐）**：
  1. **查询**：`IsActive()` / `IsVisible()` 读**缓存**的派生位，**O(1)**，**不遍历父链**（仅在缓存为 Unknown 的首次访问时上溯一次并回填）。
  2. **状态变化**：上溯 / 下推**只**发生在状态变化时（`SetActive` / `SetVisible` / 挂接变更 / `SetOwner` / 反序列化 / 反射直写 / prefab 传播）——那一刻沿子树下推失效，并做**一次**差量 reconcile。
  3. **稳态每帧**：**0 次失效、0 次上溯**。
- **「每次查询都向上查一轮」是本设计显式禁止的实现**（§F2、I7）。
- 推翻条件：剖面证明短间隔大量 SetActive 造成卡顿 → (b) 帧首 flush，需把「延迟一帧生效」写进契约。

### D6 · 命名 —— ✅ **主人已裁定（反转 v1）：`IsActive()` / `IsVisible()` 返回层级结果**
- 裁定内容：`IsActive` / `IsVisible` = **有效态**；自身 flag 用 `IsActiveSelf` / `IsVisibleSelf`（新增）；`IsActiveInHierarchy` / `IsVisibleInHierarchy` 保留为同义显式别名（§H5 给出 Self 命名的三条理由）。
- **序列化字段名不变**（只改访问器语义），旧场景不受影响。
- 必付成本：§H8 调用点分类审计（58 处；5 处必须改 Self、33 处含义变化、4 处新增有效态输入），以及 §F11 自激风险与 O8 收敛测试。
- 推翻条件：若审计发现「机制自洽」无法在不引入 Self 的前提下保持（即出现第三种必须区分「意图/许可/有效」的场景），则需再引入第三层命名，届时重开本决策。

### D7 · `visible=false` 时代理去留 —— ✅ **主人已批准：(a) 保留代理只翻绘制位**
- 推翻条件：上万隐形 widget 的遍历成本成问题 → (b)，需重测 `ForwardRenderer.cpp:1230` 循环长度。

### D8 · `Scene::Tick` 子树跳过 —— ✅ **主人已批准：(a) 跳过**
- 精确性由 I3 保证；推翻条件：允许非单调语义（= 推翻 I3）。

### D9 · 相机与光照的 visible —— ✅ **主人已批准：(a) 光照/天空盒 visible=false 不贡献光照与阴影；相机忽略 visible（主相机只看 active）**
- 推翻条件：要做多相机靠 visible 切换 → (b)，并把主相机判定改 `IsActiveAndVisible()`。

### D10 · `Invisible` 是否同时从 prefab override 传播中免除 —— ⏳ **仍待定（v2 新增）**
- 背景（已核实）：`PrefabOverrideUtility.cpp:242-258` 的 `ShouldCopyReflectedProperty` 把 `Invisible` 排除在传播外，仅 `m_Name` 例外（`:254-255`）。因此四字段一旦 `Invisible`，**GO 与组件的开关变化不会被 override 传播**；而 `Component::m_bActive` 今天本就如此。
- 选项：
  - **(a) 接受（推荐）**：两轴是「实例级开关」，与现有 `Component::m_bActive` 行为一致；prefab 里存的值在实例化时按存盘值生效，之后的 prefab 改动不推开关。零额外代码。
  - **(b) 加入例外名单**：像 `m_Name` 那样在 `:255` 加两个字段名（1 行），使开关可被 override 传播 / 可 revert。
- **推荐 (a)** 理由：开关本质是实例状态（「这个实例先关掉」），自动传播容易批量误关；且与组件现状一致，避免「GO 传播、组件不传播」的不对称。
- 推翻条件：主人要求「prefab 里改 active/visible 要能推到所有实例」或「实例上关掉要能 revert 回 prefab 值」→ 选 (b)，成本 1 行 + 1 个回归用例（`PrefabOverridesTest`）。

---

## 变更记录

| 版本 | 日期 | 说明 |
|------|------|------|
| v1 | 2026-02-14 | 首版 Design Spec（仅文档）。依据：Component 激活机制现状、GameObject 无字段现状、UI 布局 / 命中 / 渲染收集三处「只查自身却无条件递归」实证，以及主人五条约束（不写回、visible 落合适基类、GO visible 为便利 flag、允许直改旧字段、需审批后才实现）。 |
| **v3** | 2026-02-15 | **状态由 `Draft v2` 更新为 `Implemented`**：S01–S07 全部落地（字段与有效态契约 / 缓存与传播 / 六处消费面 / Inspector 两轴契约 / 审计收口）。本行只记状态与实现记录索引，设计正文未改；实现侧与本文的差异逐条列在 `CORE-F43_S06_S07_IMPLEMENTATION.md` §5 与 `CORE-F43_S03_S05_IMPLEMENTATION.md` §7。 |
| **v2** | 2026-02-14 | 吸收主人审阅的 4 条修订：**① Inspector 契约（新 §H9）**——两轴做成 GO header / Component header 特殊开关、字段本体用 `Invisible` 从通用列表移除（已核实语义与副作用）、禁用态与只读有效态显示矩阵、GO 侧需新增绘制函数；S06 扩写为编辑器契约片（S06a/b/c）。**② D2 裁定**：(a)，并明确 **active 与 visible 都**沿祖先链求与（§H3 公式与 D2 正文）。**③ D5 措辞加固**：拆成「查询 O(1) / 状态变化时才上溯下推 / 稳态 0 次」三层，§H4 与 D5 各写一遍，并明确禁止「每次查询向上查一轮」。**④ D6 反转**：`IsActive()` / `IsVisible()` 返回层级结果、自身 flag 改名为 `IsActiveSelf()` / `IsVisibleSelf()`（给理由），新增 **§H8 调用点分类审计**（58 处：域外 7 / 声明 6 / 生成代码 2 / 机制 9 / 行为门 32 / Editor 2；必须改 Self 5 处、含义变化 33 处、新增有效态输入 4 处），新增 **F11 自激 / 不收敛**、**F12 Invisible 副作用**、**I9 机制自洽不变量**、**O8 收敛 oracle**、**O9 Inspector 目视 oracle**、**D10 prefab override 传播**。其余 D1/D3/D4/D7/D8/D9 标注为**主人已批准**。 |
