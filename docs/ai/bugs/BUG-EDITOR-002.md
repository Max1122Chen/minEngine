# BUG-EDITOR-002 — Editor 冷启动偶发崩溃（DebugDrawService / Physics debug 入队路径）

## Meta
- **ID:** BUG-EDITOR-002
- **Status:** Fixed（2026-10-24 根因闭合：**AVX 对齐栈写**，不是内存踩踏；两层修复都已落地：入队 API 去掉按值拷贝 + Jolt 泄漏给 first-party TU 的 AVX 旗标已擦除。**已提交（`feat/render`）**）
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-09-02
- **Resolved:** 2026-10-24（根因定位 + 修复；全量回归数据见「验收数据」）
- **Last updated:** 2026-10-24
- **Affects:** Editor 冷启动首帧～数秒；OpenGL / Vulkan 均曾高概率（`0xC0000005`）
- **Related Feature/Slice:** `PhysicsDebugDraw`, `DebugDrawService`, `DebugDrawPass`, `SceneEditingViewportClient`

## TL;DR
冷启动偶发 `0xC0000005`，故障点稳定在 `DebugDraw::Box` 里那条入队调用。

**2026-10-24 更正：** 早期"读地址 `0xFFFFFFFFFFFFFFFF` = `m_Boxes` 控制块被写坏"的判断**是错的**。
`0xFFFFFFFFFFFFFFFF` 是 Windows 对**无线性地址的 CPU 对齐异常**填的哨兵值；崩溃发生在**按值传参的栈拷贝**上——
编译器生成的 `vmovdqa %ymm0,-0xC0(%rbp)` 要求 32 字节栈对齐，而 Win64 ABI 只保证 16 字节。
内存自始至终没有被写坏（详见下一节）。

| 实验 | 结果 | 结论 |
|------|------|------|
| **二分 A** 跳过 UI font atlas rebuild | 仍崩 | **atlas 不是主因**（日志末行 atlas rebuilt 为时序巧合） |
| **二分 B** 关闭 Physics debug 入队 | 20 次 0 崩 | **触发路径 = DebugDraw 入队** |
| **S05** 入队移至 `ForwardRenderer::Execute` | 栈变为渲染 Tick，**仍崩** | 生命周期对齐必要但不充分 |
| **ASan**（LLVM-MinGW clang++） | 多次冷启动过 atlas **不崩、无 ASan 报告** | 分配器/布局可能**掩盖**踩内存；**不能**据此判 Resolved |

**现状（2026-10-24）：** 根因已闭合，**不存在"写坏内存的代码"**；修复为入队 API 去掉按值拷贝。上表中"关闭入队则 0 崩"依然成立，因为被关掉的正是那条有问题的指令所在的路径。

---

## 根因（2026-10-24 复核，人话版）

### 一句话
不是"有人写坏了内存"，而是**编译器在栈上生成了一条要求 32 字节对齐的指令，而 Windows 只保证 16 字节栈对齐**。当目标地址刚好是 16 的倍数但不是 32 的倍数时，这条指令立刻触发 CPU 对齐异常；Windows 把它报告成"访问冲突 0xC0000005、读地址 0xFFFFFFFFFFFFFFFF"。

### 崩溃究竟发生在哪一步
`DebugDraw::Box()` 的最后一句是 `DebugDrawService::Get().EnqueueBox(command)`。`EnqueueBox` 的参数是**按值**传递的 `DebugBoxCommand`（约 100 字节），所以调用方得先把命令**复制到栈上的"传出参数区"（`rbp-0xC0`）**。编译器为这次复制选了 AVX 的对齐存储指令：

```
26a547537: vmovdqa %ymm0,-0xC0(%rbp)     ; 目标必须 32 字节对齐，否则 #GP
```

而该函数的序言只有 `push rbp / mov rsp,rbp / sub $0xe0,rsp`，**没有把栈重新对齐到 32**（Win64 ABI 只承诺 16）。于是 `rbp-0xC0` 是否 32 对齐，取决于**这个进程的栈落在哪个地址**——等于掷一次硬币。

### 为什么是 ~50%，为什么"成簇"
栈地址的奇偶性在进程启动时确定，**整个进程不变**：

- 运气好的进程：这条指令在整个生命周期里都合法 → 整轮不崩；
- 运气差的进程：**第一次画 collider（第一个 box）就炸** → 表现为"某些批次连续崩、某些批次全过"。

所以不是"跑着跑着被写坏"，而是"第一个 collider 的第一条指令就炸"。这与实测完全一致：崩溃时 `exec=1`（第一次 `ForwardRenderer::Execute`）、`boxCalls=1`（第一个 box）、`checkpoint=2`（`ClearFrameQueues` 之后、`SubmitScene` 之中）。

### 为什么看起来像"内存被 0xFF 涂写"
三个巧合叠在一起，把方向带偏了：

1. **假地址。** `0xFFFFFFFFFFFFFFFF` 不是被访问的地址。这是 Windows 对"这类没有线性地址的 CPU 例外"填的哨兵值。用 20 行程序故意做一次非对齐 `vmovdqa`，拿到的异常码/参数与 `ed_crash.log` **逐字段相同**（`scripts/debug/avx_align_probe.cpp`）。
2. **guard 字节被读成 0x01。** 本工具链是**单字节 guard**：反汇编 `Get()` 可见 `movzbl guard; test al,al; sete`，**byte0≠0 即"已构造"**，不是 Itanium 双字节协议。0x01 的含义是"构造完成"，被按"A 假设预言 guard≠0"读成了"内存被涂"。
3. **"176 字节对齐涂写"** 是对异常参数形状的拟合，不是任何一次真实内存采样。

### 直接证据
- 探针在**每次 `Get()` / 每次入队之前**校验 7 个 vector 的 21 个字：12 次崩溃期运行里**一次也没报脏**。
- 崩溃前最后一次采样（就是出事那次调用）对象**原样**：168 字节全 `00`，`guard(+0xA8)=0x01`，7 个 vector 全部 `start=finish=end=0, sane=yes`。
- 同页 4KB + ±32KB 窗口扫描：**all-FF 64B 段 0 / 1024**（页内只有 instance 自己所在的那个 mixed 块，指针全在堆上）。
- 反汇编 `FaultRIP = libminEngine.dll+0x47537` → `DebugDraw.cpp:46`，正是那条 `vmovdqa %ymm0,-0xC0(%rbp)`；同一 RVA 在两个不同构建里稳定复现。
- 对齐预测器：`(rbp-0xC0) % 32 != 0` 的进程必崩，`== 0` 的进程必不崩（修前实测样本 + 修后 6 次"旧必崩条件"存活）。

### 修复与验证
- **修复：** `DebugDrawService::EnqueueLine/Point/Box/Sphere/Capsule` 参数 **按值 → const 引用**（不再产生传出栈拷贝；顺带省掉每条 collider 一次 ~100B 拷贝）。修复后的 `DebugDraw::Box` 反汇编里已无任何对齐栈写，只剩 `vmovdqu` 与 `lea -0x60(%rbp),%rax; call EnqueueBox(DebugBoxCommand const&)`。
- **验证：** 修复前 13 次冷启动 8 崩（61%）；修复后 **11 次冷启动 0 崩**，其中 **6 次恰好满足旧必崩条件（`rbp%32==16`）**。
- **回归开关：** `SceneEditingViewportClient` 里的 `EnableDebugDraw` 已是永久置位状态，注释改成说明「编辑器在两种 RHI 下使用同一组 draw flags」，不再有绕过说明。
- **系统性收口（2026-10-24）：** `minEngine/minEngine/CMakeLists.txt` 在 `add_subdirectory(Jolt)` 之后擦掉 Jolt 导出的 `INTERFACE_COMPILE_OPTIONS` 里的 `-mavx*` / `-mf16c` / `-mfma`，并回补 `INTERFACE -msse4.2`。Jolt 自身仍按 AVX2 构建，first-party TU 退回 SSE 基线。落点在自有 CMake 而不是 Jolt submodule，因为那是上游仓库，我们没有推送权。
- **顺带保留的诊断能力（`EditorCrashDiagnostics.cpp`）：** 崩溃记录改用真实 `GetLocalTime` 时间戳（原先是编译期 `__DATE__/__TIME__`，无法判断是哪一次冷启动）、新增 `RuntimeModuleBases`（记录 exe/DLL 的加载基址，符号化时不再靠猜 image base）、日志里去掉 `0x{:p}` 造成的 `0x0x` 双前缀。

### 遗留风险（不是本次崩因，但同源）
- `-mpreferred-stack-boundary=5` 在本 MinGW **不可用**（GCC 报 `not between 3 and 4`，Windows 端口上限 16B），`-mstackrealign` 也只到 16 → 不能靠编译选项根治。
- ~~全 DLL 19 处 `vmovdqa %ymmN,…, (%rbp)`（全在 fmt v12）~~ → 收口后 `libminEngine.dll` 里**这 19 处已归零**；剩余 175 处 ymm 栈写的符号**全部是 `JPH::*`**（Jolt 自己的编译单元，仍按 AVX2 构建），first-party 为 0。收口前它们不是理论风险：`scripts/debug/fmt_avx_probe.cpp` 用同样旗标调用 `fmt::format("{}", 3.14159)`，20 次里 10 次崩溃，崩溃指令正是那 19 处之一。
- **根因来源（已修）：** Jolt 的 `Jolt.cmake` 把 ISA 旗标作为 `PUBLIC` 编译选项导出，链接 Jolt 就等于给所有 first-party TU 打开 AVX（`-mf16c`/`-mfma` 在 GCC 里隐含 `-mavx`，所以只撤 `-mavx2` 不够）。上游 submodule 不可改，落点在 `minEngine/minEngine/CMakeLists.txt` 擦除 `INTERFACE_COMPILE_OPTIONS`。
- **仍属同类隐患（未做）：** 少数注册期 API 仍按值传参大结构体（审计见 [BY_VALUE_PARAM_AUDIT](../BY_VALUE_PARAM_AUDIT.md)）；当前 SSE 基线下不再生成 32B 对齐栈写，但若将来任何目标重新打开 AVX，同形状会立刻复发。

### 诊断手法留档（下次可直接复用）

> 状态：这套探针在定位完成后已从工作树移除，生产修复只剩 `Enqueue*` 的 `const&` 改动。下面是复现时用的形状，需要时按同样思路重建即可。
- 探针开关：`minEngine/minEngine/src/Runtime/Function/Debug/DebugDrawProbe.h` 的 `MEDBG_PROBE`（置 0 即全部编译掉）。
- 探针日志：`minEngine/bin/dbgdraw_probe.log`（`CreateFile/WriteFile/FlushFileBuffers`，崩溃处理与热路径都可安全重入）。
- 记录内容：模块运行期基址与 instance/guard RVA、每次采样的 guard 值变化、3 个帧检查点、首次 Box 时的对象原始十六进制、页内 0xFF 段扫描、首次脏样本的整页/整对象 dump（本次从未触发）。
- 独立证据程序：`scripts/debug/avx_align_probe.cpp`（复现异常签名）、`scripts/debug/avx_codegen_probe.cpp`（复现编译器生成对齐栈写）。

---

## 症状
- Editor 冷启动，场景加载成功后数秒内偶发无提示崩溃。
- 退出码：`-1073741819`（`0xC0000005`）。
- 日志末行常为 `EditorAppearance: UI font atlas rebuilt`（**误导**——二分 A 已排除）。

## 期望
- 冷启动稳定；Physics collider debug 可开且不 AV。

## 复现（GCC Debug，`minEngine/bin`）
```powershell
cd minEngine\bin
.\Editor.exe --rhi opengl --project <MyMEProject.meproject>
# 多次冷启动；历史约 30%（2026-09-02）
```

## 环境
- OS: Windows 10/11
- **易复现：** MinGW-Builds GCC 15 Debug（`minEngine/build` → `bin/`）
- **难复现：** ASan / clang++（`minEngine/build-asan` → `bin-asan/`）
- 项目：`MyMEProject`，默认场景 `test`

---

## 根因调查（截至 2026-09-03）

### 崩溃点（符号化）
`DebugDraw::Box` → `DebugDrawService::EnqueueBox`（`DebugDraw.cpp`）

### 调用栈演变

**S04 / 二分期（ImGui EndFrame 入队）：**
```
EditorViewportWindow::OnDraw → SceneEditingViewportClient::EndFrame
  → PhysicsDebugDraw::SubmitScene → DebugDraw::Box → EnqueueBox
```

**S05 落地后（仍崩，栈证明入队已挪到渲染）：**
```
Editor::Run → TickRendererFrame → RenderSystem::Tick
  → ForwardRenderer::Execute
    → PhysicsDebugDraw::SubmitScene → … → EnqueueBox
```

### 机制判断（更新）

1. **已确认：** 只要走 Physics collider → `EnqueueBox`，GCC Debug 冷启动可 AV；关掉入队则稳定。
2. **已确认：** 损坏模式是 vector/`this` 读 `0xFFFFFFFFFFFFFFFF`，不是 collider 数值算错。
3. **S04 不足：** 仅同条件门控 + 帧头 `ClearFrameQueues` 无法根治。
4. **S05 不足：** 入队与 `DebugDrawPass` 同 tick 后仍崩 → 更像 **入队前单例/堆已被踩**，而非单纯「入队/消费跨阶段」。
5. **ASan 未抓到：** 多次跑过 atlas 且无 `ERROR: AddressSanitizer`；高度怀疑 ASan quarantine / 布局改变导致原踩踏不再命中 `DebugDrawService`（**假阴性可能**）。

### 已排除
- UI font atlas rebuild 主因（二分 A）
- efsw 直接写 `DebugDrawService`
- 显卡驱动主因（栈在引擎）

### 已闭合（2026-10-24）
- ~~谁在首帧前写坏 `DebugDrawService` 单例 / `m_Boxes` 控制块~~ → **没有人写坏**；对象在崩溃瞬间仍是全零 + guard 已构造（见"根因"节）。
- ~~为何 clang ASan 布局下不触发、GCC 下高概率触发~~ → ASan 之所以"无报告"，是因为**本来就没有非法访存**，只有非法**对齐**；clang/ASan 构建的栈布局与代码生成不同，那条对齐指令没有出现或没有落在半对齐帧上。ASan 抓不到对齐异常，这是预期行为，不是假阴性。

---

## 已尝试的修复 / 基础设施

| Slice / 项 | 内容 | 效果 |
|------------|------|------|
| **S01/S02** | atlas invalidate、`ed_crash.log` | 诊断用，保留 |
| **S04/S04b** | 门控 + 帧头 Clear + eager-init；撤销过度 RT 守卫 | 曾误标 Resolved；**复发** |
| **S05** | `GameplayScene`；`Execute` 内入队；去掉 Editor 帧头 Clear | 栈正确，**仍崩** |
| **ASan 工程** | `MINENGINE_ENABLE_ASAN`；`scripts/debug/configure_asan.ps1` / `build_asan.ps1` / `run_editor_asan.ps1`；LLVM-MinGW；`--export-all-symbols`；efsw `char_traits` shim | 可构建运行；**未复现 / 无报告** |

### S05 目标生命周期（保留，正确方向）
```
EndFrame: SubmitSceneDraw(drawDesc)  // GameplayScene 已填

ForwardRenderer::Execute:
  ClearFrameQueues()
  PhysicsDebugDraw::SubmitScene(GameplayScene)
  … scene graph …
  DebugDrawPass::Prepare → BuildFrameGeometry → ClearFrameQueues
```

---

## 临时绕过（历史，已被真正修复取代）

与二分 B 等价，**不修根因**，仅避免触发：

- **已落地（历史）：** `SceneEditingViewportClient::EndFrame` 不再置 `SceneDrawFlags::EnableDebugDraw`（注释指向本 bug）。
- 因此 `ForwardRenderer::Execute` 不会调用 `PhysicsDebugDraw::SubmitScene`。

**2026-10-24（收口）：** 该标志已**永久恢复**（`| SceneDrawFlags::EnableDebugDraw`），注释改为说明「编辑器在 OpenGL / Vulkan 两种 RHI 下使用同一组 draw flags」，不再包含任何绕过说明。全仓复查：没有第二处因为本缺陷关闭 DebugDraw；`MaterialEditorViewportClient` 的材质预览视口本就不置该位，与物理 debug 无关。

---

## 实施状态
- [x] 符号化确认 `EnqueueBox`
- [x] 二分 A / B
- [x] S05 生命周期对齐（代码在树；**未过回归**）
- [x] ASan 构建链路（`bin-asan`）；多次冷启动无报告
- [x] 临时关 DebugDraw 合入（workaround；根因仍 Open）
- [x] ~~找到真正写坏内存的代码~~ → 结论：**不存在写坏者**，根因是对齐异常（2026-10-24）
- [x] GCC Debug 冷启动 11 次无 AV（含 DebugDraw 开启；其中 6 次满足旧必崩条件）
- [ ] 修复正式合入（本树已验证，未提交）
- [x] **系统性收口：Jolt 导出给 first-party 的 AVX 旗标已在自有 CMake 里擦除** —— `libminEngine.dll` 的 `vmovdqa %ymmN,(%rbp)` 从 19 处降到 **0**，first-party 的 ymm 栈写清零
- [x] 临时探针删除；`EnableDebugDraw` 永久恢复且注释改为说明理由
- [x] 冷启动回归（已归档批次见「验收数据」）：修前 13 次 8 崩；修后 16 次 0 AV
- [ ] Vulkan RHI 冷启动批次：尚未在本树归档，合入 master 前补跑

## 下一步建议（按优先级）
1. **已合入（单个提交，`feat/render`）：** `Enqueue*` 改 `const&` + `EnableDebugDraw` 恢复 + `minEngine/CMakeLists.txt` 擦除 Jolt 的 INTERFACE ISA 选项 + `EditorCrashDiagnostics` 诊断改进 + 本文档。
2. **守住这条线：** 任何新引入的第三方目标都可能再次把 AVX 旗标作为 INTERFACE 选项导出；改动链接关系后复查一次 `vmovdqa …, (%rbp)` 计数（当前 0）。
3. **别再用 ASan 是否报错作为本 bug 的判定手段**：对齐异常不是非法访存，ASan 天然不报。
4. **（可选）** 收敛 `BY_VALUE_PARAM_AUDIT` 里注册期大结构体的按值传参。

## 验收数据（本树归档）

脚本：`scripts/debug/cold_start_repro.ps1`（CSV 在 `scripts/debug/`，逐次日志在 `cold_start_logs/`）。

| 阶段 | 构建 | 次数 | 结果 |
|---|---|---|---|
| 修前基线 | `Enqueue*` 按值（AVX 仍在） | 13 | **8 崩**（8.4–12.7 s，`0xC0000005 / 0xFFFFFFFFFFFFFFFF`） |
| 只改 `const&` | AVX 仍在 | 11 | **0 崩**；其中 6 次落在旧"必崩"帧对齐（`rbp%32==16`） |
| 再擦除 AVX 旗标 | 当前 HEAD | 5 | **0 AV**：3 次存活到 90 s 截止，2 次正常退出（exit 0） |

> 并发跑多份 harness 时，"杀残留 MaximumEditor"的步骤会互相杀掉对方刚启动的进程，产生 **exit -1 / 0.3 s / 无日志** 的假崩溃。这类行（`CRASH_OTHER_0xFFFFFFFF` 且 `AliveSec < 1`）不计入上表。

复查命令（AVX 对齐栈写应为 0 处）：

```
objdump -d -C minEngine/bin/libminEngine.dll | grep -c 'vmovdqa %ymm.*(%rbp)'
```

## 验收数据（2026-10-24，`EnableDebugDraw` 打开）

构建：`cmake --build minEngine/build --target Editor -- -j 20`，ExitCode=0，约 4 分钟。
被测二进制：`MaximumEditor.exe` 18:39:25 / `libminEngine.dll` 18:38:51（SSE 基线版）。
命令：`MaximumEditor.exe --rhi {opengl|vulkan} --project minEngine/MyMEProject/MyMEProject.meproject`，每次保持 90s 后强杀；任何 `0xC0000005` 即不通过。

### OpenGL — 20 次冷启动

| # | 退出码 | 存活(s) | 判定 |
|---|--------|---------|------|
| 1 | TIMEOUT | 90.1 | 存活 |
| 2 | TIMEOUT | 90.3 | 存活 |
| 3 | TIMEOUT | 90.2 | 存活 |
| 4 | 0 | 45.3 | 正常退出（外部关窗，日志有完整 shutdown 序列） |
| 5 | 0 | 20.3 | 正常退出（同上） |
| 6 | TIMEOUT | 90.3 | 存活 |
| 7 | TIMEOUT | 90.1 | 存活 |
| 8 | TIMEOUT | 90.3 | 存活 |
| 9 | TIMEOUT | 90.3 | 存活 |
| 10 | TIMEOUT | 90.3 | 存活 |
| 11 | TIMEOUT | 90.3 | 存活 |
| 12 | TIMEOUT | 90.3 | 存活 |
| 13 | TIMEOUT | 90.4 | 存活 |
| 14 | TIMEOUT | 90.2 | 存活 |
| 15 | TIMEOUT | 90.2 | 存活 |
| 16 | TIMEOUT | 90.3 | 存活 |
| 17 | TIMEOUT | 90.3 | 存活 |
| 18 | TIMEOUT | 90.3 | 存活 |
| 19 | TIMEOUT | 90.3 | 存活 |
| 20 | TIMEOUT | 90.3 | 存活 |

**0 次 `0xC0000005`。** 两次正常退出是窗口被外部关闭（日志里是 `PhysicsSystem shutdown → GLFW Terminated → ObjectManager Shutdown` 的完整序列，不是崩溃）。为凑满 20 个存活样本另跑补充批次 21-25：全部 90.1-90.4s 存活。GL 合计 25 次启动、23 次存活、2 次正常退出、0 次崩溃。

### Vulkan — 10 次冷启动

| # | 退出码 | 存活(s) | 判定 |
|---|--------|---------|------|
| 1 | TIMEOUT | 90.5 | 存活 |
| 2 | TIMEOUT | 90.3 | 存活 |
| 3 | TIMEOUT | 90.4 | 存活 |
| 4 | TIMEOUT | 90.4 | 存活 |
| 5 | TIMEOUT | 90.3 | 存活 |
| 6 | TIMEOUT | 90.3 | 存活 |
| 7 | TIMEOUT | 90.3 | 存活 |
| 8 | 0 | 6.9 | 正常退出（外部关窗） |
| 9 | 0 | 5.0 | 正常退出（同上） |
| 10 | 0 | 80.5 | 正常退出（同上） |

**0 次 `0xC0000005`。** 补充批次 11-13 全部 90.2-90.4s 存活。VK 合计 13 次启动、10 次存活、3 次正常退出、0 次崩溃。

### 说明
- 38 次启动里 `minEngine/bin/ed_crash.log` 的 mtime 停在批次之前的 17:25，没有产生任何新崩溃记录。
- 正常退出的样本照实保留，不改判为存活：它们不是崩溃，而是窗口被外部关闭。
- 较早的两个 GL 批次作废：`s2gl` 被并行会话按进程名清理时互相杀掉（`exit=-1`、存活 0.3-38.6s、当次无日志），`s2gl2` 跑在旧落点的二进制上。压测脚本已改为只 Kill 自己 `Start-Process` 返回的进程。

## 关联
- [BUG-EDITOR-001](./BUG-EDITOR-001.md)
- [手动调试指南](./DEBUG_EDITOR_MANUAL.md)
- ASan 脚本：`scripts/debug/configure_asan.ps1`、`build_asan.ps1`、`run_editor_asan.ps1`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | 登记；初判 ImGui atlas |
| 2026-09-02 | 符号化 `EnqueueBox`；S04（曾标 Resolved，后复发） |
| 2026-09-03 | 二分 A 否定 atlas；二分 B 确认入队路径；S05 落地仍崩 |
| 2026-09-03 | ASan（LLVM-MinGW）可跑、多次不复现无报告；**保持 Open**；允许临时关 DebugDraw 推进开发 |
| 2026-09-03 | 合入 workaround：Editor viewport 默认不置 `EnableDebugDraw` |
| 2026-10-24 | 追加：fmt v12 里 19 处同款对齐栈写已用 `scripts/debug/fmt_avx_probe.cpp` 复现（20 跑 10 崩），`TECH_DEBT` 登记 TD-034（Jolt AVX 旗标 PUBLIC 泄漏） |
| 2026-10-24 | 复核推翻"内存被踩"结论：FaultRIP 指令为 `vmovdqa %ymm0,-0xC0(%rbp)`，是 Win64 只保证 16B 栈对齐下的 **AVX 对齐栈写 #GP**；异常里的 `0xFFFFFFFFFFFFFFFF` 是"无线性地址"哨兵（已用 20 行程序复现同签名）。guard=0x01 是单字节 guard 的"已构造"，非被涂。修复：`Enqueue*` 按值 → `const&`；验证 11/11 冷启动无崩（含 6 次旧必崩条件）。**Status: Open → Fixed（未提交）** | |
| 2026-10-24 | **收口：** 删除临时探针；`EnableDebugDraw` 永久恢复；AVX 泄漏改在自有 `minEngine/CMakeLists.txt` 擦除 Jolt 的 INTERFACE 旗标（`-mavx*`/`-mf16c`/`-fma`，因 `-mf16c`/`-mfma` 隐含 `-mavx`，只撤 `-mavx2` 不够）并回补 `INTERFACE -msse4.2`；`libminEngine.dll` 的 `vmovdqa %ymmN,(%rbp)` 19 → **0**，剩余 175 处全在 `JPH::*`；GL 25 次 / VK 13 次冷启动 **0 次 `0xC0000005`**（TD-034 → Done） |
