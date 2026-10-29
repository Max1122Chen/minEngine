# Frame Threading — System Engineering Review

## Meta
- **Type:** System Engineering Review
- **Status:** Review
- **Owner:** project maintainer
- **Last updated:** 2026-10-25
- **Related:** [CORE-F45](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md) · [CORE-F46](./CORE-F46_TASK_SYSTEM_UNIFIED_FACADE_DESIGN.md) · [CORE-F47 Design](./CORE-F47_FRAME_THREAD_CONTRACTS_DESIGN.md) · [BUG-RENDER-016](../bugs/BUG-RENDER-016.md)
- **Skill:** engine-system-review

---

## 1) Review Scope

| Item | Content |
|------|---------|
| **Target subsystem** | 帧线程分离与任务门面（Game / Render / RHI 角色线程、调度、命令管道、RHI 录制队列） |
| **Review objective** | 多线程升级落地后的正确性 / 生命周期 / 契约复盘 |
| **Explicit focus** | 帧号契约、资源所有权、异常与活性、关停顺序 |
| **Out of scope** | 渲染算法与画面质量；编辑器 UI 功能；只读消费者仓库 |
| **Directories / files inspected** | `Runtime/Core/Tasks/*`、`Runtime/Core/Threading/*`、`Runtime/Engine.cpp`、`Runtime/Function/Render/RenderSystem.*`、`RHI/RHICommandList.*`、`RenderPipeline/ForwardRenderer.cpp`、`RenderScene.cpp`、`Framework/Scene/SceneManager.cpp`、`Framework/Components/*`、`Editor/src/Viewport/*` |
| **Scenarios traced** | 启动、稳态一帧、队列满背压、关停、PIE 进出、场景退役、异常路径 |
| **Limitations** | 无运行时复现（未跑编辑器）、无 TSan、编辑器实际每帧视口提交次数未测量 |

---

## 2) System Understanding

- **职责**：Game 角色做世界与提交；Render 角色做剔除、排序、录制；RHI 角色做回放与呈现；`TaskSystem` 统一提供角色线程与任务执行；`RenderCommandPipe` 是 Game → Render 的唯一通道。
- **非职责**：`FrameThreading` 不做同步、不持有队列；命令管道不持有被捕获对象；代理不拥有 GPU 资源。
- **所有权**：`Engine` 持有 TaskSystem / FrameThreading / Pipe / 列表池；`RenderScene` 持有 proxy 对象；GPU 资源归 Game 侧资产。
- **线程归属**：管道生产者在 Game、消费者在 Render；**但帧号计数器由 Render 独占推进**——这是所有时序问题的根。
- **不变量（声称的）**：画第 N 帧意图时世界至少是第 N-1 帧；队列有界且满则阻塞；关停顺序为停提交 → 排空 → join → 拆资源。

---

## 3) Findings

### R01 — 角色线程与命令 Flush 没有异常边界
| Item | Content |
|---|---|
| Category / Severity / Confidence | Concurrency · Error Recovery / **Critical** / Confirmed |
| Evidence | `TaskSystem.cpp` 三个线程主函数（合并图形线程、Render、RHI）均无 try/catch；`RenderCommandPipe.cpp` 的 FlushApplyForFrame 无 try/catch |
| Trigger | 任一角色回调或命令体抛出（内存不足、断言抛出、RHI 层抛出） |
| Impact | 穿出线程入口 → `std::terminate`；Flush 中途抛出还会静默丢弃后续命令，产生"移除生效、新增未生效"的半应用状态 |
| Action | 逐条 try/catch，失败结算为"本帧未完成"并记录 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R02 — 过期帧号命令永久滞留，最终挂死生产者
| Item | Content |
|---|---|
| Category / Severity / Confidence | Concurrency / **Critical** / Confirmed（结构）/ Likely（到达条件） |
| Evidence | `RenderCommandPipe.cpp` 入队在锁外读帧号；Drain 只执行帧号完全相等的条目，其余放回队列；帧号自增无锁；入队满时无限 `yield` 自旋 |
| Trigger | Game 在 Render 已 Drain 完该帧号之后入队；编辑器因同步提交而被掩盖，异步路径可达 |
| Impact | 命令永不执行 → 代理移除不生效；队列慢性占满后所有 Game 侧入队挂死 |
| Action | 帧号归 Game 拥有；Drain 用小于等于；过期条目丢弃并计数；等待改有界 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R03 — 世界世代断言恒真
| Item | Content |
|---|---|
| Category / Severity / Confidence | Architectural Risk / **High** / Confirmed |
| Evidence | `Engine.cpp` 中逻辑帧号取自管道、绘制后把已发布世代设为同一帧号再自增；断言比较的两个数出自同一函数同一写入者 |
| Impact | 该断言永远不触发，"用到了更新世界"的上游错误没有任何护栏 |
| Action | 帧号所有权归 Game；断言比较两个独立来源并打印双方数值 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R04 — 列表池借用无 RAII，失败路径泄漏导致渲染线程死锁
| Item | Content |
|---|---|
| Category / Severity / Confidence | Lifecycle / **Critical** / Confirmed |
| Evidence | `ForwardRenderer.cpp` 在 Multi 下 Acquire 列表，随后帧图设置失败直接 return，既未提交完成也未归还；池深度为 3，Acquire 在条件变量上阻塞 |
| Trigger | 视口尺寸为 0 或帧图设置失败 |
| Impact | 累计三次即永久耗尽，渲染线程无限阻塞，且无日志 |
| Action | 借用改 RAII 句柄 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R05 — 意图队列容量 3 与"每帧多视口"契约冲突
| Item | Content |
|---|---|
| Category / Severity / Confidence | Concurrency / **High** / Confirmed（机制）/ Possible（频率） |
| Evidence | `RenderCommandPipe` 意图队列上限为 3，满时自旋；生产者为编辑器视口与离屏渲染共三处调用点，缩略图服务有两个离屏实例 |
| Trigger | 单帧提交第 4 条意图，且 Render 尚未 Drain |
| Impact | 生产者永久自旋；Single 拓扑下即同线程死锁 |
| Action | 容量从契约推导或满时丢弃最旧并计数，绝不阻塞 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R06 — 退役闸口时机错误，且销毁发生在渲染线程
| Item | Content |
|---|---|
| Category / Severity / Confidence | Lifecycle / **High** / Confirmed（时序）/ Possible（RHI 影响） |
| Evidence | `Engine.cpp` 在渲染帧函数末尾 Flush 退役场景，此时仅完成录制，RHI 回放尚未运行；`SceneManager` 的退役容器为裸 vector，Game 线程写入、Render 线程交换 |
| Impact | 已退役场景可能在尚未回放的帧里被引用；场景销毁（世界子系统反初始化、对象析构）跑在渲染线程上 |
| Action | 闸口改为"该帧 RHI 完成"；销毁投递回 Game 角色；容器加同步 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R07 — 关停顺序反了
| Item | Content |
|---|---|
| Category / Severity / Confidence | Lifecycle / **High** / Confirmed |
| Evidence | `Engine.cpp` 关停路径先销毁 PIE 实例与子系统，再停止角色线程 |
| Trigger | 引擎退出 |
| Impact | Render/RHI 线程继续 tick 正在被拆除的世界 |
| Action | 先停角色（停提交 → 排空 → join）再拆世界 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R08 — 角色启动失败静默，随后撞断言硬陷
| Item | Content |
|---|---|
| Category / Severity / Confidence | Error Recovery / **High** / Confirmed |
| Evidence | `Engine.cpp` 的启动入口返回 void，前置条件不满足即 return；调用方据"角色是否在跑"退回单线程分支调用渲染，而 Render 角色未绑定 |
| Impact | `__debugbreak` 崩溃且调用方无从知道启动失败 |
| Action | 返回结果并传播；禁用路径补显式断言说明 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R09 — 背压不含 RHI 队列，Game 与 GPU 失步无上界
| Item | Content |
|---|---|
| Category / Severity / Confidence | Concurrency / **High** / Confirmed（结构） |
| Evidence | `TaskSystem` 的帧队列上限只约束未开始的渲染条目；RHI 任务队列无上限，渲染线程推入即返回 |
| Impact | GPU 越慢，Game 与 GPU 的失步窗口越大——这正是跨线程裸指针问题的温床 |
| Action | 计数口径改为两端在飞帧合计 |
| 0.1.0 bucket | Should Fix Soon |

### R10 — 代理对渲染资源无所有权（BUG-RENDER-016 的直接原因）
| Item | Content |
|---|---|
| Category / Severity / Confidence | Lifecycle / **Critical** / Confirmed |
| Evidence | 代理保存裸缓冲指针与裸材质代理指针，注释写明非拥有；资源归 Game 侧资产；移除命令延迟到 Render 侧生效 |
| Impact | Game 线程释放资产后 Render 线程解引用已释放对象 |
| Action | 代理持有强引用；详见 CORE-F47 与 BUG-RENDER-016 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

### R11 — 身份表数据竞争，作用域身份切换不恢复
| Item | Content |
|---|---|
| Category / Severity / Confidence | Concurrency / **Medium** / Confirmed |
| Evidence | `FrameThreading` 的线程标识表由角色线程写、主线程读，无发布同步；作用域切换的析构函数为空实现 |
| Impact | 形式化数据竞争；身份被永久篡改后产生误报断言 |
| Action | 原子化；作用域切换实现真恢复或删除 |
| 0.1.0 bucket | Should Fix Soon |

### R12 — 断言恒开启且无中间态
| Item | Content |
|---|---|
| Category / Severity / Confidence | Observability / **Medium** / Confirmed |
| Evidence | `Assert.h` 的开关只有默认值 1，全仓无处置 0；断言失败直接断点 |
| Impact | 发行构建里角色误用同样硬陷；同时没有"记录后继续"的档位 |
| Action | 提供 release 记录档位；角色校验至少在 release 输出明确日志 |
| 0.1.0 bucket | Should Fix Soon |

### R13 — 丢帧与丢弃不可诊断
| Item | Content |
|---|---|
| Category / Severity / Confidence | Observability / **Medium** / Confirmed |
| Evidence | 帧提交入口返回 void；停止状态与正常丢弃不可区分；队列满、条目过期、关停丢弃全部静默 |
| Impact | 线上"某帧少了 N 条命令"无法从日志判断 |
| Action | 返回结果 + 计数器 + 每帧汇总 |
| 0.1.0 bucket | Should Fix Soon |

### R14 — 单元素并行内联，语义与声明不一致
| Item | Content |
|---|---|
| Category / Severity / Confidence | Maintainability / **Low** / Confirmed |
| Evidence | 并行入口在元素数为 1 时于编排线程内联执行，运行并等待的调用方因此从不进入工作池 |
| Impact | 调用方对被静默改变的线程语义无从察觉 |
| Action | 文档化该行为或统一入池 |
| 0.1.0 bucket | Known Limitation |

### R15 — 测试护栏缺失，退化行为被固化为契约
| Item | Content |
|---|---|
| Category / Severity / Confidence | Maintainability / **High** / Confirmed |
| Evidence | 主循环的两条 tick 路径无测试；某帧线程测试把"多线程模式下三角色同 tid"写成期望值；无 PIE 场景级用例；管道测试只覆盖单线程顺序 |
| Impact | 本次复盘发现的全部机制缺陷都没有护栏，任何修复都无法回归验证 |
| Action | 按 CORE-F47 的验证策略补 headless 用例 |
| 0.1.0 bucket | Must Fix Before 0.1.0 |

---

## 4) Positive Findings

- `TaskSystem` 的完成事件发布顺序正确：先持锁发布完成事件，再在任务自身的锁下置位并唤醒，等待方返回后必然可见事件——可作为全引擎条件变量的基准写法。
- 异常隔离在并行执行的函数体与图节点上已经存在，逐条捕获、单条失败不中断同批；命令 Flush 照抄即可。
- 列表池在初始化与借用时强制关闭"立即执行"覆盖，避免回放语义随全局状态翻转而静默失效。
- 角色身份被抽成独立小对象，职责单一、无队列无锁、可单测。
- 停止角色后把图形上下文拉回主线程，让主线程仍能安全销毁 GPU 资源——关停里唯一真正做对的所有权交接。
- 退役场景先交换再清理，防住"析构中再触发退役"的重入。

---

## 5) Release Assessment (0.1.0)

| Bucket | Finding IDs |
|--------|-------------|
| **Must Fix Before 0.1.0** | R01, R02, R03, R04, R05, R06, R07, R08, R10, R15 |
| **Should Fix Soon** | R09, R11, R12, R13 |
| **Known Limitation / Acceptable** | R14 |
| **Future Enhancement** | 命令管道改无锁单生产者单消费者环形队列；渲染线程完全不回指游戏对象的数据快照化 |

**Readiness judgment:** 机制面不具备 0.1.0 条件——存在两条可永久挂死路径（R02、R04）与一条必现终止路径（R01），且帧号与资源两项契约实际未实现（R03、R10）。这些不是打磨项，是必须修的。

---

## 6) Recommended Next Steps

1. 按 CORE-F47 的四个切片落地：止血 → 帧号所有权 → 退役闸口 → 端到端背压与可观测性。
2. 每个切片先补 headless 回归用例，做到红灯驱动。
3. 把本节结论拆成 TECH_DEBT 行与缺陷记录，避免结论只活在文档里。

---

## 7) Open Questions

| ID | Question | Blocked on |
|----|----------|------------|
| Q1 | 编辑器单帧实际提交多少条场景绘制意图 | 加计数跑一次编辑器 |
| Q2 | 编辑器是否存在不经帧提交入口的第二条渲染唤醒路径 | 运行期线程轨迹 |
| Q3 | 渲染代理与场景析构是否触碰 RHI 接口 | 读析构链或加探针 |
| Q4 | 跨线程打点的帧号归属是否正确 | 分析侧采集窗口代码 |
