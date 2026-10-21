# CORE-F43 S06 + S07 实施记录 —— Inspector 两轴契约 + 审计收口
- **ID:** CORE-F43（Design: `CORE-F43_GO_COMPONENT_ACTIVITY_AND_VISIBILITY_DESIGN.md`，Status 已改为 `Implemented`）｜ **批次:** S06（Inspector a/b/c）+ S07（审计收口）｜ **Owner:** project maintainer ｜ **Last updated:** 2026-02-15
- **前作:** `CORE-F43_S01_S02_IMPLEMENTATION.md`、`CORE-F43_S03_S05_IMPLEMENTATION.md`

## 0. TL;DR
1. GO 头部两个开关（新增 `DrawGameObjectStatusSwitches`）：绑自身 flag、写 `SetActive` / `SetVisible`、undo 沿用 blob；画在 header `Selectable` 之外，不抢双击与右键。
2. 组件头行新增 visible 复选框（仅 `SceneComponent`），右侧追加只读徽标 `Inactive in Hierarchy` / `Hidden in Hierarchy`；显示矩阵四条按设计落地，**全程无 `BeginDisabled`**（灰显只是 `ImGuiStyleVar_Alpha 0.55`）。
3. 四字段仍不在通用字段列表、属性路径也写不进去 ⇒ 单一编辑点成立（§3）。
4. S07：机制 `IsActiveSelf()` 仍 **5 处**；行为门今天 grep 到 **42** 个有效态读取点（Runtime 41 + Editor 1），本批新增 **0** 个（§4）。只改编辑器 + 文档：导出 **1283 → 1283**，`verify.ps1` **exit 0**。

## 1. 改动（两个文件）
`SceneEditorInspectorSource.h` 声明 `void DrawGameObjectStatusSwitches(GameObject&)`；`.cpp` 在 `:145` 调用它，`:339-364` 画组件 visible 复选框，`:371-381` 画组件徽标，`:1369-1425` 是 GO 两开关 + 徽标。
GO 开关内部是 `drawAuthoredSwitch(label, propertyPath, selfValue, apply)`：点击 → `SetActive`/`SetVisible` → `MarkSceneDirty()` → 用 `"m_bActive"` / `"m_bVisible"` 的前后 blob 提交 undo。两个 lambda 只写自身 flag，所以祖先关着时点击也不会把「想开」写反。

## 2. 显示矩阵（四条，逐条给判据）
| 自身 | 有效态 | 复选框 | 行样式 | 徽标 | 判据 |
|---|---|---|---|---|---|
| 开 | 开 | 勾选可写 | 正常 | 无 | `inactiveStylePushed = !IsActiveSelf()`（`:276`）+ 属主开着 |
| 关 | 关 | 未勾可写 | Alpha 0.55 | 无 | 同上，但属主开着 |
| 开 | 关 | **仍勾选可写** | 正常 Alpha | `Inactive in Hierarchy` | `:372` 读**属主 GO**，不读有效态 |
| 关 | 关 | 未勾可写 | Alpha 0.55 | 同样追加 | 同上：判据在属主，自身关也照样显示 |

visible 轴同构（`:377`、`:1421`，条件换成属主 `IsVisible()`）。徽标判据刻意放在**属主**而非有效态：否则「自己也关掉」的那一行会丢掉「上面还有一层关着」这条信息。

## 3. 单一编辑点核对
- 通用字段列表：`PropertyEditPolicy.cpp:37` 对 `Invisible` 直接返回 `false`（`CanEdit` 复用 `ShouldShow`）⇒ 四字段不出现在 Inspector 列表；属性路径写入：`PropertyPath.cpp:693-696` `IsPropertyVisible` 以 `Invisible` 为否、`IsPropertyWritable` 复用它 ⇒ 第二个写入口不存在。
- specifier 现状：`GameObject.h:312/316`、`Component.h:133`、`SceneComponent.h:137` 四个字段都带 `Invisible`，本批未新增 specifier。
- 场景 dump：`DebugCommandSceneUtils.cpp:113/145/402/439` 已按 `Invisible` 跳过，无需改动。

## 4. S07 审计
**机制层 `IsActiveSelf()`：5 处**（本批未改 Runtime 一行，行号不变）—— `Component.h:58`（有效态公式）、`Component.cpp:111`（`SetActive` 去重）、`Component.cpp:151`（`ResolvePendingActivation`）、`GameObject.h:153`（GO 侧去重）、`Editor/...:277`（组件复选框）。visible 同构 3 处：`SceneComponent.h:40`、`:58`、`GameObject.h:165`。
**本批新增的 3 处自身 flag 读取全部不是行为门**（「第 6 处起要解释」的部分）：`:339` 组件 visible 复选框、`:1406`/`:1409` GO 两开关，都是意图判断，与 `:277` 同类。另有 4 处徽标判据（`:372`/`:377`/`:1417`/`:1421`）读的是**属主 / 父 GO 的有效态**，只读呈现、不参与任何 tick/绘制/命中/物理决策，因此不计入行为门。Editor 端自身 flag 读取 1 → 4 处。
**行为门（有效态读取点）今天 42 = Runtime 41 + Editor 1（`SceneEditingViewportClient.cpp:252`）**

| 形式 | 数量 | 位置 |
|---|---|---|
| `IsActive()` | 31 | UILayoutPass:14,35、ScreenUIHitTester:36、UISystem:448、Button:18,122,128,173、DropTarget:38、DragSource:48、Canvas:48、GameObject.cpp:72,468,479、RenderScene:27,88,223,271,297、Primitive:36、Light:88、SkyBox:53、Image:115、Text:80、PhysicsSystem:162,168、RigidBody:37,122、Camera:101、Lua:61 |
| `IsActiveInHierarchy()` | 4 | UILayoutPass:27、ScreenUIHitTester:139、Scene.cpp:112,289 |
| `IsActiveAndVisible()` | 6 | RenderScene:54,169,246（代理绘制位发布）、SkyBox:98、WidgetComponent:296、ScreenUIHitTester:151 |

与 S03–S05 记录的 33 处对照：33 处**全部仍在且仍读有效态**；3 处改读组合谓词（`ScreenUIHitTester`、`SkyBox`、`WidgetComponent` 绘制位），1 处判定搬到发布点（`ForwardRenderer` primitive 收集改读 `primitiveProxy->m_Enabled`，判定在 `RenderScene:54`）。前两批「新增 13 处」的净新增调用点是 8。**没有任何一处行为门写自身 flag**，这是本审计要保证的不变量。

## 5. 与既有实现记录的出入（只列出，未改写那两份）
1. `S03_S05` §4 把 `WidgetComponent` 绘制位（`:263` 与 `:349`）和 `ForwardRenderer` primitive 收集（`:1273` 与 `:1188`）分别列进「原 33」与「新增」两张表，实为同一处：今天前者只有一处 `IsActiveAndVisible()`（`WidgetComponent.cpp:296`），后者都读代理绘制位（`ForwardRenderer.cpp:1189`）。
2. `S03_S05` §1.2 说光照统一走 `ContributesLight()` 且谓词是 `IsActiveAndVisible()`；今天 `ContributesLight`（`ForwardRenderer.cpp:74-78`）读 `lightProxy->m_Enabled`，谓词在 `RenderScene.cpp:169`（边界审计据此报 `game-thread publish sites: 4, pipeline proxy reads: 5`）。
3. `S01_S02` §4 的「5 处」组成与设计表不同（数值相同），该记录已自行声明为偏差，现状未变。

## 6. 验证
`cmake --build build --target Editor --target minEngineTests` → 0 error；`scripts/verify.ps1` → **VERIFY_EXIT=0**，11 套 smoke 全绿（末套 `widget-graphic`，本批未新增用例）：

```
PASS: NumberOfFunctions=1283 <= 3000, ordinal headroom=64252 >= 60000
  PASS engine exports did not increase                1283 (baseline 1549)
PASS: editor/module boundary holds
[doctest] Status: SUCCESS!   |   VERIFY_EXIT=0
```

## 7. 手动点验清单（6 步，需要有画面的编辑器）
1. 选中一个 GO：头部下方出现 `Active` / `Visible` 两个复选框，勾选态等于自身 flag（父关时仍显示勾选）。
2. 点 GO 的 `Active`：节点连同子树灰显/消失；`Ctrl+Z` 一步回到勾选态。
3. 父 GO 关、子 GO 开：子 GO 仍勾选，旁边出现灰字 `Inactive in Hierarchy`；点它**能**改（不锁子开关）。
4. 父关时把子也关掉：子行 Alpha 灰显，徽标**仍在**（两重原因都揭示）。
5. 挂 `StaticMeshComponent` 的 GO：组件头行出现第二个复选框，取消后模型消失而该行不再灰显（active 仍开）；把它所在 GO 挂到已关的父下，徽标出现但复选框仍可点。
6. 非空间组件（如 `LuaComponent`）头行**只有一个**复选框；Inspector 属性列表里看不到 `Active` / `Visible`；连续 undo 一次只回退一项（visible 与 active 不串味）。

## 8. 偏差与未完成
1. **未做目视点验**（硬约束：不启动编辑器，避免重写项目 `.meta`）→ 用 §7 的 6 步清单替代。
2. 矩阵第 3 行的「头像区左侧描边/淡色」只落了「正常 Alpha + 徽标」：组件头行已有整行 accent 底色（`:293-304`），再加描边会与组件图标争视觉焦点；GO 头部两个复选框也未做「自身关 → Alpha 0.55」灰显，因为 GO 行没有可复用的灰显上下文（header 样式由主题作用域控制），未勾选态本身已是足够信号。
3. 设计文档 Status 已改 `Implemented` 并加 v3 变更行；`FEATURE_REGISTRY.md:165` **仍是 Draft**（属该文件自己的改动，按边界未动）；本批**未** `git add` / `commit`。
