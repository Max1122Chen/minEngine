# Content Browser — 产品意图（实现后置）

Last updated: 2026-05-24  
Status: **产品意图已记录；详细架构与实现见 [Editor 平台化规划](../../Editor/EDITOR_PLATFORM_PLAN.md)**  
父文档：[Platform 路线图](../PLATFORM_ROADMAP.md)

> **说明：** 此前版本中的 M3 分期与类级设计已撤回。Content Browser 依赖 Inspector / Previewer / AssetManager / FileDialog 四大基础板块，**在分项设计完成前不再扩展本文档的实现细节**。

---

## 产品意图（保留）

1. **任意 Editor 模式**可打开的 Shared 窗口；SceneEditing 下默认 dock 右下。
2. 展示 Project **`Assets/`** 树；条目为引擎识别资产类型（v0 无缩略图）。
3. **Import** 外部文件 → 项目目录 + meta → Registry 刷新。
4. **交互**：选中 → Inspector（meta + 预览）；右键 Delete/Move（文件 + meta + Registry）。
5. **双击** → 打开对应 Asset Editor / 切换模式；输入路由到 focusing Editor。
6. **可扩展**：数据源、过滤、动作、预览 Provider 插件化。

---

## 依赖的基础板块

| 板块 | 文档 |
|------|------|
| Inspector 门面 + Drawer | [EDITOR_PLATFORM_PLAN.md § E1](../../Editor/EDITOR_PLATFORM_PLAN.md) |
| Previewer 统一化 | [EDITOR_PLATFORM_PLAN.md § E2](../../Editor/EDITOR_PLATFORM_PLAN.md) |
| AssetManager 基础设施 | [EDITOR_PLATFORM_PLAN.md § E3](../../Editor/EDITOR_PLATFORM_PLAN.md) |
| 跨平台 FileDialog | [EDITOR_PLATFORM_PLAN.md § E4](../../Editor/EDITOR_PLATFORM_PLAN.md) |

---

## 下一步

- **分项设计：** [ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md)（E3 + FileDialog + Watcher + Browser 框架 + 实施顺序 + 待拍板表 §10）。
- 拍板后在本文件恢复「Content Browser v0 验收标准」一节。
