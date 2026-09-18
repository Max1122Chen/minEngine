# Material — 材质系统

| 文档 | 状态 |
|------|------|
| **[MAT-F01 运行时产品化路线图](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md)** | **In Progress** — Proxy → Interface → MI → MID → MIC |
| [MAT-F02 MaterialRenderProxy](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md) | **Done** |
| [MAT-F03 MaterialInterface](./MAT-F03_MATERIAL_INTERFACE_DESIGN.md) | **Done** |
| [MAT-F04 MaterialInstance](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md) | **Done** |
| [MAT-F05 MaterialInstanceDynamic](./MAT-F05_MATERIAL_INSTANCE_DYNAMIC_DESIGN.md) | **Done** |
| [MAT-F06 MIC / `.memi`](./MAT-F06_MATERIAL_INSTANCE_CONSTANT_DESIGN.md) | **Review** |

### 何时用哪个

| 类型 | 用途 |
|------|------|
| `Material` | 资产默认值；改参 = 改资产 |
| `MaterialInstance` | Parent + 覆盖（逻辑 Instance；存盘见 F06 MIC） |
| `MaterialInstanceDynamic` | 运行时 `Create`；可每帧 `Set*`；通常不存盘 |

```cpp
auto mid = MaterialInstanceDynamic::Create(parentMaterial);
mid->SetScalarParameter("Tint", 0.5f);
mesh->SetMaterial(mid);
```

| [MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md) | Phase 0–5 **作者/图侧**（维护模式） |
| [MATERIAL_EDITOR_PLAN.md](./MATERIAL_EDITOR_PLAN.md) | 编辑器 E0–E4 |
| [MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md](./MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md) | Tier B 历史桥接清单 |
| [MATERIAL_SHADING_MODEL_PLAN.md](./MATERIAL_SHADING_MODEL_PLAN.md) | 着色模型 |
| [bugs/](./bugs/) | 材质相关缺陷 |

平台能力见 [Platform](../../Platform/README.md)。
