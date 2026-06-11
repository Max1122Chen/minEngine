# Transform Quaternion Storage — Design Spec

## Meta
- **ID:** `CORE-F01`
- **Type:** Refactor
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-06-11
- **Related:** [Implementation](./CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md), [FEATURE_REGISTRY.md](../../FEATURE_REGISTRY.md), [EDITOR_APPEARANCE.md](../../Editor/EDITOR_APPEARANCE.md), [SERIALIZATION_BINARY_AND_PROPERTY_API.md](../Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md), [PHYS-F01](../../FEATURE_REGISTRY.md)（依赖本 Feature 完成后启动）

## TL;DR

将 `Transform::Rotation` 的**存储真源**从 `Vector3` 欧拉角（度）改为反射可序列化的 `Quaternion`（`ME_STRUCT`，四 float）。运行时矩阵、物理写回、Gizmo 增量旋转均基于四元数；**Inspector 仍显示与 Position/Scale 同款的 vec3 欧拉行**，编辑时映射为四元数写回。在 `physics` 分支开发，完成后 **PR → `master`**，再启动 `PHYS-F01`（Jolt）。

## Scope
- **In:** `Quaternion` 类型；`Transform` 存储与 `ToMatrix()`；`SceneComponent` / `GameObject` API；`RenderCamera` 旋转存储；序列化新格式（struct 四字段）；`TransformWidget` 欧拉展示层；Playground / Preview 调用点；序列化 round-trip 测试；切片化实施与验收。
- **Out:** 编辑器视口 `SceneEditingViewportClient::m_CameraRotation`（飞行相机，非 `Transform`）；`AttachToComponent` 的 `KeepWorldTransform` 数学实现；ImGuizmo 交互逻辑大改（已用 quat delta，仅随存储变更受益）；`MovementComponent` 占位 API 语义定型；物理引擎接入（`PHYS-F01`）；磁盘旧 `[rx,ry,rz]` 欧拉数组的**自动读档兼容**（旧资产由维护者手改，见 §3.4）。

## Reader quick start
1. 本文件 — 方案、触及范围、拍板项、验收。
2. [CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md](./CORE-F01_TRANSFORM_QUATERNION_IMPLEMENTATION.md) — 切片、PR 边界、每步命令。
3. 代码入口：`Runtime/Function/Framework/Transform/Transform.h`，`Editor/src/UI/Property/TransformWidget.cpp`。

---

## 1) 背景与目标

### Pain
- `Transform::Rotation` 存 `Vector3` 欧拉角（度），`ToMatrix()` 为固定 **X → Y → Z** 旋转链。
- `Rotate(glm::quat)` 与 ImGuizmo 旋转增量已是四元数，但每步 **quat → euler → 存储**，存在万向节锁与累积误差。
- 即将接入 Jolt（`PHYS-F01`）时，每帧 pose 写回若仍经 Euler，会放大抖动与不一致。

### Goals
- **单一存储真源：** `Transform` 内 `Quaternion Rotation`（归一化不变量）。
- **视觉与工具习惯不变：** Inspector 三行 vec3（Position / Rotation / Scale），Rotation 行显示**度**为单位的欧拉角。
- **可序列化、可反射、可 Undo：** 与 `LinearColor` 同级 `ME_STRUCT` 建模；磁盘格式明确（新 struct 四字段；旧 Euler 数组由维护者手改资产）。
- **切片可验证：** 每 slice 可独立 build + smoke，降低横跨 Runtime / Editor / 序列化的回归风险。

### Success
- `verify.ps1` / `minEngineTests test smoke` 通过。
- 已知欧拉角下 `ToMatrix()` 与迁移前数值一致（约定区间内）。
- Inspector 拖 Rotation、Gizmo 旋转、Undo 行为正确；序列化 round-trip 通过。
- `master` 合入后，`render` worktree 可 rebase 获得统一 Transform 定义。

---

## 2) 现状

### 2.1 核心类型

| 位置 | 现状 |
|------|------|
| `Transform.h` | `Vector3 Rotation`；`ToMatrix()` = T × Rx × Ry × Rz；`Rotate(quat)` 写回 Euler |
| `SceneComponent` | `GetRotation()` 返回 `const Vector3&`；Forward/Right/Up 从 Euler 转 quat |
| `GameObject` | 透传 `Vector3` Get/SetRotation |
| 反射 | `Transform.gen.*` 将 `Rotation` 注册为 `Vector3` 字段 |

### 2.2 渲染（轻耦合）

消费方仅依赖 `Transform` 整体或 `ToMatrix()`，**不直接读 `Rotation` 字段**：

- `PrimitiveSceneProxy` / `SkyBoxSceneProxy`：`Transform m_Transform`
- `RenderScene.cpp`、`RenderPipeline.cpp`：`ToMatrix()`
- `StaticMeshComponent` / `SkyBoxComponent`：同步 `GetTransform()`
- `CameraComponent.cpp`：`m_RenderCamera->SetRotation(GetRotation())` — **需改调用路径**（走 Euler 便捷 API）

**纳入本次（拍板 D4）：** `RenderCamera` 旋转存储改为 `Quaternion`，`UpdateViewMatrix` 等与 `Transform` 同一套欧拉约定（D1）。  
**不在本次范围：** 编辑器 `SceneEditingViewportClient::m_CameraRotation`（视口飞行相机，独立 Euler 状态）。

### 2.3 Editor

| 位置 | 现状 | 本次处理 |
|------|------|----------|
| `TransformWidget.cpp` | `DragFloat3` 直接绑 `transform->Rotation` | **特殊 Widget：** UI 显示 Euler vec3，读写映射 `Quaternion` |
| `SceneEditorInspectorSource.cpp` | Root / 嵌套 `m_Transform`；Undo path `m_Transform.Rotation` | 保持 path 名；值语义随 Transform 变 |
| `SceneEditingViewportWindow.cpp` | `ToMatrix()` + ImGuizmo | 基本不变 |
| `SceneEditingViewportClient.cpp` | Gizmo 旋转 → `Rotate(RotationDelta, World)` | 基本不变 |
| `SetGameObjectTransformCommand` | 快照完整 `Transform` | 自动跟随新布局 |

`EDITOR_APPEARANCE.md` §6.5 已约定 `m_Transform.Rotation` 的 **propertyPath 粒度 Undo**；本次不改变 path 字符串，仅改变 `Transform` 内存布局。

### 2.4 序列化

- `Transform` 为 `ME_STRUCT`，字段走嵌套属性序列化。
- `PrimitiveCodecRegistry` 支持 `Vector2/3/4`，**无 `glm::quat` / `Quaternion` codec**。
- 仓库内暂无含 `"Rotation"` 的已提交 scene/json 样本；**不实现**旧 `[rx,ry,rz]` 自动读档（拍板 D4）。

### 2.5 其它调用点

- `Playground.cpp`：大量 `Transform(pos, eulerDeg, scale)` 构造。
- `PreviewScene.cpp`：`SetRotation(Vector3(...))`。
- `MovementComponent::AddRotationInput(Vector3)`：占位，本次不改为正式 API。

---

## 3) 方案

### 3.1 类型设计

新增 `Runtime/Core/Math/Quaternion.h`（与 `LinearColor` 同级）。**数学实现以 GLM 为真源**（`glm::quat`、`glm::angleAxis`、`glm::mat4_cast`、`glm::extractEulerAngleXYZ` 等）；`ME_STRUCT` 四字段 `(W,X,Y,Z)` 与 `glm::quat` 分量顺序一致，经 `ToGlm()` / `FromGlm()` 转换，不在引擎内自写四元数乘法或欧拉分解公式。

```cpp
ME_STRUCT()
struct Quaternion
{
    ME_GENERATED_BODY(Quaternion)

    ME_PROPERTY(EditAnywhere)
    float W = 1.0f;
    ME_PROPERTY(EditAnywhere)
    float X = 0.0f;
    ME_PROPERTY(EditAnywhere)
    float Y = 0.0f;
    ME_PROPERTY(EditAnywhere)
    float Z = 0.0f;

    glm::quat ToGlm() const;
    static Quaternion FromGlm(const glm::quat& quat);
    static Quaternion FromEulerDegreesXYZ(const Vector3& eulerDeg);
    Vector3 ToEulerDegreesXYZ() const;
    static bool AreRotationsEqual(const Quaternion& a, const Quaternion& b, float epsilon = 1e-5f);
};
```

`Transform` 变更摘要：

| 成员 / API | 变更 |
|------------|------|
| `Quaternion Rotation` | 替代 `Vector3 Rotation` |
| `ToMatrix()` | `translate * mat4_cast(normalize(Rotation)) * scale` |
| `SetRotation(const Quaternion&)` / `GetRotation() const` | 主 API |
| `SetRotationEulerDegrees(const Vector3&)` / `GetRotationEulerDegrees() const` | 脚本、Playground、**Inspector 映射层** |
| `Transform(pos, eulerDeg, scale)` | **保留**便捷构造，内部 `FromEulerDegreesXYZ` |
| `operator==` | 四元数比较需约定（见 §6 **D2**） |
| `Rotate(glm::quat, Space)` | 直接复合到 `Rotation`，**不再**写回 Euler |

**不采用：** 在 `Transform` 内继续存 `Vector3` 并缓存 `glm::quat` 的双表示（违反 true-refactor；物理写回会留下一致性问题）。

### 3.2 Inspector：`TransformWidget` 特殊处理（用户要求）

反射层 `Transform.Rotation` 的类型变为 `Quaternion`（四字段 struct），但 **Inspector 不暴露 W/X/Y/Z 四行**。

**规则：**

1. `TransformWidget` **始终**绘制三行 `DragFloat3`：Position、**Rotation（欧拉，度）**、Scale — 与现 UI 一致。
2. Rotation 行读写流程：
   - **读：** `transform->GetRotationEulerDegrees()` → 填入 `DragFloat3`。
   - **写：** 用户改分量 → `transform->SetRotationEulerDegrees(vec3)` → 内部转 quat 并 `normalize`。
3. **不走**通用 PropertyWidgets 对 `Quaternion` 的四字段展开（即使反射注册了四字段）。
4. Undo `propertyPath` 仍为 `m_Transform.Rotation`（或 Root 区等价 path）；命令粒度保持「Rotation 行」而非 W/X/Y/Z 分列。
5. `SceneEditorInspectorSource` 嵌套 `m_Transform` 路径继续调用 `TransformWidget::Draw`，不新增 `QuaternionWidget` 作为默认嵌套展示。

**实现注意：** Euler 显示值从 quat 导出时，在万向节邻域可能跳变；Inspector 拖轴时属预期行为。若需稳定显示，留作 `CORE-F01` 之后 polish，不挡 S04。

### 3.3 欧拉顺序与 `ToMatrix()` 契约

迁移前 `ToMatrix()` 等价于：

```text
R = Rz(rz°) * Ry(ry°) * Rx(rx°)   // glm::rotate 链：先 X，再 Y，再 Z
```

`FromEulerDegreesXYZ` / `ToEulerDegreesXYZ` **必须**与此链互逆（在非奇异区间内），以保证 Playground 已有摆放**视觉不变**。

坐标系注释（保持）：**x 前、y 上、z 右**。

### 3.4 序列化

**磁盘新格式（`Rotation` 字段）：**

```json
"Rotation": { "W": 1.0, "X": 0.0, "Y": 0.0, "Z": 0.0 }
```

或实现上等价的四元素 struct 编码（与 `ME_STRUCT` 字段序列化一致）。

**读档兼容（拍板：不做自动兼容）：**

| 磁盘形状 | 处理 |
|----------|------|
| `"Rotation": { W,X,Y,Z }` | 新格式（唯一支持） |
| `"Rotation": [rx, ry, rz]`（旧 3 元数组，度） | **不实现**自动迁移；维护者手改已存场景/资产中的 `Transform` |
| 缺省 / 单位 | `Identity` |

**写出：** 仅写新格式（struct 四字段，拍板 D3）。

**资产迁移：** 合入前由维护者批量改仓库内仍含旧 Euler `Rotation` 的 JSON/场景文件；实施切片 S03 测试仅覆盖新格式 round-trip。

`Quaternion` 作为 `ME_STRUCT` 走 struct 序列化；若 codegen 对四 float 嵌套已足够，**不必**单独注册 `PrimitiveCodecRegistry`（除非测试证明 struct 路径有缺口）。

### 3.5 SceneComponent / GameObject API

| 旧 API | 新 API |
|--------|--------|
| `const Vector3& GetRotation()` | `Quaternion GetRotation() const`（或 `const Quaternion&`） |
| `void SetRotation(const Vector3&)` | `void SetRotation(const Quaternion&)` |
| — | `Vector3 GetRotationEulerDegrees() const` |
| — | `void SetRotationEulerDegrees(const Vector3&)` |

`SetRotation` / 子节点传播逻辑保持现有「比较后 dirty」结构，仅比较对象改为 `Quaternion`。

`CameraComponent` 与 `RenderCamera` 同步时传递 `Quaternion`（或共享 `GetRotation()` quat API）；`RenderCamera::SetRotation` / `GetRotation` 改为 quat 主 API，必要时保留 Euler 便捷方法供调试。

### 3.6 模块边界与并行协作

```text
physics 分支（本 Feature）
  ├── Runtime/Core/Math/Quaternion.*
  ├── Runtime/Function/Framework/Transform/*
  ├── Runtime/Function/Framework/Components/SceneComponent.*
  ├── Runtime/Function/Framework/GameObject/*
  ├── Runtime/Function/Framework/Components/CameraComponent.cpp
  ├── Runtime/Function/Render/RenderCamera.h/.cpp
  ├── Editor/.../TransformWidget.*
  ├── Editor/.../SceneEditorInspectorSource.cpp（仅当 path/接线需要）
  ├── Playground / Preview 调用点
  └── Tests/SerializationArchiveTest（+ 可选 Transform 专测）

不修改（本 Feature）
  ├── RenderPipeline / RHI / Material / SceneProxy 逻辑
  ├── ImGuizmo 源码
  └── SceneEditingViewportClient::m_CameraRotation（视口飞行相机）
```

**合入策略：** `CORE-F01` 完成后 PR → `master`。`render` worktree 与 `master` 的对齐由维护者自行安排（拍板 D7）。`PHYS-F01` 在 `CORE-F01` **Done** 后启动。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. `ME_STRUCT Quaternion` + Inspector 欧拉映射 | 反射/序列化/Undo 一致；与 `LinearColor` 同模式 | 需定制 `TransformWidget` | **选用** |
| B. `glm::quat` 注册 PrimitiveCodec | 少一个 struct | 反射 Inspector 难做欧拉行；不符合项目 struct 惯例 | 不选 |
| C. 双存储 Euler+Quat | Inspector 简单 | 双真源、物理写回不一致 | 不选 |
| D. 在 master 开发 | 两边 worktree 更早同步 | 与当前 physics 分支分工不一致 | 不选（开发在 physics，**合入 master 要快**） |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Euler ↔ Quat 顺序不一致 | 场景物体旋转突变 | S01 矩阵对比测试；固定 XYZ 链文档化 |
| Inspector 万向节跳变 | 拖 Rotation 时数值跳 | 接受；存储仍为 quat；Gizmo 旋转为主 |
| 序列化旧资产 | 读档失败 | **不自动兼容**；合入前手改仓库内旧 JSON；S03 仅测新格式 |
| `render` 分支 Transform 分叉 | merge 冲突 | 合入 master 后由维护者安排 rebase（D7） |
| 四元数 `operator==` 过于严格 | Undo / SetTransform 频繁 dirty | `q` 与 `-q` 等价比较（D2） |
| 反射暴露四字段 | Inspector 出现 WXYZ | **仅** `TransformWidget` 绘制；通用嵌套路径对 `m_Transform` 仍走 TransformWidget |

---

## 6) 维护者拍板（已确认 2026-06-11）

| # | 问题 | 决定 |
|---|------|------|
| **D1** | 欧拉顺序严格保持迁移前 **X→Y→Z** 度链？ | **是**（保持） |
| **D2** | `Quaternion` 相等：`q` 与 `-q` 视为同一旋转？ | **是**（见下说明；实施前可改） |
| **D3** | 磁盘 `Rotation` 新格式 | **struct 四字段** `{ W,X,Y,Z }`（与反射一致） |
| **D4** | 读档自动兼容旧 `[rx,ry,rz]` 度数组？ | **否** — 旧场景/资产由维护者**手改**为新格式 |
| **D5** | `RenderCamera` 纳入本 Feature？ | **是** — `m_Rotation` 改为 `Quaternion`；视口 `m_CameraRotation` **仍不纳入** |
| **D6** | Inspector Rotation 行标签仍叫 `Rotation`？ | **是** |
| **D7** | 合入 `master` 后 `render` worktree 立即 rebase？ | **维护者自行安排** |

### D2 说明（四元数相等）

同一 3D 旋转可用两个四元数表示：`q` 与 `-q`（分量全取反）数学上等价。若 `operator==` 做分量逐浮点比较，会出现「姿态相同但 `==` 为 false」，进而让 `SetTransform` 误判 dirty、Undo 快照异常。  
**采用 D2=是 时：** `Quaternion::Equals` / `Transform::operator==` 用点积判断：`abs(dot(q,a)) ≈ 1`（归一化前提下），而非 `q == -q` 为 false 的严格分量相等。

---

## 7) 验收标准

- [ ] `Quaternion` 反射生成通过；`Transform.Rotation` 类型为 `Quaternion`。
- [ ] `ToMatrix()` 对比测试：至少 3 组欧拉角与迁移前矩阵元素一致（ε 内）。
- [ ] `minEngineTests`：Transform / GameObject 序列化 round-trip（**新** struct 四字段格式）。
- [ ] `RenderCamera`：`UpdateViewMatrix` 与 `Transform` 一致；`CameraComponent` 同步 quat。
- [ ] Editor：Inspector Rotation 行显示 vec3 欧拉；修改后场景姿态正确。
- [ ] Editor：Gizmo 旋转 + Undo 正常。
- [ ] `verify.ps1` smoke 通过。
- [ ] `FEATURE_REGISTRY` / `ACTIVE_WORK` / Implementation Plan 状态更新；`PHYS-F01` 仍为 Planned 直至本 Feature Done。

---

## 8) Status note

拍板 D1–D7 已记录（§6）。可开 **`CORE-F01-S01`**（建议与 S02 同 PR land）。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-11 | 初稿：范围扫描、Inspector 欧拉映射、切片见 Implementation Plan |
| 2026-06-11 | §6 拍板：D4 不自动读档；D5 纳入 RenderCamera；D7 rebase 自行安排 |
| 2026-06-11 | §3.1：四元数数学以 GLM 为真源（`ToGlm` / `FromGlm`） |
