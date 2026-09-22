# Reflection + Header Tool Multi-Module — System Engineering Review

## Meta
- **Type:** System Engineering Review
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Branch / tip:** `master`
- **Related:** `CORE-F27`（方向修正）、`ED-F20`/`ED-F21`/`ED-F22`、`CORE-F02`/`CORE-F12`、minCraft `ENG-GAP-006`
- **Skill:** `engine-system-review`
- **Objective:** 外仓游戏模块能否走**同一套 ME_CLASS / header tool 管线**（非挂接热修）

---

## 1) Review Scope

| Item | Content |
|------|---------|
| **Target subsystem** | Reflection codegen 管线：`minEngine_header_tool.py` + CMake 钩子 + `ReflectionSystem` 注册 / Finalize + `GetClassName` / `NewObject` / `AddComponent` |
| **Review objective** | 从 minCraft 实践缺口反哺：引擎假设「只有一份 src」是否成立；重构方向应指向**多根扫描 / 分模块产物 / 分 DLL 链接** |
| **Explicit focus** | 引擎 src vs 项目/游戏 src 各自生成；游戏类型能否官方使用 `AddComponent<T>()`；与 Finalize / Startup 时序 |
| **Out of scope** | 体素玩法进 Core；完整 SDK install；LoadLibrary 热插拔；重写序列化 wire |
| **Directories / files inspected** | `scripts/minEngine_header_tool.py`；`minEngine/minEngine/CMakeLists.txt`；`ReflectionMacros.h` / `ReflectionAnnotations.h` / `Reflection.cpp`；`Engine.cpp` Initialize；`GameObject` AddComponent；`Generated/Reflection/*`；`GameModuleStub`；handbook `reflection.md` |
| **Related systems** | ED-F20 游戏 SHARED 链接；CORE-F02 ScriptBinding codegen；ObjectManager::NewObject |
| **Scenarios traced** | 引擎单根 codegen → 静态注册 → Finalize → NewObject/AddComponent；外仓无 ME_CLASS 失败路径；游戏 DLL 静态注册 vs Initialize 后 Startup |
| **Limitations** | 未跑 runtime 复现；未读 minCraft 仓源码（只读提示词缺口）；未审计全部 .gen 命名冲突边角 |

---

## 2) System Understanding

### Responsibilities
- **Header tool（Python）：** 扫描带 `ME_CLASS` / `ME_STRUCT` / `ME_ENUM` / `ME_PROPERTY` / `ME_FUNCTION` 的头文件；增量生成 `*.gen.h` / `*.gen.cpp`；写 `manifest.json`；可选 ScriptBinding 产物。
- **C++ 宏层：** 标记宏为空（给工具看）；`ME_GENERATED_BODY()` 声明 `StaticClass()` + `FieldAccessor` 友元；`ME_REFLECTION_*` 宏在 `.gen.cpp` 里完成 `GetClassName` 特化与静态注册。
- **ReflectionSystem：** 收集类/枚举/函数 → `FinalizeReflection` 解析父类与属性引用 → `Ready`；之后查询、`IsA`、序列化、Inspector、`NewObject` 按名构造。
- **消费方：** `NewObject<T>` / `AddComponent<T>` 依赖 `GetClassName<T>` 特化；运行时 `GetClass()` / `IsA` 依赖已注册的 `MEClass*`。

### Non-responsibilities
- 不解析完整 C++（regex / 轻量解析）。
- 不负责游戏玩法类型进引擎 Core。
- 不负责运行时热加载 DLL 后的二次 Finalize（今日模型是**一次性 Ready**）。

### Main abstractions / public surface
| 层 | 表面 |
|----|------|
| 作者 API | `ME_CLASS` + `ME_GENERATED_BODY` + 手写 `#include "*.gen.h"` |
| 工厂 | `NewObject<T>` / `AddComponent<T>` / `AddComponent(className)` |
| 查询 | `FindClass` / `StaticClass` / `IsA` |
| 工具 CLI | `--src-root` `--out-dir` `--manifest`（+ ScriptBinding 可选） |

### Ownership and lifetime
- `.gen.cpp` 内 `const bool REGISTER_SYMBOL = []{ CreateClass; RegisterClass; }();`：**加载期静态初始化**写入单例 `ReflectionSystem`。
- `Engine::Initialize` **最先**调用 `FinializeReflection()` → 状态 `Ready`。
- `Ready` 后 `EnsureCanRegister` **拒绝**再 `RegisterClass`（见证据）。

### Important dependencies
```text
header_tool (--src-root)
    → Generated/Reflection/*.gen.{h,cpp} + manifest
    → (GLOB) 链入 minEngine SHARED
静态构造注册 → ReflectionSystem (Collecting)
Engine::Initialize → Finalize → Ready
NewObject / AddComponent / Serialize / Inspector
```

### Key invariants
1. 每个反射类型必须有 codegen 产生的 `GetClassName<T>` 特化，否则模板路径编译失败。
2. 类名字符串为完全限定名（`#TYPE`，如 `minEngine::Component`）。
3. Finalize 一次；Ready 后不可再注册（当前实现）。
4. 头文件底部需 `#include` 对应 `.gen.h`（工具**不**自动注入 include）。

### Important state transitions / data flow
```text
Collecting ──Register*──► Collecting ──FinalizeReflection──► Ready
                              ▲                    │
                              └── Failed 可回到 Collecting
Ready ──Register*──► 失败（AppendError）
```

### Editor vs Runtime
- 同一 `ReflectionSystem`；Editor 通过 ED-F20 **链接期**加载游戏 DLL。游戏 `.gen.cpp` 的静态注册若在 Finalize **之前**随 DLL 加载执行，可进入同一收集表。

### Error / persistence / threading
- 注册失败进 `m_LastErrors`；Finalize 失败 `ME_ASSERT`。
- 主线程假设；无模块锁。

---

## 3) Findings

### R01 — Codegen 管线硬编码「单一引擎 src 根」

| Item | Content |
|------|---------|
| **Category** | Architectural Risk |
| **Severity** | High |
| **Confidence** | Confirmed |
| **Evidence** | `minEngine/minEngine/CMakeLists.txt`：`REFLECTION_SCAN_ROOT = …/src`；单次 `add_custom_command` 只传一个 `--src-root`；产物进 `src/Generated/Reflection` 并由 `GLOB_RECURSE ./src/*.cpp` 链入 **minEngine** 目标 |
| **Why** | 工具 CLI 虽已是「src-root 参数化」，但**引擎构建只调用一次、只扫引擎树**。外仓游戏头文件永远不会生成 `GetClassName` / 注册 TU |
| **Trigger** | 任意游戏 SHARED 在自有 src 写 `ME_CLASS` Component |
| **Impact** | `AddComponent<T>()` 编译失败；外仓被迫绕路（热修 API / 假 bounds / 宏拆构建）——这是缺口根因，不是症状 |
| **Recommended action** | 将 codegen 提升为 **可复用 CMake 函数**：`minengine_reflection_codegen(TARGET … SRC_ROOT … OUT_DIR …)`；引擎与游戏各调一次；游戏产物链进**游戏目标**，禁止扫进引擎 GLOB |
| **0.1.0 bucket** | **Should Fix Soon**（外仓 Dev 宿主成立的前提；与 0.1.0「可玩竖切在游戏仓」直接相关） |

### R02 — 注册生命周期假设「所有 .gen.cpp 都在 Finalize 前进入进程」

| Item | Content |
|------|---------|
| **Category** | Lifecycle / Ownership Issue |
| **Severity** | High |
| **Confidence** | Confirmed |
| **Evidence** | `Reflection.cpp` `EnsureCanRegister`：`Ready` 时拒绝 Register；`Engine.cpp` Initialize 开头即 `FinializeReflection()`。静态注册依赖 DLL load 顺序早于 Initialize |
| **Why** | 对**引擎 DLL 内**类型成立。对游戏：ED-F20 链接期加载时，游戏静态构造一般仍早于 `main`/Initialize → **可工作**。但若有人把注册放进 `GameModuleStartup`（Initialize 之后），或未来 LoadLibrary，会静默/报错失败 |
| **Trigger** | Initialize 后再 `RegisterClass`；或延迟加载游戏 DLL |
| **Impact** | 「有 codegen 仍不可见」；与 ED-F22 Startup 职责易混淆 |
| **Recommended action** | Design 写死契约：**(A) 推荐** 游戏 `.gen.cpp` 静态注册 + 链接期加载，Finalize 前收集完毕；**(B)** 若要 Startup 注册，必须先做「ReopenCollecting + Re-Finalize」或模块表增量 API（另切片）。**不要**把 ME_CLASS 注册默认放进 Startup |
| **0.1.0 bucket** | Should Fix Soon（文档 + 契约）；增量 Finalize = Future 除非 LoadLibrary 进范围 |

### R03 — 先前 CORE-F27「AddComponentInstance 热修」是症状缓解，不是体系解

| Item | Content |
|------|---------|
| **Category** | Architectural Risk |
| **Severity** | Medium（相对方向） |
| **Confidence** | Confirmed |
| **Evidence** | `AddComponent_Internal` 的 `GetClass()&&IsA` 门闩；`GetClassName` static_assert；用户与 minCraft 缺口描述 |
| **Why** | 无反射的挂接 API 可改善 Dev 摩擦，但留下：Inspector/序列化/按名 AddComponent/IsA 仍残缺；鼓励「永久无 ME_CLASS」旁路，与引擎以反射为轴的设计对立 |
| **Trigger** | 把路径 1 当官方终态 |
| **Impact** | 技术债固化；游戏仓继续双路径 |
| **Recommended action** | **重构主线改为多模块 codegen**；`AddComponentInstance` / `dynamic_cast` 门闩修正降为 **迁移期可选逃生口或小正确性修补**，不得替代主 Feature |
| **0.1.0 bucket** | Known Limitation 若短期保留逃生口；主线 = Should Fix Soon |

### R04 — 产物所有权与 include 约定未模块化

| Item | Content |
|------|---------|
| **Category** | Maintainability Issue |
| **Severity** | Medium |
| **Confidence** | Confirmed |
| **Evidence** | 引擎头手写 `#include "Generated/Reflection/Foo.gen.h"` 或 `"Foo.gen.h"`（依赖 `PUBLIC ${REFLECTION_GENERATED_DIR}`）；工具用 `relative_to(src_root)` 生成 `#include` 源路径；扁平 out-dir 按源文件 stem 命名 |
| **Why** | 多根时：同名头（两仓都有 `Types.h`）会撞 `.gen` 文件名；游戏需独立 OUT_DIR + include；游戏目标需 `target_include_directories` 指向自己的 Generated |
| **Trigger** | 第二源根启用 |
| **Impact** | 静默覆盖 / 编错文件 |
| **Recommended action** | 每模块独立 `OUT_DIR` + `manifest`；输出命名带模块前缀或保留相对路径子目录；文档规定游戏头 `#include` 方式 |
| **0.1.0 bucket** | Should Fix Soon（与 R01 同切片） |

### R05 — ScriptBinding codegen 同样单根、且聚合进引擎

| Item | Content |
|------|---------|
| **Category** | Architectural Risk |
| **Severity** | Medium |
| **Confidence** | Confirmed |
| **Evidence** | 同一次 header_tool 调用写 `Generated/ScriptBinding`；`ScriptBindingRegister.gen.cpp` 聚合 `RegisterGeneratedLuaBindings` 链在引擎内 |
| **Why** | 游戏 `ScriptCallable` 类型不会进该表；若强行扫进引擎根会错误归属 |
| **Trigger** | 游戏要 Lua 绑自有 Component |
| **Impact** | 脚本面不完整 |
| **Recommended action** | 与反射同构：每模块可选 ScriptBinding 产物；运行时 `RegisterGeneratedLuaBindings` + `RegisterGameLuaBindings`（或模块回调）。可后置于反射多根 MVP |
| **0.1.0 bucket** | Future Enhancement（MVP 可先只做反射，Script 后置） |

### R06 — `InsertRestoredComponent` / `AddComponent_Internal` 依赖 `GetClass()` 的残差

| Item | Content |
|------|---------|
| **Category** | Correctness Issue |
| **Severity** | Low→Medium（主线 codegen 落地后降为 Low） |
| **Confidence** | Confirmed |
| **Evidence** | `GameObject.cpp`：`GetClass() && IsA(SceneComponent)` |
| **Why** | 在**有** MEClass 时正确；无 MEClass 时 Root/Attach 错误。多模块 codegen 是正解；`dynamic_cast` 可作纵深防御 |
| **Recommended action** | 主线 codegen 后，可选将 Scene 判定改为 `dynamic_cast`（纵深）；勿当主交付 |
| **0.1.0 bucket** | Known Limitation / 小修补可并行 |

### R07 — 工具本身已接近「多调用友好」，缺的是产品化封装与生命周期契约

| Item | Content |
|------|---------|
| **Category** | Future Enhancement（能力已部分具备） / Positive 见下 |
| **Severity** | Informational |
| **Confidence** | Confirmed |
| **Evidence** | CLI 已要求 `--src-root` / `--out-dir` / `--manifest`；增量 cache 按文件；**并非**写死引擎路径在 Python 内 |
| **Why** | 重构重心在 **CMake 契约 + 模块产物归属 + Finalize 时序文档**，不是重写解析器 |
| **Recommended action** | 避免「重写 header tool」大爆炸；做 multi-invocation + CMake helper + stub 证明 |
| **0.1.0 bucket** | —（指导原则） |

---

## 4) Positive Findings

- **标记 / 生成分离清晰：** 空宏 + Python 生成 + C++ `ME_REFLECTION_*` 宏，职责可读（CORE-F12 进一步收紧 marker 归属）。
- **CLI 已参数化 src-root/out-dir/manifest：** 多模块是「多次调用 + 构建编排」，不是推倒工具。
- **静态注册 + 一次性 Finalize：** 对单 DLL 简单可靠；与 ED-F20 **链接期**游戏模块天然同序（只要游戏也链 `.gen.cpp`）。
- **RND-F18 已为自定义 Primitive 打开渲染扩展面：** 缺的是类型进入反射/工厂，不是再改 Renderer。
- **ED-F20 / TEST-F05 已提供宿主与测钩子：** 多模块 codegen 可落在同一 Dev 拓扑上验证。

---

## 5) Release Assessment (0.1.0)

| Bucket | Finding IDs |
|--------|-------------|
| **Must Fix Before 0.1.0** | （无单独 Critical Bug；若 0.1.0 验收依赖外仓自定义 Component Dev，则 **R01 实质升为 Must**） |
| **Should Fix Soon** | **R01**, **R02**（契约）, **R04**；主 Feature 重定向（R03） |
| **Known Limitation / Acceptable** | R06（有 codegen 后）；无逃生口时的旁路 API |
| **Future Enhancement** | R05 ScriptBinding 分模块；LoadLibrary 后增量 Finalize |

**Readiness judgment：**  
反射系统对**引擎内类型**已达可用基础。对「独立游戏仓写 `ME_CLASS` Component」**尚未就绪**——不是小 API 洞，而是 **构建/模块边界未产品化**。0.1.0 若以 minCraft 为验收竖切，应把 **多根 codegen** 视为 foundation 缺口，而非可选抛光。

---

## 6) Recommended Next Steps（有序、可验证）

1. **心态与文档纠偏（本回合）**  
   - 将 `CORE-F27` 主线改为：**Multi-module reflection codegen**（引擎 + 游戏各一 src-root / out-dir / manifest / 链入各自 TARGET）。  
   - 降级或删除「官方终态 = AddComponentInstance」表述；逃生口最多标 Transient。  
   - `ED-F22`：明确 **不**承担 ME_CLASS 注册；Startup 用于系统/默认 Host（在反射已 Ready 之后）。

2. **Design 定稿切片（确认后）**  
   - CMake：`minengine_add_reflection_codegen(target src_root out_dir)`  
   - 游戏 stub：`ME_CLASS` 空 Primitive + 自有 Generated + `AddComponent<T>()`  
   - 验证：开 ED-F20 stub → Editor/测试中 `FindClass` / `AddComponent<MyStubPrim>()`  

3. **生命周期契约写进 Design**  
   - 链接期模块：静态注册必须在 Finalize 前（默认满足）。  
   - 禁止在 Startup 里首次 RegisterClass（除非另做 Re-Finalize Feature）。

4. **后置**  
   - ScriptBinding 分模块；完整 SDK export；LoadLibrary 增量反射。

5. **不要做的**  
   - 把 minCraft 源码搬进引擎。  
   - 以 `dynamic_cast` 挂接 API 替代 codegen 主线。  
   - 重写整个 header tool 解析器（无证据表明必要）。

---

## 7) Open Questions

| ID | Question | Blocked on |
|----|----------|------------|
| Q1 | 游戏类型命名空间约定？`minCraft::` vs 全局？类名字符串是否允许无命名空间前缀？ | 维护者 |
| Q2 | 游戏模块 ScriptBinding 是否进本 Feature MVP，还是严格后置？ | 维护者（建议后置） |
| Q3 | Finalize 是否需要「模块波次」API（多 DLL 延迟加载）？0.1.0 默认仅链接期？ | 维护者（建议 0.1.0 仅链接期） |
| Q4 | `.gen.h` include 是否继续手写，还是工具改写头文件？（今日手写；改工具风险高） | 建议保持手写 + 文档 |

---

## 8) Target architecture（重构方向一句话）

```text
┌─ Engine configure ─────────────────────────────────────┐
│  header_tool --src-root <engine/src>                   │
│             --out-dir   <engine/.../Generated/Reflection>
│             --manifest  <engine-manifest>                │
│  → .gen.cpp ∈ minEngine.dll  (static register)         │
├─ Game (ED-F20 add_subdirectory) ───────────────────────┤
│  header_tool --src-root <game/src>                     │
│             --out-dir   <game/.../Generated/Reflection>│
│             --manifest  <game-manifest>                  │
│  → .gen.cpp ∈ Game.dll       (static register)         │
├─ Process load: Engine regs + Game regs → Collecting    │
├─ Engine::Initialize: FinalizeReflection → Ready        │
└─ AddComponent<GameType>() / Inspector / Serialize OK   │
```

**与热修对比：** 热修绕过表；重构让游戏类型**进入同一张表**——这才是反哺引擎的正确迭代。
