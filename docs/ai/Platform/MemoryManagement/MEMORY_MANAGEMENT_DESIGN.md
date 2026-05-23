# 内存管理 — 设计草稿

Last updated: 2026-05-23  
Status: **草稿（待 M1/M2 实施）**  
父文档：[Platform 路线图](../PLATFORM_ROADMAP.md)  
前置建议：[启动 / 路径配置](../Startup/ENGINE_STARTUP_DESIGN.md) M0

**命名约定：** 在现有 **`ObjectManager`** 上**原地重构**（不引入 `ObjectRegistry` 等新类型名）。对外 API 保持 `ObjectManager::Get()`、`NewObject`、`FindObject`；全局便捷函数 `minEngine::NewObject` / `FindObject` / `RemoveObject` 不变。

术语：对外称 **内存管理（Memory Management）**；内部实现是 **轻量收集（collection）**，不是 JVM/UE 式全堆 GC。避免与「仅 shared_ptr」混淆。

---

## 0) 现状与问题

### 当前模型

```text
NewObject<T>()  →  shared_ptr<T>  +  ObjectManager::RegisterObject (强引用)
显式 RemoveObject(guid)  →  从 map 移除  →  若无其它 shared_ptr 则析构
```

代码锚点：`ObjectManager.cpp`（`m_ObjectsByGuid` 存 `shared_ptr`）、`Scene.cpp` / `GameObject` / `SceneManager` 多处 `RemoveObject`。

### 核心矛盾

| 现象 | 原因 |
|------|------|
| 「忘了 RemoveObject」= 泄漏 | 注册表 **持有强引用**，对象永不释放 |
| 与「shared_ptr GC」直觉不符 | 实际是 **全局强引用表 + 手动注销** |
| `MEObject::~MEObject()` 默认 | **不会**自动从 `ObjectManager` 注销 |
| `m_Outer` 存在但未系统化 | 子对象生命周期靠各自 Remove，易漏 |

这不是「引用计数 GC」，而是 **双轨所有权**（外部 shared_ptr + `ObjectManager` 强引用表），长期不可维护。

### 目标（UE 化但不抄全量 GC）

1. **谁创建谁负责** 或 **Outer 拥有子对象** — 规则可教、可测。  
2. **GUID 查找** 保留（序列化、资产、编辑器）。  
3. **尽量简单**：无分代 GC、无 STW 多线程；场景边界可预测收集。  
4. **在 `ObjectManager` 内完成语义修正**；迁移后删除散落的 bad `RemoveObject` 用法。

---

## 1) 设计选项（导师视角）

### 选项 1 — 纯 `shared_ptr`，去掉 ObjectManager 注册

- 只靠 C++ RAII；GUID 存在对象上，**不**全局索引。
- **优点：** 最简单。  
- **缺点：** 无法 `FindObject(guid)`；序列化/编辑器/资产引用会断。  
- **结论：** ❌ 不符合引擎需求。

### 选项 2 — 保持强引用注册表 + 规范「必须 Remove」

- 写文档 + Code Review 约束。  
- **优点：** 零架构改动。  
- **缺点：** 已证明会漏（Scene、Component、Preview 各自 Remove）。  
- **结论：** ❌ 技术债延续。

### 选项 3 — **ObjectManager 改为 `weak_ptr` 索引** + 显式根（推荐主干）

```text
ObjectManager::m_ObjectsByGuid  →  GUID → weak_ptr<MEObject>
强引用只存在于：Scene、AssetManager 缓存、栈、其它 owner
```

- `FindObject`：对 weak 做 `lock()`，过期返回 `nullptr`。  
- **优点：** 外部 `shared_ptr` 归零即可析构；`ObjectManager` 不再制造泄漏；调用点仍认 `ObjectManager` 名字。  
- **缺点：** 需定义 **根集合**；`FindObject` 可能失败需处理。  
- **结论：** ✅ **M1 主干（原地改 `ObjectManager`）**

### 选项 4 — UE 式 `GUObjectArray` + Mark-Sweep GC

- 全对象池、标记可达、延迟销毁、`RF_*` 标志。  
- **优点：** 编辑器/PIE/循环引用最终可解。  
- **缺点：** 实现与调试成本高一个数量级；学习引擎易过度设计。  
- **结论：** ⏸ **M2+ 仅当弱引用 + 域收集不够时** 再于 `ObjectManager` 上增加 `CollectGarbage`

### 选项 5 — 句柄（`ObjectHandle = uint32`）+ 世代 ID

- 不暴露裸指针；Lua/脚本友好。  
- **优点：** 稳定 API、检测 use-after-free。  
- **缺点：** 间接层 + 代际管理；与现有 `shared_ptr` 迁移量大。  
- **结论：** 🔶 **M3 可选**（为 Lua 准备时与 `MEFunction` 同批评估）

---

## 2) 拍板：**选项 3 + 分阶段 Outer/域收集（选项 4 的极简子集）**

**一句话：**  
在 **`ObjectManager` 内** 把注册表改为 **弱引用索引**，**强所有权** 留在明确的 Root/Outer 链；场景卸载时 **`ObjectManager::CollectGarbage(Domain)`**（M2）。

**为什么沿用 `ObjectManager` 命名：**

- 全工程已依赖 `ObjectManager::Get()`、`NewObject`、`测试里的 `MaterialIRTestObjectManagerScope` 等。  
- 重构的是 **内部存储与语义**，不是换一个新单例。  
- 与 UE 的「UObject 在 GUObjectArray 里登记」类似：名字稳定，实现演进。

**为什么不是完整 GC：**

- 先修正「注册表强引用」这一结构性错误，收益最大。  
- UE 全量 GC 可 **按需** 以 `ObjectManager::CollectGarbage` 子集形式递进。

---

## 3) 目标架构（`ObjectManager` 重构后）

### 3.1 类型职责

| 类型 | 所有权 | ObjectManager |
|------|--------|----------------|
| `Scene` | `shared_ptr` 持有 GO 列表 | Scene 为 **World 根** |
| `GameObject` | Scene 拥有 | `RegisterObject` → weak |
| `Component` | **Outer** = GO；`shared_ptr` 在 GO 容器 | weak |
| `Material` / `Asset` | `AssetManager` 缓存 + 加载方 | weak |
| `MaterialEdGraph` 节点 | **Outer** = Material/Graph | Instanced 子对象，随 Outer 释放 |
| Editor Preview Scene | Editor 子系统持有 | 与 Active Scene **分域** |

### 3.2 `ObjectManager` 内部与 API（M1 目标形态）

**内部：**

```cpp
// ObjectManager.h（示意）
std::unordered_map<GUID, std::weak_ptr<MEObject>, GUID::Hash> m_ObjectsByGuid;
```

**对外 API（保留名称，调整语义）：**

| API | M1 行为 |
|-----|---------|
| `RegisterObject(shared_ptr)` | 写入/更新 `weak_ptr`；**不**增加强引用计数 |
| `FindObject(guid)` | `lock()` weak；失败返回 `nullptr` |
| `NewObject` / 全局 `NewObject` | 创建 → `RegisterObject` → 返回 `shared_ptr` 给调用方 |
| `RemoveObject` / `UnregisterObject` | 仅从 map **移除索引**（不代替析构）；M2 起多数路径由析构自动注销 |
| `GetTrackedObjectCount()` | 非 expired 的 weak 条目数（调试用） |
| `CollectGarbage(Domain)` | **M2** 从根 mark；清理过期 weak；WARN 泄漏候选 |

**析构时自动注销（M1）：**

- `MEObject::~MEObject()`（或 controlled 析构路径）调用 `ObjectManager::UnregisterObject(GetGuid())`（实现可仍对外暴露为 `RemoveObject` 的内部实现）。  
- M2 目标：业务代码 **不再** 手写 `RemoveObject`，只释放持有的 `shared_ptr`。

### 3.3 根集合（Roots）

域收集或审查泄漏时，从以下根出发保证存在强引用链：

| 根 | 持有者 |
|----|--------|
| Active `Scene` | `SceneManager` |
| 已加载 `Asset`（策略可配置） | `AssetManager` |
| Editor Preview World | `MaterialEditor` / `MaterialPreviewViewport` |
| Engine 子系统单例 | 尽量少、文档化 |

### 3.4 `Outer` 子对象规则（对齐 UE `Subobject`）

1. `NewObject(..., Outer)` 设置 `m_Outer`；子对象强引用由 Outer 容器持有。  
2. `GameObject`：`vector<shared_ptr<Component>>`。  
3. Material Graph 节点：Outer 链到 `Material`；Material 释放 → 子图节点一并释放。  
4. 序列化：先 Outer 后 Subobject；反序列化挂回同一 Outer。

### 3.5 `ObjectManager::CollectGarbage`（M2）

```text
UnloadScene / CloseProject / SwitchPreview:
  1. 释放域根（如 Scene shared_ptr reset）
  2. ObjectManager::CollectGarbage(SceneDomain):
       - 从域根 mark 可达（可选简化实现）
       - 清扫 map 中已 expired 的 weak 条目
       - 若某 GUID 仍 lock 成功但不在根可达集 → ME_CORE_WARN（Debug）
  3. Debug：GetTrackedObjectCount() 应随域卸载下降
```

**不做：** 每帧全堆 GC。  
**可做：** Editor 切换场景 / 关闭 Material 预览后手动触发。

---

## 4) 迁移计划（均在 `ObjectManager` 上完成）

### M1 — 弱引用注册表（1–2 周）

| 任务 | 说明 |
|------|------|
| OM1 | `m_ObjectsByGuid`：`shared_ptr` → `weak_ptr` |
| OM2 | `RegisterObject` / `FindObject` / `UnregisterObject` 适配 lock 与过期 |
| OM3 | `NewObject` 模板与 `NewObject(MEClass*)` 行为不变，仅注册语义变弱引用 |
| OM4 | `MEObject` 析构自动 `UnregisterObject` |
| OM5 | 测试：局部 `shared_ptr` 析构后 `FindObject` 返回 null |

**过渡期：** 现有 `RemoveObject` 调用可保留；标记 **将在 M2 删除**（非 `[[deprecated]]` 永久保留）。

### M2 — 清 bad 模式（约 1 周）

| 任务 | 说明 |
|------|------|
| C1 | 删除 Scene/GO/Component 等冗余 **显式** `RemoveObject` |
| C2 | `SceneManager::UnloadScene`（或等价）调用 `ObjectManager::CollectGarbage` |
| C3 | Preview Scene 与 Active Scene 分域 + 测试 |
| C4 | `MaterialIRTestObjectManagerScope` 继续使用独立 `ObjectManager` 实例（`SetInstance`），与生产路径一致 |

### M3 — 句柄 / 脚本（后续）

- `ObjectHandle` 可仍由 `ObjectManager` 解析 GUID；与 `MEFunction`、Lua 一并设计。

---

## 5) API 对比（迁移前后）

| 操作 | 现在 | 目标 |
|------|------|------|
| 创建 | `NewObject` → `ObjectManager` **强**持有 | `NewObject` → 仅 **弱**登记；调用方持 `shared_ptr` |
| 查找 | `FindObject` 在 Remove 前总成功 | `FindObject` = lock weak，可能 null |
| 销毁 | 手动 `RemoveObject` + 释放 shared_ptr | 释放 shared_ptr → 析构 → 自动 Unregister |
| 场景卸载 | 遍历 `RemoveObject` | `scene.reset()` + `CollectGarbage` |

---

## 6) 与 UE 对照（学习）

| UE | minEngine 目标 |
|----|----------------|
| `GUObjectArray` + `FUObjectItem` | **`ObjectManager`**（weak 索引 + 可选 mark） |
| `UObject::Outer` | `MEObject::m_Outer` + 容器拥有 |
| `RF_Standalone` / 根集 | `Scene` / `AssetManager` 根 |
| `CollectGarbage(...)` | **`ObjectManager::CollectGarbage(Domain)`** |
| `TWeakObjectPtr` | `FindObject` 失败 ≈ 对象已释放 |

**不急于实现：** `FGCObject`、`FReferenceCollector`、异步加载 GC 屏障。

---

## 7) 验收标准

### M1 完成

- [ ] `ObjectManager::GetTrackedObjectCount()` 在场景卸载后下降  
- [ ] `m_ObjectsByGuid` 无 `shared_ptr` 强引用（仅 `weak_ptr`）  
- [ ] Editor 多次打开/关闭 `test` 场景，`FindObject` 不返回已销毁 GUID  
- [ ] `--material-ir-test` 仍 exit 0  

### M2 完成

- [ ] 删除 ≥90% 业务侧显式 `RemoveObject`  
- [ ] `CollectGarbage` 对「无根但仍 lock 成功」对象打 WARN（Debug）

---

## 8) 风险与对策

| 风险 | 对策 |
|------|------|
| `FindObject` 返回 null 增多 | 区分「从未存在」与「已释放」；序列化 GUID 重解析 |
| 循环 `shared_ptr` | 禁止 Component↔GO 互持 shared_ptr |
| `Unregister` 与析构重入 | 保持现有 erase 前先拷贝/延后析构模式（见 `ObjectManager.cpp` 注释） |
| 编辑器 Preview 泄漏 | Preview 域独立根；关 Material 模式时 `CollectGarbage` |

---

## 9) 参考

- `minEngine/minEngine/src/Runtime/Core/Object/ObjectManager.{h,cpp}`
- `MEObject.h`、`GameObject.h`
- `MaterialIRTestObjectManagerScope` — `MaterialIRTest.cpp`
- UE：`GUObjectArray`、`GarbageCollection.cpp`（阅读用）
