# ED-F01 — Implementation Plan

## Meta
- **ID:** `ED-F01`
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-08-17
- **Related:** [Design](./ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md)

## TL;DR

在 `RND-F05` S07d 之上，**先 Editor 壳 + ImGui-Vulkan + 统一主循环**，再 **scene viewport + navigation**，最后在同一 Editor 完成 **shadow / sky / IBL**（承接 F05 S07e/f）。共 **8+ 切片**；第一刀 **S01–S02**（backend + 空 UI 帧）。

## Scope
- **In:** Design §Scope In；切片 S01–S08（S09+ 可选）。
- **Out:** ImGui multi-viewport OS 窗口；F11 DebugDrawing；公共 RHI 同步 API。

## Reader quick start
1. [Design](./ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) — 帧顺序与模块边界。
2. 下表 — 切片顺序。
3. 同级 `../imgui/backends/imgui_impl_vulkan.*` — backend 源。

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `ED-F01-S01` | 引入 `imgui_impl_vulkan` + CMake | Planned | VK 配置编过 Editor |
| `ED-F01-S02` | `EditorImGuiBackend` + `VulkanRHI` Editor 桥接；空 ImGui 帧 | Planned | `--rhi vulkan` dock UI 可交互；validation 无 fatal |
| `ED-F01-S03` | 统一 Editor 主循环；删除 smoke 分叉 | Planned | VK/GL 同一 `Run()`；无 `m_VulkanSceneSmokeMode` |
| `ED-F01-S04` | Scene viewport 离屏 RT + `EditorRHIImGuiTexture` | Planned | ImGui 内见 scene color；`PresentPass` off |
| `ED-F01-S05` | `SceneEditingViewportClient` navigation + 选择/gizmo | Planned | orbit/fly 与 GL 一致 |
| `ED-F01-S06` | Shadow + 场景 shadows `set=`（原 F05-S07e） | Planned | VK viewport 阴影可辨；GL 回归 |
| `ED-F01-S07` | Sky / IBL / EnvMapCapture（原 F05-S07f） | Planned | VK 天空/IBL 目视；GL 回归 |
| `ED-F01-S08` | Material viewport + 其他 ImGui 纹理消费者 | Planned | Material 编辑器 VK 预览可用 |
| `ED-F01-S09+` | 缩略图、Inspector 预览等 | Deferred | 按 backlog 单开子切片 |

---

## 2) 切片详情

### ED-F01-S01 — `imgui_impl_vulkan` 与构建

- **Goal:** Third-Party 具备 Vulkan ImGui backend；Editor 链入。
- **Touch:**
  - 从 `../imgui/backends/` 复制 `imgui_impl_vulkan.h` / `imgui_impl_vulkan.cpp` → `minEngine/Third-Party/imgui/backends/`
  - `minEngine/minEngine/CMakeLists.txt`（或 Editor CMake）：`MINENGINE_HAS_VULKAN` 时编译 vulkan backend
  - 确认 `imconfig.h` / 编译 flag 与 F05 Vulkan loader 策略一致
- **DoD:**
  - [ ] backend 文件版本与 ImGui core **1.92.7** 一致
  - [ ] `--rhi vulkan` 链接通过（本切片可不跑 UI）
  - [ ] `--rhi opengl` 不受影响
- **Verify:** 配置 + 编译 Editor（Debug）；`minEngineTests.exe --rhi vulkan test smoke` 仍 PASS

### ED-F01-S02 — ImGui-Vulkan 初始化与空 UI 帧

- **Goal:** Vulkan 下 ImGui 能 NewFrame/Render 到 swapchain。
- **Touch:**
  - 新建 `Editor/src/Platform/EditorImGuiBackend.{h,cpp}`（或同等路径）
  - `Render/Vulkan/VulkanRHIEditorBridge.{h,cpp}`（或 `VulkanRHI` 内 `#if` Editor-only 友元）：暴露 `ImGui_ImplVulkan_InitInfo` 所需句柄、swapchain render pass、image count
  - `Editor.cpp`：Vulkan 路径 init/shutdown ImGui（**暂可**保留 smoke 循环并行，便于本刀对照）
- **DoD:**
  - [ ] `ImGui_ImplGlfw_InitForVulkan` + `ImGui_ImplVulkan_Init` 成功
  - [ ] 空 dock + 菜单可显示、可点击
  - [ ] `VK_LAYER_KHRONOS_validation` 无 fatal（resize 窗口至少测一次）
  - [ ] Font atlas / theme 与 GL 路径共用 `EditorAppearance` 流程
- **Verify:** `Editor.exe --rhi vulkan --project …\MyMEProject.meproject` — 空 UI；日志无 init 失败

### ED-F01-S03 — 统一 Editor 主循环（删 smoke）

- **Goal:** 删除 `m_VulkanSceneSmokeMode`、独立 Run 循环、`m_VulkanSmokeViewport`。
- **Touch:**
  - `Editor.cpp` / `Editor.h`
  - `OpenProjectForVulkanSmoke` → 合并进 `OpenProject`（Unlit 强制等特殊逻辑 **评估删除或改为项目设置**）
  - `SetPresentPassEnabled(false)`：**Vulkan Editor 与 GL 一致**
- **DoD:**
  - [ ] 单一 `Run()` 循环；GL/VK 仅 ImGui backend 分支
  - [ ] 删除 smoke 专用成员与 early-return Initialize
  - [ ] `RegisterModules` / `SceneEditor` / InputHub 在 VK 下启用
  - [ ] 文档：Design §3.8 smoke 策略更新
- **Verify:** VK Editor 可打开 Scene 模块窗口（scene 可仍黑/占位）；GL Editor 回归；`test smoke` 双后端 PASS

### ED-F01-S04 — Scene viewport 离屏 RT + ImGui 纹理

- **Goal:** 主 scene viewport 在 ImGui 内显示 Forward 输出（Unlit Base 即可本刀）。
- **Touch:**
  - `Editor/src/Platform/EditorRHIImGuiTexture.{h,cpp}`
  - `EditorViewportWindow.cpp`：`GetRHINativeTextureHandle` → 桥接 API
  - `VulkanRHITexture`：Editor 桥接所需 image view / layout（`SHADER_READ_ONLY`）
  - `SceneEditingViewportClient::EndFrame`：确认 flags **不含** `PresentToBackBuffer`
  - `ForwardRenderer` / `PresentPass`：Editor VK 不 blit 全屏
- **DoD:**
  - [ ] ImGui 内可见 scene mesh（Unlit 可接受）
  - [ ] Viewport resize 后纹理仍正确（无 stale descriptor）
  - [ ] `PresentPass` 在 Editor VK 关闭
- **Verify:** 人工目视 VK viewport mesh；GL viewport 仍正常

### ED-F01-S05 — Navigation + 视口输入

- **Goal:** `SceneEditingViewportClient` 在 VK 下 orbit/fly、focus/hover 与 GL 一致。
- **Touch:**
  - 通常 **无 RHI 改动**；确认 `InputHub`、`EditorViewportWindow` frame state 在 VK 循环已接通
  - 相机 aspect 随 viewport buffer size 更新
- **DoD:**
  - [ ] 右键 drag look、WASD move、滚轮调速
  - [ ] Cursor capture / visible 与 GL 一致
  - [ ] Gizmo / 选择本刀可选：至少 **navigation 必须绿**
- **Verify:** VK vs GL 对照同一 `default.mescene`

### ED-F01-S06 — Shadow（原 RND-F05-S07e）

- **Goal:** ShadowPass VK 产出 shadow map；场景 includes `set=`；Lit 材质 VK 可采样阴影。
- **Touch:** `ShadowPass.cpp`；`MaterialSceneShadows.glslinc` 等；`EngineSceneBindingSets` — **沿用 F05 Impl S07e 清单**
- **DoD:** 同原 S07e DoD；验收改为 **VK Editor viewport**
- **Verify:** GL Editor 阴影对照；VK Editor viewport；`test material-ir`

### ED-F01-S07 — Sky / IBL / Bake（原 RND-F05-S07f）

- **Goal:** SkyBoxPass、IBL、EnvMapCapture 在 VK Editor 热路径可用。
- **Touch:** `SkyBoxPass`、`EnvMapCapture`、`RHICmdGenerateMips` VK 实现等 — **沿用 F05 Impl S07f 清单**
- **DoD:** 同原 S07f DoD；付清 `TD-023` / `TD-024` 中与本刀相关的项（自然触达则修）
- **Verify:** GL Editor 全功能；VK 天空/IBL 目视

### ED-F01-S08 — Material viewport 与其他 ImGui 纹理

- **Goal:** Material 编辑器 preview viewport 在 VK 可用。
- **Touch:** `MaterialEditorViewportWindow`；`EditorRHIImGuiTexture` 复用
- **DoD:**
  - [ ] Material preview 在 VK ImGui 内显示
  - [ ] GL 回归
- **Verify:** 打开 Material 编辑器对照 GL/VK

### ED-F01-S09+ — 缩略图 / Inspector 预览（Deferred）

- **Goal:** `AssetThumbnailService`、`InspectorPreviewPresenter` 等迁移到 `EditorRHIImGuiTexture`。
- **Unblock:** S04 桥接稳定 + descriptor 池策略文档化。

---

## 3) 依赖顺序

```text
S01 (backend)
  └─► S02 (ImGui init + 空 UI)
        └─► S03 (统一循环，删 smoke)
              └─► S04 (viewport RT)
                    └─► S05 (navigation)
                          ├─► S06 (shadow)
                          │     └─► S07 (sky/IBL)
                          └─► S08 (material viewport)
                                └─► S09+ (thumbnails…)
```

- **GL 回归：** 每刀 `minEngineTests.exe --rhi opengl test smoke` + 至少一次 Editor GL 目视。
- **禁止：** 在 S03 之前做 S06/S07（shadow/sky 需要在真实 viewport 环境验收）。

---

## 4) 延后 / 取消

| Slice ID | Reason | Unblock | Next check |
|----------|--------|---------|------------|
| `RND-F05-S07e` | 迁至 ED-F01-S06 | ED-F01-S05 Done | Registry |
| `RND-F05-S07f` | 迁至 ED-F01-S07 | ED-F01-S06 Done | Registry |
| `ED-F01-S09+` | 非阻塞主视口 parity | S08 Done | ACTIVE_WORK |

---

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-17 | 初稿；承接 F05 S07e/f；S01–S08 切片 |
