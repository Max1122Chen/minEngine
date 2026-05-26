# Asset Pipeline — P4 接口定稿（审批用）

Last updated: 2026-05-26  
Status: **已实现**（`0f96c45`）  
前置：**P2** `AssetManager` CRUD/events（`7758c60`）、**P3** Runtime Platform `FileDialogService`（`da69b7b`）  
父文档：[ASSET_PIPELINE_DESIGN.md](./ASSET_PIPELINE_DESIGN.md) §7、§9 P4  
P3 定稿：[ASSET_PIPELINE_P3_API.md](./ASSET_PIPELINE_P3_API.md)

---

## 1) P4 交付边界

### 1.1 包含

| 项 | 说明 |
|----|------|
| **编排入口** | `AssetWorkflowModule::ImportAssetDialog()` |
| **菜单入口** | `MainMenuWindow` 增加 `Import Asset...`（v0） |
| **对话框** | 使用 Runtime `IFileDialogService`（NFD）选择源文件（支持多选） |
| **导入执行** | 对每个选中的源文件调用 `AssetManager::ImportAsset(sourcePath, destDirectory)` |
| **目标目录策略 v0** | 导入到固定目录：`<ProjectContentRoot>/Imported/`（不存在则创建） |
| **日志与汇总** | 成功/失败逐条日志 + 结束 summary（success/fail 计数） |

### 1.2 不包含（P5+ / P6+）

- Content Browser 当前目录导入（P6 接入选中路径后再做）
- Delete/Move/Rename 的 UI/选择联动（P4.1 或 P6）
- Undo/Redo 命令封装（后续与 CommandStack 统一）
- 覆盖文件时的 UI 决策（仍由 `ImportAsset` 返回失败；v0 只展示错误）
- 自动 Open 导入资产（v0 默认不自动打开；可后续加开关）

---

## 2) 公开 API 定稿

### 2.1 `AssetWorkflowModule`

```cpp
class AssetWorkflowModule : public EditorServiceModule
{
public:
    bool OpenAsset(const AssetMeta& meta); // 已有

    void ImportAssetDialog();              // P4 新增
};
```

---

## 3) `ImportAssetDialog` 流程（v0 固定导入目录）

```text
1) 构造 FileDialogRequest
   - Title = "Import Assets"
   - Filters = AssetTypeRegistry::BuildFileDialogFilters()
   - bAllowMultiple = true
   - InitialDirectory = ProjectContentRoot（若为空则留空）

2) 调用 IFileDialogService::OpenFiles(request)
   - Cancel/空路径：return

3) destDirectory = ProjectContentRoot / "Imported"
   - create_directories(destDirectory)
   - 若 ProjectContentRoot 为空：日志 error，return

4) 对每个 sourcePath：
     ImportAssetResult r = AssetManager::Get().ImportAsset(sourcePath, destDirectory);
     - r.bSuccess: log imported + r.Meta.AssetPath
     - else: log error + r.ErrorMessage

5) summary log：N succeeded, M failed
```

**注意：** `AssetManager::ImportAsset` 已负责：
- 校验扩展名支持（由 `AssetTypeRegistry`）
- 目标文件已存在则失败（与 Move 一致）
- 复制完成后 `RegisterAsset` 并发 `Registered`

---

## 4) 验收标准（P4）

| # | 检查 |
|---|------|
| 1 | 菜单 `Import Asset...` 可打开系统对话框（支持多选） |
| 2 | 选择 `*.png` / `*.obj` 等支持文件后导入到 `Assets/Imported/`，并生成 `.meta` |
| 3 | 导入成功触发 Registry `Registered`（Browser 未实现前可用日志/后续工具验证） |
| 4 | 不支持扩展名：导入失败但 Editor 不崩溃，错误信息可见 |

---

## 5) 预计修改文件

| 文件 | 动作 |
|------|------|
| `Editor/src/Services/AssetWorkflowModule.{h,cpp}` | 新增 `ImportAssetDialog` |
| `Editor/src/UI/EditorWindows/MainMenuWindow.cpp` | 增加 `Import Asset...` 菜单项调用 |
| `docs/ai/PROGRESS_LOG.md` | P4 完成后追加一条（完成时写） |

---

## 6) 审批清单

- [ ] **A.** 同意 P4 范围（Import 对话框 + 固定导入目录 + 日志汇总）
- [ ] **B.** 同意 v0 导入目录固定为 `Assets/Imported/`（P6 再接入“当前目录”）
- [ ] **C.** 同意 v0 不自动 Open 导入资产

已实现；P5 见 [ASSET_PIPELINE_P5_API.md](./ASSET_PIPELINE_P5_API.md)。

