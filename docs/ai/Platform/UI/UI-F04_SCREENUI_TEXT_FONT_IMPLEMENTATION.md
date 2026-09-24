# UI-F04 — ScreenUI Text / Font — Implementation Plan

## Meta
- **ID:** UI-F04
- **Status:** Done
- **Last updated:** 2026-09-24
- **Related:** [Design](./UI-F04_SCREENUI_TEXT_FONT_DESIGN.md)
- **Readiness:** Ready with deferred（Design Review 2026-09-23）

## Slices

| Slice | Goal | Done when |
|-------|------|-----------|
| S01 | FontGlyphAtlas + FreeType smoke（无 Scene） | **Done** screen-ui-text |
| S02 | TextComponent + Widget 优先 Text 填 proxy | **Done** |
| S03 | BuildScreenUIQueue 局部像素 VB + 绘制 | **Done**（目视 PASS） |
| S04 | Docs / Registry Done | **Done** |

## Locks
见 Design §Implementation locks。

## Verification notes

- minEngineTests.exe test screen-ui-text PASS
- MCP accept_ui_text.ts + Editor hand-check: Content commit-on-blur safe; Font Assign refreshes immediately
- ScreenUIPass shared with Image path
