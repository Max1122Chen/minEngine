# CORE-F28 — Noise Math Library — Design Spec

## Meta
- **ID:** `CORE-F28`
- **Type:** Feature
- **Complexity:** L2
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - [Implementation](./CORE-F28_NOISE_MATH_LIBRARY_IMPLEMENTATION.md)
  - 外部输入（非权威）：[minEngine_Noise_Library_Agent_Prompt.md](../../../external/minEngine_Noise_Library_Agent_Prompt.md)
  - 催化剂：minCraft 程序化世界生成（**消费方**，非本 Feature DoD）
  - 哲学：[ENGINE_DESIGN_PHILOSOPHY.md](../../ENGINE_DESIGN_PHILOSOPHY.md) — Mechanism over Policy；Minimal Core
  - Math 先例：`Runtime/Core/Math/` · `Math::Geometry::Frustum`（RND-F19）
  - Godot（只读锚点）：`modules/noise/noise.h` · `modules/noise/fastnoise_lite.h`
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** 现有 `minEngine::Math`（`Vector2`/`Vector3`）与 `minEngineTests` harness
- **Blocks:** minCraft 可换算法的噪声场；未来 NoiseTexture / Material Noise / Domain Warp（另开 Feature）

## TL;DR

在 `Runtime/Core/Math/Noise` 落地一套**小巧、可测、可组合**的程序化噪声数学库。

- **抽象：** 虚基类 `Noise`（对齐 Godot `Noise` 分层；**不是** Resource / 资产）。
- **v1 类型：** `ValueNoise` · `PerlinNoise` · `CellularNoise` · `FractalNoise`（FBM 组合器）。
- **契约：** seed + 参数 + 坐标 → deterministic；采样输出约 `[-1, 1]`；配置完成后 `Sample*` 只读，可读侧多线程并发。
- **不做：** Terrain/Voxel/Biome、NoiseTexture、Editor、Reflection/Serialization、嵌入 FastNoiseLite。

## Scope

### In（CORE-F28）

- 目录 `Runtime/Core/Math/Noise/`
- 虚基类 `Noise` + 1D / 2D / 3D 采样
- 晶格 hash / permutation 工具（`NoiseHash`）
- `ValueNoise`、`PerlinNoise`、`CellularNoise`（Cellular 只做实用最小集）
- `FractalNoise`：对任意 `Noise` 源做 FBM（octaves / lacunarity / gain / frequency）
- 单测 suite `noise`（determinism / seed / range / continuity / fractal 边界）
- 可选：测试侧导出极小灰度图（如 `.pgm`）——**不是**引擎正式产品能力

### Out / 另轨

| 项 | 归属 |
|----|------|
| Terrain / Chunk / Biome / Voxel / WorldGen | 游戏（minCraft 等） |
| Simplex / OpenSimplex / Ridged / PingPong | 后续切片或 Feature |
| Domain Warp / Curl | 后续（API 留扩展口，见 §3.6） |
| GPU Noise / Compute | 后置 |
| NoiseTexture / Material Graph 节点 | 后置 `ASSET-*` / `MAT-*` / `ED-*` |
| Reflection / Serialization / Inspector | Out（调用方自己存参数） |
| Lua binding | 有需求再开 |
| 嵌入 FastNoiseLite 第三方 | **明确不做** |

## Reader quick start

1. §0 Pre-flight · §3 架构 · **§4 API** · §5 契约
2. §7 切片 · §8 验证 · §9 开放问题
3. Godot 对照：§3.7（学分层，不抄 API）

---

## 0) Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| Complexity | **L2** — 新子模块 + 多态 API + 数值契约；大面积开码前需 Design Review |
| Prerequisites | Math 别名 / 测试 harness：**sound**；仓库内尚无 Noise |
| Tech-debt risk | **Medium** — 风险是做成巨型 FastNoiseLite 单体，或过早绑资产/反射 |
| WIP | Primary 空闲；可升 Primary 或作侧轨 |
| Philosophy | Core 提供可采样伪随机场；世界生成策略留游戏 — **合** |
| 建议 | **Go** — Draft → Review → Impl；v1 = 四类型 + FBM + 测试 |

---

## 1) 问题与目标

### 1.1 问题

- 程序化生成（minCraft 与未来项目）需要 deterministic、可 seed、可组合的空间噪声。
- 引擎 `Math` 目前只有向量/几何。
- 各游戏自写 Perlin 会重复、难测，也无法共享给后续 Material/VFX。

### 1.2 目标

1. 上层依赖「可采样噪声场」抽象，而不是绑死某一种算法类型名。
2. v1 覆盖 Value / Perlin / Cellular + Fractal(FBM)。
3. 纯数学：无 Scene / Asset / Editor / RHI 依赖。
4. 可自动化验证（不要「看起来能跑」）。
5. 为 Domain Warp / 更多算法留扩展空间，但不一次做完。

### 1.3 成功标准

- `minEngineTests.exe test noise` 通过。
- 游戏可写：`const Noise& field = ...; field.Sample2D(x, z);`，换实现不改调用签名。
- Core 不泄漏 Terrain/WorldGen 类型。

---

## 2) 需求 / 约束 / 偏好 / 假设

### 需求（Requirements）

- 虚基类抽象（维护者拍板）：以 `Noise*` / `Noise&` / `shared_ptr<Noise>` 消费。
- 支持 1D / 2D / 3D 采样。
- Deterministic：相同配置 + seed + 坐标 → 相同结果（同平台/同编译器；浮点容差见 §5）。
- Fractal/FBM 可叠在任意基础 `Noise` 上。
- Sample 热路径：无堆分配、不隐式推进 RNG 流。
- 配置完成后：并发 `const Sample*` 安全。

### 约束（Constraints）

- 不引入新的第三方噪声库。
- 不承诺跨平台 bit-identical。
- 不把 WorldGen 语义写进 Core。
- 遵循项目 C++ 风格；公开类型使用 `MINENGINE_API`。

### 偏好（Preferences）

- 参考 Godot **分层**：抽象 `Noise` → 具体实现 →（上层）纹理/生成器；**不**机械复制 Godot API 名或 Resource 模型。
- Fractal 用 **组合**（compositor 持有源 `Noise`），不要「算法 × 分形」巨型继承树。
- 输出优先统一为约 `[-1, 1]`。

### 假设（Assumptions）

- minCraft 近期主要需要 2D/3D + FBM；Cellular 对地貌/洞穴纹理有用。
- 引擎近期不需要把 Noise 参数塞进反射/资产管线。

### 非目标（Non-Goals）

见 Out 表。另：minCraft 某关卡可玩 **不是** 本 Feature 验收条件。

---

## 3) 架构方案

### 3.1 落点

```text
Runtime/Core/Math/
├── Math.h
├── Geometry/
└── Noise/             ← NEW
    ├── Noise.h
    ├── NoiseHash.h/.cpp
    ├── ValueNoise.h/.cpp
    ├── PerlinNoise.h/.cpp
    ├── CellularNoise.h/.cpp
    └── FractalNoise.h/.cpp
```

Namespace：`minEngine::Math::Noise`。  
公开类型带 `MINENGINE_API`。源文件纳入现有 `minEngine` 目标（与其他 Math 文件一致）。

### 3.2 分层

```text
Noise (abstract field)
  ├── ValueNoise
  ├── PerlinNoise
  ├── CellularNoise
  └── FractalNoise ──owns/shares──► Noise (source)
         │
         ▼  future (not v1)
    DomainWarpNoise / Ridged / …
         │
         ▼  other Features
    Texture / Material / WorldGen (game)
```

### 3.3 抽象基类

```cpp
namespace minEngine::Math::Noise
{
    class MINENGINE_API Noise
    {
    public:
        virtual ~Noise() = default;

        virtual float Sample1D(float x) const = 0;
        virtual float Sample2D(float x, float y) const = 0;
        virtual float Sample3D(float x, float y, float z) const = 0;

        float Sample(float x) const { return Sample1D(x); }
        float Sample(const Vector2& p) const { return Sample2D(p.x, p.y); }
        float Sample(const Vector3& p) const { return Sample3D(p.x, p.y, p.z); }
    };
}
```

要点：

- 标量入口为纯虚；向量重载为非虚包装。
- **不是** `Resource` / `MEObject`；无反射。
- 未来可加非虚 `SampleBatch` 默认实现；v1 可不暴露。

### 3.4 具体类型（v1）

| 类型 | 职责 | 关键参数 |
|------|------|----------|
| `ValueNoise` | 晶格值 + 平滑插值 | `Seed`, `Frequency` |
| `PerlinNoise` | 经典梯度噪声 | `Seed`, `Frequency` |
| `CellularNoise` | Worley-like；v1 固定一种距离/返回形态 | `Seed`, `Frequency`, 可选 `Jitter` |
| `FractalNoise` | 对任意源做 FBM | `Source`, `Octaves`, `Lacunarity`, `Gain`, `Frequency` |

每种具体类型：

- `SetSeed` / `SetFrequency` 等在**配置阶段**更新内部表；`Sample*` 从不重建表。
- 默认允许值拷贝（permutation 表不大）。`FractalNoise` 拷贝语义见 §3.5。

### 3.5 Fractal 所有权

```cpp
class FractalNoise : public Noise
{
    std::shared_ptr<Noise> m_Source; // SetSource 成功后非空
    // fractal params...
};
```

| 规则 | 行为 |
|------|------|
| `SetSource(nullptr)` | 拒绝或断言；`Sample*` 前源必须有效 |
| 共享 | 多个 `FractalNoise` 可共享同一基础实例（只读采样） |
| 突变 | 不要让 `Set*` 与 `Sample*` 并发；配置期单线程 |
| 为何不只用 unique | 多态共享很常见；代价可接受 |

未选：模板静态组合——与「运行时换算法 + 虚基类」目标冲突。

### 3.6 扩展钩子（本期不实现）

- **Domain Warp：** 未来 `DomainWarpNoise : Noise`，先扭曲坐标再调源 `Sample*`。基类只暴露「坐标 → 值」，不堵死域变换。
- **Ridged / PingPong：** 可作为 `FractalMode` 或独立 compositor 后置；v1 仅 FBM。
- **Batch / SIMD：** 以后可加非虚默认批量接口。

### 3.7 Godot 对照（源码锚点）

| Godot | minEngine CORE-F28 |
|-------|-------------------|
| `Noise` + `get_noise_1d/2d/3d`（`modules/noise/noise.h`） | `Noise` + `Sample1D/2D/3D` — **采纳分层** |
| `Noise : Resource` | **不采纳** — 纯数学 |
| `FastNoiseLite` 单类内嵌 type/fractal/cellular/warp（`fastnoise_lite.h`） | **不采纳单体** — 分类型 + `FractalNoise` 组合 |
| `NoiseTexture2D/3D` | Out — 另 Feature |
| 第三方 `FastNoiseLite.h` | **不嵌入** |

结论：学 Godot 的「抽象场 + 上层工具」，不学「一个类吞掉所有算法开关」。

### 3.8 Random vs Noise

| | Random（若未来有） | Noise |
|--|-------------------|-------|
| 语义 | 独立随机事件 / 流 | 空间连续（或结构）的伪随机场 |
| 状态 | 调用推进 | 由坐标决定值；Sample 不推进 |
| 复用 | 可共享 hash 原语 | **禁止**在 Sample 内依赖会推进的 RNG |

仓库当前无通用 `Random` 模块；`NoiseHash` 自包含即可。

---

## 4) API / 接口语义

### 4.1 基类

| API | 语义 |
|-----|------|
| `Sample1D/2D/3D` | 纯只读；配置不变时可多线程读 |
| 返回值 | 约 `[-1, 1]`（若 Cellular 天然非对称，类注释写明并尽量归一） |
| 错误 | 不抛异常；非法参数在 Set 时 clamp 或拒绝并保持旧状态（实现钉一种并测） |

### 4.2 共享设置（具体类）

```cpp
void SetSeed(uint32_t seed);
uint32_t GetSeed() const;

void SetFrequency(float frequency); // > 0
float GetFrequency() const;
```

`Frequency`：采样前缩放输入域（`p * frequency`）再做晶格查询。

### 4.3 FractalNoise

```cpp
void SetSource(std::shared_ptr<Noise> source);
std::shared_ptr<Noise> GetSource() const;

void SetOctaves(int octaves);       // clamp 例如 [1, 16]
void SetLacunarity(float lac);      // 默认 2
void SetGain(float gain);           // 默认 0.5
void SetFrequency(float frequency); // 外层频率
```

**频率组合（默认拍板）：**

- `FractalNoise` 先做 `p * m_Frequency`，再按 `lacunarity^i` 缩放各层。
- 源自身的 `Frequency` **仍然生效**。文档建议：源 Frequency 常留 `1`，只调 Fractal 的 Frequency。

**归一化：** 按振幅和归一，使 octave 数变化时输出仍约在 `[-1,1]`。`octaves==1` 等价于单次源采样（含外层 frequency）。

### 4.4 Cellular v1 最小集

- Distance：Euclidean 或 Squared（实现选一并写进文档）。
- Return：**只做一种** — 默认 **Distance**（见 Q2）。
- 可选 `Jitter` ∈ `[0, 1]`。

---

## 5) 状态、确定性、线程

### 5.1 不变量

- 相同 `(type, seed, params, coord)` → 相同 float（同平台；测试用 Approx/epsilon）。
- `Sample*` 不修改逻辑状态（无 mutable 采样缓存）。
- `Set*` 与 `Sample*` 并发 = 禁止（文档约定）。

### 5.2 确定性层级

| 层级 | 承诺 |
|------|------|
| 同进程重复调用 | **必须** |
| 同平台同编译器 CI | **必须** |
| 跨平台 bit-identical | **不承诺** |

### 5.3 失败模型

- 无「业务失败」；NaN 输入行为未定义。
- Fractal 空源：Debug assert 或返回 `0`（钉一种；写测试）。
- 异常不是正常控制流。

---

## 6) 备选方案与取舍

### Option A（推荐）— 虚基类 + 分类型 + Fractal 组合

- Pros：符合维护者目标；可换算法；日后 Domain Warp 自然；避免参数墙。
- Cons：虚调用开销（chunk 生成可接受）；文件略多。

### Option B — 无虚函数，全 concrete / 模板

- Pros：可内联。
- Cons：与「依赖抽象场」不符。**不选**（已拍板虚基类）。

### Option C — 嵌入 FastNoiseLite + 薄包装

- Pros：功能全、省时间。
- Cons：第三方黑盒、参数面过大、学习动机弱。**不选**。

### Option D — Godot 式单体类（enum 切算法）

- Pros：一个对象调完。
- Cons：类膨胀、状态纠缠、难单测单算法。**不选**。

---

## 7) 实现切片（提案）

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | `Noise` + `NoiseHash` + `ValueNoise` + suite 骨架 | determinism / seed / range |
| **S02** | `PerlinNoise` | continuity 抽样 + range |
| **S03** | `CellularNoise`（最小） | determinism；可选可视化 |
| **S04** | `FractalNoise` FBM | octaves/gain/lacunarity 边界 + 归一 |
| **S05** | 文档收口 + 可选 PGM 测试工具 | Design → Done |

Design Review（L2）通过后再写 Implementation Plan。

---

## 8) 验证策略

自动化（必须）：

- Determinism：双实例、重复 Sample。
- Seed variation：不同 seed 在固定点集上结果不全等。
- Range：大量随机点落在文档约定区间。
- Continuity（Value/Perlin）：小步长相邻采样差值有界，无不合理尖峰。
- Fractal：`octaves=1`；`gain=0`；非法 octave clamp。
- Regression：锁定若干 `(seed, x,y,z) → value` golden（Approx）。

手动 / 可选：2D 灰度 `.pgm` 肉眼查伪影。

命令：`cmake --build minEngine/build --target minEngineTests`；`minEngineTests.exe test noise`。

---

## 9) 开放问题

| # | 问题 | 是否阻塞 | 默认 |
|---|------|----------|------|
| Q1 | `Frequency` 默认 `1.0` 还是 Godot 风 `0.01`？ | no | **`1.0f`**（游戏显式设世界尺度） |
| Q2 | Cellular v1 return：Distance vs CellValue？ | no | **Distance** |
| Q3 | Fractal 源：`shared_ptr` vs 仅 unique？ | no | **`shared_ptr`** |
| Q4 | 是否升 ACTIVE_WORK Primary？ | no | 维护者定 |
| Q5 | 是否要 `GetOutputRange()`？ | no | v1 **不做** |

---

## 10) 集成影响

- 仅增加 Math 子目录与测试；启动路径不变。
- minCraft 直接链接采样；WorldGen 仍留游戏仓。
- 无旧 API；forward-only。

---

## 11) 哲学检查（摘要）

| 问 | 答 |
|----|-----|
| Capability or Opinion？ | Capability（数学场） |
| 进 Core？ | 是 — 通用、无玩法语义 |
| 可不用？ | 是 |
| Agent-friendly？ | 结构化 C++ API；资产/命令后置 |
| 过早复杂？ | 避免单体与第三方；Cellular 保持最小 |

---

## Design Review

- **Verdict:** Ready with deferred items (Review skipped by maintainer go-ahead)
- **Reviewer / date:** —
- **Link or summary:** —

## Implementation Readiness

- [x] Requirements understood
- [x] Constraints identified
- [x] Relevant existing architecture inspected（Math + Godot noise 模块）
- [x] Responsibilities and boundaries defined
- [x] Key dependencies understood
- [x] Ownership/lifetime resolved（`shared_ptr` 源；配置期单线程）
- [x] Major state/invariants resolved
- [x] API semantics sufficiently defined
- [x] Important failure modes considered
- [x] Alternatives considered
- [x] Integration impact understood
- [x] Verification strategy exists
- [x] Unresolved questions resolved or explicitly deferred/accepted — Q1–Q5 采用默认

**Ready for implementation?** **yes** — 维护者批准开码；实现已完成（见 Implementation）。

## Acceptance Checklist

- [x] Design review complete (maintainer go-ahead)
- [x] Implementation plan traces to design decisions
- [x] Progress log updated when status changes
- [x] Feature registry status synced
