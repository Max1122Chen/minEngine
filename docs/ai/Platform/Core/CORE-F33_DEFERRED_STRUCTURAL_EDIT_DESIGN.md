# CORE-F33 — 运行期场景结构变更契约（Spawn 即时入世 · Destroy 延迟 · EOF 快照）

## Meta
- **ID:** `CORE-F33`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `In Progress`
- **Owner:** project maintainer
- **Last updated:** `2026-10-12`（S01+S02 已实现；待你个人 review）
- **Related:**
  - 催化剂（minCraft 反哺）：`minCraft-gameplay/docs/ai/INFRA/ENGINE_FEEDBACK_PROMPT_DEFERRED_STRUCTURAL_EDIT.md`
  - 复盘：`minCraft-gameplay/docs/ai/sessions/2026-10-12-runtime-structural-mutation-review.md`
  - 生命周期：[CORE-F31](./CORE-F31_BEGIN_END_PLAY_LIFECYCLE_DESIGN.md)
  - Play Mode：[CORE-F05](./CORE-F05_PLAY_MODE_DESIGN.md)
  - Prefab：[CORE-F23](./CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md)
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) / [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F31`（BeginPlay / EndPlay 已 Done）
- **Blocks:** minCraft 运行期 chunk / HUD / mob 结构面重建；任何模拟中增删 GO 的玩法

## TL;DR

运行期增删 GO 当前不安全（`m_GameObjects` / EOF 向量在 range-for 中被改）。**修订后的契约（更贴 Unity/UE）：**

1. **`SpawnGameObject`：立刻入 `m_GameObjects` + byId** → `Find` / `GetAll` 马上可见；若已 Play → **立刻 BeginPlay**  
2. **`Scene::Tick`（及同类 GO 遍历）改为「开头定长」下标循环** → Tick 中 Spawn 的新 GO **同帧不 Tick**（落在定长之外）  
3. **`DestroyGameObject`：只延迟** → 标记 pending，同帧跳过 Tick，EOF 之后 Flush 再真正 `erase`  
4. **EOF：swap 快照** → 允许 Dirty 扩散，本趟不吃，新 Dirty 留到下一帧  

> 早期草稿曾把 Spawn 也推迟到 Flush 才入表——那更像 Bevy command buffer，会导致 `GetAll`/`Find`（若未特殊处理）迷惑。自审后**放弃「Spawn 延迟入表」**。

## Reader quick start

1. 问题：运行期改 GO 图无安全契约  
2. 业界：**Destroy 延迟是共识**；**Spawn 入世 Unity/UE/Godot 偏即时**（Bevy 才偏命令延后）  
3. 本方案：Spawn 即时入世 + Tick 定长；Destroy 延迟 Flush；EOF swap  
4. 切片：S01 EOF + Tick 定长 → S02 Destroy 队列 + Spawn API；S03 Prefab 可后置  

---

## Problem

### 根因错位

| 角色 | 隐含假设 |
|------|----------|
| 引擎现状 | GO 列表主要是序列化 / 编辑器资产结构，不在模拟遍历中途改 |
| 真实游戏（minCraft） | 流式、HUD、刷怪需在模拟循环中持续 spawn/destroy |

### 已核对的失败模式

| 容器 | 遍历点 | 变异 | 后果 |
|------|--------|------|------|
| `Scene::m_GameObjects` | `Tick` / `DispatchBeginPlay` / `DispatchEndPlay` / `ResolvePendingActivationsForScene` | `push_back` / `erase` | 迭代器失效 / UB |
| `SceneManager::m_ComponentsThatNeedEndOfFrameUpdate` | `SendAllEndOfFrameUpdates` | Mark→`push_back`；销毁→Unmark→`erase` | UB；或新 Mark 落在已捕获的 `end()` 之后，再被 `clear()` 清掉且从未 `DoEndOfFrameUpdate`（网格不上屏 → 空洞） |

模拟循环里**不存在**可安全调用即时原语的时刻。把 spawn「挪到 EOF」只是换容器崩溃（minCraft 已证伪）。

---

## 业界怎么做（对照，非照搬）

> **本机源码已核对（2026-10-12）：**  
> - Unreal：`D:/Dev/GitRepo/UnrealEngine`  
> - Godot：`D:/Dev/GitRepo/Godot`  
> 下列带路径的结论以源码为准；Unity/Bevy 仍为公开语义对照。

### 先拆四个阶段（避免混谈）

| 阶段 | 含义 |
|------|------|
| **A. 可拿引用** | 调用方拿到句柄，能设 Transform、加组件 |
| **B. 进入可遍历世界** | 出现在场景/世界的结构列表里 |
| **C. 第一次模拟生命周期** | Unity `Start` / UE `BeginPlay` / Godot `_ready` |
| **D. 第一次常规 Tick** | 进入每帧 Update / Tick |

「创建帧即 BeginPlay」通常问的是 **C**；很多人其实在问 **D**。两者可以不同帧。

### 共性：Destroy 几乎都延迟

| 引擎 | API | 时机 |
|------|-----|------|
| Unity | `Object.Destroy` | 标记 pending，**本帧各系统 Update 之后**再真正销毁 |
| Unreal | `AActor::Destroy` | pending-kill / 延迟清理；不在遍历中途硬 erase |
| Godot | `Node.queue_free()` | 入队，**帧末 / idle** 再释放 |
| Bevy | `Commands.despawn` | 入 command buffer，**系统批次之间 apply** |

**结论：延迟销毁是业界共识。** minEngine 应对齐。

### 分歧：Spawn 与 BeginPlay（C）是否同帧

#### Unity

- `Instantiate`：**同步**完成 A；`Awake` /（激活时）`OnEnable` 往往在返回前跑完。  
- `Start`：**推迟**到该对象第一次 Update **之前**（通常仍是「启用后进入调度的那一帧」，不是硬性「再白等一整帧」）。  
- 因此：创建立刻可用；`Start` ≠ 构造当下；完整 Update 紧跟在 `Start` 之后。

#### Unreal（本机源码）

- **立刻入 Level 列表（B）：** `UWorld::SpawnActor` 在 `PostSpawnInitialize` **之前**调用 `LevelToSpawnIn->TryAddActorToList(Actor, …)`（`LevelActor.cpp` ~738–754）。Spawn 返回前 Actor 已在世界列表 → Find 可达。  
- **BeginPlay（C）在 Spawn 完成路径上同步触发：** `AActor::FinishSpawning` 中  
  `bRunBeginPlay = !bDeferBeginPlayAndUpdateOverlaps && (BeginPlayCallDepth > 0 || World->HasBegunPlay())`，为真则 `DispatchBeginPlay()`（`Actor.cpp` ~4467–4487）。  
- **Destroy 延迟 / 防重入：** `AActor::Destroy` → `UWorld::DestroyActor`；已 `IsPendingKillPending()` 则直接成功返回（~856–858）。若正在 `IsActorBeginningPlay()`，不立刻拆，只设 `bActorWantsDestroyDuringBeginPlay`，BeginPlay 结束后再 `DestroyActor`（~895–902；`DispatchBeginPlay` ~4716–4720）。最终从 Level 列表 `RemoveActor`（~1032–1033）。  
- **Tick（D）：** `BeginPlay` 内 `RegisterAllActorTickFunctions`（`Actor.cpp` ~4743）——新 Actor **有可能**在注册后的后续 Tick Group **同帧**被 Tick。我们选「定长 → 同帧不 Tick」比 UE **更保守**，可证、可测，刻意取舍。

#### Godot（本机源码）

- **立刻进父子表（B）：** `Node::_add_child_nocheck` 立刻 `data.children.insert` / cache；若父已在树则 `_set_tree`（`node.cpp` ~1652–1691）。`add_child` 在 `data.blocked > 0` 时直接失败并建议 `call_deferred`（~1709）——**结构遍历中禁改树**（与我们的守卫同族）。  
- **`_ready`（C）：** `_set_tree` 进入树后，若无父或父已 `ready_notified`，**同步** `_propagate_ready()` → `NOTIFICATION_READY`（~3376–3380）。注释：子节点在 `_ready` 期间被加入也能正确拿到 `_ready`（~4120）。**不是**「必须下一帧」；是「进树且父已 ready 则当帧可 ready」。  
- **Destroy 延迟：** `queue_free` → `SceneTree::queue_delete` 入 `delete_queue`（`node.cpp` ~3469–3477；`scene_tree.cpp` ~1612–1616）。`_flush_delete_queue` 在 `physics`/`process` **末尾**调用（~655、~724），注释写明希望「本帧处理里删的东西，本帧末尾真删掉」（~654–655）。  
- **遍历安全：** group call 用 `nodes_removed_on_group_call` 跳过本趟已移除节点（~399+）——「本趟集合受保护 / 变更延后可见」与 EOF swap 同族。

#### Bevy / ECS

- `Commands.spawn` 只入队；批次后 **apply** 才进入 World（B）。  
- 无统一 BeginPlay；等价物是 apply 之后第一个读到该实体的系统（同一次 schedule 内即可）。

### 对照表

| 引擎 | A 引用 | B 入表 / Find | C 第一次生命周期 | D 第一次 Tick | Destroy |
|------|--------|---------------|------------------|---------------|---------|
| Unity | 立即 | **立即**（Find 找得到） | `Awake` 立即；`Start` 在 Update 前 | 常同帧（Start 后 Update） | 帧末 |
| UE | 立即 | **Spawn 完成即入 World**（Find 找得到） | **默认 Spawn 完成时 BeginPlay** | 视 Tick Group | 延迟 |
| Godot | 立即 | **`add_child` 即进树** | 父已 ready 则**同步 `_ready`** | 之后的 `_process` | `queue_free` → 帧末 flush |
| Bevy | 命令/句柄 | **apply 后**才进 World | apply 后首个相关系统 | 同左 | despawn 延迟 apply |
| **minEngine（修订后）** | `Spawn` 立即返回 | **立即**入 `m_GameObjects`+byId | **Spawn 时 BeginPlay**（若已 Play） | **同帧不 Tick**（定长遍历）；下帧第一次 Tick | Flush 延迟移除 |

### 诚实对照：我们并不是「和所有引擎一模一样」

| 点 | 是否业界主流 | 说明 |
|----|--------------|------|
| Destroy 延迟 | **是** | Unity / UE / Godot / Bevy 高度一致 |
| Spawn 后立刻能 Find / 出现在世界列表 | **Unity/UE/Godot 是；Bevy 否** | 玩法直觉强依赖这一点 |
| Spawn 后同帧是否 Tick | **不统一** | Unity 常同帧；UE 可在 BeginPlay 注册 Tick 后同帧进后续 Group；**我们刻意更保守（定长不 Tick）** |
| EOF/脏列表 swap 本趟封闭 | **常见** | 脏扩散留次帧 / 次 phase，避免生长 for 死循环 |
| Spawn 也进 command buffer 拖到帧末才入世 | **主要是 Bevy 系** | 早期草稿做过；**因 Find/GetAll 迷惑已否决** |

### 自审：Find 迷惑与修订动机

若 Tick 中 `Spawn` **不**立刻进 `m_GameObjects`：

- `GetAllGameObjects()` 看不到新 GO → 用户会以为「没刷出来」  
- 即便 `FindById` 特判 pending，两套可见性（byId 有、GetAll 无）仍易踩坑  
- Unity/UE 用户心智是：**Instantiate/SpawnActor 返回后，世界里已经有它**

因此修订为：

- **入世（B）与 Find：即时**（贴 Unity/UE）  
- **同帧不 Tick（D）：靠 Tick 定长下标，不靠「瞒过列表」**  
- **Destroy：仍只延迟**（真正危险的是 erase，不是 push_back）

**默认锁定：** Spawn 即时入世 + 即时 BeginPlay（已 Play 时）；同帧不 Tick；Destroy 延迟到 EOF 后 Flush；EOF Dirty 扩散 → 下帧。

### 源码锚点一览（核对用）

| 主题 | 引擎 | 锚点 |
|------|------|------|
| Spawn 先入列表 | UE | `LevelActor.cpp` `TryAddActorToList` ~738 → 再 `PostSpawnInitialize` |
| Spawn 时 BeginPlay | UE | `Actor.cpp` `FinishSpawning` ~4467–4487 `DispatchBeginPlay` |
| BeginPlay 中 Destroy 推迟 | UE | `LevelActor.cpp` `DestroyActor` ~895–902；`DispatchBeginPlay` ~4716–4720 |
| Destroy 去重 / 真正移除 | UE | `IsPendingKillPending` ~856；`RemoveActor` ~1032 |
| add_child 即时 + blocked | Godot | `node.cpp` `_add_child_nocheck` / `add_child` ~1652–1709 |
| 进树后同步 ready | Godot | `node.cpp` `_set_tree` ~3376–3380 |
| 延迟删除 + 帧末 flush | Godot | `queue_free` / `queue_delete`；`scene_tree.cpp` `_flush_delete_queue` ~655/~724~1600 |
| 组调用跳过已删节点 | Godot | `scene_tree.cpp` `nodes_removed_on_group_call` |

### 源码核对后对设计的补强（相对仅「公开文档」）

1. **BeginPlay / `_ready` 期间再 Destroy：** 对齐 UE——若在 `BeginPlay` 调用栈内 `Destroy`，**不要立刻 erase**，记 pending，等当前 BeginPlay 返回后再进入正常延迟销毁路径（避免半初始化拆对象）。  
2. **同帧 Tick：** 明确写成 **相对 UE 的有意收紧**，验收只要求「定长外不 Tick」，不声称「与 UE Tick 时机一致」。  
3. **结构守卫：** Godot `data.blocked` 证明「树搭建中禁止结构性 mutate」有先例；我们对 `Remove` 打 Error、对 Tick 定长允许 `push_back`，方向正确。  
4. **Flush 时机：** Godot 在 process **末尾** `_flush_delete_queue`；我们 EOF 后再 Flush Destroy——同族（逻辑更新完再真删）。

---

## Requirements

1. 可在 `Tick` / `OnBeginPlay` / `OnEndPlay` / `DoEndOfFrameUpdate` 中调用 **Spawn / Destroy** 而无 UB。  
2. `Spawn` 返回后：`FindGameObjectById` 与 `GetAllGameObjects` **立刻**能见到该 GO。  
3. Tick 中 `Spawn` 的新 GO：**同帧不再被 `Scene::Tick`**；下一帧第一次 Tick。  
4. 已 Play 时 `Spawn`：**调用返回前**完成 BeginPlay（对齐 CORE-F31 / UE）。  
5. `Destroy` 后同帧起不 Tick；真正 `erase` 在 Flush（EOF 之后）。  
6. EOF：允许 Dirty 扩散；本趟只处理 swap 快照；新 Dirty 不得被 clear 吃掉，须在后续 `SendAll` 处理。  
7. 编辑器 / 加载仍可用即时 `Create`/`Remove`；玩法 Destroy 走延迟 API。  
8. 测试覆盖保障 1/2 与 Find 即时可见。

## Constraints

- 机制在 Core（`Scene` / `SceneManager` / `Engine::LogicalTick`）；体素 / UI 政策不进 Core。  
- Forward-only：玩法推荐路径只有 `Spawn`/`Destroy`；不做长期「秘密自动 defer」。  
- 与 CORE-F31 组合；Runtime 不依赖 Editor 类型。  
- 验证：具名 `minEngineTests` suite。

## Preferences

- 显式 `Spawn`/`Destroy` 玩法 API；创作仍可用 `Create`/`Remove`。  
- Tick / Dispatch* / ResolveActivations：**禁止**对 `m_GameObjects` 使用会「吃到中途 push_back、又怕 erase」的裸 range-for；统一定长或快照。  
- Find/GetAll 心智对齐 Unity/UE：Spawn 返回后世界里已有。

## Assumptions

- MVP 单线程。  
- 「同帧 Spawn 不 Tick、下帧才 Tick」可接受（比 Unity 保守；比「找不到 GO」好教）。  
- BeginPlay 在 Spawn 调用栈内同步执行；嵌套 Spawn 同样即时入世，仍受外层 Tick 定长保护不进入本趟 Tick。

## Non-Goals

- 完整 ECS command buffer（Spawn 侧不走 Bevy 式延后入世）。  
- 强制迁光所有 `CreateGameObject` 测试。  
- Core 内 chunk / HUD 政策。  
- 生长式 EOF for / 同帧无限 drain Dirty。

---

## Current Architecture

```text
Engine::LogicalTick
  Input → Scene::Tick   // range-for m_GameObjects（中途 Create/Remove 不安全）
  → Audio / Physics / UI
  → SendAllEndOfFrameUpdates  // range-for + clear（扩散 Dirty 会被吃掉）
```

## Proposed Architecture

### 心智模型（修订后）

| API | 入世 / Find | Tick | BeginPlay | 真正从列表移除 |
|-----|-------------|------|-----------|----------------|
| `CreateGameObject` | 即时 | 若在定长 Tick 中调用，同帧不 Tick* | 已 Play 则立刻 | `Remove` 即时（创作向；遍历中禁） |
| `SpawnGameObject` | **即时** | **同帧不 Tick**（定长） | 已 Play 则**立刻** | 经 `Destroy` → Flush |
| `DestroyGameObject` | 仍可 Find（dying） | 同帧起跳过 | — | **Flush 时** erase |
| `RemoveGameObjectById` | — | — | — | **即时**（创作/工具；遍历中 Error） |

\*创作路径若在 Tick 中误用 `Create`，定长循环下也不会同帧 Tick，但仍应走 `Spawn`。

```text
玩法：
  Spawn  → 立刻 push_back + byId +（可选）BeginPlay
  Destroy → 只标记 pending-destroy
  Tick   → for i in [0, startCount)：跳过 pending-destroy；不扫本趟新 append 的尾巴
  EOF    → swap 快照处理；新 Dirty 进空列表
  Flush  → 只应用 pending Destroy（真正 erase）
```

### 保障 1：Tick 内 Spawn → 立刻可 Find，同帧不 Tick

```text
Frame N: GO_A::Tick → Spawn(GO_B)
期望：
  - Find(GO_B) / GetAll 立刻成功
  - GO_B 本帧不再 Tick
  - 若已 Play：Spawn 返回前 GO_B 已 BeginPlay
  - Frame N+1：GO_B 第一次 Tick
```

**实现（贴 Unity/UE 入世，用定长保证 D）：**

```text
Scene::Tick(dt):
  const size_t count = m_GameObjects.size();   // 本趟水位
  for (size_t i = 0; i < count; ++i):
    go = m_GameObjects[i]
    if (!go || IsPendingDestroy(go)) continue
    go->Tick(dt)
    // 途中 Spawn → push_back，下标 >= count → 本趟不会 Tick 到

SpawnGameObject():
  go = NewObject…; 设 ID; push_back; byId[id]=go
  if (HasBegunPlay()) go->BeginPlay()   // 可再 Spawn；仍只 append
  return go
```

| 不变量 | 谁维护 |
|--------|--------|
| Spawn 后 `Find`/`GetAll` 立刻可见 | `Spawn` 即时 `push_back` + byId |
| 本趟 Tick 看不到「Tick 开始之后」append 的 GO | `Tick` 定长 `count` |
| pending-destroy 不 Tick | Tick 内跳过 |
| `push_back` 引起 reallocation 仍安全 | **每次用下标取 `m_GameObjects[i]`**，不用跨迭代的裸 iterator |

`DispatchBeginPlay` / `DispatchEndPlay` / `ResolvePendingActivationsForScene` 同样改为定长（或开头 snapshot `vector<shared_ptr>`），避免同类重入。

**已否决：Spawn 延迟入表。** 原因：Find/GetAll 双可见性迷惑；偏离 Unity/UE。

### 保障 2：EOF 中再次 Mark Dirty —— 允许扩散，本趟不吃（swap 快照）

关心的场景：

```text
SendAllEndOfFrameUpdates 正在处理 CompA
  → CompA::DoEndOfFrameUpdate() 里改了 CompB（或 Spawn 后 Flush 前 Mark，或同趟里别的 GO Mark）
  → CompB 需要 EndOfFrameUpdate
此时怎么处理？
```

#### 三种业界常见策略

| 策略 | 做法 | 优点 | 风险 |
|------|------|------|------|
| **① 生长式 for**（`for (i=0; i<v.size(); ++i)`，途中 `push_back`） | 本趟继续处理新 Dirty | 同帧收敛快 | A↔B 互相 Mark 可死循环；需 generation /「本趟已处理」位 |
| **② 源头禁止** | EOF 回调里禁止再 Mark | 简单 | 难强制；合法的「改 Transform → 要刷 Proxy」会被掐死；游戏侧难写 |
| **③ 快照 / swap，新 Dirty 进下一趟**（推荐） | 本趟集合固定；途中 Mark 进空列表，留给下次 `SendAll…` | 有界、无死循环、语义清晰 | 扩散最多晚一帧（通常可接受） |

Unity/UE 一类「脏标记 → 帧末刷新」系统，实务上更接近 **③**：本帧收集的一批固定处理；处理过程中新脏往往进**下一帧**的收集（或下一 phase），而不是无限拉长当前 for。Bevy 则是 phase 之间 apply，本 system 跑完才见新命令——也是「本趟封闭」。

**① 不作为默认：** 同帧收敛好看，但 EOF 里互相 MarkRenderStateDirty 很容易环；引擎还要再发明「本趟已更新」集合，复杂度和 bug 面上升。

**② 不作为默认：** 与 SceneComponent 脏路径冲突；无法从类型系统禁止。

#### minEngine 锁定：策略 ③（swap 快照 → 下一帧）

正是你描述的那种写法：

```text
SendAllEndOfFrameUpdates:
  // 1) 快照：本趟只处理「调用前已 Dirty」的集合；快照后活动列表为空
  localSnapshot.swap(m_ComponentsThatNeedEndOfFrameUpdate)
  // 此刻 m_ComponentsThatNeedEndOfFrameUpdate 为空，可承接「扩散 Dirty」

  // 2) 只遍历不会再增长的 snapshot（range-for / 下标均可；禁止边扫边往 snapshot 里 push）
  for (Component* c : localSnapshot):
    if (!c) continue
    c->SetMarked(...Unmarked)     // 允许之后再次 Mark
    c->DoEndOfFrameUpdate()       // 若内部又 Mark → push 进「空的」活动向量

  // 3) 不在这里 clear 活动向量！
  //    活动向量里剩下的 = 本趟扩散出来的 Dirty = 留到【下一次】SendAllEndOfFrameUpdates
  //    MVP：每 LogicalTick 只调一次 SendAll… → 即留到【下一帧】
```

| 问题 | 答案 |
|------|------|
| 允许 Dirty 扩散吗？ | **允许**（合法：改 Transform / 建 Proxy 链） |
| 本趟 for 吃扩散吗？ | **不吃**（不搞 `i < size()` 生长 for） |
| 从源头禁止吗？ | **不禁止** |
| 如何留到下一帧？ | swap 后新 Mark 进活动向量；本函数结束时**不清空**活动向量；下帧再 `SendAll` 时再 swap 走 |
| 和旧 bug 的差别？ | 旧代码：带着旧 `end()` 扫完再 `clear()` → 扩散 Dirty 被 clear 吃掉且 Marked 卡死。新代码：扩散 Dirty 活在活动向量里，下帧一定还能被 swap 出来 |

**同帧二次 drain（可选，默认不做）：**

```text
SendAll… 之后 while (!empty && passes < K) 再 swap 一趟
```

能让部分扩散同帧收敛，但 K 要设上限，且仍可能把「本应下帧」的工作拉进本帧。MVP **不做**；若 minCraft 出现「必须同帧二次 EOF 才上屏」，再开切片评估。

**与 Spawn 的交叉：** Tick/EOF 中 `Spawn` 已入世；若 `DoEndOfFrameUpdate` / `BeginPlay` 触发 Mark，新 Dirty 进活动向量 → **下帧 EOF**。新 GO 下帧才 Tick，Proxy 多半下帧 EOF 建立——可接受。Flush **不再**负责 Spawn 入表，只负责 Destroy erase（及清理 pending 标记）。

### EOF 还可再钉的两点（明晰之上的收紧）

1. **Unmark 时机：** 进入 snapshot 处理时先 Unmark，再 `DoEndOfFrameUpdate`；这样回调里再次 Mark 是「新一票 Dirty」，不会被「仍 Marked 直接 return」吞掉。  
2. **指针寿命：** 因 Destroy 延迟到 EOF **之后** Flush，本趟 snapshot 里的 `Component*` 在 `DoEndOfFrameUpdate` 期间保持有效（禁止在 EOF 回调里走即时 `RemoveGameObjectById`）。  
3. **同帧二次 drain：** 默认不做；避免与「下帧处理扩散」契约打架。

### 结构遍历守卫

- 对 `m_GameObjects` 的模拟向遍历：必须定长/快照。  
- `RemoveGameObjectById` 在遍历 depth>0：Error（+ Debug assert）。  
- `DestroyGameObject` 始终安全（只标记）。  
- `Spawn`/`Create` 的 `push_back` 在定长 Tick 下安全；仍推荐玩法走 `Spawn`。

## Responsibilities

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `Scene` | Spawn 即时入世；Destroy 队列；Tick/Dispatch 定长；Flush 应用 Destroy | EOF 列表 |
| `SceneManager` | EOF swap；编排 Flush Destroy；可选 depth | Prefab 政策 |
| `Engine` | EOF 后调用 Flush | GO 语义 |
| 游戏 | 用 Spawn/Destroy | 给容器打补丁 |

## Ownership & Lifetime

- **Spawn：** 立刻 `NewObject` + ID + `push_back` + byId；已 Play 则立刻 BeginPlay。  
- **Destroy：** 标记 pending-destroy；Tick 跳过；Flush 时 `RemoveGameObjectById` 真删。  
- **BeginPlay 栈内 Destroy（对齐 UE）：** 若 `GO::BeginPlay` / `OnBeginPlay` / 组件 BeginPlay 尚未返回，只记 pending，**禁止**在栈内立刻 erase；当前 BeginPlay 返回后对象已是 pending-destroy，由本帧末 Flush（或下一 Flush）真删。  
- **dying：** Flush 前仍可 `Find`；`GetAll` 默认仍包含直至 erase；提供 `IsPendingDestroy`。

## Control Flow

```text
Input → Scene Tick（定长）→ Audio → Physics → UI
→ SendAllEndOfFrameUpdates   // swap 快照
→ FlushDeferredDestroys      // 只 erase pending-destroy
```

## State & Invariants

- Spawn 后立刻在 `m_GameObjects` / byId / Find / GetAll。  
- Tick 定长 → 本趟新 Spawn 不 Tick。  
- pending-destroy：不 Tick；Flush 前可 Find。  
- BeginPlay：Spawn 路径上、已 Play 时同步一次。  
- EOF Dirty 扩散 → 下帧；禁止默认生长 for。

## API / Interface Semantics

- `Scene::SpawnGameObject()` → 立刻入世的 `shared_ptr`；已 Play 则返回前 BeginPlay。  
- `Scene::DestroyGameObject(id)` → 标记延迟销毁。  
- `SceneManager::FlushDeferredDestroys()`（名可定为 `FlushDeferredStructuralEdits`，但语义以 Destroy 为主）→ EOF 后。  
- `Create`/`Remove`：创作向；`Remove` 遍历中禁止。  
- Prefab：S03 可选；运行期 Instantiate 应走与 Spawn 相同的入世/定长契约。

## Failure Model

| 失败 | 行为 |
|------|------|
| Destroy 未知 id | false / no-op |
| 重复 Destroy | no-op（对齐 UE `IsPendingKillPending`） |
| BeginPlay 栈内 Destroy | 只标记 pending；BeginPlay 返回后再 Flush erase（对齐 UE `bActorWantsDestroyDuringBeginPlay`） |
| 遍历中即时 Remove | Error（+ assert） |
| EOF 中即时 Remove | Error；应改 Destroy |

## Alternatives & Trade-offs

- **A'（现行推荐）** Spawn 即时入世 + Tick 定长 + Destroy 延迟 + EOF swap。贴 Unity/UE Find 心智。  
- **A₀（已否决）** Spawn 也延迟到 Flush 才入表 — Bevy 味；Find/GetAll 迷惑。  
- **B** Create/Remove 魔法自动 defer — 不选。  
- **C** 生长式 EOF for — 不选（环风险）。  
- **D** 下一帧才 BeginPlay — 不选（偏离 UE/CORE-F31）。

## Integration Impact

`Scene::Tick`/Dispatch* 改定长；Spawn/Destroy API；EOF swap；Engine 调 Flush Destroy。编辑器 Create 可暂不改。minCraft 运行期改 Spawn/Destroy。

## Migration / Compatibility

- `Create` 保留。  
- 玩法销毁必须迁到 `DestroyGameObject`。  
- 遍历中 `Remove` 打 Error 加压迁移。

## Implementation Slices

| Slice | 范围 | 验证 |
|-------|------|------|
| **S01** | EOF swap；`Scene::Tick`/Dispatch* 定长 | Dirty 扩散下帧；Tick 中 Create/Spawn 不炸 |
| **S02** | `Spawn`/`Destroy` + Flush Destroy；BeginPlay 在 Spawn；BeginPlay 中 Destroy 推迟 | Find 立刻可见；同帧不 Tick；延迟删除；BeginPlay 内 Destroy 不立刻 erase |
| **S03** | Prefab 运行期路径对齐（可选） | Tick 中刷 Prefab |
| **S04** | 文档 / minCraft 交接 | Doc DoD |

**首发：** S01+S02。

## Verification Strategy

1. Tick 内 Spawn → **立刻 Find/GetAll 成功**；同帧 Tick 计数 0；下帧 1  
2. 已 Play 时 Spawn → **返回前** BeginPlay 一次  
3. Destroy → 同帧不 Tick；Flush 后 Find 失败  
4. EOF：A Mark B → 本趟不跑 B；下帧跑 B  
5. 遍历中 Remove → Error  
6. BeginPlay 内 Destroy → 返回后对象仍在列表但 pending；Flush 后消失  

## Open Questions（终审已接受）

1. dying 仍出现在 `GetAllGameObjects` + `IsPendingDestroy` — **Accepted**  
2. S03 Prefab 后置 — **Accepted**  
3. Flush 宽名 `FlushDeferredStructuralEdits`（语义以 Destroy 为主）— **Accepted**  
4. 同帧不 Tick（定长，相对 UE 保守）— **Accepted**  
5. Pending spawn Find / 下一帧 BeginPlay — 已关闭  

## Design Review

- **Verdict:** `Ready with deferred items`（S03 Prefab 后置）
- **Reviewer / date:** maintainer / 2026-10-12
- **Link or summary:** 本机 UE/Godot 锚点核对 + OQ 全接受

## Implementation Readiness

**Ready for implementation?** `yes` — 开工 S01+S02 on `feat/core`。

## Acceptance Checklist

- [ ] Design Review（你个人 code review）  
- [x] 契约：Spawn 即时 Find；定长不 Tick；Destroy 延迟；EOF swap；BeginPlay 内 Destroy 推迟  
- [x] suite `deferred-structural-edit` 绿  
- [ ] minCraft 可接回  

## Philosophy check

| 原则 | 契合 |
|------|------|
| Capabilities, not Opinions | 结构变更机制，无玩法政策 |
| Mechanism over Policy | 入世/销毁/脏列表规则清晰 |
| Prefer Simplicity | 否决 pending Spawn；Destroy 单侧延迟；Tick 定长优于 Tick 注册表 MVP |
| Agent-Friendly | Find/Spawn/Destroy 行为可预期 |

## Changelog

| 日期 | 变更 |
|------|------|
| 2026-10-12 | Draft 英文稿 |
| 2026-10-12 | 正文改中文；业界 BeginPlay 对照 |
| 2026-10-12 | 保障 1/2（当时为 Spawn 延迟入表） |
| 2026-10-12 | 自审：即时入世 + Tick 定长；仅 Destroy 延迟 |
| 2026-10-12 | **本机 UE/Godot 源码核对**：锚点表；修正 Godot `_ready` 描述；补 BeginPlay 中 Destroy 推迟；标明同帧 Tick 相对 UE 的保守取舍 |
