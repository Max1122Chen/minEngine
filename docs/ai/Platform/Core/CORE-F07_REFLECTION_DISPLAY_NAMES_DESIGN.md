# CORE-F07 — Reflection Display Names

## Meta
- **ID:** `CORE-F07`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-02（+ camelCase 单词空格拆分）
- **Branch:** `feat/editor`
- **Depends on:** P4 Reflection（`MEProperty` metadata 已有 `DisplayName` 键）
- **Related:** [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK.md](../../ACTIVE_WORK.md) · [ED-F03 Debug Console Design](../../Editor/ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md)（`inspect` 输出共享展示名规则）

## TL;DR

Inspector / 未来 Console `inspect` 在展示反射成员时，**默认去掉**工程前缀（`m_`、`x_`、`b_`），再对 **PascalCase / camelCase** 做单词拆分并插入空格（`m_LightColor` → `Light Color`）；显式 `meta(DisplayName=…)` **优先**。C++ 标识符、序列化路径、Undo property path **不变**。

## Scope
- **In:**
  - Runtime 层统一 **展示名解析 API**（metadata 优先 → 前缀剥离 fallback）
  - Editor `PropertyEditPolicy::GetDisplayName` 改调该 API（Scene Inspector、Material NodeDef 属性行自动受益）
  - 单元测试：前缀规则、**驼峰分词**、显式 DisplayName 覆盖、边界 case
  - 文档：与 `ED-F03` PropertyPath / `inspect` 的展示约定对齐（路径仍用真实成员名）
- **Out:**
  - 改 C++ 成员变量命名
  - Codegen 批量写入 `DisplayName` metadata（可 follow-up；本期 runtime fallback 即可）
  - 类型名 / 枚举值展示名（`GetShortTypeName` 另议）
  - Console / Agent 全链路（由 `ED-F03` 消费本 API）

## Reader quick start
1. 本文件 — 规则与 API 契约。
2. 代码入口（落地后）：`Runtime/Core/Reflection/ReflectionDisplayNames.*`；消费点 `Editor/.../PropertyEditPolicy.cpp`。
3. 测试：`Tests/Suites/ReflectionDisplayNamesTest.*`（名称以实现为准）。

---

## 0) Pre-flight（2026-09-02）

| 项 | 结论 |
|----|------|
| 现状 | `PropertyEditPolicy::GetDisplayName` 仅读 metadata `DisplayName`，否则 `property.GetName()` → Inspector 显示 `m_*` |
| 样本 | `ReflectionSample` 已有显式 `DisplayName`；多数 Component 字段无 metadata |
| 前缀 | 仓库成员以 `m_` 为主；Registry / 用户约定亦含 `x_`（扩展字段）、`b_`（bool，UE 风格） |
| 风险 | **low** — 纯展示层；Undo/序列化路径继续用 `GetName()` |
| 建议 | **Go** — 小竖切，为 `ED-F03` inspect 可读性铺路 |

---

## 1) 背景与目标

### Pain
- Scene Inspector 属性列显示 `m_LightColor`、`m_CastShadow`，噪声大、不符合产品化观感。
- `ED-F03` Console `inspect` / `get` 若直接 dump 成员名，Agent 与人类都难读。
- 已有 `DisplayName` metadata 机制，但要求每个字段手写 meta，成本高。

### Goals
- **默认可读**：无 metadata 时自动美化。
- **显式优先**：`meta(DisplayName = "…")` 完全覆盖自动规则。
- **单一真源**：所有 UI / Console **展示** 走同一 API；**逻辑标识** 仍用 `MEProperty::GetName()`。

### Success
- Inspector 中 `LightComponent` 等组件常见字段显示为 `Light Color`、`Cast Shadow` 等可读标签。
- `ReflectionDisplayNames` 测试覆盖规则表。
- `verify.ps1` 通过。

---

## 2) 现状

| 区域 | 行为 | 位置 |
|------|------|------|
| 展示名 | metadata `DisplayName` → 否则原始名 | `PropertyEditPolicy::GetDisplayName` |
| 逻辑名 | `MEProperty::GetName()` | Codegen `ME_REFLECTION_CLASS_ADD_FIELD` |
| Undo / 序列化 | `capturePropertyPath` 用 `GetName()` | `SceneEditorInspectorSource` |
| 样本 | `ReflectionSampleClass::IntField` 有 `DisplayName = "Sample Int"` | `ReflectionSample.h` |

**不变量（必须保持）：**

```text
SerializePropertyByPath / Undo / Script binding → 真实成员名（m_Transform 等）
ImGui::PushID / 内部比较 → 真实成员名
仅 TextUnformatted 标签 → 展示名 API
```

---

## 3) 方案

### 3.1 API（Runtime）

新增 `minEngine::Reflection::DisplayNames`（或 `ReflectionDisplayNames`）：

```cpp
namespace minEngine::Reflection
{
    // Returns storage-backed C string valid until next call on this thread.
    const char* GetPropertyDisplayName(const MEProperty& property);

    // Pure helper for tests / non-reflection callers.
    std::string FormatMemberDisplayName(std::string_view memberName);
}
```

**解析顺序：**

1. `property.FindMetadata("DisplayName")` 非空 → 原样返回（**不**再自动分词，作者完全控制文案）。
2. 否则 `FormatMemberDisplayName(property.GetName())`（前缀剥离 → 驼峰分词）。

### 3.2 前缀剥离规则

| 前缀 | 条件 | 示例 in → out（分词前） |
|------|------|-------------------------|
| `m_` | 前缀后首字符为 `A–Z` 或 `a–z` | `m_LightColor` → `LightColor` |
| `x_` | 同上 | `x_CustomData` → `CustomData` |
| `b_` | 同上 | `b_Enabled` → `Enabled` |

**不剥离：**

- 无前缀或前缀后非字母（如 `m_1st` 保持原样，避免误伤）。
- 名称本身为 `m_` / `x_` 仅两字。
- 已有 `DisplayName` metadata。

### 3.3 驼峰 / PascalCase 单词拆分

在前缀剥离之后（或成员名本身无前缀时），将标识符拆为「空格分隔的单词」，提升 Inspector / `inspect` 可读性。

**算法（推荐 heuristic，与 UE 展示名接近）：**

遍历字符，在位置 `i`（`i > 0`）前插入空格，当：

1. 当前为大写，且前一字符为小写（`LightColor` → `Light` + `Color`）。
2. 当前为大写，且下一字符为小写，且前一字符亦为大写（`HTTPResponse` → `HTTP` + `Response`；`InnerConeAngle` 走规则 1 即可）。
3. 当前为数字边界可选：小写/大写字母 → 数字 之间插空格（`Layer2Name` → `Layer 2 Name`，低优，本期可选）。

**不拆分：**

- 连续单字母缩写保持紧凑（`R`/`G`/`B`/`A` 分量名保持 `R` 不拆）。
- 全大写短 token（`UBO`）无小写邻居时不强行拆。

**端到端示例：**

| 成员名 | 展示名 |
|--------|--------|
| `m_LightColor` | `Light Color` |
| `m_InnerConeAngle` | `Inner Cone Angle` |
| `m_CastShadow` | `Cast Shadow` |
| `b_Enabled` | `Enabled` |
| `Intensity` | `Intensity` |
| meta `DisplayName="主光强度"` | `主光强度`（原样） |

**不处理（本期）：**

- `snake_case` → Title Case
- `SCREAMING_SNAKE`
- 类型名 `minEngine::SpotLightComponent`（继续 `GetShortTypeName`）

### 3.4 存储与线程

`GetPropertyDisplayName` 返回 `const char*` 以匹配现有 `PropertyEditPolicy` 签名。实现使用 **`thread_local std::string`** 缓冲格式化结果；调用方须在同一帧内用完（与 ImGui 用法一致）。

显式 metadata 仍返回 `metadata.c_str()`（生命周期由 `MEProperty` 持有）。

### 3.5 Editor 接线

```cpp
const char* PropertyEditPolicy::GetDisplayName(const MEProperty& property)
{
    return Reflection::GetPropertyDisplayName(property);
}
```

已走 `PropertyEditPolicy::GetDisplayName` 的路径自动生效：

- `SceneEditorInspectorSource::DrawProperty`
- `MaterialNodeDefPropertyDrawer`

### 3.6 与 ED-F03 的契约

| 用途 | 使用 |
|------|------|
| `inspect` 输出标签列 | `GetPropertyDisplayName` |
| `get` / `set` / PropertyPath 解析 | `GetName()`（真实成员名） |
| Agent schema `name` 字段 | 建议双字段：`id`（真实名）、`displayName`（可选） |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Runtime fallback 剥离（本期） | 零 codegen 改动、立刻生效 | 极少数字段需手写 DisplayName 覆盖 | **选用** |
| B. Codegen 自动填 `DisplayName` | 展示名可序列化到 metadata | 需改 codegen + 全量 regen；与 A 重复 | Defer |
| C. 仅 Editor 本地 strip | 改动最小 | Console/Agent 重复逻辑 | 否决 |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 误剥离非成员字段名 | 罕见命名显示异常 | 仅剥 `m_`/`x_`/`b_` + 字母规则；测试 + 可 `DisplayName` 覆盖 |
| 驼峰分词过度/不足 | 个别字段可读性下降 | 启发式 + golden tests；显式 `DisplayName` 兜底 |
| 误拆缩写（如 `IO`） | 个别字段显示怪异 | 规则 2 + 测试用例；`DisplayName` 覆盖 |
| `const char*` 生命周期 | Use-after-free | thread_local + 文档约定；metadata 指针仍来自 property |
| Material 图节点 `Name` 等非反射字段 | 不在范围 | NodeDef drawer 已用 `PropertyEditPolicy`；图 pin 名另列 |

---

## 6) 验收标准

- [ ] `FormatMemberDisplayName("m_LightColor") == "Light Color"`
- [ ] `FormatMemberDisplayName("m_InnerConeAngle") == "Inner Cone Angle"`
- [ ] `FormatMemberDisplayName("x_Foo") == "Foo"`；`FormatMemberDisplayName("b_Enabled") == "Enabled"`
- [ ] 有 `DisplayName` metadata 时返回 metadata（不自动分词）
- [ ] Inspector 目视：`LightComponent` 等字段为 `Light Color` 风格标签
- [ ] Undo / Save Scene 仍用真实路径（回归：改 `m_Intensity` → Undo 描述/序列化正常）
- [ ] `verify.ps1` 通过

---

## 7) 建议切片（Implementation）

| Slice | 内容 | 状态 |
|-------|------|------|
| S01 | `ReflectionDisplayNames` + 单元测试 | **Done** |
| S02 | `PropertyEditPolicy` 接线 + Inspector 目视 | **Done**（待用户目视确认） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | Registry 占位 |
| 2026-09-02 | 初版 Design（`feat/editor`） |
| 2026-09-02 | 增加 camelCase 单词空格拆分规则 |
