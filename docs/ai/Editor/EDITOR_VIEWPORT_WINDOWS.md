# Editor 视口窗口分层设计

> 状态：已拍板（2026-05）。与 [RENDER_REFACTOR_PLAN.md](../Render/RENDER_REFACTOR_PLAN.md) 中 P2b / P3 / P5 衔接。  
> 命名原则：**不混用 Level 与 Scene**；关卡编辑视口统一用 **SceneEditing** 前缀。

---

## 1. 拍板结论

| 项 | 决定 |
|----|------|
| UI 基类名 | **`EditorViewportWindow`**（继承 `EditorWindow`） |
| 关卡编辑视口窗口 | **`SceneEditingViewportWindow`**（由现 `ViewportWindow` 改名） |
| 关卡编辑视口 Client | **`SceneEditingViewportClient`**（由现 `EditorViewportClient` 改名） |
| 材质预览视口窗口 | **`MaterialPreviewViewportWindow`**（后续 Material 编辑器） |
| 材质预览 Client | **`MaterialPreviewViewportClient`** |

---

## 2. 为什么要分层

现 `ViewportWindow` 同时承担：

- 通用 ImGui RT 显示 + `ViewportFrameState`
- 关卡专属：Gizmo、FPS Overlay、绑定 `SceneManager` 主 RT
- 与 `EditorViewportClient` 的飞行/拾取/Gizmo 消费

增加 Material Preview 等视口后，**语义与依赖都会膨胀**。应对齐 UE：**面板壳（Window）** 与 **视口行为（Client）** 分离，并按视口种类派生。

---

## 3. 分层结构

```text
EditorWindow
└── EditorViewportWindow              // 凡显示 SceneRenderTarget 的 Dock 面板
        ├── SceneEditingViewportWindow    // 关卡场景编辑（Gizmo、场景 Overlay、拾取）
        └── MaterialPreviewViewportWindow // 材质预览（无 Gizmo、独立 Preview Scene）

EditorViewportClient                  // 帧状态、RT 缩放、EndFrame 提交钩子（P3 起持 SceneViewport）
├── SceneEditingViewportClient        // 飞行相机、拾取、Gizmo 消费、观察当前关卡 RenderScene
└── MaterialPreviewViewportClient     // 轨道/固定相机、观察 Preview RenderScene、第二条 Submit
```

Runtime（已有 / 计划）：

```text
SceneViewport                    // 每个 Editor Client 拥有一份（P3+）
MaterialPreviewViewport          // P5：独立 RenderScene + 小 RT（可选专用类型或配置）
```

---

## 4. 职责矩阵

| 能力 | `EditorViewportWindow` | `SceneEditingViewportWindow` | `MaterialPreviewViewportWindow` |
|------|------------------------|------------------------------|--------------------------------|
| `ImGui::Image` RT | 基类 | 继承 | 继承 |
| `ViewportFrameState` | 基类 | 继承 | 继承 |
| Gizmo | — | **有** | 无 |
| 可拖拽 Overlay（FPS 等） | 可选钩子 | **有** | 精简（材质/编译状态） |
| 显示纹理来源 | 虚方法 / Client | Client `SceneViewport` | Preview `SceneViewport` |
| 绑定 Hierarchy 选中 | — | **是** | 否（仅材质预览对象） |

| 能力 | `EditorViewportClient` | `SceneEditingViewportClient` | `MaterialPreviewViewportClient` |
|------|------------------------|------------------------------|--------------------------------|
| `SyncRenderTargetSize` | 基类 | 继承 | 继承 |
| `SubmitSceneDraw`（P3） | `EndFrame` | 当前关卡 desc | Preview desc（可关 Shadow） |
| WASD / Look | — | **有** | 可选轨道 |
| 拾取 GameObject | — | **有** | 无 |
| `ConsumeGizmoManipulation` | — | **有** | 无 |

---

## 5. `EditorViewportWindow` API 草案

```cpp
class EditorViewportWindow : public EditorWindow
{
public:
    void OnDraw() override final;

protected:
    virtual EditorViewportClient& GetOrCreateViewportClient() = 0;
    virtual const std::shared_ptr<RHITexture2D>& GetDisplayColorTexture() const;
    virtual ImGuiWindowFlags GetViewportWindowFlags() const;

    // 子类扩展：Overlay、Gizmo 等画在 RT 之上
    virtual void OnDrawViewportOverlay(EditorViewportClient& client,
                                       const ViewportFrameState& frameState) {}

private:
    void DrawSceneColorImage(ViewportFrameState& outFrameState);
};
```

**固定 `OnDraw` 流程：**

1. `client.BeginFrame(delta)`
2. `ImGui::Begin(title)`
3. 基类 `DrawSceneColorImage` → `client.UpdateFrameState`
4. `OnDrawViewportOverlay`（`SceneEditing` 画 Overlay + Gizmo）
5. `ImGui::End`
6. `client.EndFrame()`（P3：缩放 RT + `SubmitSceneDraw`）

---

## 6. `Editor` 注册与 ID

- Window `GetId()` 与 `Editor::m_ViewportClients` 的 key **一致**。
- 建议默认关卡视口 id：`"scene_editing_viewport"`（由现 `"viewport"` 迁移时可保留别名一帧或一次性改）。
- 材质预览：`"material_preview_viewport"`。

```cpp
// EditorGUIManager 初始化示例
RegisterWindow(std::make_unique<SceneEditingViewportWindow>(editor));
// Material 编辑器打开时再 Register MaterialPreviewViewportWindow
```

Client 工厂可保留 map，或按 kind 创建：

```cpp
enum class EditorViewportKind { SceneEditing, MaterialPreview };
std::unique_ptr<EditorViewportClient> CreateViewportClient(EditorViewportKind kind, ...);
```

---

## 7. 与渲染重构阶段的衔接

| 阶段 | Editor UI | Editor Client | Runtime / Submit |
|------|-----------|---------------|------------------|
| **P2（已完成）** | 仍 `ViewportWindow` | 仍单例式 `EditorViewportClient` | `SceneManager::SubmitMainEditorViewportDraw` |
| **P2b（本设计）** | 抽 `EditorViewportWindow`；`ViewportWindow` → **`SceneEditingViewportWindow`**；Client → **`SceneEditingViewportClient`** + 基类 | 行为不变，仅改名与文件拆分 | 不变 |
| **P3** | `GetDisplayColorTexture()` 走 Client 的 `SceneViewport` | **`SceneEditingViewportClient` 拥有 `SceneViewport`**；`EndFrame` Submit | 从 `SceneManager` 移除主视口代持 |
| **P5** | 新增 **`MaterialPreviewViewportWindow`** | **`MaterialPreviewViewportClient`** + 第二条 Submit | Preview `RenderScene` + 独立 RT |

**依赖建议：** P2b 可与 P3 **并行**；若先做 P2b，P3 改接线时窗口类名已稳定。Material 窗口在 P5 做，依赖 P3 的 Client 持 `SceneViewport` 模式。

---

## 8. 目录结构（目标）

```text
Editor/src/UI/EditorWindows/
  EditorWindow.h
  EditorViewportWindow.h / .cpp
  SceneEditingViewportWindow.h / .cpp    # 替代 ViewportWindow.*
  MaterialPreviewViewportWindow.h / .cpp # P5

Editor/src/Viewport/
  EditorViewportClient.h / .cpp
  SceneEditingViewportClient.h / .cpp     # 替代 EditorViewportClient.*
  MaterialPreviewViewportClient.h / .cpp # P5
```

---

## 9. P2b 实施步骤（低风险）

1. 新增 `EditorViewportClient` 基类：上移帧状态、`Begin/EndFrame`、`SyncRenderTargetSize` 声明。
2. `EditorViewportClient` 实现类改名为 **`SceneEditingViewportClient`**（逻辑暂不改）。
3. 新增 `EditorViewportWindow` 基类：抽出 `DrawSceneColorImage` + 固定 `OnDraw` 流程。
4. `ViewportWindow` 改名为 **`SceneEditingViewportWindow`**；Gizmo / Overlay 留在子类。
5. 更新 `Editor.h` / `Editor.cpp` / `EditorGUIManager` 类型与 include；默认注册 `SceneEditingViewportWindow`。
6. 编译 + Editor 目视：Gizmo、导航、拾取、RT 缩放与 P2 一致。

**本阶段不碰：** `SceneManager` 主视口代持、第二条 Submit、Material 窗口。

---

## 10. UE 对照（便于以后读引擎）

| UE | minEngine |
|----|-----------|
| `SEditorViewport` | `EditorViewportWindow` |
| `FEditorViewportClient` | `EditorViewportClient` / `SceneEditingViewportClient` |
| Level Editor 视口 | `SceneEditingViewportWindow` |
| `FPreviewScene` + 预览视口 | `MaterialPreviewViewportWindow` + Preview `RenderScene` |

---

## 11. 验收（P2b）

- [x] 工程编译通过（Editor + minEngine）
- [ ] 默认 **`SceneEditingViewportWindow`** 行为与改名前 `ViewportWindow` 一致（需本地目视）
- [x] `SceneManager` 兼容 API 仍由 `SceneEditingViewportWindow` 使用（P3 再迁到 Client `SceneViewport`）
- [x] `MaterialPreviewViewportWindow` 已注册（P5 调试面板，默认打开）
- [x] `MaterialPreviewViewportWindow` 未打开时不影响主视口 Submit

**待定（非 Editor）：** GameRuntime / `GameMainViewport` — 见 [RENDER_REFACTOR_PLAN.md §11](../Render/RENDER_REFACTOR_PLAN.md#11-待定事项editor-优先gameruntime-后做)。
