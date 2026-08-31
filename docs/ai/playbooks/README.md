# Playbooks — typical bugs & debugging patterns

**Purpose:** Distilled, reusable debugging knowledge (Tier B reference).  
**Not** individual defect tickets — those stay in [`../bugs/`](../bugs/) as `BUG-*.md`.

**When to use**

| Situation | Read |
|-----------|------|
| Shadow looks wrong on Vulkan | [Render/VK_SHADOW_DEBUGGING.md](./Render/VK_SHADOW_DEBUGGING.md) |
| Closed bug, need full incident history | `docs/ai/bugs/BUG-RENDER-*.md` |
| Design / fix rationale | `docs/ai/Render/RND-F*.md` |

## Index

### Render

| Playbook | Summary |
|----------|---------|
| [VK_SHADOW_DEBUGGING.md](./Render/VK_SHADOW_DEBUGGING.md) | VK shadow isolation, UBO lifetime, cull/bias audit, **handoff §7** |

## How to add an entry

1. Write a **pattern** doc under the right domain folder (`Render/`, `Editor/`, …).
2. Link to related `BUG-*` and Feature/Design docs — do not duplicate full bug narratives.
3. Add a row to this README.
4. Optional: one line in [BOOTSTRAP_DIGEST.md](../BOOTSTRAP_DIGEST.md) read order.

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Initial scaffold; VK shadow playbook from BUG-013 / RND-F14 |
