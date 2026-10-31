# BUG-RENDER-025 — 一次 drain 越过了“已公告完成”的帧，把游戏线程正在用的那一帧当成已画完

- **Status:** Fixed
- **First seen:** 真机 Multi 拓扑，minCraft 编辑器，一次 PIE 进出后退出编辑器
- **Area:** Core / Threading / RenderCommandPipe

## 现象

PIE 退出后的关闭阶段，48 条（本例 47 条）命令被拒：

```
RenderCommandPipe: command tagged with frame 126 cannot be applied
(render already drained frame 126); dropped. Queue depth 0.
```

被丢的是场景 teardown 排队的 `RenderScene::RemovePrimitive`。丢命令等于场景变更丢失：
代理留着、场景被销毁，正是 `SceneManager` 那条“销毁前先把代理摘干净”的注释要防的事。

## 根因

`RenderCommandPipe` 的标签机制本身没有第三条来源：`Enqueue` 与 `SubmitViewIntent` 都在队列
锁内用 `NewEntryFrame(Current, Completed)` 取样，标签只能来自那两个已发布的计数器。缺的是
**另一侧**：一次 drain 可以声明“已画到某一帧”，而这一帧从没被公告为完成。

`SceneManager` 的关闭冲刷（`SceneManager.cpp` 的 `DrainQueuedRemovals`）调
`FlushApplyForFrameChecked(pipe.GetCurrentGameFrame())` —— `Current` 是 `Completed + 1`，
也就是游戏线程**正在跑**的那一帧。旧实现把这个帧号直接写进 `m_DrawnFrame`：

1. 关闭时 `Completed = 125`、`Current = 126`，冲刷把 drain 标成 126；
2. 之后 teardown 排队的命令按 `NewEntryFrame(126, 125) = 126` 打标签；
3. `Enqueue` 看到 `tag(126) <= drawn(126)`，按“已画完的帧”丢弃 —— 日志里 tag 与 drawn 都是 126。

同一处还让 `CheckPublishedGenerationForDrawnFrame` 去比较一个游戏线程从未提交的帧。

真机插桩（丢弃时打印线程角色 + 三个帧号 + 调用栈，结束后还原）实测：

```
PROBE drop: thread=2973250430 role=Game+Render+RHI tag=93 current=93 completed=92 drawn=93
    #0 libminEngine.dll+0x44C8B   RenderCommandPipe::Enqueue            RenderCommandPipe.cpp:222
    #1 libminEngine.dll+0x47361   EnqueueRenderCommand                 RenderCommandPipe.cpp:528
    #2 libminEngine.dll+0x10F941  EnqueueRenderCommand<RemovePrimitive lambda>  RenderCommandPipe.h:238
    #3 libminEngine.dll+0x10BEED  RenderScene::RemovePrimitive         RenderScene.cpp:425
    #4 libminEngine.dll+0x80F18   DetachSceneRenderState               SceneManager.cpp:107
    #5 libminEngine.dll+0x83DA5   SceneManager::RetireScene            SceneManager.cpp:892
    #6 libminEngine.dll+0x81C2E   SceneManager::UnloadActiveScene      SceneManager.cpp:325
    #7 libminEngine.dll+0x81686   SceneManager::Shutdown               SceneManager.cpp:254
    #8 libminEngine.dll+0x4B82D   Engine::ShutdownSystems              Engine.cpp:942
    #9 libminEngine.dll+0x4849E   Engine::Shutdown                     Engine.cpp:209
```

入队点是 `SceneManager::Shutdown` 第 254 行的 `RetireScene` → `DetachSceneRenderState` →
`RenderScene::RemovePrimitive`；它排在 `DrainQueuedRemovals()`（第 217 行）**之后**，所以那条
提前冲刷已经把这批命令要用的帧声明掉了。线程角色是 `Game+Render+RHI`（角色线程停掉后关闭线程
同时持三个角色，与 `TaskSystem: frame roles stopped; the calling thread now holds Game/Render/RHI`
一致）。本次会话共 55 条，与报告中 47 条同源。

## 修复

drain 能声明的帧被限制在**游戏线程已公告完成**的那一帧（`RenderCommandPipe.cpp`，
`FlushApplyForFrameChecked`）：越前的调用（关闭冲刷）照样把它找到的命令跑完，只是不把声明
往前推。公告帧与命令队列在同一把锁下发布，所以这里读到的是完整的已发布状态。

这样 `tag > drawn` 恒成立（在 `Current == Completed + 1` 不变式下），"迟到命令"那条防御分支
仍然保留，用于编号倒退的调用方。

## 回归验证

- `minEngineTests.exe test render-command-pipe` —— 新增
  `render-command-pipe: a drain ahead of the announced frame keeps the commands of that frame`：
  公告 125/126 → 用 `Current` 冲刷 → 声明停在 125 → 之后按 126 入队的命令计数 0 丢弃。
  修复前该用例失败：`GetDrawnFrame() == 126`，且入队当场打印上面那条一模一样的日志
  （`DroppedStaleCommands == 1`）。
- `render-command-pipe: a command tagged behind the drain is refused and counted` 保留防御分支的覆盖。
- 真机 Multi（minCraft 编辑器，同一工程）：关闭阶段 0 条 `cannot be applied`，管道
  "shut down with 0 queued command(s)"。

---

## 同批排查：RHIDeferredCreateQueue 一次会话丢 492 个 'openGL texture'

```
RHIDeferredCreateQueue: dropped 246 creation request(s) of kind 'openGL texture'
because nothing referenced the handle any more (492 dropped for this kind so far).
```

**判定：丢弃是对的**，不是“有人要用却没人持住”。队列只在自己是最后一个持有者时才丢
（`RHI::ScheduleNativeCreate` 的 `use_count() == 1`），此时已经没有帧能再点名这个对象。

**创建者归类（真机插桩实测，一次冷载 246 次创建 / 123 个不同名字，每个名字恰好两次）**：

1. 消费者 `minCraft-render/src/Render/BlockTextureAtlas.cpp` 的 `BindFromAssetPath`
   **每冷载把图集连载两次**：第一次 RHI 为空
   （`BlockTextureAtlas: '...blocks.mespriteatlas' has no generated page and RHI is null.`）返回
   false，随后带 RHI 再载一次。每次载入都会解析 123 个 `Sprite` 成员的 `SourceTexture`
   （`Textures/Blocks/*.metex`），即 123 个 GPU 纹理；两次 = 246。
2. 引擎侧：`Sprite::m_SourceTexture` 是强引用（`std::shared_ptr<Texture2D>`），引用解析阶段
   就会为每个 tile 建 GPU 纹理，而这个纹理随后只当作 **CPU 像素源**用
   （`SpriteAtlasPacker::TryLoadMemberSourcePixels` 走 `TryGetCookedPixels` 从磁盘读像素）。
   页面（`blocks_Atlas`）是第 247 次创建，消费者只留住它
   （`m_Texture = generated[0]`），图集对象连同 123 个 tile 纹理随即释放。

因此这 492 个纹理既没被画过也没人再要；浪费来自“eager 建 GPU 纹理”和“图集载两次”，
消除它们要改资源装载策略或消费者，二者都不在本记录改动的范围内。

## 关联

- 同族：`RenderCommandPipe` 的发布顺序（先标签后 drain 目标）与 `NewEntryFrame` 取较新者
- 消费者：`BlockTextureAtlas::BindFromAssetPath`（只报告，未改）
