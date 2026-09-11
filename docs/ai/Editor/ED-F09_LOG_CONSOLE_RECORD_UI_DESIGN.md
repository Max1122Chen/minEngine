# ED-F09 — Editor Log Console (LogRecord UI) — Design Spec

## Meta
- **ID:** `ED-F09`
- **Type:** Feature
- **Status:** Planned（草稿；依赖 CORE-F17）
- **Owner:** project maintainer
- **Last updated:** 2026-09-11
- **Related:**
  - [CORE-F17](../Platform/Core/CORE-F17_LOGGING_CHANNELS_DESIGN.md)（LogRecord / LogChannel / LogConsoleStorage）
  - Code today: `Editor/src/UI/EditorWindows/ConsoleWindow.*`
- **Depends on:** `CORE-F17` S00（Storage 已为 `LogRecord`，旧 Entry 已删）
- **Blocks:** 无（不挡 F17 合入；可紧随或并行于 F17 尾声）

## TL;DR

把 Maximum **Output / Console 窗口**从「Core/Client 二分 + `LogConsoleEntry`」升级为消费 **`LogRecord`**：按 **LogChannel**、**Severity** 过滤，展示 channel 名 / 消息 / 源位置；为日后字段展开与 Agent 查询留口。本期先草拟，**详设与实现跟在 CORE-F17 硬切之后**。

## Scope

### In（目标能力，可切片）
- `ConsoleWindow` 改为 `Snapshot()` → `LogRecord`
- 过滤：按 Channel（多选/列表，来自已注册 Channel 或出现过的名）、按 Severity 门槛或勾选
- 展示列：时间、Severity、Channel 名、Message；（可选）file:line
- 保留现有：清空、暂停滚动、复制（若已有）
- 去掉 `LogSource` / Core·Client 专用开关

### Out（本期草稿不展开）
- Collapse / 重复聚合
- 点击跳转 IDE / Asset
- 结构化 Fields 展开（等 LogField 实现）
- 改 ED-F04 Command 系统（可另刀：`log SetSeverity Asset Warn`）

## Reader quick start

1. CORE-F17 §3.5 LogRecord、§3.7 Storage  
2. 现 `ConsoleWindow.cpp` PassFilter  
3. 本文件 §3 方案草稿  

---

## 1) 背景

今日 Console 用 `LogSource::Core|Client` 过滤，与多 Channel 模型不匹配。CORE-F17 删除 Entry 后，Editor **必须**改读 `LogRecord`，否则无法链接。ED-F09 负责把「能编译的最小适配」提升为「可用的诊断 Console」。

## 2) 现状

- `ConsoleWindow::PassFilter` 依赖 `entry.source`  
- `m_ShowCore` / `m_ShowClient`  
- 与 typed Channel、Severity 阶梯未对齐  

## 3) 方案草稿

```text
LogConsoleStorage::Snapshot()
        │
        ▼
ConsoleWindow
  - severity mask / min severity
  - channel allow-set (names or pointers)
  - text search (message)
        │
        ▼
ImGui list: [Time][Sev][Channel][Message]
```

- 过滤优先用 `record.channel` 指针（与 allow-set 比指针或比 `GetName()`）。  
- Channel 列表：启动时从 `LogSystem` 枚举已注册 Channel；或首次出现时动态加入。  
- **CORE-F17 S00：** 可先「全部显示 + 按 channelName 字符串简单勾选」保证链接；**ED-F09** 做完整 UX。

## 4) 建议切片（待Impl）

| Slice | 内容 |
|-------|------|
| S00 | 编译通过：读 LogRecord，去掉 LogSource UI |
| S01 | Channel 多选过滤 + Severity 过滤 |
| S02 | 搜索、复制、暂停/清空打磨 |

## 5) 验收（预告）

- [ ] 无 Core/Client 专用过滤  
- [ ] 可按至少一个 Channel / Severity 过滤  
- [ ] 列表展示 channel 显示名（方案 A 短名）  
- [ ] 与 CORE-F17 联调通过  

## 6) 风险

| 风险 | 缓解 |
|------|------|
| 与 F17 并行改 Console 冲突 | F17 S00 最小改；ED-F09 紧随 |
| Channel 列表过多 | 默认常用子集；搜索 Channel |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-11 | Planned 草稿登记 |
