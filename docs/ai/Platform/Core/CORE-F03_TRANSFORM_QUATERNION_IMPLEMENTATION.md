# Transform Quaternion Storage — Implementation Plan

## Meta
- **ID:** `CORE-F03`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-02
- **Related:** [Design](./CORE-F03_TRANSFORM_QUATERNION_DESIGN.md), [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md)
- **Note:** 原 physics 分支 `CORE-F01`；合入 master 改号为 `CORE-F03`。

## TL;DR

在 `physics` 分支分 **6 个切片**落地；每切片可独立编译验证，尽量 **一个 slice 一个 PR 至 `physics`**，全部完成后 **一个 PR `physics` → `master`**（或按你习惯 2–3 个递进 PR）。**禁止**在 S04 之前改 Inspector；**禁止**在 S01 之前改序列化。

## 依赖关系

```text
S01 → S02 → S03
          ↘
S01 → S04（依赖 S02 API 稳定）
S02 → S05（调用点）
S03 + S04 + S05 → S06（收尾合入）
```

`PHYS-F01-S01` **Blocked until** `CORE-F01-S06` Done。

---

## 切片总览

| Slice | 标题 | 主要交付 | 验证 |
|-------|------|----------|------|
| S01 | 类型与 Transform 内核 | `Quaternion`、`Transform` 存储与 `ToMatrix` | 编译 + 可选矩阵等价测试 | **Done** |
| S02 | Scene 图 API + RenderCamera | `SceneComponent`、`GameObject`、`CameraComponent`、`RenderCamera` | 编译 | **Done** |
| S03 | 序列化 | 新 struct 四字段写入/读回（**无**旧 Euler 自动兼容） | `SerializationArchiveTest` | **Done** |
| S04 | Editor Inspector | `TransformWidget` 欧拉行 + Inspector 跳过逻辑 | 手动：拖 Rotation、Undo | **Done**（Inspector 走 `TransformWidget`；待手动验 Gizmo/Undo） |
| S05 | 调用点与样本 | Playground、PreviewScene | Editor / Playground 目视 |
| S06 | 回归与合入 | Registry、ACTIVE_WORK、Progress | `verify.ps1` smoke |

---

## CORE-F01-S01 — 类型与 Transform 内核

### 目标
引入 `Quaternion`；`Transform` 以四元数存储；`ToMatrix()` / `Rotate()` 正确；**暂不**改 Editor、序列化、SceneComponent 对外 API（可用适配层编译）。

### 任务
- [ ] 新增 `Runtime/Core/Math/Quaternion.h/.cpp`（`ME_STRUCT`；欧拉/矩阵运算委托 **GLM**，见 Design §3.1）
- [ ] 修改 `Transform.h`：`Quaternion Rotation`；更新 `ToMatrix`、`Rotate`、`operator==`
- [ ] 保留 `Transform(pos, eulerDeg, scale)` 便捷构造
- [ ] 运行 reflection codegen
- [ ] （若 D6=是）新增 `TransformQuaternionTest`：典型欧拉角下新 `ToMatrix()` 与旧实现数值接近

### 触及文件
- `Quaternion.h`（+ `.cpp` 若实现放 cpp）
- `Transform.h`
- `Generated/Reflection/Transform.gen.*`
- `Tests/`（可选）

### 刻意不碰
- `SceneComponent`、`TransformWidget`、Serializer

### 验收
```text
cmake --build minEngine/build --target minEngine
cmake --build minEngine/build --target minEngineTests
minEngine\bin\minEngineTests.exe test smoke   # 或仅新 suite
```

### 风险
S01 单独改 `Transform` 会导致 `SceneComponent` 等编译失败 — **本切片应同时做最小 API 断裂修复**（仅把 `m_Transform.Rotation` 类型变化引起的编译错误在 `Transform.h` 内用便捷方法缓解，或 **将 S01+S02 合并为第一个 PR**）。

> **实施建议：** 若 S01 无法独立编译，**将 S01 与 S02 合并为首个 landable PR**；文档仍保留逻辑分界便于 review。

---

## CORE-F01-S02 — Scene 图 API + RenderCamera

### 目标
`SceneComponent` / `GameObject` 暴露 quat 主 API + Euler 便捷 API；方向向量从 quat 计算；`RenderCamera` 旋转存储与 `Transform` 对齐（拍板 D5）。

### 任务
- [ ] `SceneComponent`：`GetRotation()` → `const Quaternion&`；`SetRotation(Quaternion)`；`Get/SetRotationEulerDegrees`
- [ ] 更新 `SetRotation` / `Rotate` 实现（无 Euler 回写）
- [ ] `GetForwardVector` / `GetRightVector` / `GetUpVector` 从 `Rotation.ToGlm()` 计算
- [ ] `GameObject` 透传对齐
- [ ] `RenderCamera.h/.cpp`：`m_Rotation` → `Quaternion`；`UpdateViewMatrix` 用 quat；保留 Euler 便捷 API（可选）
- [ ] `CameraComponent.cpp`：`GetRotation()` quat → `RenderCamera::SetRotation`

### 触及文件
- `SceneComponent.h/.cpp`
- `GameObject.h/.cpp`
- `CameraComponent.cpp`
- `RenderCamera.h/.cpp`

### 验收
- 全量编译通过（Editor + Tests）
- 无 Inspector 改动时，旧 `TransformWidget` 需临时改绑 Euler API 或本切片与 S04 合并

> **合并策略：** **S02 + S04 同一 PR** 更贴近「可运行 Editor」原则；若坚持严格切片，S02 末尾用 `#if 0` 或最小 `TransformWidget` 改动仅绑 `GetRotationEulerDegrees`（不推荐）。

---

## CORE-F01-S03 — 序列化

### 目标
磁盘新格式（struct 四字段，D3）；**不**实现旧 `[rx,ry,rz]` 自动读档（D4）。合入前维护者手改仓库内旧资产。

### 任务
- [ ] 确认 `Quaternion` 作为 `ME_STRUCT` 嵌套序列化路径可用
- [ ] 扩展 `SerializationArchiveTest`：
  - [ ] `Quaternion` property round-trip
  - [ ] `Transform` 含 quat 的 round-trip（新格式）
- [ ] （维护者）批量更新仍含旧 Euler `Rotation` 的 scene/json 文件

### 触及文件
- `Serializer` / `PrimitiveCodecRegistry`（按需）
- `Tests/Suites/SerializationArchiveTest.cpp`

### 验收
```text
minEngine\bin\minEngineTests.exe test serialization-archive
# 或 smoke 全 suite
```

### 依赖
S01（`Quaternion` 类型存在）

---

## CORE-F01-S04 — Editor Inspector（欧拉 Widget）

### 目标
实现 Design §3.2：`Rotation` 在 UI 上显示为 vec3 度，写入映射为 quat。

### 任务
- [ ] `TransformWidget.cpp`：
  - [ ] Rotation 行：`ToEulerDegrees()` → `DragFloat3` → `SetRotationEulerDegrees` / 赋值 `Quaternion::FromEulerDegrees`
  - [ ] Undo 回调 `fieldName` 仍为 `"Rotation"`
- [ ] `SceneEditorInspectorSource.cpp`：
  - [ ] 遍历 `Transform` 子属性时 **跳过** `Rotation` 的默认反射绘制（避免 W/X/Y/Z 四行）
  - [ ] Root Transform 与嵌套 `m_Transform` 两条路径均走 `TransformWidget`
- [ ] 手动测试：Inspector 拖 Rotation、Gizmo 旋转、Undo/Redo

### 触及文件
- `TransformWidget.cpp`
- `SceneEditorInspectorSource.cpp`

### 验收
- Editor 中选中带 mesh 的 GO：三轴 Position / Rotation / Scale 视觉一致
- Gizmo 旋转后 Inspector Euler 数值合理更新
- Undo `m_Transform.Rotation` 生效

### 依赖
S02（Euler 便捷 API）

---

## CORE-F01-S05 — 调用点清扫

### 目标
Playground / Preview 等显式 `Vector3` rotation 调用迁移；无遗漏编译警告。

### 任务
- [ ] `Playground.cpp`：优先保留 `Transform(pos, euler, scale)` 构造；`SetRotation` → `SetRotationEulerDegrees` 或删除冗余调用
- [ ] `PreviewScene.cpp`：同上
- [ ] 全仓库 grep `GetRotation()` / `SetRotation(` / `.Rotation` 扫尾

### 验收
- Playground（若 BUILD_PLAYGROUND=ON）或 Editor 加载场景正常

---

## CORE-F01-S06 — 回归、文档与合入

### 目标
Feature Done；解锁 `PHYS-F01`。

### 任务
- [ ] `.\scripts\verify.ps1`
- [ ] 更新 [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md)：`CORE-F01` → Done
- [ ] 更新 [ACTIVE_WORK.md](../../ACTIVE_WORK.md)：CORE 完成，`PHYS-F01` 可 In Progress
- [ ] [PROGRESS_LOG.md](../../PROGRESS_LOG.md) 追加条目
- [ ] Design Meta Status → Done；勾选验收标准
- [ ] PR `physics` → `master`
- [ ] （可选）通知 render worktree 对齐 `master`（D7：维护者自行安排）

### 验收
- Design §8 全部勾选
- 待拍板项 D1–D7 已填「你的决定」

---

## 推荐 PR 打包（务实）

文档逻辑 6 片；**landable PR** 建议合并为 **3 个**：

| PR | 包含切片 | 说明 |
|----|----------|------|
| PR1 | S01 + S02 | 内核 + Scene API，编译通过 |
| PR2 | S03 + S04 | 序列化 + Inspector，可端到端编辑存盘 |
| PR3 | S05 + S06 | 清扫 + 合入 master |

若坚持 **6 PR**，仅 S03、S06 适合独立；S01/S02、S04/S05 不宜单独 land。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-11 | 初稿：6 切片 + 3 PR 打包建议 |
| 2026-06-11 | 对齐 Design §6 拍板：S02 含 RenderCamera；S03 无旧格式读档 |
