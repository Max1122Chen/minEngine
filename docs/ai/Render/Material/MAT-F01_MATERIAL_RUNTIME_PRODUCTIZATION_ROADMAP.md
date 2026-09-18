# MAT-F01 — Material Runtime Productization Roadmap（UE 式运行时产品面）

## Meta
- **ID:** MAT-F01（本文件为 **Capability 路线图**；落地切片见 MAT-F02+）
- **Type:** Roadmap
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:**
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - [MAT-F02 Design](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md)（**首刀：Proxy**）
  - 图/Editor 已交付：[MATERIAL_SYSTEM_ROADMAP](./MATERIAL_SYSTEM_ROADMAP.md)（Phase 0–5，**作者侧**；本路线图 = **运行时产品侧**）
  - UE：`UMaterialInterface` / `UMaterial` / `UMaterialInstance` / `UMaterialInstanceDynamic` / `FMaterialRenderProxy`
- **Trust:** Tier A（与 ACTIVE_WORK 共同指导 MAT 运行时轨）
- **Supersedes:** 旧 [MAT-F01_MATERIAL_INSTANCE_DESIGN](./MAT-F01_MATERIAL_INSTANCE_DESIGN.md)（单 MI 窄范围；已改为指针页）

## TL;DR

把材质升级为 **UE 式运行时产品面**。F02–F05 Done。**下一刀 MAT-F06（MIC / `.memi`）** — Design Review。

## Scope
- **In:** Proxy 绑定真源；Interface 多态；Material 编译真源；Instance 覆盖；Dynamic；后期 MIC
- **Out:** Static Switch / 静态 permutation；材质层叠 / Substrate；完整双线程 Proxy 镜像（可分阶段）；图节点编辑器大改

## Reader quick start
1. §2 类型地图  
2. §3 Feature 表（**注意：Proxy = Order 1**）  
3. 首刀 Design：[MAT-F02](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md)

---

## 1) 方向与原则

| 原则 | 含义 |
|------|------|
| **Proxy first** | 渲染绑定边界先于组件多态与覆盖表；避免 Draw utils 散落业务分支 |
| **Capabilities, not Opinions** | Interface / Instance / Proxy 机制；不写玩法语义进 Core |
| **对标 UE 职责，不克隆体积** | 学 `FMaterialRenderProxy` / `FMaterialInstanceResource` 数据流；不搬 StaticPermutation |
| **Material 唯一编 shader** | Instance / Dynamic **不**为换 float/贴图重编图 |
| **Draw 只认 Proxy** | 组件持 Interface（F03+）；最终绑定只走 `GetRenderProxy()` |
| **可增量验收** | 每 Feature 竖切 DoD |

---

## 2) 类型地图（产品目标态）

```text
MaterialInterface          ← 组件 / 资源槽（F03+）
    ├── Material           ← 图 + Compile + 默认参数（Asset）
    │     └── owns DefaultMaterialRenderProxy
    └── MaterialInstance   ← Parent + 覆盖表（F04）
            ├── owns InstanceMaterialRenderProxy
            └── MaterialInstanceDynamic  ← 运行时 Create + 高频 Set*（F05）

MaterialRenderProxy        ← Draw 唯一绑定视图（F02 地基）
    Default / Instance 实现参数解析：本地 → Parent Proxy
```

| 类型 | 职责 | 非职责 |
|------|------|--------|
| `MaterialRenderProxy` | Draw 查询 shader/BindingSet、Bind、参数解析视图；**由 Material/MI 拥有，不进 RenderScene** | 拥有图编辑器；玩法对象身份；≠ Primitive SceneProxy |
| `MaterialInterface` | `GetBaseMaterial`、`GetRenderProxy`、半透明等查询 | 存图 IR、独自 Compile |
| `Material` | 图、Compile、默认参数、默认 Proxy | 玩法每帧改参的主入口 |
| `MaterialInstance` | Parent、覆盖、Instance Proxy | Static Switch |
| `MaterialInstanceDynamic` | 运行时工厂 + 动态 Set* | 必须先有 `.memi` |

---

## 3) Feature 优先级

| Order | ID | 名称 | Status | Design |
|-------|-----|------|--------|--------|
| 0 | **MAT-F01** | 本路线图 | **In Progress** | 本文 |
| 1 | **MAT-F02** | **MaterialRenderProxy**（绑定真源） | **Done** | [Design](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md) |
| 2 | **MAT-F03** | MaterialInterface + Material 迁入 | **Done** | [Design](./MAT-F03_MATERIAL_INTERFACE_DESIGN.md) |
| 3 | **MAT-F04** | MaterialInstance（覆盖 + Instance Proxy） | **Done** | [Design](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md) |
| 4 | **MAT-F05** | MaterialInstanceDynamic | **Done** | [Design](./MAT-F05_MATERIAL_INSTANCE_DYNAMIC_DESIGN.md) |
| 5 | **MAT-F06** | MIC 资产 / Editor（`.memi`） | **Review** | [Design](./MAT-F06_MATERIAL_INSTANCE_CONSTANT_DESIGN.md) |

**依赖：** **F02 → F03 → F04 → F05**；F06 依赖 F04。  
**修订说明（2026-09-18）：** 原「Interface 首刀、Proxy 殿后」已否决；Proxy 提前为 Order 1。

---

## 4) 里程碑

| Milestone | 交付 | 验收 |
|-----------|------|------|
| **M1** | F02：Draw 经 Proxy；Material 自代理 | 现网可画；生产路径无 `Material::BindForDraw` |
| **M2** | F03：组件持 Interface | grep 槽位多态；回归 |
| **M3** | F04：两 Mesh 同 Parent 异覆盖 | 单测 + 手验；经 Instance Proxy |
| **M4** | F05：Runtime Dynamic Set* | Demo/脚本可每帧改色 |
| **M5** | F06：可选 MIC | CB 可创建/打开 Instance 资产 |

---

## 5) 已拍板 / 默认

| # | 议题 | 默认 |
|---|------|------|
| D0 | **交付顺序** | **Proxy → Interface → MI → MID → MIC**（本修订拍板） |
| D1 | Parent 链 MVP | F04：**仅允许 Parent = Material***；链到 Instance 后置 |
| D2 | 组件存储 | F03：`shared_ptr<MaterialInterface>` |
| D3 | MID vs MI | MID **是** MI 子类（或同存储 + Dynamic API） |
| D4 | Proxy 线程模型 | F02–F05：**游戏线程 Proxy 足够**；双线程 Cache 后置 / TD |
| D5 | `.memi` | **F06**；不挡 M1–M4 |
| D6 | 旧单 MI Design | **作废为窄方案**；以本路线图 + F02+ 为准 |
| D7 | **GPU BindingSet/UBO 归属** | **在 Proxy 上**（F02 §3.2；对标 UE Uniform cache-on-proxy）；Shader/Layout 仍在 Material |
| D8 | Proxy 参数查询 | 基类虚 `Get*ParameterValue*`，供 Default / Instance 链（F02→F04） |

---

## 6) 依赖与风险

| 风险 | 缓解 |
|------|------|
| 无 Proxy 先做 Interface/MI | **已否决**；见 D0 |
| Draw 迁移遗漏 | F02 grep 门禁 + 场景回归 |
| Proxy 过早双线程 | D4 |
| 与图 Phase 文档混淆 | README：作者侧 vs 运行时侧 |

---

## 7) 与旧文档关系

| 文档 | 关系 |
|------|------|
| `MATERIAL_SYSTEM_ROADMAP` Phase 0–5 | **作者/图/着色**；不替代本运行时路线图 |
| 旧 `MAT-F01_MATERIAL_INSTANCE_DESIGN` | 指针页 → 本文 + F04 |
| 曾用「F02=Interface / F05=Proxy」编号 | **已重排**；以 §3 表为准 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | **完善：** D7/D8（GPU 缓存在 Proxy；参数虚查询）；F02–F05 按 UE 源码对照加厚 |
| 2026-09-18 | **修订：** Proxy 升为 Order 1；F02=Proxy 详设；Interface→F03；MI→F04；MID→F05 |
| 2026-09-18 | 初版：MAT-F01 升格为 UE 式运行时产品化路线图；登记 F02–F06 |
