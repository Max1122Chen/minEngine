# CORE-F43 S01 + S02 实施记录 —— 字段与契约 + reconcile 传播

- **ID:** CORE-F43（Design: `CORE-F43_GO_COMPONENT_ACTIVITY_AND_VISIBILITY_DESIGN.md`，D1–D10 已批准）
- **批次:** S01（字段与契约，零行为变化）+ S02（reconcile 传播，本批的核心证明点）
- **不做:** S03–S07 的消费面改造（UI 布局 / UI 命中 / 渲染收集 / 模拟 / Inspector 契约）
- **Owner:** project maintainer
- **Last updated:** 2026-02-14

## 0. TL;DR

1. 三个新作者化字段（`GameObject::m_bActive` / `GameObject::m_bVisible` / `SceneComponent::m_bVisible`）
   全部 `ME_PROPERTY(Invisible)`、缺省 `true`、经反射往返。
2. 有效态契约按 §H3 原样实现：`IsActive() = IsActiveSelf() ∧ Owner->IsActive()`，
   `IsVisible()` 同构；**查询 O(1)，稳态不遍历父链**（§H4 第一层）。
3. 祖先切换只做两件事：**下推失效** + **差量 reconcile**；`m_bActive` / `m_bVisible`
   一个比特都不写（§I1）。这一条由 O1 用例在代码层面钉死，并用「故意写回」反向验证过。
4. §H8 的 Self 落位完成（含 Editor `:273` → 现 `:275`），行为门 33 处全部读有效态。
5. **引擎导出条目数仍为 1283**（`engine exports did not increase 1283 (baseline 1549)`）。
   本批新增的访问器全部是 header 内 inline；唯一新增的可执行代码在 inline 函数体内，
   不需要（也不能）进 `cmake/minEngine_exports.def`。
6. `scripts/verify.ps1` exit 0，smoke 由 8 套变 9 套（新增 `hierarchy-activity`）。

---

## 1. 契约（§H3 的代码映射）

| §H3 公式 | 落点 | 实现 |
|---|---|---|
| `GO::IsActive(g) = g.IsActiveSelf() ∧ (g.m_Parent ? GO::IsActive(g.m_Parent) : true)` | `GameObject.h` | inline 递归上溯，命中缓存即返回（三态位，路径压缩） |
| `GO::IsVisible(g)` | `GameObject.h` | 同上（visible 轴） |
| `Component::IsActive(c) = c.IsActiveSelf() ∧ (c.m_Owner ? GO::IsActive(c.m_Owner) : true)` | `Component.h` | inline，**O(1) 纯缓存读**：祖先项 `m_bOwnerEffectiveActive` 由 owner GO 在 push-down 时发布 |
| `SceneComponent::IsVisible(s)` | `SceneComponent.h` | 同上（`m_bOwnerEffectiveVisible`） |
| `SceneComponent::IsActiveAndVisible(s) = IsActive(s) ∧ IsVisible(s)` | `SceneComponent.h` | inline |

**为什么 Component 侧不做懒上溯**：`Component.h` 与 `GameObject.h` 是包含环（`GameObject.h`
包含 `Component.h`），`m_Owner->IsActive()` 在 `Component.h` 里拿不到完整类型。把祖先项
「发布」进组件（`PublishOwnerEffectiveActive` / `PublishOwnerEffectiveVisible`，均由
`friend class GameObject` 调用）既解掉了环，又让 **组件查询变成真正的 O(1) 单次内存读**，
严格满足 §H4「查询不遍历父链 + 不分配」。

**派生量（不序列化、不作者化、不可直写）**

```cpp
// GameObject（private）
mutable uint8_t m_EffectiveActiveCache { kEffectiveStateUnknown };
mutable uint8_t m_EffectiveVisibleCache{ kEffectiveStateUnknown };

// Component（private）
bool            m_bOwnerEffectiveActive { true };   // 祖先链贡献，owner GO 维护
mutable uint8_t m_EffectiveActiveCache  { kEffectiveStateUnknown };
bool            m_bAppliedEffective     { false };  // 差量基准

// SceneComponent（private）
bool            m_bOwnerEffectiveVisible{ true };
mutable uint8_t m_EffectiveVisibleCache { kEffectiveStateUnknown };
```

命名（§H5 / D6 反转）：`IsActive()` / `IsVisible()` = 有效态；`IsActiveSelf()` /
`IsVisibleSelf()` = 自身 flag；`IsActiveInHierarchy()` / `IsVisibleInHierarchy()` = 同义别名。

## 2. 缓存与失效（§H4 三层）

**第一层 · 查询 O(1)。** `Component::IsActive()` 与 `SceneComponent::IsVisible()` 是「一次
`uint8` 读 + 一次分支」；`GameObject::IsActive()` 只有在缓存为 `Unknown` 时才上溯一次并在
返回路上回填整条路径（路径压缩），此后同样 O(1)。任何消费点都**没有**父链循环（§F2 / §I7）。

**第二层 · 状态变化（下推 + 差量 reconcile）。** `GameObject::PropagateEffectiveStateChange()`：

```cpp
void PropagateEffectiveStateChange()
{
    m_EffectiveActiveCache = kEffectiveStateUnknown;
    m_EffectiveVisibleCache = kEffectiveStateUnknown;

    ApplyEffectiveStateToComponents(IsActive(), IsVisible());   // 发布 + 逐组件差量

    for (size_t index = 0; index < m_Children.size(); ++index)  // 无堆分配、无虚调用
    {
        GameObject* child = m_Children[index];
        if (child != nullptr) { child->PropagateEffectiveStateChange(); }
    }
}
```

`ApplyEffectiveStateToComponents` 逐组件做三件事，**全程不读写任何作者化字段**：

1. `component->PublishOwnerEffectiveActive(ownActive)` —— 发布祖先项（`m_EffectiveActiveCache = Unknown`）；
   `SceneComponent` 额外发布 `ownVisible`，**仅当有效 visible 真的翻转**才 `MarkRenderStateDirty()`；
2. `component->ReconcileEffectiveActivation()`；
3. 差量本体在 `Component::SyncActivationWithActiveFlag()`：

```cpp
const bool want = IsActive();                    // 许可判据 = 有效态（§H8 R-D）
if (want == m_bAppliedEffective) { if (!want) { m_bPendingActivation = false; } return; }
m_bAppliedEffective = want;
if (want) { TryActivate(); } else { Deactivate(); }
```

`TryActivate` 的许可门改成 `if (!IsActive()) return;`（原 `!m_bActive`）；pending 分支额外把
`m_bAppliedEffective` 复位为 `false`，保证「想开但还开不了」不会被当成已对账。
`Deactivate()` 同时复位 `m_bAppliedEffective`。因此 **reconcile 幂等收敛**（§I9）：
第二次调用 `want == m_bAppliedEffective`，零 `ApplyActivationToSystems` / `RemoveActivationFromSystems`。

**第三层 · 稳态每帧 0 次失效 / 0 次上溯。** 没有状态变化就没有遍历。

**失效触发点落位（§H4 的 7 类）**

| # | 触发点 | 落位 |
|---|--------|------|
| 1 | `GameObject::SetActive` / `SetVisible` | `GameObject.h` inline（F9 首行值比较，值没变零成本返回） |
| 2 | `AttachToParent` / `DetachFromParent` / `SetRootComponent` | `GameObject.cpp`（`DetachFromParent` 只在真的有过 parent 时重算） |
| 3 | `AddComponent_Internal` | 经 `Component::SetOwner` 的 republish 覆盖 |
| 4 | `Component::SetOwner` / `SceneComponent::SetOwner` | `Component.cpp` / `SceneComponent.cpp` |
| 5 | `SceneManager::ResolvePendingActivationsForScene` | 改**两阶段**：先全场景 `InvalidateEffectiveStateSubtree()`（不求值），再逐 GO `ReconcileEffectiveState()`（§F5） |
| 6 | 反射直写兜底 | `ColliderComponent::PostEditChangeProperty` 里 `"m_bActive"` 命中后先 `ReconcileEffectiveActivation()`；Editor 侧 undo 路径走 `SceneEditor.cpp` 的同一调用 |
| 7 | prefab 实例化 / 传播 | 复用 5 的统一口；两轴带 `Invisible`，按 D10 (a) **不参与** prefab override 传播（与 `Component::m_bActive` 现状一致，不新增代码） |

另外 `Component::SetActive` / `SceneComponent::SetVisible` 在写自身 flag 后先把自己那一格
缓存置 `Unknown`，再 reconcile —— 这一条是**用例抓出来的真缺陷**（见 §6.1）。

## 3. 逐文件改动

| 文件 | 改动 |
|------|------|
| `Runtime/.../Components/Component.h` | `IsActive()` 语义反转为有效态（inline O(1)）；新增 `IsActiveSelf()` / `IsActiveInHierarchy()` / `ReconcileEffectiveActivation()`；`m_bOwnerEffectiveActive` + 三态缓存 + `m_bAppliedEffective` + apply/remove 计数器；`kEffectiveState*` 三常量；`friend class GameObject` |
| `Runtime/.../Components/Component.cpp` | `SetActive` 用 `IsActiveSelf()` 去重；`SyncActivationWithActiveFlag` 改成有效态差量 reconcile（符号名保留，见 §5）；`ResolvePendingActivation` 用 `IsActiveSelf()`；`TryActivate` 许可门改 `IsActive()`；`Deactivate` 复位差量位；`SetOwner` 重发布祖先项并 reconcile |
| `Runtime/.../Components/SceneComponent.h` | 新增 `m_bVisible`（`ME_PROPERTY(Invisible)`）、`SetVisible` / `IsVisible` / `IsVisibleSelf` / `IsVisibleInHierarchy` / `IsActiveAndVisible`、`m_bOwnerEffectiveVisible` + 三态缓存、`friend class GameObject` |
| `Runtime/.../Components/SceneComponent.cpp` | `SetOwner` 重发布 visible 祖先项 |
| `Runtime/.../GameObject/GameObject.h` | 新增 `m_bActive` / `m_bVisible`（`ME_PROPERTY(Invisible)`，缺省 true）+ 两个三态缓存；`SetActive` / `SetVisible` / `IsActive` / `IsVisible` / `IsActiveSelf` / `IsVisibleSelf` / 两个别名（全 inline）；`InvalidateEffectiveStateSubtree` / `ReconcileEffectiveState`；private `PropagateEffectiveStateChange` / `ApplyEffectiveStateToComponents` |
| `Runtime/.../GameObject/GameObject.cpp` | `AttachToParent` / `DetachFromParent` / `SetRootComponent` 末尾加链变更传播 |
| `Runtime/.../Scene/SceneManager.cpp` | `ResolvePendingActivationsForScene` 两阶段（先失效全场景、再逐 GO 发布+reconcile） |
| `Runtime/.../Physics/ColliderComponent.cpp` | `"m_bActive"` 直写兜底补 `ReconcileEffectiveActivation()` |
| `Runtime/Test/TestSuiteRegistry.cpp` | `kSmokeOrder` 追加 `"hierarchy-activity"` |
| `Tests/TestSuiteRegistration.cpp` | 注册 `hierarchy-activity`（`InSmoke=true, InFull=true`） |
| `Tests/Suites/HierarchyActivityTest.{h,cpp}` | **新增**：O1 / O8 / 链与重挂三个用例，全部 `[smoke]` |
| `Editor/.../SceneEditorInspectorSource.cpp` | `:275`（原 `:273`）复选框改读 `IsActiveSelf()` |
| `Editor/.../SceneEditor.cpp` | undo 直写路径改调 `ReconcileEffectiveActivation()` |
| `Generated/Reflection/GameObject.gen.*`、`SceneComponent.gen.*` | 由 `reflection_codegen` 自动重新生成（未手改） |

**未触碰（硬性约束）**：`Render/ScreenUI/ScreenUICoords.*`、`Sprite/SpriteQuadMesh.*`、
`PointerConstraintState.*`、`GLFWWindowSystem.*`、`WindowSystem.h`、`Render/Material.*`、
`ShaderCompiler.*`、`SpriteMaterialFactory.*`、`ScreenUIMaterialFactory.*`、
`EngineShaderUtils.*`、`RHI/RHI.h`、`RenderSystem.cpp`、`minEngine/cmake/minEngine_exports.def`、
`minEngine_export_baseline.json`、`FEATURE_REGISTRY.md`、`bugs/*`、`CORE-F38_*`、
`CORE-F43_*` 设计文档、`UI-F09_*`、外仓 `D:\Dev\GitRepo\minCraft*`。

## 4. §H8 调用点分类审计（落地结果）

**机制（(ii) 类）：除定义与公式外，`IsActiveSelf()` 共 5 处**

| # | 位置 | 性质 | `§H8` 对应行 |
|---|------|------|---------------|
| 1 | `Component.h:58`（`IsActive()` 内的 `§H3` 公式） | 公式自身项 | §H3 原文写法 |
| 2 | `Component.cpp:80`（`SetActive` 去重） | 意图 | `:77/:79/:84` 合并落位（`:84` 是字段写入本身，不可能调用访问器） |
| 3 | `Component.cpp:119`（`ResolvePendingActivation` 前置） | 意图 | `:102` |
| 4 | `GameObject.h:134`（`GameObject::SetActive` 去重） | 意图 | 本 Feature 新增的 GO 侧同构点（§H8 写于 GO 还没有 flag 的版本） |
| 5 | `Editor/.../SceneEditorInspectorSource.cpp:275`（原 `:273`） | 意图 | Editor `:273` |

**任何行为门都没有第 6 处 `IsActiveSelf()`**（grep 见 §8）。

**行为门（(i) 类）：33 处，全部读有效态 `IsActive()`（代码文本大多不变，语义随访问器改变）**

| 域 | 位置 | 数量 |
|----|------|------|
| UI 布局 | `UILayoutPass.cpp:14, 33` | 2 |
| UI 命中 | `ScreenUIHitTester.cpp:36, 148` | 2 |
| UI 交互 | `UISystem.cpp:448`、`ButtonComponent.cpp:18, 122, 128, 173`、`DropTargetComponent.cpp:38`、`DragSourceComponent.cpp:48` | 7 |
| UI tick | `CanvasComponent.cpp:16` | 1 |
| GO 生命周期 | `GameObject.cpp:72, 468, 479` | 3 |
| 渲染代理 | `RenderScene.cpp:26, 79, 207, 230, 255` | 5 |
| 渲染组件 | `PrimitiveComponent.cpp:36`、`LightComponent.cpp:88`、`SkyBoxComponent.cpp:53, 98`、`WidgetComponent.cpp:251` | 5 |
| 渲染收集 | `ForwardRenderer.cpp:1244` | 1 |
| 物理 | `PhysicsSystem.cpp:162, 168`、`RigidBodyComponent.cpp:37, 122` | 4 |
| 相机 | `CameraComponent.cpp:101` | 1 |
| 脚本 | `LuaComponent.cpp:61` | 1 |
| Editor | `SceneEditingViewportClient.cpp:252` | 1 |
| **合计** | | **33** |

**（新增有效态输入、无祖先关闭时输出不变）4 处**：`Component.cpp:93`（reconcile 基准）、
`Component.cpp:130`（`TryActivate` 门）、`Component.cpp:231`（`SetOwner` 重试）、
`RigidBodyComponent.cpp:110`（建体前检查）。

**域外 7 处（不动）**：`AudioVoice.h:23`、`AudioVoice.cpp:127`、`AudioSystem.cpp:490/623/684/793/846`。

## 5. 导出条目数（硬性约束 #1）

**结论：1283 → 1283，未新增任何导出符号。**

- 引擎 DLL 走 `MINENGINE_EXPORT_MODE=whitelist` + `--exclude-all-symbols`
  （`cmake/MinEngineExports.cmake`），导出集合**只**由 `cmake/minEngine_exports.def` 决定；
  本批没有改那个文件，所以新增的 inline 访问器与两个新反射字段都**不会**成为导出符号。
- 但「不新增导出」有一个反向代价：**非 inline 的新函数在 consumer 侧不可用**。
  `minEngineTests.exe` 与 `libMaximumEditord.dll` 都是 DLL consumer，所以：
  `GameObject::SetActive` / `SetVisible` / `IsActive` / `IsVisible` / `InvalidateEffectiveStateSubtree` /
  `ReconcileEffectiveState` / `Component::IsActive` / `IsActiveSelf` / `ReconcileEffectiveActivation` /
  `SceneComponent::SetVisible` / `IsVisible` / `IsVisibleSelf` / `IsActiveAndVisible` /
  `GetActivationApplyCount` / `GetActivationRemoveCount` **全部是 header 内 inline**。
- `Component::SyncActivationWithActiveFlag()` 的符号被 `_ZN9minEngine9Component28SyncActivationWithActiveFlagEv`
  冻结在 .def 里（也是 Editor DLL 的链接契约）。因此 **§H5 的改名落在 API 表面**：
  头文件对外提供 `ReconcileEffectiveActivation()`（inline 转发），`.cpp` 保留旧符号名作为实现。
  所有调用点（Runtime 与 Editor）都写新名字，导出表逐字节未变。
- 验证命令与输出见 §8。

## 6. 自动化证据

### 6.1 O1 —— I1「祖先切换不写回组件字段」

`HierarchyActivityTest.cpp` 用例 `hierarchy-activity: O1 ancestor switch never writes authored flags [smoke]`：
三个时刻（作者关 / 祖先关 / 祖先开）比较 `IsActiveSelf` + `IsVisibleSelf` + `IsActive` + `IsVisible`；
作者化关掉的组件必须全程 `IsActiveSelf()==false`、作者化开着的兄弟组件必须全程 `IsActiveSelf()==true`，
只有 `IsActive()` 允许在中间时刻为 `false`。

**过程中用这条用例抓到两个真缺陷（都已修复）**：
1. `Component::SetActive` 写自身 flag 后没有失效自己那一格缓存 → `IsActive()` 返回旧值，reconcile 空转；
2. `SceneComponent::SetVisible` 写 `m_bVisible` 后没有失效 `m_EffectiveVisibleCache` →
   `IsVisible()` / `IsActiveAndVisible()` 陈旧（`IsActiveAndVisible()` 断言直接红）。

### 6.2 O1 反向验证（**故意写回 → 断言必须红**）

在 `GameObject::SetActive` 里临时插入 §P5 的禁止实现（对 `m_Components` 逐个 `component->SetActive(active)`），
重新构建后 `hierarchy-activity` 的结果：

```
D:\Dev\GitRepo\minEngine\minEngine\Tests\Suites\HierarchyActivityTest.cpp:68:
TEST CASE:  hierarchy-activity: O1 ancestor switch never writes authored flags [smoke]

D:\Dev\GitRepo\minEngine\minEngine\Tests\Suites\HierarchyActivityTest.cpp:122: ERROR: CHECK( siblingComponent->IsActiveSelf() == siblingBefore.activeSelf ) is NOT correct!
  values: CHECK( false == true )

Aborting - too many failed asserts!
===============================================================================
[doctest] test cases:  1 |  0 passed | 1 failed | 242 skipped
[doctest] assertions: 26 | 25 passed | 1 failed |
[doctest] Status: FAILURE!
```

即：祖先 `root->SetActive(false)` 之后，兄弟组件的**作者化 flag** 被改写成 `false` —— 正是 §P5 描述的
破坏路径，断言立刻红。注入已完整回退（`grep REVERSE-VALIDATION` = 0 命中），随后重新构建并复验为绿。

> 附注：另一版注入（写回放在 `ApplyEffectiveStateToComponents` 里）在 `AttachToParent` 的传播路径上
> 就触发了写回，比预期更早红在第 102 行 `CHECK_FALSE( authoredBefore.activeSelf )` —— 说明 I1 的断言
> 在「挂接」和「切换」两条路径上都能抓到写回。

### 6.3 O8 —— I9 收敛 / 不自激

用例 `hierarchy-activity: O8 reconcile converges without self-excitation [smoke]`：

1. root→mid→leaf 三层、各挂一个 `SceneComponent`；
2. 固定种子 `std::mt19937(0xC043F11)` 跑 200 次随机 `GO::SetActive` / `component::SetVisible`；
3. 收敛到「全开全显」后快照 `GetActivationApplyCount()` / `GetActivationRemoveCount()`；
4. **连续调 `ReconcileEffectiveActivation()` 2 轮**（共 6 次调用）→ 两个计数器必须**一动不动**；
5. **`scene->Tick(1/60)` × 60 帧**（无人改状态）→ 两个计数器仍然一动不动，且 `IsActive()` 始终为真。

`ApplyActivationToSystems` / `RemoveActivationFromSystems` 只在 `TryActivate` / `Deactivate` 里被调用，
计数器就在那两处自增，所以「计数器不动」= §O8 的「Apply/Remove 调用计数 0」。
`m_bAppliedEffective` 与 `IsActive()` 的一致性是**间接**断言的：任何不一致都会在下一轮 reconcile 里
产生一次 Apply/Remove，计数器立刻动。

> 偏差：§O8 原文说「每个组件的 `m_bAppliedEffective` 与 `IsActive()` 始终一致」。该字段是 private 差量位，
> 本批没有为它新增公开访问器（避免扩大 API 与导出面），改为用计数器等价断言。

### 6.4 第三个用例

`hierarchy-activity: effective state follows the chain and survives re-parenting [smoke]`：
祖父切换穿透到孙、中间层切换穿透到叶、`DetachFromParent` 迁出关闭中的祖先必须重新求值、
`IsActiveAndVisible()` 的 `§I4` 从属性、两个同义别名与 `IsVisibleInHierarchy`。

## 7. 回归（S01 零行为变化 / S02 不改变既有语义）

改动前（基线，S01 之前的同一构建目录）与改动后逐个套件一致：

| 套件 | 改动前 | 改动后 |
|------|--------|--------|
| `test smoke`（8 套） | exit 0，全绿 | exit 0，全绿（**9 套**：+hierarchy-activity） |
| `ui-layout` | 4/4 passed | 4/4 passed |
| `screen-ui-hit` | 6/6 passed | 6/6 passed |
| `screen-ui-button` | 4/4 passed | 4/4 passed |
| `screen-ui-pointer` | 3/3 passed | 3/3 passed |
| `begin-play` | 2/2 passed | 2/2 passed |
| `gameobject-hierarchy` | 6/6 passed | 6/6 passed |
| `scene-clone` | 1/1 passed | 1/1 passed |
| `prefab-overrides` | 5/5 passed | 5/5 passed |
| `prefab` | 17/17 passed | 17/17 passed |
| `physics-smoke` | 1/1 passed | 1/1 passed |
| `audio-smoke` | 1/1 passed | 1/1 passed |
| `play-main-camera` | 1/1 passed | 1/1 passed |
| `lua-script-mvp` | 1/1 passed | 1/1 passed |

为什么「零行为变化」成立：三个新字段缺省 `true`，且今天没有任何代码调用
`GameObject::SetActive/SetVisible`（它们是本批新增的 API），所以全仓每棵树的
**有效态恒等于自身 flag**，行为门读到的值与改动前完全相同。旧场景 / prefab 反序列化后新字段
取缺省 `true`（§C1 / §F4），不需要迁移层。

## 8. 验证命令与输出

**导出条目数（改前 / 改后同一条命令）**

```
> python scripts/audit_dll_exports.py --dll minEngine/bin/libminEngined.dll --report
NumberOfFunctions ... 1283   (NumberOfNames 1283)
maxNamedOrdinal ... 1283   PE limit 65535   headroom 64252
composition:
  minEngine          1282
  other                 1

> python scripts/audit_module_boundaries.py --assert --fail
  PASS engine exports did not increase                1283 (baseline 1549)
  PASS engine imgui exports did not increase          0 (baseline 253)
  ...
PASS: editor/module boundary holds
```

**构建期同一条断言**：`PASS: NumberOfFunctions=1283 <= 3000, ordinal headroom=64252 >= 60000`。

**`scripts/verify.ps1`**（`powershell.exe -NoProfile -File scripts/verify.ps1`）：

```
[doctest] test cases:   3 |   3 passed | 0 failed | 240 skipped
[doctest] assertions: 108 | 108 passed | 0 failed |
[doctest] Status: SUCCESS!
[11:54:51] [Test] TestRunner: suite 'hierarchy-activity' PASSED.
VERIFY_EXIT=0
```

同一份日志里的其余断言逐条无失败（`grep -c "^  FAIL" = 0`），并包含：
`PASS: NumberOfFunctions=1283 <= 3000, ordinal headroom=64252 >= 60000`、
`PASS engine exports did not increase 1283 (baseline 1549)`、`PASS: editor/module boundary holds`。

smoke 套件清单（9 套，新增最后一项）：

```
object-manager / serialization-archive / asset-ref-type / scene-property-ref-type /
asset-manager / reflection-function / material-ir / screen-ui-game-viewport /
hierarchy-activity
```

**新用例确实进门禁的两个开关**：
`minEngine/src/Runtime/Test/TestSuiteRegistry.cpp` 的 `kSmokeOrder` 追加 `"hierarchy-activity"`
（只置 `InSmoke` 会被静默跳过）；`Tests/TestSuiteRegistration.cpp` 的
`HierarchyActivityTestSuiteTraits::BuildMetadata` 返回 `InSmoke=true`；
三个 `TEST_CASE` 都带 `[smoke]` tag（`DoctestSuiteRunner` 的 smoke 过滤是 `*<id>*[smoke]*`）。

## 9. 偏差与未完成项

1. **§H8 的 5 处 Self 组成与设计表不完全一致**（数值仍是 5，见 §4）：
   设计表的 `Component.cpp:77/79/84` 三行同属 `SetActive` 函数体且 `:84` 是字段写入本身，
   落地为 1 处 `IsActiveSelf()`；腾出的位置给了 `Component.h:58`（§H3 公式原样写法）与
   `GameObject.h:134`（GO 侧同构去重，§H8 成文时 GO 还没有 flag）。
   **没有任何行为门写 `IsActiveSelf()`**，这是该审计真正要保证的事。
2. **`UISystem.cpp` 行为门前移**：设计表记 `:410`，本 checkout 是 `:448`（设计成文后该文件有过改动）。
   分类与计数不变。
3. **O8 的 `m_bAppliedEffective` 一致性**改为计数器等价断言（见 §6.3 偏差说明）。
4. **O8 的「60 帧」用 `scene->Tick` 驱动**：`GameObject::Tick` 不在冻结的导出契约里，
   `minEngineTests.exe` 是 DLL consumer 调不到它；`Scene::Tick` 是导出符号且内部正是逐 GO `Tick`，
   语义等价。
5. **`SyncActivationWithActiveFlag` 符号名保留**（见 §5）：§H5 的改名只落在 API 表面，
   因为该符号冻结在 `.def` 且是 Editor DLL 的链接契约；删名会同时触发
   「no-removals-vs-baseline」与 Editor 链接失败。
6. **`SceneComponent::SetVisible` 未做级联 reconcile**：visible 是正交的呈现轴（§D3），
   只影响自身绘制位，翻转时 `MarkRenderStateDirty()`，不触碰系统注册。子树只经 `GameObject`
   的祖先项受影响，那一路由 push-down 覆盖。
7. **S03–S07 的消费面未动**（按批次边界）：UI 布局 / UI 命中 / `WidgetComponent::FillSceneProxy` /
   `RenderScene` 五门 / `ForwardRenderer` 两处守卫 / 3D 三组件 / `Scene::Tick` 子树跳过都还是今天的代码。
   因此「父 GO 关掉后 UI 仍可能被画/被点」这类现象**在本批之后依然存在**，由 S03/S04/S05 清零。
8. **O2/O3/O4/O5/O6/O7/O9 未做**：属于后续切片（`§切片` 表）。
9. **备注（非本批改动）**：`git status` 里 `docs/ai/Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md`
   显示为已修改，本批**从未读写**该文件（进批次前即为此状态）。
10. **`SetRootComponent` 的传播收窄**：只在 `m_Parent` 真的变化（`RebindRootToGameObjectParent`
    溶解了 GO 边）时才传播。纯 Root 换位不改祖先链，不需要重算；更重要的是
    `RemoveComponent` 内部就会调 `SetRootComponent`，在那里 reconcile 会把「即将被摘除」的组件
    重新注册进系统（churn），收窄后没有这条多余路径。
11. **组件遍历改为逐元素快照**（`ApplyEffectiveStateToComponents`）：reconcile 可能通过
    `ApplyActivationToSystems` 间接改动 `m_Components`，range-for 会悬垂（§F10）。
    快照 `shared_ptr` 的元素 + 每轮重读 `size()`，代价是一次引用计数自增。
---

## 10. 本批排查中发现、但**不是本批引入**的缺陷（建议单独开 bug）

**现象**：`minEngineTests.exe test prefab`（filter `*prefab*`，同时覆盖 `prefab` 与
`prefab-overrides`）**间歇性** SIGSEGV（exit 139）。实测 20 次 8 次崩、12 次过；崩溃点固定在
`prefab-overrides propagate skips overridden and root transform` 用例收尾处，
日志最后一行总是 `GameObject with ID 0 and name 'PropagatedName' is being destroyed.`
（`test full` 也在同一区域崩过一次）。**`test smoke` 与全部 13 个相关套件在 6 次 / 39 次重复下 0 失败。**

**机制（代码路径本批一行未动）**：
1. `PrefabUtility::CreatePrefabFromGameObject` 用序列化把子树克隆成 **prefab 模板树**
   （`PrefabUtility.cpp:45-62`）。序列化**只恢复 `m_Parent`**，而 `m_Children` 不是 `ME_PROPERTY`
   （`GameObject.h`）—— `PrefabOverridesTest.cpp:129-130` 的注释就是这件事：
   *"Prefab template trees keep parent Guid links but do not rebuild m_Children"*。
   全仓 `Prefab/*.cpp` 对 `m_Children` 的引用数为 **0**。
2. 因此模板子树里 **子节点的 `m_Parent` 指向父节点，但父节点的 `m_Children` 是空的**。
3. `Prefab` 的 `std::vector<std::shared_ptr<GameObject>> m_TemplateObjects`
   （`Prefab.h:40`）按**正序**析构：先 root（'PropagatedName'），后 child（'Child'）。
4. `~GameObject(root)` 的 children 循环遍历的是**空的** `m_Children`，所以它**无法**把 child 的
   `m_Parent` 置空（`GameObject.cpp:34-49`）。
5. root 被释放后，child 的 `~GameObject` → `UnlinkFromCurrentParent()` 执行
   `auto& siblings = m_Parent->m_Children;` —— **对已释放内存解引用**（`GameObject.cpp:524-534`）。
   若该块尚未被复用，读到的仍是「空 vector」，`remove/erase` 是空操作 → 静默通过；
   若已被复用/poisoned → SIGSEGV。**这就是它间歇的原因。**

**为什么与 CORE-F43 无关**：`UnlinkFromCurrentParent` / `m_Parent` / `m_Children` 的语义与代码
本批完全没碰（本批只给 `AttachToParent` / `DetachFromParent` / `SetRootComponent` 在**链真的变化**
后加了一次传播调用）。本批让 `GameObject` / `Component` 变大（新增缓存位与作者化字段），
**改变了堆布局**，从而改变了这块悬垂内存被复用/污染的概率——所以**复现率上升，但缺陷本身在改动前就成立**。

**建议修复（未在本批实施，属于 prefab/序列化域的独立改动）**：
在 `Prefab` 释放模板对象之前先把模板子节点的父链断开，两种等价落点：
- `Prefab::ClearTemplateObjects()` / `~Prefab` 里：对 `m_TemplateObjects` 先逐个
  `DetachFromParent(KeepWorldTransform)`，再 `clear()`（6 行，语义安全：prefab 正在消失）；
- 或让模板树在克隆后**重建 `m_Children`**（更根本，但会牵动 override 路径的
  「模板树不走 `m_Children`」约定，需要单独设计）。
**在这两处之外加任何「防御性 `m_Parent` 判空」都是无效的**——悬垂指针的判空本身就要解引用。

## 11. 回退

本批**四个可独立回退的落点**（全部无数据迁移、无 .def 变更）：

1. `git checkout -- minEngine/Tests/Suites minEngine/Tests/TestSuiteRegistration.cpp` 之类的全量回退不推荐
   （会带走 Registry 的 id 追加）。推荐按下表顺序回退；
2. 字典：先撤 `Table` 之外的两处消费面（Editor `SceneEditorInspectorSource.cpp` / `SceneEditor.cpp`）→
   再撤 `SceneManager.cpp` 两阶段与 `ColliderComponent.cpp` 直写兜底 →
   最后撤 `GameObject.h/.cpp`、`SceneComponent.h/.cpp`、`Component.h/.cpp`。
   撤掉最后三个类之后 `IsActive()` 回到「自身 flag」语义，所有消费面（含 33 处行为门）自动回到改动前；
3. `Generated/Reflection/*.gen.*` 不要手改：撤掉 `ME_PROPERTY` 后重新构建，`reflection_codegen` 会自己把
   这些文件还原；
4. 场景 / prefab / 资产**无需回退**：新字段缺省 `true`，旧存档从未写过它们，读了也是 `true`。
---

## 12. 逐文件状态（`git status --porcelain`，未执行任何 `git add`）

```
 M minEngine/Editor/src/SubEditor/Scene/SceneEditor.cpp
 M minEngine/Editor/src/SubEditor/Scene/SceneEditorInspectorSource.cpp
 M minEngine/Tests/TestSuiteRegistration.cpp
 M minEngine/minEngine/src/Generated/Reflection/GameObject.gen.cpp
 M minEngine/minEngine/src/Generated/Reflection/GameObject.gen.h
 M minEngine/minEngine/src/Generated/Reflection/SceneComponent.gen.cpp
 M minEngine/minEngine/src/Generated/Reflection/SceneComponent.gen.h
 M minEngine/minEngine/src/Runtime/Function/Framework/Components/Component.cpp
 M minEngine/minEngine/src/Runtime/Function/Framework/Components/Component.h
 M minEngine/minEngine/src/Runtime/Function/Framework/Components/SceneComponent.cpp
 M minEngine/minEngine/src/Runtime/Function/Framework/Components/SceneComponent.h
 M minEngine/minEngine/src/Runtime/Function/Framework/GameObject/GameObject.cpp
 M minEngine/minEngine/src/Runtime/Function/Framework/GameObject/GameObject.h
 M minEngine/minEngine/src/Runtime/Function/Framework/Scene/SceneManager.cpp
 M minEngine/minEngine/src/Runtime/Function/Physics/ColliderComponent.cpp
 M minEngine/minEngine/src/Runtime/Test/TestSuiteRegistry.cpp
?? minEngine/Tests/Suites/HierarchyActivityTest.cpp
?? minEngine/Tests/Suites/HierarchyActivityTest.h
?? docs/ai/Platform/Core/CORE-F43_S01_S02_IMPLEMENTATION.md
```

同一次 `git status` 里还有两条**不属于本批**的条目，这里明确区分：

- `?? docs/ai/Platform/Core/CORE-F43_GO_COMPONENT_ACTIVITY_AND_VISIBILITY_DESIGN.md` —— 本批的权威输入，
  未跟踪但**未改动**；
- ` M docs/ai/Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md` —— 进批次前就是已修改状态，本批**从未读写**。
