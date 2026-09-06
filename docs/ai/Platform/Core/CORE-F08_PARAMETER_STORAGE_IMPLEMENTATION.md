# CORE-F08 — Parameter Schema / Layout / Store — Implementation Plan

## Meta
- **ID:** CORE-F08
- **Type:** Implementation Plan
- **Status:** Done（S00–S02）
- **Owner:** project maintainer
- **Last updated:** 2026-09-06（S02 Schema ME_STRUCT JSON 往返 Done）
- **Related:** [Design Spec](./CORE-F08_PARAMETER_STORAGE_DESIGN.md) · [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Branch:** feat/animation
- **Unblocks:** ANIM-F03

## TL;DR
落地 `ParameterSchema` / `ParameterSchemaEntry` / `ParameterLayout` / `ParameterStore`。  
S00 核心闭环 + 单测；S01 Defaults/校验硬化；S02 可选 `ME_STRUCT` 序列化 Schema。

## Scope
- **In:** Design In 项；测试 suite；目录 **`Runtime/Function/Framework/Parameters/`**
- **Out:** Blackboard 产品；Trigger 类型；Anim Graph；动态加键；Lua binding

## Reader quick start
1. [Design](./CORE-F08_PARAMETER_STORAGE_DESIGN.md) — Locked + DefaultBytes + §0.2 目录
2. 下表切片
3. Verify：`minEngineTests.exe test parameter-store`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| CORE-F08-S00 | 类型 + Schema + Compile Layout + Store 读写 + 单测 | **Done** | `test parameter-store` |
| CORE-F08-S01 | DefaultBlob / ResetToDefaults / Validate 硬化 | **Done** | 同上 |
| CORE-F08-S02 | `ME_STRUCT` 序列化 Schema 往返 | **Done** | JSON round-trip case |

## 2) 切片详情

### CORE-F08-S00 — Core API + tests
- **Goal:** Schema→Layout→Store 可 Compile、按 KeyId 读写；无 Animation include
- **Touch:**
  - `minEngine/minEngine/src/Runtime/Function/Framework/Parameters/ParameterValueType.h`
  - `ParameterSchema.h/.cpp`（含 `ParameterSchemaEntry`）
  - `ParameterLayout.h/.cpp`
  - `ParameterStore.h/.cpp`
  - `Tests/Suites/ParameterStoreTest.cpp`（或同等登记）
  - CMake / Test suite 注册（按仓库既有 TEST 惯例）
- **Locked in this slice:**
  - 目录 = `Function/Framework/Parameters/`（Design Locked #7）
  - `Bool` 存储 **1 byte**（0/1）；`Int32`/`Float` **4 bytes**；Layout 按 entry align 累加 offset（Float/Int32 align 4；Bool align 1）
  - `KeyId` = Schema 声明顺序 `0..n-1`；内存可按 size 排序打包，但 `ValueOffsets[KeyId]` 寻址
  - `DefaultBytes` 空 → Compile 时按 Type 填零
  - 错误类型 / 越界 KeyId / 名未找到 → API 返回 `false`，不写内存
- **DoD:**
  - [x] 公开 API 与 Design §3.1 一致（命名锁定）
  - [x] `ParameterLayout::Compile` + `ParameterStore::{Set,TryGet}*`
  - [x] 单测：三键往返、错误路径、两 Store 隔离、空 Schema
  - [x] 无 `#include` Animation；无对 GameObject/Scene 的依赖
- **Verify:** build `minEngineTests`；`minEngineTests.exe test parameter-store` PASS

### CORE-F08-S01 — Defaults + validate
- **Goal:** Layout 持有完整 DefaultBlob；`ResetToDefaults`；Validate 严格检查 DefaultBytes 尺寸
- **Touch:** Layout/Schema/Store；补测
- **DoD:**
  - [x] 非空 DefaultBytes 尺寸 ≠ SizeOf(Type) → Validate/Compile 失败
  - [x] Reset 后值等于 Entry 默认
  - [x] `CopyFrom` 同 Layout
- **Verify:** 单测扩展 PASS

### CORE-F08-S02 — Schema 序列化
- **Goal:** `ParameterSchema` / `ParameterSchemaEntry` / `ParameterValueType` 可 `ME_STRUCT`/`ME_ENUM` JSON 往返，供 Graph 资产内嵌
- **Status:** **Done**
- **DoD:**
  - [x] Entry：`Name`/`Type`/`DefaultBytes` 反射序列化
  - [x] Schema：`m_Entries` 反射序列化
  - [x] Round-trip 测（`parameter-store: schema JSON round-trip`）+ Compile/Store 仍可用
- **Verify:** `minEngineTests.exe test parameter-store` PASS

## 3) 依赖顺序

```text
S00 → S01 → S02
         ↘ ANIM-F03 可在 S00/S01 Done 后开；资产内嵌 Schema 需 S02（现已满足）
```

## 4) 延后 / 取消

| Slice | Reason | Unblock |
|-------|--------|---------|
| Object/Vector 类型 | Design Out | AI/需求驱动 |
| Trigger 枚举 | Anim 策略层 | ANIM-F03 |

## 5) 工程备注
- 代码根：`Runtime/Function/Framework/Parameters/`（非 Core）
- cpp-style：优先类成员；避免无必要 anonymous namespace 自由函数（enum helper / SizeOf(Type) 可放 `ParameterValueTypeUtil` 或 Layout 静态成员）
- 不引入 `std::variant` 作落盘格式
- Endian：MVP 本机内存布局即可（与当前引擎一致）；跨端另议

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | 建 Impl；对齐命名与 DefaultBytes；Bool=1B |
| 2026-09-05 | 目录改为 `Function/Framework/Parameters/` |
| 2026-09-06 | S00–S01 Done；`test parameter-store` 6 cases PASS |
| 2026-09-06 | **S02 Done**：ME_ENUM/ME_STRUCT + JSON round-trip；suite 7 cases PASS |
