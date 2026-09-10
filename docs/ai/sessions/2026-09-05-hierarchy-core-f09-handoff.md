# Handoff — Hierarchy / CORE-F09（2026-09-05）

## Background
- Branch: `feat/ui`
- Closed: CORE-F08 + ED-F05 + CORE-F09（Hierarchy 变换语义收口）
- Latest commit: `2b3957d` — `feat(core): sync world transforms for parented GameObjects`

## Work done
- GO 父子 ≡ 子 Root SC Attach 父 Root；KeepWorld 改父；Create Empty 默认 Root
- World transform：渲染 Proxy / 物理 / DebugDraw / Gizmo / 拾取用 `GetWorldTransform`
- `NotifyLocalTransformChanged` 下传脏标记到附着子
- 文档 Registry / ACTIVE_WORK / Design Status → **Done**；焦点切到 **UI-F01**

## Explicit non-goals / deferred
- 父子同时 Dynamic 刚体「跟随 + 独立积分」：不支持；需另开物理 Feature（约束/焊接或子关 Simulate）
- 兄弟排序、Socket、activeInHierarchy（TD-031）

## Open / next
- **Next focus:** UI-F01（见 `docs/ai/ACTIVE_WORK.md`）
- 未提交脏文件：`MyMEProject/*`、`build_*.log`、`tools/`（勿误提交）

## First next action
1. Bootstrap：`PROJECT_CONTEXT` / `ACTIVE_WORK` / `PROGRESS_LOG` / `FEATURE_REGISTRY`
2. 读 `docs/ai/Platform/UI/UI-F01_UI_SYSTEM_DESIGN.md` Meta Status
3. Pre-flight 后再动代码
