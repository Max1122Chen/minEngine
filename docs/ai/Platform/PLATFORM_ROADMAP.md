# Platform 路线图（UE 化大方向）

Last updated: 2026-05-26  
Status: **拍板** — 渲染/材质阶段性完成；平台线为主战场；**P2 Editor 平台化进行中（P6.1 → E1 → E2）**

## 1) 产品方向

minEngine 目标从「渲染学习 demo」升级为 **更像 Unreal 的编辑器驱动引擎**：

- **Runtime**：可配置启动、清晰所有权与内存管理、反射驱动序列化（已有基础）
- **Editor**：Editor 平台化（Inspector / Previewer / Asset 基础设施）、Content Browser、Undo、模式化解耦
- **后续**：`MEFunction` + Lua；不阻塞当前平台线

渲染/材质（[Render/Material](../Render/Material/MATERIAL_SYSTEM_ROADMAP.md)）维持维护，新功能以平台能力为主。

## 2) 优先级（与用户拍板一致）

```text
P0  启动 / 路径配置化     → ENGINE_STARTUP_DESIGN
P1  内存管理（非重型 GC） → MEMORY_MANAGEMENT_DESIGN
    └─ 清除旧 bad 模式（ObjectManager 强引用等）
P2  Editor 平台化           → EDITOR_PLATFORM_PLAN（E0–E4）
    └─ E0 / E3 / E4 / P6 基础设施 [Done]
    └─ P6.1 Content Browser UI（Appearance）[当前]
    └─ E1 Inspector 统一化 → E2 Preview
    └─ Appearance M0–M6b [Done，见 EDITOR_APPEARANCE]
P3  编辑器 Command/Undo   → [EDITOR_COMMAND_HISTORY](../Editor/EDITOR_COMMAND_HISTORY.md)
    └─ 前置：序列化 Binary + Property API → [Serialization](./Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md) S1–S2
P4  反射 MEFunction       → 设计待写
P5  Lua 脚本              → 设计待写
```

**原则：** 先让「路径 + 对象活着」可靠，再堆编辑器与脚本。

## 3) 能力矩阵

| 能力 | 现状 | 目标 | 设计文档 |
|------|------|------|----------|
| 启动配置 | `EngineConfig.meconfig` 绝对路径；Playground 硬编码 | 三层路径 + 相对解析 | [Startup](./Startup/ENGINE_STARTUP_DESIGN.md) |
| 对象生命周期 | `ObjectManager` 强 `shared_ptr` 注册表；手动 `RemoveObject` | **原地重构** `ObjectManager`：`weak_ptr` 索引 + 根 + Outer | [MemoryManagement](./MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md) |
| 资产 | Registry + Watcher + CB 数据层 | P6.1 UI + Move/过滤；E1 Meta Inspector | [E3](../Editor/EDITOR_PLATFORM_PLAN.md)；[P6](../Platform/ContentBrowser/ASSET_PIPELINE_P6_API.md) |
| Content Browser | P6 框架（裸 ImGui） | Appearance 抛光 + Preview | [CONTENT_BROWSER_UI_DESIGN](./ContentBrowser/CONTENT_BROWSER_UI_DESIGN.md) |
| 编辑器 Shell | E0 模块体系 | P7 默认 Dock | [EDITOR_SHELL_DESIGN](../Editor/EDITOR_SHELL_DESIGN.md) |
| Editor 外观 | M0–M6b 主题/字体/Property | M6c / i18n / CB-UI | [EDITOR_APPEARANCE](../Editor/EDITOR_APPEARANCE.md) |
| 反射 | Property only | + UFunction 等价物 | （待写，P4） |
| Undo | E1.1–E1.2 Scene 命令 | Property blob + Snapshot | [Command History](../Editor/EDITOR_COMMAND_HISTORY.md) |
| 序列化 | JsonArchive + 私有 `SerializeProperty` | BinaryArchive + 公开 Property API | [Serialization](./Serialization/SERIALIZATION_BINARY_AND_PROPERTY_API.md) |
| 脚本 | 无 | Lua 薄绑定 | （待写，P5） |

## 4) 与 UE 对照（学习用）

| UE | minEngine 目标 |
|----|----------------|
| `FPaths` / `FConfigCache` | `PathRegistry` + Engine/Project config |
| `GUObjectArray` + GC | **`ObjectManager`** 弱引用索引 + `CollectGarbage(Domain)` |
| `UObject::Outer` | `MEObject::m_Outer` 系统化 |
| Asset Registry + Content Browser | `AssetManager` 变更 API + Browser UI |
| Details + Preview | Inspector Drawer + PreviewScene |
| `UFunction` + Blueprint | `MEFunction` + Lua（后期） |
| `FTransaction` / Undo | `EditorCommandHistory`（后期） |

## 5) 里程碑（建议工期量级）

| 里程碑 | 交付 | 约 |
|--------|------|-----|
| **M0** | 相对路径配置；启动日志打印 resolve 结果；去掉 Playground 绝对路径 | 3–5 天 |
| **M1** | 重构 `ObjectManager`（`weak_ptr` 图）；`NewObject` 不延长寿命 | 1–2 周 |
| **M2** | `ObjectManager::CollectGarbage`；Outer 规则；删除多余 `RemoveObject` | 1 周 |
| **M3** | **Editor 平台化** E1–E4 分项设计 + 首轮实现；Content Browser 随基础层推进 | 按需 |
| **M4+** | Undo、MEFunction、Lua | 按需 |

## 6) 参考

- [docs/ai/README.md](../README.md) — 文档布局
- [Render/Material 路线图](../Render/Material/MATERIAL_SYSTEM_ROADMAP.md)
