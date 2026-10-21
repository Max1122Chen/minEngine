# BUG-CORE-006 — Prefab 模板树析构时对已释放父节点解引用（test prefab 间歇 SIGSEGV）

## Meta
- **ID:** BUG-CORE-006
- **Status:** Open
- **Severity:** S2（test smoke / verify.ps1 不受影响；test prefab 约 40% 崩，test full 间歇崩 → 阻把 full 当 CI 门禁）
- **Owner:** 待分配（Prefab 模板生命周期）
- **Found:** 2026-10-20（CORE-F43 S01/S02 期间偶然发现，**非该批引入**）
- **Affects:** Prefab::m_TemplateObjects 析构路径；GameObject::UnlinkFromCurrentParent
- **Related Feature/Slice:** [CORE-F43](./CORE-F43_GO_COMPONENT_ACTIVITY_AND_VISIBILITY_DESIGN.md)（发现场景）· [BUILD-F01](../ProductBuild/BUILD-F01_PRODUCT_BUILD_AND_PROJECT_TEMPLATE_DESIGN.md)（prefab 模板来源）

## TL;DR

minEngineTests.exe test prefab（filter 同时命中 prefab 与 prefab-overrides）**间歇** exit 139（实测 20 次崩 8 次），
崩溃点固定在 prefab-overrides propagate skips overridden ... 收尾，最后一行日志总是
GameObject ... 'PropagatedName' is being destroyed.。

根因：prefab 模板树由序列化克隆而来，**序列化只恢复 m_Parent，不重建 m_Children**（PrefabOverridesTest.cpp:129-130 的注释即写明
template trees keep parent Guid links but do not rebuild m_Children）。于是模板子节点的 m_Parent 指向父节点，
而父节点的 m_Children 为空。m_TemplateObjects（Prefab.h:40）按正序析构 → root 先释放 → 子节点析构里
UnlinkFromCurrentParent() 执行 auto& siblings = m_Parent->m_Children;（GameObject.cpp:524-534）→ **对已释放内存解引用**。
未复用时那块内存还像空 vector，于是静默通过；被复用/中毒时 SIGSEGV —— 这就是间歇的成因。

## 症状

- test prefab：约 8/20 次 exit 139。
- test full：同一区域偶发崩。
- **test smoke（9 套）与 verify.ps1 稳定 0 失败** —— 门禁不受影响，这是它长期被漏掉的原因。

## 复现

```
cd minEngine/bin
for i in $(seq 1 20); do ./minEngineTests.exe test prefab >/dev/null 2>&1; echo $?; done | sort | uniq -c
# 期望：既有 0 也有 139
```

**复现率会随堆布局变化**：CORE-F43 S01/S02 让 GameObject/Component 变大后，复现率明显上升（该批一行未动此路径）。
这意味着任何改变对象尺寸的改动都可能把它从偶发变成常发。

## 根因（file:line）

| # | 事实 | 位置 |
|---|------|------|
| 1 | 模板树克隆只恢复 m_Parent；m_Children 不是 ME_PROPERTY，不重建 | Tests/Suites/PrefabOverridesTest.cpp:129-130（注释） |
| 2 | 模板对象容器按正序析构 | Runtime/Function/Framework/Prefab/Prefab.h:40（m_TemplateObjects） |
| 3 | 子节点析构时通过 m_Parent->m_Children 摘链 | Runtime/Function/Framework/GameObject/GameObject.cpp:524-534 |
| 4 | 父先释、子后摘链 → 悬垂 m_Parent 解引用 | 由 1+2+3 组合 |

**为什么加判空无效**：悬垂指针判空本身就要解引用该指针。

## 建议修复方向（未实施，待授权）

| 方案 | 做法 | 代价 | 评价 |
|------|------|------|------|
| **A（推荐）** | Prefab::ClearTemplateObjects() / ~Prefab 在 clear() 前，先对每个模板对象 DetachFromParent(KeepWorldTransform) 断开父链 | 约 6 行，语义安全 | 消除父先于子释放的前提，不改既有约定 |
| B（更根本） | 模板树在克隆后重建 m_Children | 较大 | 牵动「模板树不走 m_Children」这条既有约定，需单独设计 |

## 验证方向

- 修复后 test prefab 连跑 ≥20 次 0 崩溃；test full 连跑 ≥3 次 0 崩溃。
- 断言：断开模板父链之后不存在任何 m_Parent 指向已释放对象（析构顺序日志或 ASan 构建验证）。
- 回归：prefab / prefab-overrides / scene-clone 全绿。

## 兼容影响

方案 A 只影响析构顺序，不改序列化格式、不改运行时可见行为。

## 变更记录

| 版本 | 日期 | 说明 |
|------|------|------|
| v1 | 2026-10-20 | 建档：由 CORE-F43 批 1 期间发现，含根因、复现、两个修复方向；未修。 |