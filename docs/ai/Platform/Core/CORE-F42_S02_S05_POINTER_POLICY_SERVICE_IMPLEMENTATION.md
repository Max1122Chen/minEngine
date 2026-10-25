# CORE-F42 · S02–S05 —— PointerPolicyService + Editor 迁移 + 游戏面 facade

| 项 | 值 |
|----|----|
| 切片 | `S02` 服务骨架 · `S03` 编辑器迁移 · `S05` 游戏面 facade（本波合并竖切） |
| 设计输入 | `CORE-F42_POINTER_POLICY_DESIGN.md`（Approved） |
| 落地日期 | 2026-10-25 |
| 验收 | `test pointer-policy` **11** cases / **105** asserts 全绿；`Editor` 目标可链 |

## 1. 一句话

编辑器与游戏不再直调 `SetCursorVisible` / `SetCursorClipRect` 争写；**`PointerPolicyService`** 用 tier 栈 + RAII 句柄仲裁，经 sink 落到 focus 作用域约束层。PIE 默认 Captured 挂在 **Game** tier，因此 `Pointer::ShowUi()`（UI tier）能盖住它——开背包可见鼠标。

## 2. 切片对照

| Slice | 本波 | 说明 |
|-------|------|------|
| S01 | 已有 | focus 作用域；未改 |
| **S02** | **Done** | `PointerPolicyService`、`MapModeToWindowCommand`、`IPointerModeSink`、Recording sink 单测 |
| **S03** | **Done** | `SEVC` / `Editor` / `PlayInEditorSession` 迁出直调；空栈 → Free |
| S04 | **Deferred** | Shift+F1 改绑仍用 LeftAlt；提示文案已预告 |
| **S05** | **Done（facade）** | `Pointer::Show/Hide/Capture/Confine*/ShowUi/ConfineUiTo`；位置采样契约未接线（仍空 `GetPosition`） |
| S06 | **Deferred** | UI 自动 Confined；消费方自行 `ShowUi`/`ConfineUiTo` |

## 3. 与 Design 的有意偏差

| 项 | Design | 本波 | 理由 |
|----|--------|------|------|
| PIE Captured tier | Play (80) | **Game (40)** | Play > UI 时背包 `ShowUi` 无法盖住 PIE；挂 Game 后 UI Confined/Free 可叠加（对齐「容器叠在游戏 Captured 上」） |
| `ViewportPointerMode` | 删除 (D-9) | **暂留**为 SEVC 内部 desired 枚举 | 行为已走 Request；删枚举可跟 S04 一并收 |
| 位置契约 | BeginFrame 采样 | **未接** | 不挡开背包；`GetPointerPosition` 仍可用 |

## 4. 消费方迁移（minCraft）

把 `WindowSystem::SetCursorVisible(menuOwnsPointer)` 换成：

```cpp
// 成员：minEngine::PointerModeRequest m_MenuPointerRequest;
if (menuOwnsPointer)
    m_MenuPointerRequest = minEngine::Pointer::ShowUi(); // 或 ConfineUiTo(viewportClip)
else
    m_MenuPointerRequest.Release();
```

持有句柄期间 UI Free 压过 Game Captured；释放后自动回到 PIE Captured。

## 5. 验证

```powershell
cmake --build minEngine/build --target minEngineTests Editor -j 8
cd minEngine/bin
.\minEngineTests.exe test pointer-policy
```

手工：PIE → 游戏侧 `ShowUi`（或临时 console）→ 光标可见；Release → 回到隐藏+clip。
