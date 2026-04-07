# minEngine ImGui GUI 设计复盘与调参 FAQ

## 1. 设计复盘（当前状态）

### 1.1 已经做得好的地方
- 窗口职责边界清晰：Viewport、Hierarchy、Inspector、Console、MainMenu 分工明确。
- 编辑器主循环结构合理：每帧先引擎 Tick，再 UI Tick，再 ImGui Render。
- Inspector 已接入反射字段编辑，且支持继承层级字段显示。
- Console 已有日志过滤、搜索、暂停、复制、清空等完整工作流。
- Viewport Overlay 已具备可折叠、可拖拽、边界限制等高级交互。

### 1.2 主要可优化点（面向“可复用”）
- 样式参数分散在多个窗口：颜色、FramePadding、Header 高度在窗口内重复设置。
- 常见交互模式重复实现：重命名、过滤器下拉、属性行布局、Overlay 面板。
- 向量编辑控件策略分散：DragFloat2/3/4 的宽度/内边距在多处硬编码。
- 窗口小组件缺少统一命名与封装：后续维护成本会上升。

## 2. 建议的组件复用拆分

> 目标：不改架构前提下，把“视觉 + 交互微件”沉淀为可复用 helper。

### 2.1 样式域封装（推荐优先）
建议新增一个轻量 UI 样式帮助头，例如 `Editor/src/UI/ImGuiStyleScope.h`：
- `ScopedStyleVar`
- `ScopedStyleColor`
- `PushCompactControls()` / `PopCompactControls()`
- `PushSectionHeaderStyle()` / `PopSectionHeaderStyle()`

收益：减少 Push/Pop 漏配，避免样式污染。

### 2.2 通用过滤器下拉组件
建议抽出 `DrawMultiSelectFilterDropdown(...)`：
- 输入：分类分组（Source/Level）+ 布尔状态引用
- 输出：过滤状态变化、摘要字符串（如 `Filters (6/8)`）

收益：Console、后续资源浏览器、Profiler 过滤都能复用。

### 2.3 通用可拖拽 Overlay 组件
建议抽出 `DrawDraggableOverlay(...)`：
- 支持折叠/展开
- 支持边界钳制（限制在父区域）
- 支持最小尺寸与偏移持久化

收益：Viewport、Profiler、调试统计面板都可直接复用。

### 2.4 属性编辑器组件化
建议把 Inspector 的字段绘制拆成：
- `DrawScalarField(...)`
- `DrawVectorField(...)`
- `DrawStringField(...)`
- `DrawBoolField(...)`

收益：同一字段类型风格统一，减少未来新增类型时的重复代码。

### 2.5 重命名交互统一
建议统一 `BeginRename/CommitRename/CancelRename` 模式：
- Hierarchy 与 Inspector 使用同一套行为规范（双击/F2、Enter 提交、Esc 取消）

收益：交互一致性更好，减少状态变量分叉。

## 3. 最近修改内容复盘（你近期连续需求）

### 3.1 全局主题
- 建立了深色工业风基础主题。
- 强化了 Tab 对比度，解决标签与背景融合问题。

### 3.2 Inspector
- 顶部改为 GO 名称主头。
- 支持双击/F2 重命名。
- 仅在存在 RootComponent 时显示 Transform 区域。
- Component 标题高度收紧。
- Vector 拖拽输入框宽度和内边距放宽。

### 3.3 Console
- 主窗口禁止上下滚动，仅日志区滚动。
- 顶部按钮高度压缩。
- 过滤器从大面积横排改为下拉多选（接近 VSCode 问题过滤器风格）。

### 3.4 Viewport Overlay
- 文本自动换行。
- 锚定到 Viewport 图像区域。
- 支持折叠为小项。
- 支持拖拽且不能拖出 Viewport 子区域。

## 4. GUI 常见问题与参数速查（FAQ）

## Q1. 控件太“高”、界面显得臃肿怎么办？
优先调：
- `ImGuiStyleVar_FramePadding`（Y）
- `ImGuiStyleVar_ItemSpacing`（Y）

经验值：
- 紧凑工具栏：`FramePadding = (6, 3)`
- 普通表单：`FramePadding = (8~10, 5~6)`

## Q2. 标题栏/Tab 与背景混在一起怎么办？
优先调色：
- `ImGuiCol_Tab`
- `ImGuiCol_TabActive`
- `ImGuiCol_TabUnfocused`
- `ImGuiCol_WindowBg`

原则：Tab 亮度和饱和度都要高于 WindowBg 一个层级。

## Q3. Vector 输入框太窄，数字看不清怎么办？
优先调：
- `ImGui::SetNextItemWidth(-FLT_MIN)`（让控件吃满列宽）
- `ImGuiStyleVar_ItemInnerSpacing`
- `ImGuiStyleVar_FramePadding`

常用搭配：
- `ItemInnerSpacing = (8, 6)`
- `FramePadding = (10, 6)`

## Q4. 长文本越界怎么办？
优先用：
- `ImGui::TextWrapped(...)`
- 或按区域宽度手动限制 child 宽度

适用：Overlay、日志摘要、路径显示、状态提示。

## Q5. 窗口本体不想滚动，只想内容区滚动怎么办？
窗口 Flags：
- `ImGuiWindowFlags_NoScrollbar`
- `ImGuiWindowFlags_NoScrollWithMouse`

然后把可滚动内容放进 `BeginChild(...)`。

## Q6. Overlay 如何避免拖出 Viewport？
关键做法：
- 记录 overlay 偏移 `offset`
- 在每帧和拖动时 `std::clamp`：
  - `x in [padding, parentW - overlayW - padding]`
  - `y in [padding, parentH - overlayH - padding]`

## Q7. 多选过滤器怎么做得不臃肿？
推荐模式：
- 顶栏只留一个摘要下拉：`Filters (x/n)`
- 下拉内做 `Selectable(..., DontClosePopups)`
- 提供 `All / None`

## Q8. 重命名交互怎样才顺手？
建议标准：
- 触发：双击 + F2
- 提交：Enter 或失焦后提交
- 取消：Esc
- 进入编辑时自动全选

## 5. 常用参数建议表（可直接抄）

| 场景 | 推荐参数 |
|---|---|
| 顶部工具栏紧凑 | FramePadding=(6,3), ItemSpacing=(6,4) |
| Inspector 表单 | FramePadding=(9,6), ItemInnerSpacing=(8,6) |
| Component 标题高度 | Header 处临时 FramePadding=(8,3) |
| Overlay 面板 | ChildBg alpha≈0.8, Border 对比略高 |
| Console 日志区 | 主窗口禁滚，Child 区滚动 |
| 大量过滤项 | 统一改为下拉多选 + 摘要字符串 |

## 6. 后续建议（你自己调参的最短路径）

1. 先调“密度”：只动 `FramePadding/ItemSpacing/ItemInnerSpacing`。
2. 再调“对比”：只动 `WindowBg/Tab/Header/Button/Text`。
3. 最后调“反馈”：hover/active 的亮度差至少保留一个层级。

这样能避免一次改太多导致视觉回归难定位。
