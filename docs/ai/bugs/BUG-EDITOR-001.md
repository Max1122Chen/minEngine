# BUG-EDITOR-001 — Editor intermittent crash after UI font atlas rebuild

## Meta
- **ID:** BUG-EDITOR-001
- **Status:** Fixed (pending visual verify)
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-09-01
- **Last updated:** 2026-09-01
- **Affects:** Editor startup (OpenGL ImGui path); font atlas rebuild / CJK toggle
- **Related Feature/Slice:** `EditorAppearance`, `EditorImGuiBackend` (M5 typography)

## TL;DR
启动日志停在 `EditorAppearance: UI font atlas rebuilt` 后偶发崩溃。根因是 ImGui 1.92 动态字体纹理路径下，atlas 重建后未通知渲染后端丢弃过期 GPU 资源；另在 `RendererHasTextures` 下不应再手动 `io.Fonts->Build()`。

---

## 症状
- Editor 启动至 `MINENGINE: EditorAppearance: UI font atlas rebuilt (...)` 后**偶发**直接退出（无后续日志）。
- 日志位置在 `Editor::Initialize()` 末尾，实际崩溃更可能发生在紧接着的 `Editor::Run()` 首帧（ImGui `NewFrame` / 首绘 / `RenderDrawData`）。

## 期望
字体 atlas 重建后，下一帧 ImGui 使用与新 atlas 一致的 GPU 纹理；启动与切换 CJK 字体均稳定。

## 复现
1. `Editor.exe --rhi opengl --project <MyMEProject.meproject>`
2. 多次冷启动；或在运行中切换 **Enable CJK Glyphs**（`EditorAppearance::SetCjkGlyphsEnabled`）
3. 间歇性；本地 20+ 次批量启动未稳定复现

## 环境
`master`；ImGui 1.92 + `ImGuiBackendFlags_RendererHasTextures`；OpenGL 为主验证路径。

## 根因
1. **`EditorImGuiBackend::NotifyFontAtlasRebuilt()` 为空实现且从未被调用** — `RebuildUiFontAtlas()` 执行 `io.Fonts->Clear()` 后，OpenGL 后端 `PlatformIO.Textures` 中可能仍持有旧 TexID。
2. **注释误导** — `EditorAppearance.cpp` 假定「下一帧 OpenGL3 会自动重建」，但未走官方 device-object 失效路径。
3. **ImGui 1.92 动态 atlas** — 在 `RendererHasTextures` 已开启时不应再调用 legacy `io.Fonts->Build()`（由 `NewFrame` 懒加载字形）。

## 修复
- `EditorAppearance::FinalizeFontAtlasBuild()`：仅在非 `RendererHasTextures` 时 `Build()`；结束后调用 `NotifyFontAtlasRebuilt()`。
- `Editor::Initialize`：`m_Appearance.SetImGuiBackend(&m_ImGuiBackend)`。
- `EditorImGuiBackend::NotifyFontAtlasRebuilt()`（OpenGL）：`ImGui_ImplOpenGL3_DestroyDeviceObjects()`，下一 `NewFrame` 重建 shader/纹理。

**代码：** `EditorAppearance.*`, `EditorImGuiBackend.cpp`, `Editor.cpp`

## 回归验证
- [ ] Editor 冷启动 10+ 次无崩溃（待你确认）
- [ ] 菜单切换 CJK glyphs 后 UI 正常
- [x] `cmake --build ... --target Editor` 通过

## 关联
- [FONT_ASSET_DESIGN.md](../Editor/FONT_ASSET_DESIGN.md) §7 `RebuildUiFontAtlas`
- ImGui 1.92 `ImGui_ImplOpenGL3_DestroyDeviceObjects` / dynamic `ImTextureData` 生命周期

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 登记；根因 + 修复落地（未单独 commit，与 BUG-RENDER-014 工作区并存） |
