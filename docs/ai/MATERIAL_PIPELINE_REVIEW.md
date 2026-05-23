# Material 渲染管线隐患审查（基础设施）

Last updated: 2026-05-23  
Scope: Editor 材质图 → MIR → GLSL → Shell → GPU 预览/网格绘制

---

## 管线分层（与 UE `Translate_New` 对齐）

```text
MaterialEdGraph
  → MIRBuilder + MIREmitter          (≈ FMaterialIRModuleBuilder + FEmitter)
  → MIRGraph (link / NumUsers)
  → GLSLMaterialTranslatorImpl       (≈ FMaterialIRToHLSLTranslation)
  → GLSLMaterialShellAssemblerImpl   (template 填参)
  → Material::CommitCompileResult    (shader 对象 + 参数布局)
  → Render / MaterialPreviewViewport
```

---

## 已修复（本次）

| ID | 问题 | 修复 |
|----|------|------|
| **MATERIAL-001** | `Constant3` RGB → Normal 触发 `ME_ASSERT` 静默退出 | live `NumUsers`；按需子 output；GLSL diagnostic + foldable multi-use inline |
| **INFRA-002** | `Translate()` 恒设 `Succeeded=true` | 按 `Diagnostics` 中 Error 决定 `Succeeded` |
| **INFRA-003** | 编译器路径 `ME_ASSERT` → Debug 断点 | GLSL lowering 改为 `MaterialCompileDiagnostic::Error` |
| **P1-Poison** | Poison 进 `SetMaterialOutput` / lowering | `EmitDiagnostic`（含节点类名）；`FlowValueIntoMaterialOutput` 回退 default；常量折叠除零报错 |
| **P1-Link** | Link/Analyze/Branch | `Step_ResetLinkState`；`Step_VerifyLinkCoverage`；`VerifyIfThenElseAlbedoBlinnPhong` |
| **P2-NonFoldable** | `TextureRead` 多 use 未进链 | Link 校验 + `LowerBlock` materialize；`VerifyTextureSampleSharedByTwoOutputs` |
| **P2-Capability** | struct 与 capability 漂移 | `GetFragmentPropertiesEmittedAtCompile`；`VerifyFragmentStructMatchesCapability`（Unlit/BlinnPhong） |

---

## 仍须关注（未改或部分覆盖）

### P1 — MIR / 图构建

| 风险 | 说明 | 建议 |
|------|------|------|
| **MaterialOutput 双根** | 多 `MaterialOutput` 节点时 root 行为未充分测试 | 文档约束「单输出节点」或显式报错 |

### P2 — GLSL / Shell

| 风险 | 说明 | 建议 |
|------|------|------|
| **Cast / 矩阵** | `MIREmitter` 矩阵 cast 未实现 | 节点面板勿暴露；或 `AddDiagnostic` |

### P3 — Editor / 运行时

| 风险 | 说明 | 建议 |
|------|------|------|
| **编译失败仍预览旧 shader** | `CommitCompileResult` 失败时是否保留上一版成功 shader | 明确 UX：显示 diagnostic，预览标「stale」 |
| **参数绑定** | `MaterialShaderParameterLayout` 与 CPU 上传路径 | 对照 `MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md` |
| **Masked Opacity 未接** | 仅 Warning，运行时 clip 可能无效 | Editor 高亮 Opacity pin |

### P4 — 测试缺口

| 缺失用例 | 优先级 |
|----------|--------|
| `MakeFloat3` → Normal（多输入 merge） | 中 |
| `Lerp` / `Divide` 多 use 纯表达式 | 中 |
| `IfThenElse` + Albedo | 高 |
| Masked + Opacity 图驱动 clip | 中 |
| 磁盘 `MaterialIRSmoke.memtl` round-trip + 改 Normal 连线 | 低 |

---

## 回归入口

```powershell
cd D:\Dev\GitRepo\minEngine\minEngine\bin
.\Editor.exe --material-ir-test
```

必过：`BlinnPhong` smoke、`Constant3→Normal`、`IfThenElse→Albedo`、`TextureSample` 双属性、`divide-by-zero` diagnostic、`capability struct`（Unlit + BlinnPhong）、`Unlit` prune smoke。

---

## 相关文档

- [MATERIAL-001](bugs/MATERIAL-001-constant3-normal-glsl-lowering.md)
- [MATERIAL_EDITOR_PLAN.md](MATERIAL_EDITOR_PLAN.md)
- [MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md](MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md)
