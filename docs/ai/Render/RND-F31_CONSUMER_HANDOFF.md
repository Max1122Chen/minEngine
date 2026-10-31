# RND-F31 消费者侧交接：图集冷启动优化 + chunk 生成 Task 化

- **Status:** Handoff（供消费者侧 agent 执行）
- **Date:** 2026-10-31
- **引擎基线:** feat/core（RND-F31 S01–S05 已落地，已合入 master）
- **适用仓库:** D:/Dev/GitRepo/minCraft-render（消费者）

> 本文只做**指导与边界划分**，不含实现代码。两部分可独立执行、互不阻塞，但都**不要改引擎仓库**。

## 0. 先读：这轮在引擎侧立起来的契约

消费者要做的两件事都建立在这些契约之上（引擎已强制，违反会**被拒绝并计数**，不是静默降级）：

| 契约 | 含义 | 违反的症状 |
|---|---|---|
| **命名即持有** | 任何要画的几何（VB/IB/布局）必须把**句柄**（RHIBufferRef / RHIVertexInputLayoutRef）和帧视图一起交给 MeshBatch / packet | 日志 `Draw not submitted: ... is handed to the command list as a bare pointer`，该 draw 被丢弃 |
| **门禁先判句柄，再解引用** | 引擎不会对裸视图查询可用性 | 消费者无需关心；但**别指望引擎替你持住裸指针** |
| **worker 是纯 CPU** | Task worker 上不得调任何 RHICreate* / Draw | 延迟创建排队或按角色被拒，并计入 OffRoleRenderCommands |
| **并行要能证明跑了** | ParallelFor 被拒是**静默**的，必须做效果校验（见 §2.4） | 没有校验就会看起来并行、实际串行 |
| **顺序确定** | 并行产出按分片索引合并，和串行结果逐字节一致 | 画面偶发错画（不崩） |

## 1. Part A：图集冷启动优化（消费者侧）

### 1.1 现状时序（已核实到行）

**先纠正一个容易误判的前提**：图集页面**不是**用编辑器烘焙好的资产。

- `SpriteAtlas::m_GeneratedTextures` 是 `ME_PROPERTY()`（`SpriteAtlas.h:102`），序列化的是 **Texture2D 资产引用（GUID）**；
- 但页面由 `CreateFromPixels` 生成（`SpriteAtlas.cpp:267-268`），是**运行时纹理、没有注册成资产**；
- 于是重启后 GUID 解析不到对象，`generated` 为空，**必须 `Rebuild(rhi)` 用成员像素重新打包**。

**这是刻意设计**：Bake 只组织映射，不产资产；页面在启动时由 `Rebuild(rhi)` 现造。所以**加载后 Rebuild 是正确行为，不是缺陷**。真正的引擎缺陷是另一个：`m_GeneratedTextures` 这个纯运行时集合被标成了 `ME_PROPERTY()` 写进了序列化（见 A1）。

真正让代价翻倍的是构造期一次没要到 RHI 的调用：

```
WorldRenderer 构造                                          WorldRenderer.cpp:19
  └─ m_BlockAtlas->BindProjectAtlasOrFallback();     // 未传参，rhi 默认 nullptr
      └─ BindFromAssetPath(path, nullptr)                     BlockTextureAtlas.cpp:196
          ├─ AssetManager::LoadAssetByPath(path)   ← 贵：解析 123 个成员
          │    └─ 每个 Sprite::m_SourceTexture 是 shared_ptr<Texture2D>
          │       加载该资产时按正常路径建出 GPU 纹理（而读像素并不需要它）
          ├─ generated 页面为空
          ├─ rhi == nullptr → Warn has no generated page and RHI is null → false   :242-247
          └─ 回退 BuildDefaultOverworldTiles() → 程序化色块

RHI 就绪后，EnsureSharedMaterial()                            WorldRenderer.cpp:60
  └─ 再走一遍同一路径（此时带 rhi）→ 又解析 123 成员 → Rebuild → Bind
```

**注意时序**：WorldRenderer 由 `MinCraftWorldSubsystem::Initialize` 创建，那时 RHI **已经存在**
（`Engine::StartSystems` 里 RenderSystem 在 SceneManager 之前）。问题不是对象出生太早，
而是**构造期做依赖 RHI 的工作却没有把 RHI 传进去**。

**代价**：一次冷载 246 次 GPU 纹理创建（123 x 2），全部无人引用后被延迟创建队列丢弃；
另有一次注定失败但很贵的加载解析。

### 1.2 两个可优化点（按收益与归属排序）

| # | 改动 | 归属 | 收益 | 风险 |
|---|---|---|---|---|
| **A0** | 构造期把 `RenderSystem::Get().GetRHI()` 传给 `BindProjectAtlasOrFallback(rhi)`（一行） | **消费者** | 直接消掉重复加载：第一次即可 Rebuild 成功，不再回退色块 | 极低，只是把已存在的 RHI 传进去 |
| **A1** | 把 `m_GeneratedTextures` 从序列化中移除（运行时数据不应进资产） | **引擎** | 旧资产不再带悬垂 GUID；消费者不再需要"generated 为空则 Rebuild"的兜底判断 | 低；需处理旧资产兼容 |

**已确认不做**：Sprite 强引用 Texture2D 是可接受的，不做像素优先加载；Bake 语义不变，不持久化烘焙结果。

### 1.3 验收标准

1. 一次冷载的 `openGL texture` 创建数从 123 x 2 降到 123（A0 生效）；资产加载后不再出现 `has no generated page` 之外的悬垂 GUID（A1 生效）；
2. 日志里 `has no generated page and RHI is null` **不再出现**；
3. 首帧画面与改动前一致（程序化色块 → 真图集的切换时机不变或更早）；
4. 冷启动墙钟时间对比（改动前/后各跑 3 次取中位）。

### 1.4 不要做的事

- **不要**为了消掉那条 Warn 而降级日志级别（它告警的是白干一轮，消掉浪费它自然消失）；
- **不要**把 BindProjectAtlasOrFallback 从构造期整个删掉——先手绑定是刻意的；
- **不要**改引擎的 SpriteAtlas::Rebuild 或 Sprite 的引用语义（那是 A3 的范围，另行登记）。

## 2. Part B：chunk 生成搬进 Task

### 2.1 目标与复用对象

目标：把 **chunk 网格构建**（体素遍历、面剔除、顶点/索引生成、包围盒）从游戏线程搬到 worker，缩短“挖方块到画面更新”的延迟，并让大范围地图生成不再独占游戏线程。

引擎侧 RND-F31 已把这些机制验证并落地，消费者直接复用同一套形状，不要另发明：

| 机制 | 引擎落点 | 消费者对应物 |
|---|---|---|
| 分片（区间不交不漏） | MeshAssemblyChunk.h 的 ComputeMeshAssemblyChunkRanges | chunk 索引区间 |
| 按片序合并 | MergeMeshAssemblyChunks | 按 chunk 索引升序合并产物 |
| 效果校验 | VisitCount == 0 → 就地补跑 + 计数 | 每个任务末尾写 Built = true |
| 降级 | 能力判定在调用方，退化为同一条串行路径 | 同 |
| 观测 | MeshAssemblyChunksNotRun 进每帧摘要 | 新增消费者计数，同样只在丢失/违约时计 |

### 2.2 形状（建议）

```
输入快照（游戏线程，纯 POD）：chunk 坐标区间 + 该区间体素数据的只读快照或版本号
   ↓
扇出：ParallelFor(chunkCount) → 每个任务只读输入、只写自己的输出槽（顶点/索引 POD + 包围盒 + 空标志）
   ↓
串行交接（游戏线程/渲染角色）：从输出槽取 POD → EntityMesh::Upload（走既有延迟创建）
   ↓
收尾：计划 vs 实跑计数比对 + 顺序断言
```

### 2.3 必须先解决的三个前置条件

1. **快照或版本号**：地形数据在 worker 读的同时会被游戏线程改（挖/放方块）。要么传不可变快照，要么给 chunk 数据加版本号并在合并时校验（版本变了就丢弃该产物重做）。**这是最容易出错的地方。**
2. **内存峰值**：kMaxInFlightFrames 只约束帧，不约束消费者自己的队列。必须有自己的上限（例如同时在飞的 chunk 产物 ≤ N）并计数。
3. **上传时机**：产物**必须在本帧内消费**（走既有 RHIDeferredCreate），不要跨帧缓存裸数据等下一帧——跨帧会撞上引擎那条“命名即持有”门禁（句柄必须跟着产物一起交）。

### 2.4 效果校验（照抄引擎形状，不要省）

```
每个任务最后一行：slot.Built = true;      // 异常或被拒绝都走不到这行
ParallelFor 返回后：
  for slot where !Built: 就地串行构建 + ++ChunksNotBuilt
  正常帧成本：一次长度为 n 的扫描
```

**必须有一条反向用例**：让 ParallelFor 被拒（不绑角色 / 池未运行），断言 ChunksNotBuilt > 0 且**产物仍完整**——证明降级真的在工作，而不是永远用不到。

### 2.5 切片建议

| 片 | 内容 | 规模 | 红灯 |
|---|---|---|---|
| B1 | 抽出按 chunk 索引分片的骨架，**纯串行**，ChunkCount == 1 也走新结构 | 0.5–1 天 | 产物与改动前逐字节一致（顶点/索引 digest） |
| B2 | 真扇出，但**只在测试里启用**（产品仍单片） | 1 天 | k∈{2,4,8} 下 digest 恒等 + 探针证明批次在非调用线程产生 |
| B3 | 接产品路径 + 能力判定 + 效果校验 + 观测 | 1 天 | 开关关闭后等价；被拒时补跑计数 > 0 且产物完整 |
| B4 | 版本号或快照（若 B2 已需要） | 1–2 天 | 数据版本变化时旧产物被丢弃并重做 |
| B5 | 测量与阈值定稿 | 0.5 天 | 给出交叉点，阈值有实测依据 |

### 2.6 不要做的事

- **不要在 worker 里创建任何 RHI 对象**（缓冲/纹理/PSO）。worker 只产 POD；上传一律回主线程走 RHIDeferredCreate。
- **不要**直接改引擎的 chunking 与 merge 逻辑来顺手复用——它们是引擎内部实现，消费者写自己的一份同形状代码。
- **不要**把 MeshBatch 只填裸指针（会触发“命名即持有”门禁，draw 被丢弃并计数）。
- **不要**用完成顺序合并产物（必须按 chunk 索引），否则画面偶发错画且**不会崩**——最难查的一类缺陷。

## 3. 分工与协作边界

| 事项 | 归属 | 说明 |
|---|---|---|
| Part A（图集冷启动） | 消费者 | 只改 minCraft-render；若需引擎配合（A3）另行登记 |
| Part B（chunk 生成） | 消费者 | 只改 minCraft-render；引擎侧只读 |
| 引擎 RND-F31（并行装配） | 引擎 | **已定稿**，消费者不要改 |
| 引擎 A3（Sprite 像素优先） | 引擎 | 另开切片，别与 Part A 混做 |

**构建口径**：消费者统一用 -DENGINE_ROOT=<目标引擎工作树> 的独立构建目录；不要与引擎的契约构建（minEngine/build）共用输出目录，否则 bin/ 会混用两套 DLL（已踩过一次）。

## 4. 交付时请附的证据

1. 目标与不改动清单；
2. 每片的**红灯原文**（修复前失败的那一行）与绿灯计数；
3. 顺序不变性证据（digest 或等价物）；
4. 降级路径的反向用例；
5. 冷启动与帧时间的前后对比（各 ≥3 次取中位）；
6. 遗留风险逐条。
