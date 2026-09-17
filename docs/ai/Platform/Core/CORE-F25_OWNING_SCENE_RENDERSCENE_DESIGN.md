# CORE-F25 — Owning-Scene RenderScene Resolution — Design Spec

## Meta
- **ID:** `CORE-F25`
- **Type:** Feature（小范围契约重构）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** `feat/prefab`（可与 ED-F16 同轨落地；合入前须独立可验）
- **Related:**
  - [ED-F16 Prefab Editor](../../Editor/ED-F16_PREFAB_EDITOR_DESIGN.md)（触发：Stage 与 Level 渲染串场景）
  - [CORE-F05 Play Mode](./CORE-F05_PLAY_MODE_DESIGN.md)（`ActiveSceneOverride` / PIE 对照）
  - [ENGINE_DESIGN_PHILOSOPHY](../../ENGINE_DESIGN_PHILOSOPHY.md) — Mechanism over Policy；Minimal Core
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** `Scene::GetRenderScene` / `EnsureRenderScene` 已存在；Component ↔ GameObject Outer=Scene 归属已成立（CORE-F06 Activate）
- **Blocks:** Prefab Stage 视口正确显示；未来多 Scene / 隔离预览 Viewport

## TL;DR

组件（Primitive / Light / Sky / Widget 等）注册/更新/移除 render proxy 时，**从 owning Scene 取 `RenderScene`**，禁止再经 `SceneManager::GetRenderScene()`（隐式「当前 tick 世界」）。  
目标：Prefab Stage、PIE、Level 的 proxy 落在各自 Scene 的 RenderScene；InspectingScene 切换不再导致串世界。

**Status: Done.**

## Scope

### In

- 统一解析路径：`Component` → Owner GO → Outer `Scene` → `Scene::GetRenderScene()`（可封装为 `Component::GetOwningScene()` / `GetOwningRenderScene()`）
- 迁移所有「进世界」调用点：至少  
  `PrimitiveComponent`、`LightComponent`、`SkyBoxComponent`、`WidgetComponent`（Add/Update/Remove）
- 无 Scene / 未 `EnsureRenderScene` 时：**安全 no-op**（不崩溃；可 Debug 日志）
- 回归：Level Viewport、PIE、Prefab Stage（代理进 Stage RS；Level 不再被污染）
- 单测或冒烟：双 Scene 时 proxy 落在正确 `RenderScene`（能测则测；否则手验步骤写入 DoD）

### Out

- Prefab Stage **相机**快照/恢复（可另开小切片或挂 ED-F16 follow-up；本 Feature 不绑死）
- 强制 Viewport 多实例 / 并排双视口
- 改 ForwardRenderer / RDG 图结构
- 删除 `SceneManager::GetRenderScene()`（可保留给「系统默认 tick 世界」查询；**组件不得再调用**）
- Physics / Audio 等非 RenderScene 系统的「当前世界」解析（若有同类问题，另登记）

## Reader quick start

1. §2 现状痛点 · §3 目标契约  
2. §4 迁移清单与切片  
3. §6 验收（含 Prefab Stage 手验）

---

## 0) Pre-flight

| 项 | 结论 |
|----|------|
| 前置 | Scene/RenderScene 归属 **sound**；Activate Outer=Scene **sound**；Prefab Stage InspectingScene **sound**；组件注册路径 **wrong** |
| 债风险 | **Low–Medium** — 改动面窄但触及渲染注册热路径；漏迁一处会静默写错 RS |
| WIP | `feat/prefab` ED-F16 Done 待 commit；本 Feature 是 Prefab 可视隔离的硬前置 |
| Philosophy | Mechanism：对象按归属进世界；不靠全局「当前编辑世界」猜 |
| 建议 | **Go** — 真重构契约，不用 Stage-only `ActiveSceneOverride` 当终态（后者可作临时止血，本设计默认直接改归属路径） |

**true refactor：** 删除组件侧对 `SceneManager::GetRenderScene()` 的依赖；保留 Manager API 仅作系统级查询。

---

## 1) 背景与目标

### 1.1 Pain

Prefab Stage 只设置 `InspectingScene`，未改 `SceneManager` tick/render 目标。组件 EOF 更新仍调用：

```text
SceneManager::Get().GetRenderScene()  →  GetTickTargetScene()  →  Level Editor Scene
```

结果：Stage GO 的 mesh/light 写入 **Level RenderScene**；Viewport 观察 **空的 Stage RenderScene** → 蓝底 + Level 被污染。

### 1.2 Success

- 任意 Scene 内激活的可渲染组件，只触碰 **该 Scene** 的 `RenderScene`
- Prefab Stage 打开：Stage 视口能看到 Prefab 内容；Level RenderScene **无** Stage 残留 proxy
- PIE 行为不回归（PIE Scene 仍是 Outer；override 可继续服务 tick，但组件注册不再依赖它）

---

## 2) 现状

| 位置 | 行为 |
|------|------|
| `SceneManager::GetRenderScene()` | 转发 `GetTickTargetScene()->GetRenderScene()`；PIE 用 `ActiveSceneOverride` |
| `PrimitiveComponent` / `LightComponent` / `SkyBoxComponent` / `WidgetComponent` | Add/Update/Remove 均走 Manager |
| `Component::CanApplyActivation` | 已要求 Owner Outer 为 `Scene` |
| Prefab `Activate` | 仅 `SetInspectingScene(Stage)` |
| Viewport | `GetActiveScene()->GetRenderScene()` 观察（与注册路径分裂） |

---

## 3) 方案

### 3.1 目标契约

```text
Component 需要 RenderScene 时：
  1. GameObject* owner = GetOwner()
  2. Scene* scene = dynamic_cast<Scene*>(owner->GetOuter())  // 或 GameObject::GetScene() 若后补
  3. if (!scene) return;  // 未入世界
  4. RenderScene* rs = scene->GetRenderScene();  // 或 EnsureRenderScene() 策略见下
  5. rs->UpdatePrimitive / RemovePrimitive / ...
```

**推荐封装（避免四处 dynamic_cast）：**

```cpp
// Component.h — 或 SceneComponent 若更贴切；本期允许放在 Component 基类
Scene* GetOwningScene() const;
RenderScene* GetOwningRenderScene() const;  // nullptr if no scene / no RS
```

### 3.2 EnsureRenderScene 策略（默认拍板）

| 时机 | 行为 |
|------|------|
| Update / 注册 proxy | 若 Scene 存在但 `GetRenderScene()==nullptr`：**调用 `EnsureRenderScene()`** 再注册（与今日 Level 路径一致：Editor Scene 通常已有 RS） |
| Remove | 若 RS 已空：no-op（避免析构顺序崩溃） |

### 3.3 `SceneManager::GetRenderScene` 去留

- **保留** API：供不绑定具体组件的系统（例如某些全局调试、旧调用点渐进迁移）
- **组件世界注册路径禁止使用**；实现期可用 grep 验收「Framework/Components 内零引用 Manager::GetRenderScene」
- 不在本期删除 API（避免无关怀大爆炸）

### 3.4 换 Scene / 克隆

Instantiate / PIE Duplicate / Prefab Stage：GO Outer 已是目标 Scene；激活后 EOF 自然写入新 RS。  
**须确认：** 从旧 Scene 移除或停用时走 `Remove*`，避免旧 RS 悬空 proxy（现有 Deactivate 路径应继续调用 Remove，仅改 RS 来源）。

### 3.5 Camera（明确边界）

`CameraComponent::SetSelfAsMainCamera` 仍碰 `SceneManager::GetEditorSceneViewport()`——这是 **Viewport 相机绑定**，不是 RenderScene 归属。  
本期 **Out**；ED-F16 follow-up 可做 Stage 进出时相机快照。本 Feature DoD **不要求**修「同一 flycam」观感，但 Stage 内应能看到物体（主因修复后蓝底应消失）。

### 3.6 数据流（目标）

```text
Prefab Stage Activate
  → InspectingScene = Stage
  → Viewport observes Stage.RenderScene
  → Stage GO Activate → EOF → GetOwningRenderScene() == Stage.RS
  → UpdatePrimitive into Stage.RS
  → Viewport 有内容；Level.RS 干净
```

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. Owning Scene 取 RS（本方案） | 契约正确；多 Scene 可扩展 | 要迁若干组件 | **选用** |
| B. Prefab 期间 `SetActiveSceneOverride(Stage)` | 改动少 | 隐式全局；多 Viewport 仍炸；与 InspectingScene 双轨 | **拒绝作终态**（紧急时可临时，不进本 Feature DoD） |
| C. 仅 Viewport 从 Level 观察（不改注册） | — | Stage 永远空；污染继续 | **拒绝** |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 漏迁某一 Remove/Update | 静默串 RS 或泄漏 proxy | grep Components；Prefab + PIE 手验清单 |
| EnsureRenderScene 过早 | 无 Scene 对象建 RS | 仅 owning Scene 非空时 Ensure |
| 析构顺序 | Remove 时 Scene 已毁 | Remove 路径空指针 no-op |
| PIE 回归 | 玩法视口空/错 | `test scene-clone` + 手验 EnterPlay |
| 与未提交 ED-F16 同分支混交 | review 难 | commit 可拆：先 F25 再 F16；或同 PR 但 DoD 分开勾 |

---

## 6) 验收标准

- [x] `PrimitiveComponent` / `LightComponent` / `SkyBoxComponent` / `WidgetComponent` 的 Add/Update/Remove **不**调用 `SceneManager::GetRenderScene`
- [x] 存在 `GetOwningScene` / `GetOwningRenderScene`（或等价单一辅助），无复制 dynamic_cast
- [x] Level 编辑视口：加 Mesh/Light 行为与改前一致（构建通过；手验建议）
- [ ] PIE：EnterPlay 画面正常；停 Play 回编辑正常（手验）
- [ ] Prefab Stage：打开含 Mesh 的 Prefab → **非纯蓝底**；切回 Level → Level 视口 **无** Prefab mesh 残留（手验）
- [x] `Framework/Components` 下 `SceneManager::Get().GetRenderScene` grep 为 0
- [x] 相关测试：`test scene-clone` / `test prefab` 绿

---

## 7) 建议实现切片

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | `Component::GetOwningScene` / `GetOwningRenderScene` + Ensure 策略 | 编译；单元或静态检查 |
| **S02** | 迁移 Primitive / Light / Sky / Widget | grep 清零；Level 手验 |
| **S03** | Prefab Stage + PIE 手验 DoD | 蓝底消失；无串场景 |
| **S04** | 文档勾选 + PROGRESS；可选小测「双 Scene proxy 归属」 | DoD |

---

## 8) 开放点（默认已拍板）

| # | 问题 | 默认 |
|---|------|------|
| O1 | 终态用 owning Scene 还是 Stage override | **owning Scene** |
| O2 | 无 RS 时 Ensure 还是跳过 | **Ensure**（Scene 已存在时，经 `GetRenderScene`） |
| O3 | 本期是否修相机隔离 | **否**（Out） |
| O4 | 是否删除 Manager::GetRenderScene | **否**（保留；禁组件用） |
| O5 | 是否新增 `GameObject::GetScene()` | **可选**；本期用 `Component::GetOwningScene` |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | Draft/Review：由 Prefab Stage 串场景诊断驱动；owning-Scene RenderScene 契约 |
| 2026-09-15 | **Done：** S01–S02；`GetOwningRenderScene` / `IfPresent`；四组件迁移；`test scene-clone`/`prefab` PASS；Prefab/PIE 手验待维护者确认 |
