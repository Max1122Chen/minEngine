# minEngine Noise Math Library — Agent Implementation Prompt

## 0. 任务目标

请在 minEngine 中设计并实现一套**小巧、通用、易扩展、可长期复用的 Procedural Noise 数学库**。

这套库的定位不是 Minecraft Terrain Generator，也不是某个具体游戏的程序化生成系统，而是 Runtime/Core 层可被多个系统和未来项目复用的数学基础设施。

核心设计思想参考 Godot `Noise -> FastNoiseLite -> NoiseTexture` 的分层理念，但**不要机械复制 Godot API**。

目标：

- API 小而稳定
- 实现足够常用的 Noise 类型
- 支持 deterministic / seed
- 支持 1D / 2D / 3D sampling
- 支持 Noise 的组合与 Fractal
- 为未来 Domain Warp、Noise Texture、Procedural Generation、Material/VFX 等系统留下扩展空间
- 不把 Terrain/Voxel/Minecraft 业务逻辑塞进 Noise 模块
- 尽量保持纯数学、低耦合、易测试
- 不为了“完整”而一次性实现大量算法
- 设计优先于代码数量

---

# 1. 开始工作前：先阅读项目

不要直接开始写代码。

首先检查当前 minEngine 仓库的：

- `src/Runtime/Core`
- `src/Runtime/Function`
- `src/Runtime/Resource`
- `src/Runtime/Core/Math` 或等价数学模块
- `../Reflection`
- CMake/build 配置
- 现有数学类型，例如 Vector、Transform、Color 等
- 命名规范、namespace、头文件组织方式
- DLL export / `ENGINE_API` 使用方式
- 单元测试/测试项目现状
- serialization/reflection 是否应该参与本模块

目标是让 Noise Library **融入现有架构，而不是建立一套孤立风格**。

如果仓库当前已有类似 Noise、Random、Procedural、Math utility 实现，优先评估是否可以扩展，而不是重复造轮子。

---

# 2. 架构定位

推荐的概念层次：

```text
Runtime
└── Core
    └── Math
        └── Noise
            ├── Noise 基础抽象
            ├── 基础 Noise
            │   ├── Value
            │   ├── Perlin / Gradient
            │   └── Cellular
            ├── Fractal / FBM
            └── 未来扩展
                ├── Domain Warp
                ├── Simplex/OpenSimplex
                ├── Ridged
                └── 其他组合器
```

最终希望能够形成类似：

```text
Noise
  ↓
Basic Noise
  ↓
Fractal / Compositor
  ↓
Domain Warp / Other Modifier
  ↓
Gameplay / Terrain / Material / VFX / Texture
```

注意：

**Noise Library 不负责：**

- Terrain
- Voxel
- Biome
- Chunk
- World Generation
- Texture Asset
- Material
- Gameplay
- Editor UI

这些属于上层系统。

---

# 3. 核心抽象

优先考虑一个足够简单的“可采样 Noise Field”抽象。

概念上：

```cpp
class Noise
{
public:
    virtual float Sample(float x) const;
    virtual float Sample(Vec2 p) const;
    virtual float Sample(Vec3 p) const;
};
```

但不要盲目照抄。

结合 minEngine 当前代码风格判断：

- 是否应该使用 interface / abstract class
- 是否应该使用模板/static polymorphism
- 是否需要虚函数
- 是否应该提供 concrete type
- 是否需要同时提供 scalar sampling 和 batch sampling

重点是：

> 上层代码应该依赖“一个可以在空间中采样的连续伪随机场”，而不是依赖具体算法。

例如未来：

```cpp
NoiseField terrain;
NoiseField cave;
NoiseField biome;
```

而不是：

```cpp
PerlinNoise terrain;
PerlinNoise cave;
```

---

# 4. 第一版支持范围

第一版只实现“少而够用”的功能。

## 必须支持

### 4.1 Value Noise

用途：

- 简单 procedural pattern
- Terrain
- Texture
- 作为其他 Noise 的基础

要求：

- 1D
- 2D
- 3D
- deterministic
- seed
- 平滑插值

---

### 4.2 Gradient / Perlin Noise

优先实现成熟、经典、容易验证的 Perlin/Gradient Noise。

要求：

- 1D
- 2D
- 3D
- deterministic
- seed
- 合理的连续性
- 输出范围和归一化策略明确

不要为了性能过早进行复杂 SIMD 优化。

---

### 4.3 Cellular / Worley Noise

第一版可以只实现一个实用版本，不需要一次支持所有 distance metric / return type。

至少能够产生典型 Cellular / Voronoi-like pattern。

如果完整实现明显增加复杂度，可以将高级 feature 设计为未来扩展。

---

### 4.4 Fractal / FBM

这是第一版的重要组成部分。

应该允许把一个基础 Noise 多层叠加。

至少支持：

```text
octaves
frequency
lacunarity
gain/amplitude
```

概念：

```text
result =
    noise(p * frequency0) * amplitude0
  + noise(p * frequency1) * amplitude1
  + ...
```

需要考虑：

- 如何归一化
- 参数合法性
- octave = 0 / 1 的行为
- gain / lacunarity 的边界
- seed 是否在不同 octave 中变化
- 输出范围是否稳定

API 应该让未来增加：

```text
FBM
Ridged
PingPong
```

时不需要破坏基础 Noise API。

---

# 5. 暂时不要实现的内容

第一版不要为了“功能齐全”实现：

- Simplex
- OpenSimplex
- Ridged
- PingPong
- Domain Warp
- Curl Noise
- GPU Noise
- Noise Texture
- Material Graph Noise Node
- Editor Noise Asset
- Terrain Generator
- Voxel Generator

但 API / 架构应当允许未来自然增加这些能力。

尤其是：

```text
Domain Warp
```

未来可能非常有用，因此不要把当前 API 设计成“只能修改 noise value、无法修改 sampling domain”。

---

# 6. Seed / Determinism

这是 Noise Library 的核心要求。

同样的：

```text
algorithm
seed
parameters
position
```

必须得到相同结果。

例如：

```cpp
Noise a(seed);
Noise b(seed);

a.Sample(p) == b.Sample(p)
```

在允许的浮点误差范围内成立。

不同 seed 应产生不同场。

需要明确：

- seed 的类型
- permutation/hash 的生成方式
- 是否允许跨平台严格 bit-identical
- 是否只是“同一平台/编译器下 deterministic”
- 浮点计算造成的跨平台差异如何处理

不要在没有必要的情况下承诺“所有 CPU/GPU/平台 bit-exact”。

---

# 7. 输出范围

必须明确每一种 Noise 的输出约定。

推荐优先考虑：

```text
[-1, 1]
```

作为底层 Noise 的统一数学语义。

但如果实际实现天然是：

```text
[0, 1]
```

也可以选择其他方案。

关键是：

> 不允许不同 Noise 随意使用不同范围而没有明确文档。

Fractal Noise 也必须明确其输出范围或 normalization 行为。

例如：

```cpp
float value = noise.Sample(p);
```

开发者应该能够可靠理解：

```text
value ∈ approximately [-1, 1]
```

而不是必须阅读源码才能知道。

---

# 8. 参数设计

避免 API 膨胀。

推荐围绕以下核心参数：

```text
Seed
Frequency
Octaves
Lacunarity
Gain
```

不要第一版加入几十个参数。

参数应该：

- 有明确默认值
- 有明确含义
- 有合理边界
- 尽可能与其他 Noise 类型共享概念

例如：

```cpp
NoiseSettings
{
    uint32 seed;
    float frequency;
};
```

以及：

```cpp
FractalSettings
{
    int octaves;
    float lacunarity;
    float gain;
};
```

具体是否拆成 settings struct，要结合项目现有风格判断。

---

# 9. 性能设计

第一版不需要过度优化，但需要避免明显错误。

Noise 常见调用方式可能是：

```cpp
for (int z = 0; z < chunkSize; ++z)
    for (int x = 0; x < chunkSize; ++x)
        noise.Sample(x, z);
```

因此需要注意：

- 不要每次 Sample 都动态分配
- 不要每次 Sample 都重新初始化 permutation table
- seed 初始化应该发生在构造/配置阶段
- 尽量保持 Sample 为轻量操作
- 不要使用 mutex 等不必要同步
- 允许未来增加 batch/vectorized sampling

如果 permutation table / lookup table 是合理的缓存，应作为 Noise instance 的 immutable state。

---

# 10. Thread Safety

目标是：

> 配置完成后的 Noise 对象应该尽可能可以被多个线程并发读取。

因此：

```cpp
noise.Sample(...)
```

不应该隐式修改共享状态。

避免：

```cpp
mutable cache
```

这种容易造成 data race 的设计，除非有明确必要。

如果未来需要 runtime mutation：

```cpp
SetSeed(...)
SetFrequency(...)
```

则这些属于配置阶段，不应该与并发 Sample 混用而没有明确语义。

---

# 11. 不要把 Random 和 Noise 混为一谈

项目里可能已经有 Random。

需要明确：

```text
Random
    → 独立随机事件

Noise
    → 空间连续的伪随机场
```

例如：

```cpp
Random.NextFloat();
```

与：

```cpp
Noise.Sample(x, y);
```

是完全不同的抽象。

如果项目已有 Random，Noise 应该复用其合理的 hash/seed 基础设施，但不要让 Noise 依赖一个会不断推进内部状态的 RNG。

---

# 12. 推荐的实现策略

不要从复杂公式开始。

建议：

## Step 1

先建立最小 Noise abstraction。

## Step 2

实现 hash / deterministic lattice random utility。

## Step 3

实现 Value Noise。

## Step 4

实现 Gradient / Perlin Noise。

## Step 5

实现 Cellular。

## Step 6

实现 Fractal / FBM compositor。

## Step 7

补齐测试和文档。

## Step 8

再评估是否需要 batch sampling / SIMD。

---

# 13. 测试要求

Noise 非常容易出现“看起来能跑，实际上数学实现有 bug”的情况。

必须建立自动化测试。

至少测试：

### Determinism

```text
same seed + same coordinate
→ same result
```

### Seed variation

```text
different seed
→ generally different result
```

### Continuity

对于 Perlin / Value：

```text
Sample(p)
Sample(p + ε)
```

结果应该不会出现不合理的跳变。

### Range

验证输出是否符合文档声明。

### Boundary

测试：

```text
integer coordinates
negative coordinates
large coordinates
zero
very small values
```

### Fractal

测试：

```text
octaves = 1
octaves = N
gain = 0
lacunarity = 1
```

### Regression

以后修复 Noise bug 时加入 regression case。

---

# 14. 可视化验证

如果 minEngine 当前有合适的测试/示例环境，建议创建一个非常小的 Noise visualization。

例如：

```text
2D Noise → grayscale image
```

分别展示：

```text
Value
Perlin
Cellular
FBM
```

这不是 Noise Library 的正式业务功能。

它只是开发/测试工具。

如果引擎现有架构不适合加入图片生成工具，不要为了它修改大量系统。

可以使用一个简单 test executable 输出：

```text
noise_test.pgm
```

或者其他已有的最小图片输出方式。

重点是能够肉眼确认：

- 是否连续
- 是否存在方向性/重复伪影
- 是否出现明显 discontinuity
- seed 是否工作
- FBM 是否符合预期

---

# 15. 文档要求

为 Noise Library 编写简短文档。

至少说明：

## What is Noise?

说明：

> Noise 是一种根据空间坐标产生确定性、连续或具有空间结构的伪随机值的数学函数。

## Random vs Noise

解释区别。

## Supported Noise

列出：

```text
Value
Perlin
Cellular
Fractal
```

## Sampling

示例：

```cpp
float height = terrainNoise.Sample(x, z);
```

## Seed

解释 deterministic 行为。

## Output Range

明确每个类型的范围。

## Future Extensions

列出但不要实现：

```text
Simplex
Ridged
Domain Warp
Curl
Batch Sampling
GPU
```

---

# 16. API 设计原则

遵循以下原则：

### 16.1 小 API

不要暴露实现细节。

### 16.2 Composition over inheritance

如果 Fractal、Domain Warp 等可以通过组合实现，不要为每种组合创建巨大继承体系。

### 16.3 Stable abstraction

上层应该依赖：

```text
Sample(position)
```

而不是：

```text
GetPermutationTable()
GetGradient()
...
```

### 16.4 No gameplay knowledge

Noise 不知道：

```text
Terrain
Chunk
Biome
Voxel
Actor
Component
Asset
```

### 16.5 No editor dependency

Core Noise 不依赖 Editor / ImGui。

### 16.6 No unnecessary serialization

第一版不需要为了“未来可能做 Noise Asset”而把所有 Noise 参数接入反射/序列化。

只有在当前项目架构确实要求时才加入。

---

# 17. 关于第三方库

可以调研成熟实现作为参考，例如：

- FastNoiseLite
- libnoise
- stb_perlin 等

但不要因为存在成熟库就直接把第三方 API 暴露给 minEngine。

首先判断：

1. License 是否适合 minEngine
2. 代码质量
3. 功能范围
4. API 是否符合 minEngine
5. 是否值得维护
6. 是否需要自己实现核心算法以便学习和控制

如果引入第三方库：

- 保留清晰的第三方边界
- 不污染 Engine Core API
- 明确 license
- 不复制不必要代码

如果算法实现本身简单，优先考虑自己实现一个小而可靠的版本。

---

# 18. 与现有 minEngine 架构的适配要求

特别检查：

- namespace
- `ENGINE_API`
- DLL boundary
- include path
- CMake target
- Core/Runtime dependency direction
- Math 类型
- naming convention
- const correctness
- warning level
- compiler compatibility
- Windows/MinGW/MSVC 等当前支持情况

不要因为 Noise Library 而引入新的第三方依赖，除非有明确收益。

---

# 19. 最终验收标准

实现完成后必须回答：

### 架构

- Noise 是否真正属于 Core Math？
- 是否和 Terrain/Voxel 解耦？
- 是否容易加入新 Noise？
- 是否容易加入 Domain Warp？
- 是否容易加入 batch sampling？

### 正确性

- deterministic 是否成立？
- seed 是否有效？
- range 是否明确？
- negative coordinates 是否正常？
- large coordinates 是否合理？
- continuity 是否正常？

### 性能

- Sample 是否无动态分配？
- permutation/lookup 是否重复构建？
- 是否存在明显不必要的虚函数/间接调用？
- 是否有未来 batch sampling 的扩展空间？

### 工程

- CMake 是否正确？
- DLL API 是否正确？
- tests 是否加入构建？
- 文档是否完整？
- 是否新增了不必要的依赖？

### 可扩展性

确认未来可以自然增加：

```text
Simplex
OpenSimplex
Ridged
Domain Warp
Curl Noise
Batch Sampling
GPU implementation
Noise Texture
```

而不需要推翻现有 API。

---

# 20. 重要：不要过度设计

这是本任务最重要的约束之一。

不要因为：

> “未来可能支持 GPU / Material Graph / Editor Asset / Procedural Graph”

就提前实现这些东西。

第一阶段只需要：

```text
small
correct
deterministic
testable
composable
extensible
```

一个好的结果应该让人感觉：

> “这是一个很小的数学库，但它的抽象边界非常干净。”

而不是：

> “这是一个半成品的 Procedural Generation Framework。”

---

# 21. 实施流程

按照以下工作流执行：

```text
1. 阅读仓库
      ↓
2. 找到现有 Math / Random / Test 架构
      ↓
3. 提出 Noise API 设计
      ↓
4. 检查设计与现有架构是否冲突
      ↓
5. 实现最小版本
      ↓
6. 编写自动化测试
      ↓
7. 做 2D 可视化验证
      ↓
8. Review API
      ↓
9. Review 模块边界
      ↓
10. Review 可扩展性
      ↓
11. Build + Test
      ↓
12. 总结实现与未来扩展点
```

---

# 22. Agent 行为要求

在开始实现前，不要直接修改大量代码。

先输出：

1. 当前仓库中相关模块调查结果
2. 建议的目录结构
3. Noise abstraction 设计
4. 第一版 API
5. 每种 Noise 的实现计划
6. 测试计划
7. 与现有架构的潜在冲突

如果发现现有架构存在多个合理选择：

- 明确列出选项
- 分析 trade-off
- 选择一个与当前项目最一致的方案
- 不要为了抽象而抽象

实现过程中，如果发现原设计存在问题，应主动修正，并说明原因。

完成后进行一次独立的 Architecture + API + Testing Review。

重点寻找：

- API 是否过度设计
- abstraction 是否泄漏实现细节
- 是否把 Terrain 需求污染到了 Core
- 是否有隐藏的随机状态
- 是否有线程安全问题
- 是否有未定义的 range
- 是否有 seed 行为不明确
- 是否有负坐标 bug
- 是否有性能陷阱
- 是否有未来扩展需要破坏 API 的风险

最终输出：

```text
Implemented
Tests
Architecture
Known Limitations
Future Extensions
```

不要仅报告“代码已经写完”，要说明为什么当前设计适合作为长期复用的 Engine Core Math Library。
