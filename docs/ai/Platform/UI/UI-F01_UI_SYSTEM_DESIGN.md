# UI-F01 — UI System

## Meta
- **ID:** `UI-F01`
- **Type:** Feature
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-04
- **Branch:** `feat/ui`
- **Related:**
  - [RND-F16](../../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)（ScreenUI Pass / `WidgetComponent` 基础）
  - **前置硬依赖：** [CORE-F08 GameObject Hierarchy](../Core/CORE-F08_GAMEOBJECT_HIERARCHY_DESIGN.md)（UI Tree = GO 父子）
  - **编辑器配合：** [ED-F05](../../Editor/ED-F05_HIERARCHY_TREE_DESIGN.md)（树形 Hierarchy + 拖拽改父）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - 外部底稿：[minEngine_ui_mvp_suggestions.md](../../../external/minEngine_ui_mvp_suggestions.md)
- **Depends on:** `RND-F16` Path B Done；**`CORE-F08` MVP**（GO 父子可用）
- **Blocks:** 完整 Layout 树、Hit-test、后续 Widget 样式组件
- **Implementation:** （待 Design Planned 后写）

## TL;DR

在已有 ScreenUI（`WidgetComponent` + Path B）之上，建立 **Canvas + 极简 Layout（在 Widget 上）+ Image**。  
**`WidgetComponent` = ScreenUI 基础节点**：几何 / 序 / 可见 / **Layout 意图（Anchor·Margin·Size）**；不承载贴图等视觉样式。  
**`ImageComponent` = 第一款具体视觉**（Texture + Tint）。  
UI Tree = **GO 父子**（CORE-F08）。**Blocked until CORE-F08 MVP.**

## Scope

### In（本 Feature MVP）
- `CanvasComponent`（ScreenSpace only）：参考分辨率、视口适配、子树 UI 根
- **Layout 做在 `WidgetComponent` 上**（不另拆 LayoutComponent）：固定 Size + Anchor + Margin → Computed Rect
- `ImageComponent`：Texture × Color，几何来自同 GO 的 Widget
- 与 `CORE-F08`：Canvas 子树 = GO 父子遍历

### Out（本 Feature）
- Text / Button / Panel 专用组件（后续）
- 完整 Flex/Grid/CSS
- Hit-test / Focus / Input Routing（可另切片或 UI-F02）
- World Space Canvas（Path C）
- Editor ImGui；第二套 Object/Lua Runtime
- GO 父子本身（属 **CORE-F08**）

## Reader quick start
1. §3 组件分层与数据流  
2. §8 与 CORE-F08 的依赖  
3. RND-F16 §10（ScreenUI）  
4. 外部底稿 Level 2（Tree + Layout）

---

## 1) 背景与目标

**Pain：** Path B 已能画单个 Widget，但没有 UI 根、没有相对布局、没有「图片」语义；手摆像素无法表达 UI 树。

**成功标准（MVP）：**
- 场景中有 Canvas GO；其子 GO 树下挂 Image，按参考分辨率 Layout 后在 ScreenUI 正确显示
- 改父节点 / 改 Anchor·Margin 后重算 Computed Rect，画面更新
- `WidgetComponent` 本身不承载「图片样式」专用字段（由 Image 提供）

---

## 2) 现状（代码事实）

| 能力 | 状态 |
|------|------|
| ScreenUI Pass / Queue | **有**（RND-F16 Path B） |
| `WidgetComponent` | **有**：Texture/Color/Size/StableOrder + 像素左上（将收窄为无样式基础，见 §3.2） |
| `GameObject` 父子 | **无** — `Scene` 平铺 `m_GameObjects`；Hierarchy 平铺列表 |
| `SceneComponent` 附着 | **有** — 仅同一 GO 内 Root/子组件，**不是** UI Tree |
| Layout / Canvas / Image | **无** |

---

## 3) 方案

### 3.1 组件分层（拍板）

```text
GameObject (Canvas root)
 └── CanvasComponent
      └── child GOs…（CORE-F08）
           ├── WidgetComponent   ← 几何 + Layout（Anchor/Margin/Size）+ 序
           └── ImageComponent    ← Texture + Tint（可选）
```

| 类型 | 职责 | 非职责 |
|------|------|--------|
| **`WidgetComponent`** | ScreenUI 基础：Layout 意图、Computed Rect、StableOrder、可见 | 贴图/字体等视觉样式 |
| **`ImageComponent`** | Texture + Color | Layout |
| **`CanvasComponent`** | 参考分辨率、Scale Mode、子树根 | Hit / 样式 |

> **不另拆 `LayoutComponent`。** Layout 是 Widget 在 ScreenUI 世界的本职。  
> 迁移：现有 Widget 上的 Texture/Color **下沉到 Image**（推荐一次切干净）。

### 3.2 Canvas（ScreenSpace）

| 项 | MVP 约定 |
|----|----------|
| `RenderMode` | 仅 **ScreenSpace**（World 预留枚举可不实现） |
| 参考分辨率 | 例如 `1920×1080`（可配置） |
| Scale Mode | **Stretch** 或 **Letterbox** 二选一先做一种（推荐 **Letterbox** 保比例） |
| 子节点坐标 | Layout/手摆均在 **Canvas 参考像素空间**；再映射到视口像素给 Path B |
| 谁入队 | MVP：Canvas 子树内、带有效 Widget（+ Image 内容）的节点入 `ScreenUIQueue` |

```text
参考像素 (refX, refY, refW, refH)
        ↓  Canvas Scale / Offset（视口适配）
视口像素 (px, py, w, h)  →  WidgetSceneProxy / Path B
```

### 3.3 Layout 基本规则（MVP）

**原则（对齐外部底稿）：** Tree ≠ Layout。GO 父子提供树；Layout 产出 Computed Rect。

**MVP 只做：**

| 概念 | 含义 |
|------|------|
| `Size` | 固定像素（相对参考分辨率） |
| `Anchor` | 相对父 Computed Rect：Min/Max 归一化（至少支持：左上固定、四边拉伸） |
| `Margin` | 相对锚点矩形的内缩（左/上/右/下） |

**计算顺序（深度优先，父先于子）：**

```text
Canvas.RefRect = (0,0, RefWidth, RefHeight)
for each node in GO tree under Canvas (pre-order):
  parentRect = parent.ComputedRect  (Canvas 根用 RefRect)
  node.ComputedRect = ApplyAnchorMargin(parentRect, Anchor, Margin, Size)
  write through → Widget 几何（再经 Canvas 映射到视口像素）
```

**手摆 vs Layout：**  
- 默认：**Computed Rect 覆盖** 绘制用 TopLeft/Size（由 Widget 上 Anchor/Margin/Size 算出）。  
- 调试可用「跳过 Layout / 手摆像素」开关（实现时可选）。

**不做：** Flex 主轴、Grid、百分比链、intrinsic Text 测高（无 Text）。

### 3.4 Widget 上的 Layout 字段（拍板）

不另拆 LayoutComponent。`WidgetComponent` 持有：

```text
AnchorMin / AnchorMax   // 父 Rect 归一化
Margin (L,T,R,B)
Size (当锚点非拉伸时用固定尺寸)
StableOrder
ComputedRect（运行时缓存，可不序列化）
```

同 GO 可选 `ImageComponent` 提供视觉。

### 3.5 ImageComponent

| 属性 | 说明 |
|------|------|
| `Texture` | 可选；空则纯色（白贴图×Tint） |
| `Color` | RGBA Tint |

绘制：复用 Path B；几何来自同 GO `WidgetComponent`。  
**Invariant：** 无 Widget 则 Image 不入队（推荐成对）。

### 3.6 数据流

```text
[Edit/Tick]
  CORE-F08 GO 树
       ↓
  Layout pass（读各 Widget 的 Anchor/Margin/Size）
       ↓
  Computed Rect → 视口像素
       ↓
[Render] BuildScreenUIQueue → ScreenUIPass
```

### 3.7 与外部底稿的对齐

| 底稿 | 本设计 |
|------|--------|
| Level 2 Tree + Layout | Canvas + GO 父子 + **Widget 内 Layout** |
| 先 Panel/Image 非 Button | **仅 Image** |
| Tree ≠ Layout | GO 树提供结构；Widget 算 Computed Rect |
| UI ≠ 2D Renderer | 继续消费 Path B |
| 不另建 Runtime | 复用 GO/Component/Lua |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| 另建 Widget Tree Runtime | **拒绝** — 违背 Entity+Component 方向 |
| 仅用 SceneComponent 附着模拟 UI 树（单 GO 巨树） | **拒绝** — 无法多 GO/Lua/序列化友好 |
| Layout 直接写 GPU | **拒绝** — 先 Computed Rect |
| UI-F01 内夹带 GO 父子实现 | **拒绝** — 抽出 **CORE-F08**，避免 UI 绑架内核 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| 无 GO 父子则 Canvas 子树无法表达 | **先做 CORE-F08**；UI-F01 Status 可 Blocked |
| Widget 去 Texture 破坏现有场景 | 迁移说明；Image 替代；CHANGELOG |
| Layout 每帧全树重算 | MVP 可接受；dirty 标志后置 |
| Letterbox vs Stretch 选错 | Design 拍板一种；另一种后加枚举 |

---

## 6) 验收标准（UI-F01 MVP，依赖 CORE-F08）

- [ ] `CanvasComponent` ScreenSpace + 参考分辨率 + 一种 Scale Mode
- [ ] `LayoutComponent`（或等价）Anchor+Margin+固定 Size → Computed Rect
- [ ] `ImageComponent` + 收窄后的 `WidgetComponent` 可画
- [ ] Canvas 子树随 **GO 父子** 遍历；改父后 Layout/绘制正确（需 CORE-F08 + 建议 ED-F05）
- [ ] 不进入 Opaque/Translucent；仍走 ScreenUI
- [ ] 无 Text/Button；无完整 Flex

---

## 7) 建议实现切片（草案）

| Slice | 内容 | 依赖 |
|-------|------|------|
| — | **CORE-F08** GO 父子 + 序列化 | — |
| — | **ED-F05** Hierarchy 树 + 拖拽改父 | CORE-F08 |
| `UI-F01-S00` | Canvas + 视口映射；子树 Widget 入队策略 | CORE-F08 |
| `UI-F01-S01` | Widget Layout（Anchor/Margin/Size → Computed Rect） | S00 |
| `UI-F01-S02` | ImageComponent；Widget 去掉 Texture/Color 样式字段 | S00 |
| （后） | Hit-test / Text / Button | S01+ |

---

## 8) 与 CORE-F08 / ED-F05（必读）

**事实：** 今日 Hierarchy 是 **flat list**；`GameObject` **无** `Parent`/`Children`。  
`SceneComponent::AttachToComponent` 只解决 **同一 GO 内** 组件树，不能表达「Canvas 下多个子面板 GO」。

因此：

1. **UI Tree = GameObject 父子树**（拍板）  
2. **必须先做 CORE-F08**（运行时父子、世界/相对变换、存档）  
3. **强烈建议同期或紧随 ED-F05**（否则只能靠代码/Console 改父，无法在编辑器搭 UI）

UI-F01 在 CORE-F08 MVP 完成前：**不写实现代码**（Design 可继续评审）。

---

## 9) Status note

| 字段 | 内容 |
|------|------|
| Status | **Draft** |
| Blocked by | `CORE-F08`（GO 父子）；编辑体验依赖 `ED-F05` |
| Next | 评审本 Draft → Planned；并行推进 CORE-F08 Design |
| Unblock | CORE-F08 可创建父子并序列化 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 占位 |
| 2026-09-03 | Canvas GO 方向；Widget 命名；对齐 RND-F16 |
| 2026-09-04 | Path B 无 Canvas；等 Widget 可画 |
| 2026-09-04 | **Draft 扩写：** Canvas + Layout + Image；阻塞 CORE-F08 |
| 2026-09-04 | Layout **并入 WidgetComponent**（不拆 LayoutComp）；Inactive 传播不属本 Feature |
