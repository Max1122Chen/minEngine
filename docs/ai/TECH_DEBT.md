# Technical Debt Register

Last updated: 2026-09-02（CORE-F06 load reconcile；TD-026 Open）

Purpose: explicit queue of **deferred or risky work** for Pre-flight and roadmap planning. Not a bug list — use [bugs/](./bugs/) for defects.

**Rules:** add a row when deferring non-trivial work; link Feature ID when known; do not delete rows — set Status `Done` or `Cancelled`.

| ID | Title | Module | Severity | Status | Feature / doc | Notes |
|----|-------|--------|----------|--------|---------------|-------|
| TD-001 | Scattered argv test flags, no unified CLI | TEST / Runtime | **High** | Done | `CLI-F01` | 2026-05-28: `ApplicationCommandLine` + `test` subcommand; legacy `--*-test` removed |
| TD-002 | No shared test runner / fixtures | TEST | **High** | Done | `TEST-F01` / `TEST-F02` | 2026-05-28: `TestRunner`, registry, `TestContext`; `minEngineTests.exe` |
| TD-003 | No `verify` script (build + smoke one command) | WF | **Medium** | Done | `TEST-F01` | 2026-05-28: `scripts/verify.ps1` → `minEngineTests test smoke` |
| TD-004 | Content Browser full tree rebuild on registry change | ED / ASSET | **Medium** | Open | [CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md](./Platform/ContentBrowser/CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md) | Incremental registry OK; UI model rebuilds whole tree |
| TD-005 | P4 reflection docs lag implementation | CORE | **Medium** | Open | [REFLECTION_FUNCTIONS_CURRENT_STATE.md](./Platform/Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md) | Code has invoke MVP; state doc still “no UFunction” — reconcile |
| TD-006 | Delegates + Lua not designed for implementation | CORE | **Medium** | Done | **CORE-F04** | **2026-08-04：** Native multicast MVP 落地（`Runtime/Core/Delegates/`，`test delegates`）。Dynamic/反射/Lua 正式绑定不在本期；另开 Feature。PHYS-F03 可重开。 |
| TD-007 | Render viewport refactor (multi-viewport) | RND | **Medium** | Deferred | [RENDER_REFACTOR_PLAN.md](./Render/RENDER_REFACTOR_PLAN.md) | Large; true refactor only with plan |
| TD-008 | Playground unmaintained / path hardcoding | Platform | **Low** | Deferred | PLATFORM_ROADMAP P0 tail | BUILD_PLAYGROUND off |
| TD-009 | Editor E1 Inspector unification | ED | **Medium** | Deferred | EDITOR_PLATFORM_PLAN E1 | Product; queue behind infra |
| TD-010 | GitHub Actions / remote CI | WF | **Low** | Deferred | After CLI+verify local | **CI = Continuous Integration**; separate from CLI |
| TD-011 | Post-commit context hook | WF | **Low** | Deferred | — | Optional; digest reduces need |
| TD-012 | Legacy doc IDs (Phase/M/E/P) vs F/S | WF | **Low** | Open | DOC_GOVERNANCE §10 | Migrate gradually; new docs use F/S only |
| TD-013 | Enum property codec reads/writes as int64 | CORE / Serialization | **High** | Done | 2026-08-03 master | Fixed: `SetCodecForEnums` uses `MEEnum::GetSize()` for storage load/store; wire still int64. Smoke: `serialization-archive` uint8 enum neighbor test. Re-save scenes that were corrupted by old codec if needed. |
| TD-014 | Material 纹理 SRV 未走 `RHITextureViewCache` | RND | **Medium** | Done | `RND-F09` · `Material.cpp` | 2026-08-03 F09-S02（原 render 轨编号；与 CORE TD-013 不同号段冲突已用并集保留 CORE 为 TD-013） |
| TD-015 | `EnvMapCapture` 旁路 draw + 引擎层 `OpenGLRHIShaderResourceView` | RND | **Medium** | Done | `RND-F10` · `EnvMapCapture.cpp` | 2026-08-03：`CreateShaderResourceView` + `RHICmdGenerateMips`；cube `NumMips` 分配在 OpenGL 后端；Capture 无 glad |
| TD-016 | `RHIGraphicsPSOStateFallback` / GL Apply 子集 | RND | **Low** | Done | `RND-F09` · `OpenGLRHI.cpp` | 2026-08-03 F09-S04；blend 因子固定 alpha，尚未 desc 驱动 |
| TD-017 | `RenderSystem` `static_cast<OpenGLRHI*>` 窗口 clear | RND | **Low** | Done | `RND-F09` · `RenderSystem.cpp` | 2026-08-03 F09-S03 |
| TD-018 | `ShaderResource` 反射 / ContentBrowser 残留 | ASSET / RND | **Low** | Done | `RND-F09` · `RND-F03` §15.4 P2 | 2026-08-03 F09-S05：删除类与 gen；CB 去 Shader 图标 |
| TD-019 | `ShadowTypes` GL texture unit 常量与 layout 双轨 | RND | **Low** | Done | `RND-F09` · `ShadowTypes.h` · `EngineShaderBindings` | 2026-08-03 F09-S06；unit 仅 Bindings |
| TD-020 | Shadow atlas 仍由 `ShadowResourceManager` 分配 | RND | **Medium** | Done | `RND-F08` | 2026-08-02：图拥有 Dir/Spot/Point；Manager 已删 |
| TD-021 | EnvironmentMap Editor/CLI Bake 按钮 + 可选写回磁盘 | RND / ED | **Low** | Open | `RND-F10` S06 | 运行时 EnsureGPU bake 已够用；显式 Bake UX / face PNG 落盘后置 |
| TD-022 | `BuildSceneSet0` 每帧 `CreateBindingSet`（原 render TD-013） | RND | **Medium** | Done | `RND-F09` · `EngineSceneBindingSets.cpp` | 2026-08-03 F09-S01：脏标记。合入 master 时与 CORE enum TD-013 撞号，改记为 TD-022 Done |
| TD-023 | Scene pass ordering / clear contract still fragile after VK smoke | RND / ForwardRenderer | **Medium** | Open | `RND-F05` S07d / `ED-F01` | BasePass clears only when Sky off; Sky `NeedRenderPass` must still enter to clear when draw prep fails. 2026-08-25: fixed `NeedRenderPass`→`m_ShouldEnterPass` + Vulkan `LoadEngineRenderingAssets` (was OpenGL-only). Broader ordering still fragile. |
| TD-024 | Vulkan frame sync leftovers after S07d smoke | VulkanRHI | **Medium** | Open | `RND-F05` S07d / `ED-F01` | Present semaphore reuse still triggers validation on fast shutdown; `RHICmdGenerateMips()` remains VK no-op. 2026-08-25: removed S07d DrawIndexed diagnostic logs; HDR bake DEVICE_LOST fixed (immediate submit before PSO destroy + cube layout defer). |
| TD-025 | Clip-space / texture-origin policy hardcoded to `IsVulkan()` | RND / RHI | **Medium** | Done | [RND-TD025](./Render/RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md) · ED-F01 | 2026-08-28: `RHIClipSpaceCapabilities` + shadow scheme A landed；2026-08-31 user visual verify with RND-F14 batch |
| TD-026 | Scene deserialize `m_Owner` bypasses `Component::SetOwner` | CORE / Serialization | **Medium** | Open | `CORE-F06` · [Design](./Platform/Core/CORE-F06_COMPONENT_ENABLE_DESIGN.md) | 2026-09-02: `ResolvePendingActivationsForScene` calls `SyncActivationWithActiveFlag` as load reconcile（首 Deactivate 失效 workaround）。**根治：** pending ref resolve 后对 `m_Owner` 走反射 Setter / `SetOwner`，与 `AddComponent_Internal` 一致；见 Serializer `ResolvePendingObjectRefs` raw ptr assign。 |

---

## Adding a row

```text
| TD-0nn | Short title | DOMAIN | High/Med/Low | Open/Deferred/Done | Feature or link | One line why deferred |
```

When paid down: set Status `Done`, add date in Notes, optional Progress log entry.
