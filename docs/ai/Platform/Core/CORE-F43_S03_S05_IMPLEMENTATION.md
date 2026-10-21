# CORE-F43 S03–S05 实施记录 —— 消费面（UI / 渲染 / 模拟）

- **ID:** CORE-F43（Design: `CORE-F43_GO_COMPONENT_ACTIVITY_AND_VISIBILITY_DESIGN.md`，已批准）
- **批次:** S03（UI 消费面）+ S04（渲染消费面）+ S05（模拟消费面）
- **前作:** `CORE-F43_S01_S02_IMPLEMENTATION.md`（字段 / 有效态契约 / 缓存 / 下推失效 + 差量 reconcile / 机制 5 处）
- **不做:** S06（Inspector 两轴契约）、S07（收尾与审计清零）
- **Owner:** project maintainer
- **Last updated:** 2026-02-15

## 0. TL;DR

1. 三个消费面全部接上：UI 布局 / UI 命中 / UI 绘制，渲染绘制 + 阴影 + 光照，模拟 tick / fixed tick；
   物理 / 相机 / Lua 在第一批已经读有效态，本批只做核对。
2. **主人本批新增要求**：UI 的内容源组件（`ImageComponent` / `TextComponent`）自己的 active 决定它
   是否贡献内容（之前只有 `ButtonComponent` 看 active，同一族约定不一致），并**配套失效联动**：
   源组件 `SetActive` 主动把同 GO 上 widget 的渲染代理标脏，否则画面会停在最后一帧。
3. **空 widget 策略**：当 widget 的所有内容源都不贡献内容时，它既不绘制（代理绘制位为假），
   也不参与命中测试（不再出现「看不见的板子吞掉下面所有按钮」）。
4. 审计：机制层 `IsActiveSelf()` 仍为 **5 处**，设计里的 **33 处行为门全部仍读有效态**；
   本批**新增 13 处**行为门调用点，逐条列在 §4。
5. 导出条目数 **1283 → 1283**；`scripts/verify.ps1` exit 0；smoke 套数 9 → 10（新增 `activity-consumers`）。
6. **诚实发现（§7.1）**：布局与命中的「子树跳过」在第一批的有效态逐组件门落地之后，
   **结果上是冗余的**——不活跃子树里的组件有效态本来就为假，因此不会写矩形、也不会成为命中候选。
   它们仍然按批准的设计落地（遍历成本 + 不变量就地化），但反向验证证实移除后**行为不变**，
   所以「用例守护行为」的部分由贡献门与失效联动承担（那两处反向验证是致命的）。

## 1. 三个消费面

### 1.1 UI

| 位置 | 改动 | 为什么 |
|------|------|--------|
| `UILayoutPass::LayoutGameObject` | 入口处 `!gameObject->IsActiveInHierarchy()` 直接返回 | 不活跃节点带走整棵子树，不再无条件递归；子树里的矩形保持上一次的值，重新激活后下一帧布局重建 |
| `ScreenUIHitTester::CollectHitWidgetsRecursive` | 同上（入口处返回） | 不活跃子树里的 widget 不可命中 |
| `ScreenUIHitTester` 命中候选 | `widget->IsActiveAndVisible() && IsHitTestVisible() && HasContentContribution()` | 绘制位与可见轴的组合门；再加上「内容源是否贡献」这一条实现空 widget 策略 |
| `WidgetComponent::FillSceneProxy` | 文字路径与图片路径都前置 `bWidgetCanDraw = IsActiveAndVisible()` | 隐藏但 active 的 widget 保留代理、保留 tick，只是绘制位为假 |
| `ImageComponent::CanDraw` / `TextComponent::CanAttemptDraw` | 前置 `!IsActive()` 返回 false | 内容源自己的开关必须决定它是否贡献内容（§2.1） |
| `Component::SetActive` | 写自身 flag 后调用 `NotifyOwnerWidgetContentChanged` | 内容源切换必须立刻弄脏同 GO 上 widget 的代理（§2.2） |

未改：`CanvasComponent::Tick` 的 `IsActive()`、`UISystem` / `ButtonComponent` / `DragSourceComponent` /
`DropTargetComponent` 的交互门——它们在第一批已经读有效态，语义正确，不需要动。

### 1.2 渲染

| 位置 | 改动 | 为什么 |
|------|------|--------|
| `ForwardRenderer::BuildRenderQueue` 的 primitive 循环 | 新增 `!sourceComponent->IsActiveAndVisible()` 守卫 | 这一处循环同时喂主视图收集器与阴影收集器，一个守卫同时覆盖「不绘制」与「不投影」 |
| `ForwardRenderer::BuildScreenUIQueue` | 取到 widget 后新增 `!widget->IsActiveAndVisible()` 守卫 | 代理绘制位已经回答过一次；活读一次防住「代理是关掉之前组装的」这种陈旧态 |
| `ForwardRenderer` 定向光 / 点光 / 聚光（光照填充 3 处 + 阴影请求 3 处） | 统一走文件内 `ContributesLight()`，谓词是 `IsActiveAndVisible()` | 隐藏但 active 的光不照明、不投影，但照旧 tick |
| `RenderScene::UpdateSkyBox` 的 `m_Enabled`、`SkyBoxComponent::CreateSceneProxy` 的 `m_Enabled` | `IsActive()` → `IsActiveAndVisible()` | `m_Enabled` 是绘制位而不是生存期位 |
| `RenderScene` 四类代理注册入口（primitive / light / skybox / widget） | 保持由 `IsActive()`（有效态）决定 | 生存期跟随 active：¬active ⇒ 摘代理（走既有 `RemoveActivationFromSystems`）；visible 只翻绘制位 |

### 1.3 模拟

| 位置 | 改动 | 为什么 |
|------|------|--------|
| `Scene::Tick` | 扁平遍历里新增 `!gameObject->IsActiveInHierarchy()` 跳过 | ¬active 的子树必然整棵 ¬active，扁平遍历在节点处停即可 |
| `Scene::FixedTickGameObjects` | 同上 | fixed tick 同一语义 |
| `GameObject::BeginPlay` / `Tick` / `FixedTick`、`PhysicsSystem`、`RigidBodyComponent`、`CameraComponent`、`LuaComponent` | **未改**（第一批已读有效态） | 本批只做核对：这些门的调用点与语义一致，物理只看 active、不看 visible |

## 2. 主人本批新增要求

### 2.1 内容源贡献门

`ImageComponent::CanDraw()` 与 `TextComponent::CanAttemptDraw()` 原来只判断「数据是否有效」，
于是关掉 Image 的 active 画面不变；`ButtonComponent::CanAcceptClick` 却一直看 active，同族不一致。
现在两个谓词都前置于 `IsActive()`（有效态，与其它行为门同源），因此：

- 关掉内容源 ⇒ `CanDraw` / `CanAttemptDraw` 立刻为假；
- `WidgetComponent::FillSceneProxy` 的两条路径都消费这两个谓词 ⇒ 代理里不再有内容；
- `WidgetComponent::HasContentContribution` 消费同一对谓词 ⇒ 命中测试与绘制对「有没有内容」的判定只有一份实现。

### 2.2 失效联动（为什么必须挂在 `SetActive` 上）

widget 的代理只在 `m_bRenderStateDirty` 为真时才重填（`DoEndOfFrameUpdate`）。内容源切换
active 不会碰 widget 的任何状态，所以不做联动就会出现「关掉了但画面还在，直到别的原因触发重建」——
比不改更糟。落点是 `Component::SetActive`：写自身 flag 并完成 reconcile 之后，把同 GO 上所有
`WidgetComponent` 标脏。

为什么用「同 GO 上所有 widget」而不是「判断自己是不是内容源」：内容源与 widget 是同 GO 兄弟，
组件基类不该知道派生类名单；这条规则对任何「自己的 active 决定兄弟代理内容」的组件都成立，
也不影响正确性（多标脏一次只是下一帧多重填一次代理）。选择无条件执行是为了让外部组件
（外部仓组件同样能挂 widget 与内容源）不需要额外注册就自动获得联动。

### 2.3 空 widget 策略

「所有内容源都不贡献」时：

- **不绘制**：`FillSceneProxy` 的 `m_bVisible` 由 `bWidgetCanDraw && 内容源可绘 && 材质/缓冲可用` 组成，
  内容源不贡献 ⇒ 绘制位为假 ⇒ 不进 `ScreenUIQueue`；
- **不作为遮挡**：命中测试要求 `HasContentContribution()`，因此空 widget 不再是命中候选，
  不会把下面按钮的点击吃掉。

## 3. 逐文件改动

| 文件 | 改动 |
|------|------|
| `Framework/Components/Component.cpp` | 新增匿名命名空间里的 `NotifyOwnerWidgetContentChanged`；`SetActive` 末尾调用 |
| `Framework/Components/ImageComponent.cpp` | `CanDraw` 前置 `IsActive()` |
| `Framework/Components/TextComponent.cpp` | `CanAttemptDraw` 前置 `IsActive()` |
| `Framework/Components/WidgetComponent.h` | 新增 `HasContentContribution()` 声明；新增 `Testing::TestAccess<WidgetComponent>` friend |
| `Framework/Components/WidgetComponent.cpp` | 新增 `HasContentContribution()` 实现；`FillSceneProxy` 两条路径前置 `IsActiveAndVisible()` |
| `Framework/Components/SkyBoxComponent.cpp` | `m_Enabled` 改 `IsActiveAndVisible()` |
| `Framework/Scene/Scene.cpp` | `Tick` / `FixedTickGameObjects` 跳过不活跃节点 |
| `Render/RenderScene.cpp` | skybox `m_Enabled` 改 `IsActiveAndVisible()` |
| `Render/RenderPipeline/ForwardRenderer.cpp` | primitive 绘制/阴影守卫、ScreenUI 活读守卫、`ContributesLight` + 6 处光照/阴影循环 |
| `UI/UILayoutPass.cpp` | 布局子树跳过 |
| `UI/ScreenUIHitTester.cpp` | 命中子树跳过 + `IsActiveAndVisible()` + 内容贡献门 |
| `Tests/Access/WidgetComponentTestAccess.h` | 新增：读/清 `m_bRenderStateDirty` |
| `Tests/Suites/ActivityConsumersTest.{h,cpp}` | 新增 4 个 smoke 用例 |
| `Tests/TestSuiteRegistration.cpp`、`Runtime/Test/TestSuiteRegistry.cpp` | 注册 `activity-consumers`（`InSmoke=true` + `kSmokeOrder` 追加） |

**未触碰（硬性约束）**：`ScreenUICoords.*`、`SpriteQuadMesh.*`、`PointerConstraintState.*`、
`GLFWWindowSystem.*`、`WindowSystem.h`、`Render/Material.*`、`ShaderCompiler.*`、
`SpriteMaterialFactory.*`、`ScreenUIMaterialFactory.*`、`EngineShaderUtils.*`、`RHI/RHI.h`、
`minEngine_exports.def`、`minEngine_export_baseline.json`、`FEATURE_REGISTRY.md`、`bugs/*`、
`CORE-F38_*`、`CORE-F43_*` 设计文档、`UI-F09_*`、外仓 `minCraft*`。

## 4. §H8 调用点分类审计

**机制层 `IsActiveSelf()`：5 处（与第一批一致，行号随本批改动漂移）**

| # | 位置 | 性质 |
|---|------|------|
| 1 | `Component.h:58`（`IsActive()` 内的有效态公式） | 公式自身项 |
| 2 | `Component.cpp:111`（`SetActive` 去重） | 意图 |
| 3 | `Component.cpp:151`（`ResolvePendingActivation`） | 意图 |
| 4 | `GameObject.h:153`（`GameObject::SetActive` 去重） | 意图 |
| 5 | `Editor/.../SceneEditorInspectorSource.cpp:275`（Inspector 复选框） | 意图 |

**没有任何第 6 处 `IsActiveSelf()`。** 行为门一处都没写自身 flag。

**行为门读有效态：设计里的 33 处全部仍在，本批新增 13 处**

原 33 处（位置随本批漂移）：`UILayoutPass.cpp:14,35`、`ScreenUIHitTester.cpp:36,153`、
`UISystem.cpp:448`、`ButtonComponent.cpp:18,122,128,173`、`DropTargetComponent.cpp:38`、
`DragSourceComponent.cpp:48`、`CanvasComponent.cpp:16`、`GameObject.cpp:72,468,479`、
`RenderScene.cpp:26,79,207,230,255`、`PrimitiveComponent.cpp:36`、`LightComponent.cpp:88`、
`SkyBoxComponent.cpp:53,98`、`WidgetComponent.cpp:263`、`ForwardRenderer.cpp:1273`、
`PhysicsSystem.cpp:162,168`、`RigidBodyComponent.cpp:37,122`、`CameraComponent.cpp:101`、
`LuaComponent.cpp:61`、`Editor/.../SceneEditingViewportClient.cpp:252` = **33**。

本批新增的 13 处行为门：

| # | 位置 | 门 |
|---|------|-----|
| 1 | `ImageComponent.cpp:111` | 内容源贡献 |
| 2 | `TextComponent.cpp:80` | 内容源贡献 |
| 3 | `WidgetComponent.cpp:349` | widget 绘制位（`IsActiveAndVisible()`） |
| 4 | `UILayoutPass.cpp:27` | 布局子树跳过 |
| 5 | `ScreenUIHitTester.cpp:139` | 命中子树跳过 |
| 6 | `ForwardRenderer.cpp:76` | `ContributesLight`（光照/阴影共用） |
| 7 | `ForwardRenderer.cpp:1188` | primitive 绘制 + 阴影 |
| 8 | `ForwardRenderer.cpp:1267` | ScreenUI 活读守卫 |
| 9 | `Scene.cpp:112` | fixed tick 跳过 |
| 10 | `Scene.cpp:289` | tick 跳过 |

另有 3 处**原地加强**（不是新调用点，原本已经是 33 里的行为门，只是从 `IsActive()` 变成
`IsActiveAndVisible()`，即多了可见轴）：`RenderScene.cpp:230`、`SkyBoxComponent.cpp:98`、
`ScreenUIHitTester.cpp:153`（后者同时加上了内容贡献门）。

**「(ii) 机制里的许可判断」4 处仍是有效态**：`Component.cpp:125`（reconcile 差量基准）、
`Component.cpp:162`（`TryActivate` 门）、`Component.cpp:263`（`SetOwner` 重试）、
`RigidBodyComponent.cpp:110`（建体前检查）。

**域外（不动）**：`AudioVoice.h:23`、`AudioVoice.cpp:127`、`AudioSystem.cpp:490/623/684/793/846`、
`PhysicsWorld.cpp:964/995`（Jolt `bodyInterface.IsActive`，与组件无关）。

## 5. 用例与反向验证

新增套件 `activity-consumers`（4 个用例，全部 `[smoke]`）：

| 用例 | 覆盖 |
|------|------|
| `activity-consumers: inactive node skips its whole UI layout subtree` | 布局：子树停摆后矩形保持不变，重新激活后重建 |
| `activity-consumers: inactive node skips its whole hit-test subtree` | 命中：子树停摆后不再命中 |
| `activity-consumers: widget without contributing content does not take the pointer` | 空 widget 策略：无内容源的大板不再抢走下面有内容 widget 的命中 |
| `activity-consumers: content source switch gates its contribution and dirties the widget` | 内容源贡献门 + 失效联动 + 往返（图片/文字各一遍） |

### 5.1 反向验证（去掉实现后用例变红，贴原文）

**(a) 去掉内容源贡献门**（`ImageComponent::CanDraw` / `TextComponent::CanAttemptDraw` 的 `IsActive()` 前置）：

```
D:\Dev\GitRepo\minEngine\minEngine\Tests\Suites\ActivityConsumersTest.cpp:283: ERROR: CHECK_FALSE( image->CanDraw() ) is NOT correct!
  values: CHECK_FALSE( true )
```

**(b) 去掉失效联动**（`Component::SetActive` 里的 `NotifyOwnerWidgetContentChanged`）：

```
D:\Dev\GitRepo\minEngine\minEngine\Tests\Suites\ActivityConsumersTest.cpp:285: ERROR: CHECK( Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*imageWidget) ) is NOT correct!
  values: CHECK( false )
```

**(c) 去掉空 widget 的内容贡献门**（命中候选不再要求 `HasContentContribution()`）：

```
D:\Dev\GitRepo\minEngine\minEngine\Tests\Suites\ActivityConsumersTest.cpp:252: ERROR: CHECK( hit.Widget == board.get() ) is NOT correct!
  values: CHECK( 0x298895ada00 == 0x298895718b0 )
```

**(d) 去掉布局 / 命中的子树跳过**：**四个用例全绿**（见 §7.1 的诚实说明——这两处跳过在
结果上被第一批的逐组件有效态门覆盖，所以没有可观察差异；用例只在保护「不活跃子树不可被
布局写入 / 不可命中」这个结果本身）。

## 6. 导出与门禁

- 导出条目数：**1283**（`PASS: NumberOfFunctions=1283 <= 3000, ordinal headroom=64252 >= 60000`），
  `minEngine_exports.def` 与 `minEngine_export_baseline.json` 均未改；本批新增的
  `HasContentContribution()` 是非 inline 成员（DLL 内部使用，不需要也不允许新增导出），
  用例通过 friend `TestAccess` 读脏位，不引入新的导出符号。
- `scripts/verify.ps1`：见下方尾行。

- `scripts/verify.ps1`：**exit 0**，smoke 套数 **9 → 10**（新增 `activity-consumers`），
  10 套全部 PASSED：

  ```
  PASS: NumberOfFunctions=1283 <= 3000, ordinal headroom=64252 >= 60000
  PASS: all 1283 baseline symbols are still exported by libminEngined.dll
  PASS: editor/module boundary holds
    PASS engine exports did not increase                1283 (baseline 1549)
  [doctest] test cases:  4 |  4 passed | 0 failed | 243 skipped
  [doctest] assertions: 45 | 45 passed | 0 failed |
  [16:16:33] [Test] TestRunner: suite 'activity-consumers' PASSED.
  EXITCODE=0
  ```

## 7. 偏差 / 未完成

### 7.1 布局与命中的「子树跳过」在结果上是冗余的

设计 P2 写的「父 GO 关掉后子 widget 的 `m_ComputedRect` 照旧被写入」在**第一批之后不再成立**：
子 widget 的有效态也随祖先变为假，因此它自己的门就已经挡住了写入；命中同理。
本批仍然按批准的设计落地了子树跳过（遍历成本、不变量就地化），但它的**可观察结果**与逐组件门
重叠，所以反向验证无法让它变红。结论：这两处是成本与可读性改动，不是行为修复。

### 7.2 空 widget 策略的「不绘制」半边没有直接用例

无 RHI 的 headless smoke 里 `WidgetSceneProxy::m_bVisible` 恒为假（材质代理需要 RHI 才能编译），
所以「不绘制」无法与「绘制」在用例里区分。当前覆盖是：
`HasContentContribution()`（命中侧，可直接观察）+ `CanDraw`/`CanAttemptDraw`（贡献谓词，可直接观察）
+ 两者与 `FillSceneProxy` 的绘制位共用同一对谓词。真机绘制验证留给渲染侧手工回归。

### 7.3 光照门没有自动化覆盖

`ContributesLight` 的 6 处消费点在 `ForwardRenderer` 里，需要真实渲染上下文才能观察到照明/阴影
差异，headless smoke 覆盖不到。风险由「谓词只有一份实现 + 代理生存期仍由 active 决定」限制。

### 7.4 未做（下一批）

- Inspector 两轴契约（GO header 开关 + 可见性复选框 + 有效态只读徽标）——S06。
- 审计清零与收尾（`ReconcileEffectiveActivation` 更名的 API 表面收口、旧驱动点复核）——S07。
