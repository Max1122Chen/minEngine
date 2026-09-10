# ME_GENERATED_BODY() no-arg cleanup — Design Spec

## Meta
- **ID:** `CORE-F12`
- **Type:** Feature（cleanup）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Related:** [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · handbook [reflection.md](../../../handbook/runtime/core/reflection.md)

## TL;DR

`ME_GENERATED_BODY(TypeName)` 的 `TypeName` 从未参与宏展开；类名由 `ME_CLASS`/`ME_STRUCT` + 声明解析。改为无参 `ME_GENERATED_BODY()`，并收紧 header tool 的 marker 归属，避免缩短宏调用后误反射邻近无标记类型。

## Scope
- **In:** `ReflectionAnnotations.h`；全部 Runtime 标注头；`minEngine_header_tool.py`（attached marker）；README / handbook 示例；`TOOL_CACHE_VERSION` 16。
- **Out:** 改变 `StaticClass` 语义；要求 header tool 解析 `ME_GENERATED_BODY`；历史 design 文档中的旧示例（Tier B，不强制改）。

## 方案

### 宏
```cpp
#define ME_GENERATED_BODY() \
    template<typename T> friend struct ::minEngine::Reflection::FieldAccessor; \
public: \
    static const minEngine::Reflection::MEClass* StaticClass();
```

### Codegen
- Header tool **从不**读写 `ME_GENERATED_BODY`。
- `has_class_marker`：仅当 `ME_CLASS`/`ME_STRUCT(...)` 与 `class`/`struct` 关键字之间只有空白/注释时，才视为附着于该类型（修复 256 字符回看误匹配）。

## 验收
- [x] 无参宏 + 全量 call site
- [x] `MaterialGraphNodeDef` 等无 `ME_CLASS` 的邻近类型不被生成
- [x] `minEngine` / `minEngineTests` 编译；`reflection-function` PASS

## 风险与非目标
- 缩短 `GENERATED_BODY` 行会改变「到上一 marker 的距离」——依赖 attached-marker 修复，而非依赖窗口碰巧够大。
