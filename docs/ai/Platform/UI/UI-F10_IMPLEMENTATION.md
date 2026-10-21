# UI-F10 — Widget 图形组件抽象 — 实现记录

## Meta
- **ID:** `UI-F10` · **Status:** `Implemented`（S01–S03 全部落地，`verify.ps1` exit 0）
- **Design:** [UI-F10_WIDGET_GRAPHIC_COMPONENT_DESIGN.md](./UI-F10_WIDGET_GRAPHIC_COMPONENT_DESIGN.md)（含 D1–D5）
- **吸收:** `BUG-CORE-007`（Canvas 组件失活失效）

## S01 抽象基类 + 迁移

- 新增 `Framework/Components/WidgetGraphicComponent.h`：`ME_CLASS(Abstract)` 抽象基类（先例 `SceneComponent`），
  三个纯虚 `CanContribute()` / `BuildDrawData(widgetSizePx, out) -> bool` / `ApplyMaterialParameters(Material&)`；
  同文件定义纯数据 `WidgetDrawData`（无任何 Component 反向指针）。**基类不新增持久字段**，序列化不变。
- `ImageComponent`：`CanContribute` 复用 `CanDraw()`；`BuildDrawData` 产出纹理/UV/颜色，声明走共享 quad；
  `ApplyMaterialParameters` 接管原 `WidgetComponent::SyncMaterialParameters` 的逻辑。
- `TextComponent`：`CanContribute` 复用 `CanAttemptDraw()`；`BuildDrawData` 做重建网格并产出顶点/索引/布局/图集；
  删掉组件私有的运行时材质（材质实例按 D4 只由 widget 持有），`ApplyMaterialParameters` 写颜色 + 图集。
- `WidgetComponent`：新增 `GetGraphic()`（**header 内 inline**，取组件顺序中第一个 `CanContribute()` 为真者）与
  `HasGraphic()`；`FillSceneProxy` 改走 `ResolveDrawSource`（内部调 `GetGraphic` + `BuildDrawData`）+
  `ApplyMaterialParameters`；删除 `FindSiblingImage` / `FindSiblingText` / `HasContentContribution` /
  `SyncMaterialParameters(ImageComponent*)` 与两处前向声明。`WidgetComponent.{h,cpp}` 不再出现任何具体图形类型名。
- 命中侧 `ScreenUIHitTester` 改用 `HasGraphic()`，与绘制侧共用 `GetGraphic()`：画谁就命谁。

## S02 多图形裁决 + 告警

- 裁决只由组件列表顺序决定（作者用 `GameObject::MoveComponent` 调整），无新增排序字段。
- 代理重建时若 `≥2` 个图形组件：每次重建（widget 处于可绘制状态时）打一条 `ME_LOG(LogUI, Warn)`，
  写明选中谁、忽略谁、怎么调整。
- 用例：Image+Text 同 GO → 选中/绘制/命中均为 Image；关掉 Image 的 `active` → Text 接管；
  空 widget（无图形）不画不命中。

## S03 Canvas 失活失效（`BUG-CORE-007`）

- `CanvasComponent` 重写 `OnActivate()` / `OnDeactivate()`（`SetActive` 签名未动），递归把 `GetOwner()` 子树内
  所有 `WidgetComponent` 标脏。只做失效，不写任何作者化字段。
- 用例：关 Canvas → 当帧后代 Widget 变脏且不再命中；恢复后重新变脏并恢复命中；
  `IsActiveSelf()` / `IsVisibleSelf()` 值未变。

## 与设计文档的偏差（3 条）

1. `BuildDrawData` **非 const**：文字必须先重建网格/顶点缓冲才能产出数据，const 化会逼出 `mutable` 缓存谎报。
   设计草图写的是 `const`；已确认的 D3（返回 bool）保持不变。
2. 类型判定用 `WidgetGraphicComponent::LookupClass()`（`ReflectionSystem::FindClass` 按名取一次并缓存），
   而不是 `StaticClass()`：后者的定义在生成代码里、不在 DLL 导出表中，header 内 inline 的 `GetGraphic()`
   无法链接到它（导出必须保持 1283，不能加行）。
3. 命中侧按设计取 `HasGraphic()`（选择级）；设计 Failure Model F1 的「选中但画不出」目前在**绘制侧**
   退化（`BuildDrawData` 返回 false → 代理不装配可绘制位），命中侧不做第二次网格重建。

## 验证证据

| 项 | 结果 |
|----|------|
| `grep -c -E 'ImageComponent\|TextComponent'` on `WidgetComponent.h` / `.cpp` | `0` / `0` |
| `FindSiblingImage` / `FindSiblingText` / `HasContentContribution` 全仓残留 | `0` |
| 告警样例 | `Widget '' on GameObject 'graphic-widget' holds 2 graphic components: drawing 'minEngine::ImageComponent', ignoring [minEngine::TextComponent]. The first contributing one in component order wins; reorder the components on the GameObject to choose a different one.` |
| 绘制/命中同源 | 用例断言 `hit.Widget->GetGraphic() == image`，且 `ResolveDrawSource`（代理装配的唯一入口）返回同一组件 |
| 反向验证（S03） | 去掉 `OnDeactivate` 的传播后：`WidgetGraphicTest.cpp:324 ERROR: CHECK( ... IsRenderStateDirty(*panel) ) is NOT correct! values: CHECK( false )` → 用例红；恢复后绿 |
| 导出 | `1283 exports`（未变），`minEngine_exports.def` 与 baseline 均未改动 |
| smoke | `10 → 11` 套件（新增 `widget-graphic`，`InSmoke=true` + `kSmokeOrder` 追加） |
| 门禁 | `scripts/verify.ps1` → `exit 0`；`screen-ui-*` 全套 + `ui-layout` + `activity-consumers` 单独复跑全绿 |

## 改动文件

- 新增 `minEngine/minEngine/src/Runtime/Function/Framework/Components/WidgetGraphicComponent.h`
- 新增 `minEngine/minEngine/src/Generated/Reflection/WidgetGraphicComponent.gen.{h,cpp}`（构建生成，别手改）
- `ImageComponent.{h,cpp}` · `TextComponent.{h,cpp}` · `WidgetComponent.{h,cpp}` · `CanvasComponent.{h,cpp}` · `UI/ScreenUIHitTester.cpp`
- 测试 `Tests/Suites/WidgetGraphicTest.{h,cpp}` · `Tests/Access/WidgetComponentTestAccess.h` ·
  `Tests/TestSuiteRegistration.cpp` · `Runtime/Test/TestSuiteRegistry.cpp`

## 遗留

- 代理仍在 widget 上（设计明确不做下移）；`SkyBoxComponent` 同形状的失效问题未纳入。
- 编辑期「多个图形组件」的检查属于编辑器侧，未做（告警只在运行时出现）。
- 无 GPU 的测试进程里文字无法产出网格，因此「Text 接管」用例断言的是**选择**（`GetGraphic()`）与
  `BuildDrawData` 的退化行为，不是真实像素。
