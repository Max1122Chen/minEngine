# Platform 路线图（UE 化大方向）

Last updated: 2026-05-23  
Status: **拍板** — 渲染/材质阶段性完成；平台线为主战场

## 1) 产品方向

minEngine 目标从「渲染学习 demo」升级为 **更像 Unreal 的编辑器驱动引擎**：

- **Runtime**：可配置启动、清晰所有权与内存管理、反射驱动序列化（已有基础）
- **Editor**：Content Browser、事务化 Undo、模式化解耦（Material 模式已验证）
- **后续**：`MEFunction` + Lua；不阻塞当前平台线

渲染/材质（[Render/Material](../Render/Material/MATERIAL_SYSTEM_ROADMAP.md)）维持维护，新功能以平台能力为主。

## 2) 优先级（与用户拍板一致）

```text
P0  启动 / 路径配置化     → ENGINE_STARTUP_DESIGN
P1  内存管理（非重型 GC） → MEMORY_MANAGEMENT_DESIGN
    └─ 清除旧 bad 模式（ObjectManager 强引用等）
P2  Content Browser       → 设计待写（依赖 AssetManager + 资产事件）
P3  编辑器 Command/Undo   → 设计待写
P4  反射 MEFunction       → 设计待写
P5  Lua 脚本              → 设计待写
```

**原则：** 先让「路径 + 对象活着」可靠，再堆编辑器与脚本。

## 3) 能力矩阵

| 能力 | 现状 | 目标 | 设计文档 |
|------|------|------|----------|
| 启动配置 | `EngineConfig.meconfig` 绝对路径；Playground 硬编码 | 三层路径 + 相对解析 | [Startup](./Startup/ENGINE_STARTUP_DESIGN.md) |
| 对象生命周期 | `ObjectManager` 强 `shared_ptr` 注册表；手动 `RemoveObject` | **原地重构** `ObjectManager`：`weak_ptr` 索引 + 根 + Outer | [MemoryManagement](./MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md) |
| 资产 | `AssetManager` 扫描/meta/加载 | Registry 事件 + Content Browser UI | （待写） |
| 反射 | Property only | + UFunction 等价物 | （待写） |
| 编辑器 | `EditorUIMode`；无 Undo | Subsystem + `IEditorCommand` | （待写） |
| 脚本 | 无 | Lua 薄绑定 | （待写） |

## 4) 与 UE 对照（学习用）

| UE | minEngine 目标 |
|----|----------------|
| `FPaths` / `FConfigCache` | `PathRegistry` + Engine/Project config |
| `GUObjectArray` + GC | **`ObjectManager`** 弱引用索引 + `CollectGarbage(Domain)` |
| `UObject::Outer` | `MEObject::m_Outer` 系统化 |
| Asset Registry + Content Browser | `AssetManager` + UI |
| `UFunction` + Blueprint | `MEFunction` + Lua（后期） |
| `FTransaction` / Undo | `EditorCommandHistory`（后期） |

## 5) 里程碑（建议工期量级）

| 里程碑 | 交付 | 约 |
|--------|------|-----|
| **M0** | 相对路径配置；启动日志打印 resolve 结果；去掉 Playground 绝对路径 | 3–5 天 |
| **M1** | 重构 `ObjectManager`（`weak_ptr` 图）；`NewObject` 不延长寿命 | 1–2 周 |
| **M2** | `ObjectManager::CollectGarbage`；Outer 规则；删除多余 `RemoveObject` | 1 周 |
| **M3** | Content Browser v0（浏览+打开） | 1–2 周 |
| **M4+** | Undo、MEFunction、Lua | 按需 |

## 6) 参考

- [docs/ai/README.md](../README.md) — 文档布局
- [Render/Material 路线图](../Render/Material/MATERIAL_SYSTEM_ROADMAP.md)
