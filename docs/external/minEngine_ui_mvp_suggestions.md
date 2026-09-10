minEngine UI System — Design Bootstrap Prompt

你正在参与 minEngine 的 UI System 设计与实现。

minEngine 是一个以学习现代游戏引擎架构、工程实践以及长期演进为目标的自研 C++ Game Engine。当前引擎已经具备 Entity/Component、Reflection、Serialization、Lua Script、Renderer/RHI 等基础设施，并正在逐步扩展 Animation、Audio、UI、异步任务等系统。

Repository：

https://github.com/Max1122Chen/minEngine

在开始设计或修改代码之前，请先理解现有工程结构，并尽量基于现有架构进行演进，而不是重新建立一套平行基础设施。

1. minEngine 的核心设计哲学

minEngine 的总体方向不是复刻 UE / Unity，而是建立一个：

克制（Restrained）
可组合（Composable）
可插拔（Pluggable）
低耦合（Low Coupling）
数据驱动（Data-First）
声明式（Declarative where appropriate）
Agent-Friendly
不强迫用户采用特定 Gameplay 范式

的 Game Engine。

重要原则：

Engine 应该提供通用能力，而不是替用户决定游戏应该如何组织。

因此：

不要为了“完整”而提前加入复杂 Framework。
不要为了模拟 UE/Unity API 而复制其大量历史包袱。
优先设计清晰的底层抽象和可组合 primitive。
高层功能应该建立在底层能力之上，而不是反过来污染底层。
如果一个功能可以作为独立模块存在，就不要无必要地塞进另一个系统。
不要因为某个大型商业引擎这样设计，就认为 minEngine 也必须如此设计。

同时：

不要为了追求“极简”而过早牺牲未来的扩展空间。

MVP 可以小，但核心抽象应该具有合理的长期演进方向。

2. UI 的总体方向

minEngine 的 UI 不准备直接复制 UE UMG，也不准备完整复制 Unity UI。

目前倾向采用：

Entity + Component 作为 UI 的统一对象模型。

即：

Entity
 ├── UILayout
 ├── UIImage
 ├── UIText
 ├── UIButton
 ├── LuaScript
 └── ...

UI Entity 与普通 World Entity 共用底层：

Entity
Component
Reflection
Serialization
Event / Delegate
Lua

UI 不应该拥有第二套完全独立的 Object Runtime。

但是：

同一个 Object Model ≠ UI 与 World Entity 行为完全相同。

UI 可以拥有自己的：

UI Tree
Layout
Hit Testing
Focus
Input Routing
UI Rendering

等系统。

目标是：

                Entity / Component
                       │
          ┌────────────┴────────────┐
          │                         │
      World Systems             UI Systems
          │                         │
     World Rendering       Layout / Input / UI

UI 是一个建立在 Engine 通用 Runtime 之上的专用系统，而不是第二个 Runtime。

3. Lua 的设计方向

minEngine 已经拥有 Lua Script 系统。

UI 不应该再创建一套：

Lua -> Widget -> Widget Components

这样的独立脚本体系。

更倾向：

Lua Runtime
     ↓
LuaScript Component
     ↓
Entity
     ↓
Components / Events

因此 Lua 应尽量理解：

Entity
Component
Property
Event

而不是强依赖：

Widget
Actor
Pawn
...

这样 World 与 UI 可以共享同一个 Script Runtime。

4. UI 与 2D Rendering 的边界

这是本阶段一个非常重要的设计原则：

UI ≠ 2D Renderer

推荐的依赖方向：

UI
 ↓
2D Rendering Foundation
 ↓
Renderer
 ↓
RHI

而不是：

2D Renderer
 ↓
UI

2D Rendering Foundation 应该是一个比 UI 更底层、更通用的 Rendering 能力。

它未来可能服务：

UI
Sprite
2D Game
Debug Draw
Editor Overlay
Gizmo
Particle / Billboard
Image / Texture Visualization
Screen-space rendering
其他需要 Quad / Sprite 类绘制的系统

因此：

不要把 2D Rendering Foundation 设计成“UI Renderer”。

UI 只是它的重要消费者之一。

5. 2D Rendering Foundation 应独立进行设计

不要假设：

“2D Rendering = 一个 Quad + 一个 Texture + 一个 DrawCall。”

真实的 2D Rendering Foundation 需要考虑的内容可能包括：

Geometry

至少需要明确：

Quad
Rectangle
Position
Size
Rotation
Scale
Pivot / Origin
UV
UV Rect
Z / Layer

但具体 Transform 数据模型可以进一步讨论，不要立即固化。

Texture

需要考虑：

Texture reference
UV mapping
Texture sampling
Texture filtering
Texture atlas
Sub-region rendering
Texture lifetime / resource dependency

以及：

2D Renderer 是否直接持有 Texture Handle / Asset Reference？

这需要结合现有 RHI / Resource / Asset 系统设计。

Material / Shader

不要假设所有 2D 元素都必须使用完全相同的 shader。

未来可能存在：

Sprite Material
UI Material
Text Material
Debug Material
Custom 2D Material

因此应该考虑：

2D Draw Command
    ↓
Material / Pipeline
    ↓
RHI

但 MVP 可以只支持非常有限的 material model。

Draw Command / Draw List

建议重点讨论：

2D Renderer 是否应该首先建立一个 CPU-side Draw List / Command List？

例如概念上：

DrawCommand
{
    Texture
    Material
    Transform
    UV
    Rect
    Layer
    ClipRect
}

这只是示意，并非要求照此实现。

真正需要讨论的是：

什么是 2D Draw Command？
什么数据应该进入 Command？
什么应该由 Renderer 推导？
Draw Command 是否 immutable？
是否允许排序？
如何进行 batching？
如何处理 texture/material/pipeline state？
如何处理 clipping / scissor？
如何处理 layer / depth？
如何保证 UI 可以控制绘制顺序？
如何最终转化成 RHI commands？
6. 2D Rendering 与 Rendering Batching

2D Renderer 应从设计上考虑 batching，但：

不要为了 batching 过早设计复杂的 mega-system。

首先明确：

2D Scene/UI
      ↓
Draw Commands
      ↓
Sort / Batch
      ↓
GPU-friendly Commands
      ↓
RHI

需要考虑：

Texture batching
Material batching
Pipeline state batching
Vertex/index buffer batching
Draw order constraints
Layer ordering
Alpha blending
Scissor / clip state

尤其需要认识到：

2D Rendering 的 batching 与普通 3D rendering batching 的问题既相似又不同。

UI 经常存在严格的绘制顺序：

Background
    ↓
Image
    ↓
Text
    ↓
Overlay

因此不能简单地：

按照 Texture 排序

然后忽略原本的视觉顺序。

Batching 必须尊重 rendering order / ordering constraints。

7. Clipping / Scissor

2D Rendering Foundation 应考虑 clipping。

尤其 UI Layout 中会产生：

Parent Rect
    ↓
Child Rect
    ↓
Clip Rect

最简单的基础能力可以是：

Scissor Rect

之后再考虑：

nested clipping
stencil clipping
mask
rounded clipping
advanced UI masks

不要在 MVP 阶段实现所有类型。

但：

Clip Rect 应作为 2D Rendering Foundation 的一等概念之一，而不是 UI 临时 hack。

8. Coordinate System

需要明确 2D Rendering 的坐标体系。

至少需要讨论：

World Space
Screen Space
Viewport Space
Normalized Space
Pixel Space

以及：

Origin
Y direction
Pixel center
viewport transform
resolution scaling
DPI scaling
camera-independent rendering

尤其 minEngine 同时存在 OpenGL / Vulkan RHI，因此：

2D Rendering 不应该把 OpenGL 的坐标假设泄漏到上层。

2D Renderer 应建立自己的稳定语义，然后由 Renderer/RHI 负责映射到底层 Graphics API。

9. UI MVP 的三个层级

UI MVP 暂时划分为三个主要层级。

Level 1 — 2D Rendering Foundation

这一层不是完整 UI。

目标是建立可靠的：

Texture
Quad / Rectangle
Transform2D
UV
Material
Draw Command
Draw List
Layer / Ordering
Scissor / Clip
Batching
2D Renderer

最终可以做到：

Draw Texture A
Draw Texture B
Draw Texture Region
Draw Colored Quad

并能够稳定、高效地输出到现有 Renderer/RHI。

这一层应该独立于 UI。

这一阶段值得单独进行架构设计，而不是简单实现一个 Sprite 类。

Level 2 — UI Tree + Layout

建立 UI 的层级结构：

Root
 ├── Panel
 │    ├── Image
 │    └── Text
 │
 └── Panel
      └── Button

UI Tree 可以由 Entity parent/child relationship 表达。

但是：

Tree Structure 与 Layout 是两个概念。

Tree：

Parent
 └── Child

Layout：

Parent Rect
     ↓
Layout Rules
     ↓
Child Computed Rect

推荐：

UI Tree
   ↓
Layout System
   ↓
Computed Geometry

而不是让 Entity Transform 本身承担所有 Layout 语义。

10. Layout Philosophy

Layout 可以大量借鉴 CSS 的思想：

描述布局意图，而不是直接描述最终像素坐标。

例如：

width: 50%
height: auto
anchor: center
margin: ...
padding: ...
align: ...

而不是：

x = 173
y = 421
width = 347
height = 82

但：

不要尝试实现完整 CSS。

CSS 的复杂度远远超过 Flex。

长期可能涉及：

fixed size
percentage
fill
fit / intrinsic size
anchor
margin
padding
alignment
min/max constraints
flex
grid

但 MVP 应该从非常有限的 Layout Primitive 开始。

具体 layout model 留待后续讨论。

11. Input / Hit Testing

第三层：

Mouse / Touch / Controller
          ↓
      UI Input
          ↓
       Hit Test
          ↓
     Entity / Component
          ↓
        Event
          ↓
         Lua

Input 应使用 Layout System 已经计算好的 geometry。

即：

Input 不负责计算 Layout。

而是：

Layout
  ↓
Computed Rect
  ↓
Hit Testing

MVP 可以首先实现：

Hover
Pressed
Released
Click

之后再考虑：

Focus
Capture
Bubble
Consume
Drag
Keyboard navigation
Gamepad navigation
12. Data-First / Declarative Authoring

这是 minEngine 非常重要的长期设计方向。

UI 尤其适合结构化数据表达。

例如概念上：

UI Asset
 ├── Entity
 │    ├── Components
 │    └── Children
 │
 ├── Layout Data
 ├── Visual Data
 └── Script / Event References

Editor、CLI、Agent 都应该尽可能操作同一个底层数据模型：

          UI Data
        /    |    \
   Editor   CLI   Agent
        \    |    /
         Runtime

Editor 不应该成为唯一的 Source of Truth。

原则：

Editor 是数据的可视化编辑器，而不是 UI 本身。

数据模型应该尽可能：

machine-readable
machine-writable
human-readable
diffable
deterministic
version-control friendly

但：

不要为了“Data-First”而强迫所有内容都使用 JSON。

具体数据格式应该根据资产类型和语义选择。

13. MVP Components

第一阶段不需要完整 Widget Framework。

建议只验证少量 primitive：

Root
Panel
Image
Text
Button

对应的底层 Component 可以类似：

UILayout
UIImage
UIText
UIButton

但具体 Component 划分不要直接假设，应先检查现有 Entity/Component/Reflection/Serialization 架构。

尤其避免：

Button
    ↓
Huge Widget Base Class
    ↓
Widget Lifecycle
    ↓
Widget Tree
    ↓
Widget Controller
    ↓
Widget Manager

这种过早 Framework 化。

14. Recommended System Boundary

最终可以形成大致这样的依赖关系：

                UI Data
                   │
          Editor / CLI / Agent
                   │
                   ▼
            Entity / Component
                   │
        ┌──────────┼──────────┐
        ▼          ▼          ▼
      Layout      Input     Visual
        │          │          │
        ▼          ▼          ▼
 Computed Rect  Hit Test   Draw Commands
        │          │          │
        └──────────┼──────────┘
                   ▼
             2D Renderer
                   │
                   ▼
                Renderer
                   │
                   ▼
                  RHI

其中：

2D Renderer

属于 Rendering Foundation，而不是 UI。

15. 目前明确不做 / 暂缓

不要在 UI MVP 中提前实现：

完整 CSS
完整 Flex/Grid
Rich Text
Accessibility
Localization Framework
Data Binding Framework
UI Animation Framework
Complex Mask System
Virtualized List
Complex Widget Lifecycle
UE-style Widget Blueprint system
大型 Widget Framework
Editor-only UI Runtime
第二套 Lua Runtime
第二套 Object / Reflection System

这些未来都可以讨论。

16. 第一阶段最重要的问题

在真正开始写 UI 之前，优先深入设计：

A. 现有 Renderer / RHI 如何承载 2D Rendering？

重点检查当前：

Renderer
Render Command
Mesh
Material
Texture
RHI
Resource Lifetime
Pipeline State

然后决定 2D Renderer 应如何复用它们。

B. 2D Draw Command 的模型

重点讨论：

What is a 2D Draw Command?

以及：

Draw Command
    ↓
Sort?
    ↓
Batch?
    ↓
Generate Vertex/Index?
    ↓
Renderer
    ↓
RHI
C. 2D Coordinate / Transform Model

确定：

Position
Size
Rotation
Scale
Pivot
Layer

的语义。

不要直接把 UI Layout 与 GPU Transform 混为一谈。

D. Batching Model

设计最小但合理的：

Draw List
    ↓
Ordering
    ↓
Batching

确保不会为了 batching 破坏 UI 的视觉顺序。

E. Clipping

先确定：

Scissor

如何进入 Draw Command / Renderer。

Nested clipping 等高级能力可以暂缓。

F. Layout Data Model

在 2D Rendering 基础设施稳定后，再设计：

UILayout

以及：

Parent
Child
Anchor
Size
Margin
Padding
Alignment

等概念。

不要先从 Button 开始。

17. Implementation Philosophy

每一个阶段都优先遵循：

Foundation
    ↓
Minimal Primitive
    ↓
Validate
    ↓
Generalize
    ↓
Extend

而不是：

先设计完整系统
    ↓
一次实现所有功能

特别是 2D Renderer：

先证明抽象正确，再增加功能。

建议每一步都能够通过 Playground / Debug Scene 验证。

例如第一阶段可以逐渐验证：

1. Colored Quad
2. Textured Quad
3. Multiple Quads
4. Texture Region / UV
5. Transform
6. Layer Ordering
7. Scissor
8. Draw List
9. Batching
10. UI consumes 2D Renderer
18. 最重要的总体原则

在整个 UI 系统设计中，请始终牢记：

不要把 UI 当成一个独立的大型 Framework。

minEngine 更希望得到的是：

Generic Runtime
        +
2D Rendering Foundation
        +
UI-specific Systems

而不是：

Huge UI Framework
        +
Special UI Object Runtime
        +
Special UI Scripting Runtime
        +
Special UI Rendering

最终目标是让 UI 成为：

Entity/Component Runtime + Declarative Layout + 2D Rendering + Input Systems 的一个自然组合。

同时保持每个基础系统本身具有独立价值。

给 Agent 的工作方式要求

在开始实现之前：

先阅读现有代码。
找出当前 Renderer/RHI/Texture/Material/Mesh/Entity/Component/Serialization 的真实实现。
不要假设现有架构与描述完全一致。
对现有架构与目标架构之间的 Gap 做分析。
对存在多种合理方案的地方，先提出设计比较，而不是直接拍板。
优先复用已有基础设施。
避免为了 UI 创建重复基础设施。
保持模块边界清晰。
MVP 可以不完整，但不要制造明显阻碍未来扩展的临时抽象。
每完成一个基础层，都应该能够独立验证。

当前最优先的设计对象不是 Button，也不是 Widget，而是 2D Rendering Foundation。

请首先围绕这一层分析 minEngine 当前代码，并与我讨论：

2D Rendering Foundation
├── Coordinate / Transform
├── Quad / Geometry
├── Texture / UV
├── Material / Pipeline
├── Draw Command
├── Draw List
├── Ordering
├── Batching
├── Clipping / Scissor
└── Renderer / RHI integration

然后再进入 UI Tree、Layout 和 Input 的设计。

我尤其赞成你现在把 2D Rendering Foundation 单独抬高一个层级。这其实会让整个 UI 设计更漂亮：

                UI
                 │
        ┌────────┴────────┐
        │                 │
      Layout            Input
        │                 │
        └───────┬─────────┘
                ↓
          UI Draw List
                ↓
       ┌────────────────┐
       │ 2D Rendering   │  ← 真正值得设计的基础设施
       └────────────────┘
                ↓
             Renderer
                ↓
               RHI

而且它与你之前讨论的 Rendering Batching、RHI 抽象、未来可能的 Editor/Gizmo/Debug Draw 都会发生联系，所以这部分确实不应该被当成“UI 开工前顺手写个 Sprite Renderer”。