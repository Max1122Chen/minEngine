# CORE-F08 — Parameter Schema / Layout / Store — Design Spec

## Meta
- **ID:** `CORE-F08`
- **Type:** Feature（Foundation）
- **Status:** Done（S00–S01；S02 Deferred）
- **Owner:** project maintainer
- **Last updated:** 2026-09-06（S00–S01 落地；`test parameter-store` PASS）
- **Branch:** `feat/animation`（实现可随 ANIM 轨落地；**代码**在 Function/Framework，**不**依赖 Animation）
- **Related:**
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - Consumer（规划）：[ANIM-F03 Animation Graph](../../Animation/ANIM-F03_ANIMATION_GRAPH_DESIGN.md)
  - 未来 Consumer（未登记）：AI Blackboard / Behavior Tree
  - UE 对照（只读）：`UBlackboardData` / `UBlackboardComponent`（`ValueMemory` + `ValueOffsets`）；AnimBP ≠ BB
- **Depends on:** 无强依赖（仅 STL / 现有 Core 基础类型；**不**依赖 GameObject/Scene/Animation）
- **Code location:** **已锁定** `Runtime/Function/Framework/Parameters/`（见 Locked #7）
- **Naming:** **已锁定**（见 §0）
- **Implementation Plan:** [Impl](./CORE-F08_PARAMETER_STORAGE_IMPLEMENTATION.md)

## TL;DR
提供 **Schema → Compile(`ParameterLayout`) → 实例紧凑内存** 的可复用参数袋基础设施。  
Anim Graph 与未来 AI Blackboard **共用这一层原语**；**不**实现完整 Blackboard，也 **不**扮演 UE AnimBP「类资产上的变量」。

## Scope
- **In:**
  - `ParameterValueType`：MVP `Bool` / `Int32` / `Float`（可扩展枚举槽，本 Feature 不实现 Object/Vector）
  - `ParameterSchemaEntry`：`Name` + `Type` + `DefaultBytes`（见 Locked #9）
  - `ParameterSchema`：有序 Entry 列表；校验唯一名
  - `ParameterLayout`：由 Schema **Compile** 得到（KeyId、offset、size、align、totalBytes；default blob）
  - `ParameterStore`：持有 `Layout*` + `vector<uint8>`（或等价缓冲）；按 **KeyId** 热路径读写；按 name 冷路径查找
  - `ResetToDefaults` / 从另一 Store 拷贝（同 Layout）
  - 单测：Compile 打包、读写、未知名失败、同 Layout 拷贝
- **Out:**
  - AI Blackboard 产品（Observer、Parent 继承、InstanceSynced、Object/Class 键）
  - Animation Trigger 语义（raise/consume）——属 **ANIM-F03** 策略层（可用 Bool 槽 + 领域 API）
  - 独立「Parameter 资产」编辑器（Schema 可作为 Graph/BB 资产内嵌字段即可）
  - Lua ScriptBinding 必做
  - 反射生成整袋自动 Inspector（可后置；本 Feature 先保证 C++ API）

## Locked decisions

| # | 决策 | 说明 |
|---|------|------|
| 1 | 分层 | **Def / Schema → Layout（不可变）→ Store（实例）**；禁止热路径 `map<string, variant>` |
| 2 | 封闭 Schema | MVP **不允许**运行时动态加键；动态键留给未来 Blackboard 产品策略 |
| 3 | 访问 | 热路径 **KeyId（uint16）**；name 仅 Compile/调试/编辑器 |
| 4 | 打包 | Compile 时按 size（及 align）排列以减 padding（对齐 UE BB 思路，实现可简化） |
| 5 | 默认值 | Entry=`Type`+`DefaultBytes`；Layout 汇总 default blob；`ResetToDefaults()` |
| 6 | 与 UE 对照 | 结构近 **Blackboard**；**不是** AnimBP 类成员变量模型 |
| 7 | 目录 | **`Runtime/Function/Framework/Parameters/`**（与 `Scene/` / `GameObject/` / `Transform/` 同级）；**禁止**放在 `Animation/` 或 `Runtime/Core/` |
| 8 | 命名 | **已锁定** §0 |
| 9 | Entry.Default | **不**每类型一槽；`DefaultBytes`+Type 解释；**不做**通用 union 序列化 |
| 10 | Bool 尺寸 | **1 byte**（0/1）；见 Impl S00 |

## Reader quick start
1. §0 命名（已锁）+ DefaultBytes + **§0.2 目录** + 与 UE/Anim 边界
2. Locked + §3 类型与 Compile 规则
3. §4 谁消费、谁不消费

---

## 0) 命名（已锁定）

| 角色 | 名称 |
|------|------|
| 值类型枚举 | `ParameterValueType` |
| 单条声明 | `ParameterSchemaEntry` |
| 声明集合 | `ParameterSchema` |
| 编译结果 | `ParameterLayout` |
| 键索引 | `ParameterKeyId` (`uint16`) |
| 实例袋 | `ParameterStore` |

**废弃旧稿名：** `ParameterDef`、`ParameterStorageLayout`。  
**不要用：** `Blackboard`、`AnimParam`。

### 0.1 Entry 默认值怎么存？（无 union 序列化时）

不想为 Bool/Int/Float 各留一个字段槽；又没有引擎级 union/`variant` 序列化。选项：

| 方案 | 做法 | 评价 |
|------|------|------|
| A. 多字段槽 | `DefaultBool`+`DefaultInt32`+`DefaultFloat` | 简单但浪费；**不采用** |
| B. 通用 variant 序列化 | 先做引擎 tagged-union 反射 | 正确但范围大；**Out of F08** |
| C. **Type + DefaultBytes** | 只存类型与字节；helper 按 Type 解释 | **采用**：与 Layout/Store 同构；`vector<uint8>` 现有序列化可吃 |
| D. Entry 手写 JSON | C++ 可用 union，序列化旁路反射 | 可用但双轨；资产若走 `ME_STRUCT` 不如 C |
| E. Default 不进 Entry | 默认只在 Layout | authoring 拆碎；MVP 不推荐 |

**锁定：C。**

```cpp
struct ParameterSchemaEntry
{
    std::string Name;
    ParameterValueType Type = ParameterValueType::Float;
    std::vector<uint8_t> DefaultBytes; // size must match SizeOf(Type); empty => zero default
};
```

Validate / Compile：检查 `DefaultBytes.size()`；Layout 生成整块 default blob 供 `ParameterStore::ResetToDefaults()`。  
日后若有 tagged-value 序列化，可把磁盘格式写漂亮些，**不必改** Layout/Store 契约。

### 0.2 代码目录（已锁定）

| 项 | 值 |
|----|-----|
| 根路径 | `minEngine/minEngine/src/Runtime/Function/Framework/Parameters/` |
| 同级邻居 | `Framework/Scene` · `GameObject` · `Components` · `Project` · `Transform` |
| Feature ID | 仍为 **`CORE-F08`**（Registry 基础能力编号）；文档可留在 `docs/ai/Platform/Core/` |

**为何 Framework，不放 Core：**  
这是玩法/运行时 **参数袋**（Anim Graph、未来 AI 共用），与 Scene/GO 同属 Function 层消费面；不是 GUID/Math/Reflection 那种无领域原语。放 Core 会暗示「全引擎底层依赖」，实际首批消费者都在 Function。

**模块边界：**
- Parameters **可**被 `Function/Animation`、未来 AI、Editor 工具 include
- Parameters **禁止** `#include` Animation / Physics / Render
- Parameters **禁止**依赖 `GameObject` / `Scene`（保持可单测、可复用；Store 不是组件）

## 1) 背景与目标

### 1.1 为何独立 Feature
ANIM-F03 与未来 AI 都需要「按名声明的类型化值 + 实例存储」。若各自实现，必然双份 `variant` 袋。  
抽成 **CORE-F08** 后：Anim 只做状态机；AI 日后加厚为 Blackboard。

### 1.2 与 UE 的对应（确认你的理解）

| UE | 本质 | 与本 Feature |
|----|------|----------------|
| **AnimBP** | 基于 BP 生态的 **类资产**；「参数」多为 AnimInstance **成员变量** | **不**模仿；minEngine 无 BP 类生成 |
| **Blackboard** | **数据资产** `UBlackboardData` + 组件上 `ValueMemory`/`ValueOffsets` | **结构上**最接近本 Feature 的 Layout/Store |

因此：共享的是 **「特殊数据资产式 Schema + 紧凑实例内存」** 这一基础设施，不是「把 Anim 做成 UE AnimBP」。

### 1.3 成功标准
1. Schema → Layout → Store 可单测闭环  
2. 同 Layout 多实例互不干扰  
3. ANIM-F03 Design 可改为 **依赖 CORE-F08**，不再自拥 NamedValue 实现  
4. 文档明确：本层 **≠** Blackboard 产品

---

## 2) 架构位置

```text
Gameplay / Anim Graph / (future BT)
        │
        ▼
 ParameterStore  (instance bytes)
        │ uses
        ▼
 ParameterLayout  (compiled, shareable)
        ▲ compile
        │
 ParameterSchema  (entries: name, type, default bytes)
```

```text
Animation/
  AnimationGraph  → embeds/owns ParameterSchema (or refs)
  GraphInstance   → ParameterStore

AI/ (future)
  BlackboardData  → ParameterSchema + 扩展键类型/策略
  BlackboardComponent → ParameterStore + Observers…
```

---

## 3) 方案

### 3.1 类型

```cpp
enum class ParameterValueType : uint8_t
{
    Bool = 0,
    Int32 = 1,
    Float = 2,
    // Reserved: Vector3, ObjectPtr, … — not in F08 MVP
};

struct ParameterSchemaEntry
{
    std::string Name;
    ParameterValueType Type = ParameterValueType::Float;
    std::vector<uint8_t> DefaultBytes; // interpreted by Type; empty => zero
};

class ParameterSchema
{
    // AddEntry / Find / GetEntries(); Validate unique names + DefaultBytes size
};

using ParameterKeyId = uint16_t;
static constexpr ParameterKeyId kInvalidParameterKeyId = 0xFFFF;

struct ParameterLayoutEntry
{
    ParameterKeyId KeyId = kInvalidParameterKeyId;
    ParameterValueType Type = ParameterValueType::Float;
    uint16_t Offset = 0;
    uint16_t Size = 0;
    // Name via side table or stored for cold FindKeyId
};

class ParameterLayout
{
    // Immutable after Compile; ValueOffsets[KeyId]; TotalBytes; DefaultBlob
    static bool Compile(const ParameterSchema& schema, ParameterLayout& out, std::string* outError);
};

class ParameterStore
{
    void BindLayout(std::shared_ptr<const ParameterLayout> layout);
    void ResetToDefaults();

    bool SetBool(ParameterKeyId id, bool v);
    bool SetInt32(ParameterKeyId id, int32_t v);
    bool SetFloat(ParameterKeyId id, float v);
    bool TryGetBool/Int32/Float(ParameterKeyId id, …) const;

    ParameterKeyId FindKeyId(std::string_view name) const; // cold
};
```

### 3.2 Compile 规则（MVP）

1. Schema 空 → Layout `TotalBytes==0`，合法。  
2. 名空或重复 → Compile 失败。  
3. 为每个 Def 分配递增 `KeyId`（0..n-1）或按打包重排后仍稳定映射（**推荐：KeyId = 声明顺序**；内存 offset 可按 size 排序，**ValueOffsets[KeyId]** 指向打包后位置——同 UE）。  
4. `TotalBytes` = 打包后末尾；**Bool = 1 byte**；Int32/Float = 4；按类型 align 累加 offset（与 Impl S00 一致）。  
5. Defaults：Compile 生成 Layout **DefaultBlob**；`Store::ResetToDefaults()` 拷贝该 blob。

### 3.3 并发 / 线程
MVP：**单线程**假定（与当前 Engine Tick 一致）。不设原子；不设锁。

### 3.4 序列化
- Schema / Entry：**可** `ME_STRUCT` 序列化（S02）；供 Graph 资产内嵌。  
- Layout：可 **不落盘**（Load Schema 后 Compile）；若要缓存再议。  
- Store：一般不单独存盘（运行时）；Play Mode 复制策略后议。

### 3.5 Trigger（明确边界）
CORE-F08 **不**定义 Trigger 类型。  
ANIM-F03 可：`SetBool("Attack", true)` + 过渡匹配后 `SetBool(..., false)`，或在 Anim 层包 `RaiseTrigger`/`ConsumeTrigger`。  
未来若多系统需要脉冲语义，再考虑升格为可选 `ParameterValueType::Trigger`（另切片）。

---

## 4) 非目标与反模式

| 反模式 | 为什么拒绝 |
|--------|------------|
| 类名 `Blackboard` | 暗示 BT Observer/Object/Synced |
| Store 热路径 string 查找 | 违背 Layout 初衷 |
| 运行时 AddKey | 破坏 Layout 共享与紧凑性 |
| 放进 `Animation/` | AI 无法干净依赖 |
| 本 Feature 实现 Anim SM | 范围膨胀；属 ANIM-F03 |

---

## 5) 测试

| 用例 | 期望 |
|------|------|
| 三键 Bool/Int/Float Compile | KeyId 稳定；TotalBytes > 0 |
| Set/Get 往返 | 值正确 |
| 未知名 / 错误类型 | false，不写内存 |
| ResetToDefaults | 回到 Entry DefaultBytes |
| 两 Store 同 Layout | 互不覆盖 |
| 打包：大值优先或声明序+offset 表 | 与 Compile 规则一致且可断言 |

Suite 建议名：`parameter-store`（最终以 Test 登记为准）。

---

## 6) 验收标准

- [x] ANIM-F03 Design 改为依赖本 Feature；不再自描述 NamedValue 实现
- [x] Registry / ACTIVE_WORK / Progress 对齐
- [x] Schema → Layout → Store API 落地于 `Framework/Parameters/`，且无 Animation include
- [x] 单测 PASS（`minEngineTests.exe test parameter-store`）
- [x] Out 清单未实现（Observer / Object / Trigger 类型 / 动态键）

---

## 7) 建议切片预览

见 [Implementation Plan](./CORE-F08_PARAMETER_STORAGE_IMPLEMENTATION.md)。摘要：

| Slice | 内容 |
|-------|------|
| S00 | Schema + Layout Compile + Store + 单测（Bool=1B） |
| S01 | DefaultBlob / Validate 硬化 |
| S02 | （可选）ME_STRUCT 序列化 |

```text
S00 → S01 → (S02?) → ANIM-F03
```

---

## 8) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done**（S00–S01）；S02 ME_STRUCT 序列化 **Deferred** |
| What's not | S02 Schema 反射序列化 |
| Unblock | ANIM-F03 可开 Impl / 编码 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | Draft：从 ANIM-F03 抽出；Def→Layout→Store；对齐 UE BB 存储思想，区分 AnimBP 类资产模型 |
| 2026-09-05 | 命名锁定：Schema/SchemaEntry/Layout/Store；Default=`DefaultBytes`+Type |
| 2026-09-05 | 补 Impl Plan；Bool=1B；链接 Impl；修正废弃名笔误 |
| 2026-09-05 | 目录锁定：`Runtime/Function/Framework/Parameters/`（不再放 Core） |
| 2026-09-06 | S00–S01 实现 + `parameter-store` PASS；Feature **Done**（S02 Deferred） |
