# Technical Debt Register

Last updated: 2026-06-11  
Purpose: explicit queue of **deferred or risky work** for Pre-flight and roadmap planning. Not a bug list — use [bugs/](./bugs/) for defects.

**Rules:** add a row when deferring non-trivial work; link Feature ID when known; do not delete rows — set Status `Done` or `Cancelled`.

| ID | Title | Module | Severity | Status | Feature / doc | Notes |
|----|-------|--------|----------|--------|---------------|-------|
| TD-001 | Scattered argv test flags, no unified CLI | TEST / Runtime | **High** | Done | `CLI-F01` | 2026-05-28: `ApplicationCommandLine` + `test` subcommand; legacy `--*-test` removed |
| TD-002 | No shared test runner / fixtures | TEST | **High** | Done | `TEST-F01` / `TEST-F02` | 2026-05-28: `TestRunner`, registry, `TestContext`; `minEngineTests.exe` |
| TD-003 | No `verify` script (build + smoke one command) | WF | **Medium** | Done | `TEST-F01` | 2026-05-28: `scripts/verify.ps1` → `minEngineTests test smoke` |
| TD-004 | Content Browser full tree rebuild on registry change | ED / ASSET | **Medium** | Open | [CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md](./Platform/ContentBrowser/CONTENT_BROWSER_REGISTRY_REFRESH_ISSUE.md) | Incremental registry OK; UI model rebuilds whole tree |
| TD-005 | P4 reflection docs lag implementation | CORE | **Medium** | Open | [REFLECTION_FUNCTIONS_CURRENT_STATE.md](./Platform/Reflection/REFLECTION_FUNCTIONS_CURRENT_STATE.md) | Code has invoke MVP; state doc still “no UFunction” — reconcile |
| TD-006 | Delegates + Lua not designed for implementation | CORE | **Low** | Deferred | P5/P6 placeholders | After CLI/Test stable |
| TD-007 | Render viewport refactor (multi-viewport) | RND | **Medium** | Deferred | [RENDER_REFACTOR_PLAN.md](./Render/RENDER_REFACTOR_PLAN.md) | Large; true refactor only with plan |
| TD-008 | Playground unmaintained / path hardcoding | Platform | **Low** | Deferred | PLATFORM_ROADMAP P0 tail | BUILD_PLAYGROUND off |
| TD-009 | Editor E1 Inspector unification | ED | **Medium** | Deferred | EDITOR_PLATFORM_PLAN E1 | Product; queue behind infra |
| TD-010 | GitHub Actions / remote CI | WF | **Low** | Deferred | After CLI+verify local | **CI = Continuous Integration**; separate from CLI |
| TD-011 | Post-commit context hook | WF | **Low** | Deferred | — | Optional; digest reduces need |
| TD-012 | Legacy doc IDs (Phase/M/E/P) vs F/S | WF | **Low** | Open | DOC_GOVERNANCE §10 | Migrate gradually; new docs use F/S only |
| TD-013 | `BuildSceneSet0` 每帧 `CreateBindingSet` | RND | **Medium** | Open | `RND-F04` · `EngineSceneBindingSets.cpp` | S04 已为 Set1 加脏标记 + SRV flyweight；Set0 仍每帧重建 |
| TD-014 | Material 纹理 SRV 未走 `RHITextureViewCache` | RND | **Medium** | Open | `RND-F04` · `Material.cpp` | compile/改纹理时 `CreateShaderResourceView`；可复用 flyweight |
| TD-015 | `EnvMapCapture` 旁路 draw + 引擎层 `OpenGLRHIShaderResourceView` | RND | **Medium** | Open | `RND-F03` §16.5 · `EnvMapCapture.cpp` | IBL 停用中；恢复时须迁 packet + 仅 `RHICreateShaderResourceView` |
| TD-016 | `RHIGraphicsPSOStateFallback` / GL Apply 子集 | RND | **Low** | Open | `RND-F03` §15.4 P1 | cull/raster/RT format 未全进 Apply；F05 VK 前对齐 |
| TD-017 | `RenderSystem` `static_cast<OpenGLRHI*>` 窗口 clear | RND | **Low** | Open | `RenderSystem.cpp` | 应收敛为 Pass 或 RHI 中性入口 |
| TD-018 | `ShaderResource` 反射 / ContentBrowser 残留 | ASSET / RND | **Low** | Open | `RND-F03` §15.4 P2 | `Shader` Asset 已删；反射类型仍注册 |
| TD-019 | `ShadowTypes` GL texture unit 常量与 layout 双轨 | RND | **Low** | Open | `ShadowTypes.h` · `EngineShaderBindings` | 逻辑 set 已有；unit 常量仍散落 |

---

## Adding a row

```text
| TD-0nn | Short title | DOMAIN | High/Med/Low | Open/Deferred/Done | Feature or link | One line why deferred |
```

When paid down: set Status `Done`, add date in Notes, optional Progress log entry.
