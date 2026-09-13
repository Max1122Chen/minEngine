# WF-F03 — Maximum Product Branding (display name) — Design Spec

## Meta
- **ID:** `WF-F03`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-13
- **Related:**
  - [ENGINE_0_1_0_ROADMAP.md](../../ENGINE_0_1_0_ROADMAP.md) Phase F3 · 验收 D8
  - [CORE-F18](../Serialization/CORE-F18_SCHEMA_ENGINE_VERSION_DESIGN.md)（**Done** — `GetEngineVersion().ToString()` 真源）
  - [BUG-EDITOR-003](../../bugs/BUG-EDITOR-003.md)（shipping 名改为 MaximumEditor.exe）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `CORE-F18` Done（软：版本数字）
- **Blocks:** 无；合入后 Phase F 可 fan-out

## TL;DR

对外**产品显示名**改为 **Maximum**；版本数字继续走 `GetEngineVersion()`（当前 **0.0.9**）。  
构建产物输出名为 **`MaximumEditor.exe`**（CMake target 仍为 `Editor`，`--target Editor` 不变；显示名仍为 Maximum）。  
**不**改仓库名、目录、C++ 命名空间。About 做最小可用模态。  
> **BUG-EDITOR-003：** 避免短名 `Maximum.exe`（本机曾与 GPU/OS 配置耦合导致最大化卡顿）。

## Scope

### In
- 单一真源：`ProductBranding`（显示名常量 + 可选 title 拼装辅助）
- Editor 窗口标题（`Editor::UpdateWindowTitle`）
- GLFW 初始窗口标题（启动闪现）
- CLI：`--help` app 名、`--version` 文案
- Help → About：最小模态（产品名 + 版本字符串）
- Vulkan `pApplicationName` / `pEngineName` 对齐显示名（驱动侧一致性）
- 构建产物 `OUTPUT_NAME MaximumEditor` → `bin/MaximumEditor.exe`；Launcher 默认解析同名（BUG-EDITOR-003）
- Docs / Progress 记 D8 部分完成（完整「0.1.0 打标」仍属 Phase D）

### Out
- 重命名 git 仓库 / 目录 `minEngine` / CMake **target** 名 `Editor` / 命名空间
- Launcher GUI Tauri `productName`（exe 解析已跟 Maximum；产品壳名可另开）
- 把 `kEngineVersion` 抬到 `0.1.0`（留给 Demo 打标；F3 只消费现有真源）
- 完整安装程序 / 商店 listing / 图标套件

## Reader quick start

1. 本文件 §契约  
2. 代码：`Editor.cpp` · `GLFWWindowSystem.cpp` · `ApplicationCommandLine.cpp` · `MainMenuWindow.cpp` · `VulkanRHI.cpp`  
3. 版本：`EngineVersion.h`

---

## 现状（代码）

| 表面 | 今天 | 文件 |
|------|------|------|
| 窗口标题 | `"minEngine Editor"` + 文档后缀 | `Editor::UpdateWindowTitle` |
| GLFW 初始标题 | `"minEngine Window"` | `GLFWWindowSystem` |
| CLI | `"minEngine Editor"` / `"minEngine " + version` | `ApplicationCommandLine` |
| About | MenuItem **disabled**，无对话框 | `MainMenuWindow::DrawHelpMenu` |
| Vulkan 名 | `"minEngine"` | `VulkanRHI` |
| 版本数字 | `GetEngineVersion()` → `0.0.9` | `EngineVersion.h` |

无 `ME_VERSION` CMake define；无独立产品名常量。

---

## 契约

### 显示名 vs 版本

| 概念 | 真源 | 例子（当前构建） |
|------|------|------------------|
| **产品显示名** | `kProductDisplayName` / `GetProductDisplayName()` | `"Maximum"` |
| **引擎版本** | `GetEngineVersion().ToString()` | `"0.0.9"` |
| **CLI --version** | `Product + " " + version` | `Maximum 0.0.9` |
| **About 一行** | 同上 | `Maximum 0.0.9` |
| **窗口标题基串** | `Product + " Editor"` | `Maximum Editor`（+ `- scene` / dirty） |

Roadmap D8 写「Maximum 0.1.0」指 **Phase D 打标时**的对外说法；F3 不偷偷改 `kEngineVersion`。

### API 草图

```cpp
// Runtime/Core/ProductBranding.h
namespace minEngine
{
    inline constexpr const char* kProductDisplayName = "Maximum";

    inline const char* GetProductDisplayName()
    {
        return kProductDisplayName;
    }

    // e.g. "Maximum Editor" or "Maximum Editor - Scene -*"
    std::string FormatEditorWindowTitle(const std::string& documentSuffix /* may be empty */);
}
```

`FormatEditorWindowTitle`：基串固定；`documentSuffix` 为空则仅基串，非空则 `" - " + suffix`（suffix 已含 dirty 标记时由调用方拼好）。

### About

- 启用 Help → About
- 最小 ImGui 模态：`Maximum` + `GetEngineVersion().ToString()`；可选一行「Engine identity: minEngine」（内部代号，避免误解改了仓库）
- 无链接、无许可证全文（Out）

---

## 切片

| ID | 内容 | 验证 |
|----|------|------|
| **S00** | `ProductBranding.h`；改标题 / CLI / GLFW 初始 / Vulkan 名 | 构建；`--version` 输出 Maximum |
| **S01** | About 最小模态 | 手动：Help → About |
| **S02** | Docs Done + Progress D8 注记 | Registry / ACTIVE_WORK |

S00+S01 可同一切合入。

---

## 验收

- [x] Editor 窗口标题以 **Maximum Editor** 开头  
- [x] `--version` → `Maximum 0.0.9`（随 `kEngineVersion`）  
- [x] Help → About 可用且展示产品名 + 版本  
- [x] 构建产物为 **`MaximumEditor.exe`**（CMake `--target Editor` 仍可用；非短名 `Maximum.exe`，见 BUG-EDITOR-003）  
- [x] 不改命名空间 / 仓库路径  
- [x] Progress 记录：D8 显示名 + 产物名完成；0.1.0 数字待 Phase D  

---

## 开放点（推荐默认）

| # | 问题 | **推荐** |
|---|------|----------|
| 1 | 标题基串 | **`Maximum Editor`**（非仅 `Maximum`） |
| 2 | 标题是否带版本号 | **否**（版本在 About / `--version`） |
| 3 | About | **做**最小模态 |
| 4 | Launcher | **Defer** |
| 5 | Vulkan `p*Name` | **改为 Maximum** |
| 6 | 本期抬版本到 0.1.0 | **否** |

---

## 风险

| 风险 | 缓解 |
|------|------|
| 文档/脚本仍写 minEngine | 预期；内部代号保留 |
| D8 文案与 0.0.9 不一致 | Progress 写明：显示名 F3，数字 Phase D |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-11 | Planned stub |
| 2026-09-12 | Design 充实；In Progress；锁定推荐默认 |
| 2026-09-12 | S00+S01 Done：ProductBranding + 标题/CLI/GLFW/Vulkan/About |
| 2026-09-12 | 增补：`OUTPUT_NAME Maximum` → `Maximum.exe`；Launcher/脚本对齐 |
| 2026-09-13 | BUG-EDITOR-003：改为 `MaximumEditor.exe`；显示名不变 |
