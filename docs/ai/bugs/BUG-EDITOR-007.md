# BUG-EDITOR-007 — Prefab Stage 反复 save+close 后偶发访问违例

## Meta
- **ID:** BUG-EDITOR-007
- **Status:** Fixed（2026-10-20）
- **Severity:** S2（不阻日常使用；阻把 smoke 当 CI 门禁）
- **Owner:** Runtime / Prefab 写回与场景销毁路径
- **Found:** 2026-10-19
- **Affects:** `PrefabStageController` save/close 循环之后的编辑器主循环
- **Related Feature/Slice:** [MCP-F05](../Editor/MCP/MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_DESIGN.md) S08 · `scripts/ui_prefab_smoke.ts`

## TL;DR

连续执行 `prefab.open → 改树 → prefab.save → prefab.close` 循环后，编辑器有约 **1/3** 概率在下一帧
以 `0xC0000005` 崩溃。

根因：`prefab.close` 在本帧的 UI/自动化阶段销毁 Stage 场景，而本帧更早的时候场景编辑视口已经把
"指向该场景 RenderScene 的场景绘制请求"提交进 `RenderSystem` 的待绘制队列；渲染 tick 随后在**同一帧**
消费这条请求，读到的是一块已释放的 `RenderScene`。修法是把场景销毁推迟到渲染 tick 之后
（`SceneManager::RetireScene` / `FlushRetiredScenes`）。

## 复现

```powershell
cd minEngineMcpServer
npx tsx scripts/ui_prefab_smoke.ts    # 修复前约 3 次就能看到一次
```

最小探针（单会话内连续 open → 建子节点 → save → close → 文档场景请求）：修复前固定在第 4 个循环崩。

## 隔离结论（已做的实验）

| 场景 | 结果 |
|------|------|
| 只 open/close（不 save）× 4 | 稳定 |
| open → 加子节点 → close(discard) × 4 | 稳定 |
| open → 加子节点 → save（Stage 保持打开）× 4 | 稳定 |
| open → 加子节点 → save → close | 修复前必崩/高频崩；修复后 20 循环 × 3 轮稳定 |

## 已修复（两处悬垂引用）

1. **`prefab.close` 未把 inspect 目标重置回文档场景**（2026-10-19，isolation probe 15/15 绿）。
2. **待绘制队列持有已销毁 Stage 场景的裸指针**（2026-10-20，本单根因，见下）。

## 根因（2026-10-20）

### 证据（gdb 观测，Debug 构建 + DWARF）

```
DBG_SceneDtor       0x9b53af0        # Stage Scene 在 prefab.close 中被销毁
DBG_RenderSceneDtor 0xbe29150        # 它的 RenderScene 随之销毁
=== CRASH ===
#0 RenderScene::CollectOrphanedSceneProxies (this=0xbe29150)  RenderScene.cpp:388
#1 ForwardRenderer::Execute                                    ForwardRenderer.cpp:618
#2 RenderSystem::Tick                                          RenderSystem.cpp:160
#3 Engine::RendererTick → Engine::TickRendererFrame → Editor::Run (Editor.cpp:637)
desc = {Scene = 0xbe29150, GameplayScene = 0x9b53af0, Camera = …, RenderTarget = …, ViewId = 1, Flags = 11}
```

`desc.Scene` 与被销毁的 `RenderScene`（0xbe29150）、`desc.GameplayScene` 与被销毁的 Stage `Scene`
（0x9b53af0）**地址完全一致**；`AV operation=0 address=0xFFFFFFFFFFFFFFFF` 是读到已释放内存的表现。

### 生命周期在哪一步被破坏

`Editor::Run` 一帧内的顺序是：视口 tick（`SceneEditingViewportClient::SyncObservedScene` +
`SubmitSceneDraw`）→ `AutomationHost::Tick`（处理 `prefab.close`）→ `TickRendererFrame`。
即"提交绘制"先于"关闭 Stage"，而"消费绘制"晚于它。`SceneDrawDesc` 里的两个场景指针都是
**非拥有的裸指针**，Stage 场景的唯一强引用是 `PrefabEditorStage::StageScene`，
`PrefabStageController::DestroyStage` 当场把它释放，队列里就留下了悬垂绘制请求。

### 修法（切断方式）

- `SceneManager::RetireScene(std::shared_ptr<Scene>)`：把场景的最后一个强引用交给 SceneManager 暂存。
- `SceneManager::FlushRetiredScenes()`：由 `Engine::TickRendererFrame` 在渲染 tick **之后**调用，
  此时本帧的绘制请求已被渲染器消费，场景才真正析构（`SceneManager::Shutdown` 也会清空队列）。
- `PrefabStageController::DestroyStage` 改为退休而非就地 `reset()`。

Stage 场景因此多活不到一帧，`prefab.open/save/close` 的请求/响应与错误码、写回语义均未改变。

## 残余嫌疑的处置

| 嫌疑 | 结论 |
|------|------|
| `PrefabUtility` Stage 回写时销毁/重建模板 GameObject | 未观察到越界；smoke/prefab suite 全绿，无证据 |
| `RefreshEditCloneMapAfterWrite` 映射重建时序 | 同上，fail-closed 校验仍在（无证据） |
| `ObjectManager` GUID 重映射 | 同上（`prefab stage writeback twice keeps edit map root` 仍绿） |
| 场景/渲染销毁路径在 Stage 关闭后的回调 | **命中**，已修（本单根因） |

## 残余风险

- `PlayInEditorSession::Stop` 走 `SceneManager::UnregisterPIEScene`，同样在 UI tick 内释放 PIE 场景；
  同一类窗口理论上存在（视口在当帧更早已提交 PIE 场景的绘制）。当前无复现证据，未纳入本单；
  若复现，把该处也换成 `RetireScene` 即可。

## 验证

| 项 | 命令 | 结果 |
|----|------|------|
| 端到端 smoke 连跑 | `npx tsx scripts/ui_prefab_smoke.ts` × 20 | 20/20 全绿（每次 `79/79 checks passed`，exit=0，无 `ed_crash.log`） |
| 引擎全量 | `scripts/verify.ps1` | `VERIFY_EXIT=0` |
| 回归护栏 | `minEngineTests test smoke` | 新增 `deferred-structural-edit: retired scene outlives the frame` |
| 无泄漏确认 | 单次 smoke 的编辑器会话日志 | 8 次 `prefab.close` → 8 条 `Scene 'UI_PrefabSmoke' is being destroyed`（延迟销毁确实发生，未漏析构） |
| smoke 固定步骤 | `ui_prefab_smoke.ts` [7b] | 6 次 save→close 循环（每次 close 后再打一条文档场景请求） |

## 退出条件

已满足：`ui_prefab_smoke.ts` 连续 20 次全绿（不崩），`open/save/close` 可进 CI；smoke 已含固定 save+close 循环步骤。
