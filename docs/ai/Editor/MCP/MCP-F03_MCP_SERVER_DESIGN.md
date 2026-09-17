# MCP-F03 — minEngine MCP Server（TypeScript + 官方 SDK）— Design Spec

## Meta
- **ID:** `MCP-F03`
- **Type:** Feature
- **Status:** Planned（维护者已认可设计；待 Implementation）
- **Last updated:** 2026-09-17（升 Planned）
- **Branch:** `feat/mcp`
- **Depends on:** `MCP-F02` · `ED-F17`（可先 mock）· `CLI-F02`
- **Related:** [MCP-F01](./MCP-F01_ARCHITECTURE_DESIGN.md) §3.4（接口分层）· §7.5（粗 Tools）
- **Product name:** **minEngine MCP Server**

## TL;DR

Node/TS：**stdio MCP** 对 Agent；**Connection Manager** 支持 Spawn/Attach；Pipe Client 把粗 Tools 译成协议细 op。业务零实现——只调用 Maximum 已开放的同源 Q/E/V。暂用 `editor_query/edit/verify` 观察 Agent 可发现性；粒度可后调；业界 MCP 对照后置。

## Scope
- **In:** 工程骨架、粗 Tools、连接状态机、Pipe（length-prefix）、Spawn/Attach、错误映射、Tool/Schema 描述（帮 Agent 知道能做什么）
- **Out:** C++、域服务、细粒度产品 Tool 目录、业界 MCP 调研结论（另开讨论）

## Reader quick start
1. MCP-F01 §3.4（L3 只做翻译）
2. §3.3 粗 Tools 与「如何让 Agent 知道能做什么」
3. §3.4 连接

---

## 1) 背景与目标

- Cursor 配置为 MCP server。  
- Spawn + Attach。  
- **不在 Server 内实现编辑语义**（同源在 Editor L1）。

---

## 2) 现状

- 尚无 `minEngineMcpServer/` 目录。

---

## 3) 方案

### 3.1 在接口栈中的位置

```text
Agent  →  (L3) minEngine MCP Server  →  Protocol  →  (L2) Host  →  (L1) 域服务
              翻译 / 连接 / 校验 / 描述
```

#### 3.1.1 Server「知不知道自己在服务 Editor」？

**业务上可以几乎不知道。** 理想形态：Server 只认「对端是一个讲 Automation Protocol 的进程」——校验 JSON、转发、把错误码映射成 MCP 错误。它不必理解 Scene、Undo、材质图。

**工程上仍会「知道一点非业务的事」**（见下表）——那是适配与运维，不是编辑语义。

| Server 负责 | 不算「懂 Editor 业务」 |
|-------------|------------------------|
| MCP Tool 目录与 description | 只是对外说明书 |
| Schema：类型/必填/enum | 防胡传参数 |
| Spawn/Attach、等 ready-file、Pipe 读写 | 进程与连接 |
| `domain`+`action` → 协议 `kind`+`op` | **字符串翻译**，不执行编辑 |
| 超时、断线、把 Host 错误码转成 Agent 可读结果 | 适配 |

| Server **不**负责 | 应在 Maximum L1 |
|-------------------|-----------------|
| 改属性/建 GO/校验引用的真实规则 | 域服务 |
| Undo、Session、对象生命周期 | Editor |
| 「这个 Guid 在不在场景里」的判定 | Query/Edit/Verify |

若发现必须在 TS 里写 `if (action==="set_property")` 去解释 Transform 含义 → 设计错误，应回流 Editor。

若发现只是 `op = `${domain}.${action}`` → 正常，仍算「傻转发」。

### 3.2 布局

```text
minEngineMcpServer/
  package.json
  src/
    index.ts
    connection/manager.ts | spawn.ts | attach.ts
    tools/
      editor_connect.ts
      editor_get_state.ts
      editor_query.ts
      editor_edit.ts
      editor_verify.ts
    automation/client.ts | protocol.ts
  schemas/
```

### 3.3 粗 Tools（实验口径 · 已拍板）

| Tool | 说明 |
|------|------|
| `editor_connect` | `spawn` \| `attach` |
| `editor_get_state` | 连接 + sessions |
| `editor_query` | `domain` + `action` + … |
| `editor_edit` | 同上；**必填 sessionId** |
| `editor_verify` | 同上 |

- **不**把 `ping` 做成产品 Tool（连接层内部可用）。  
- **不**默认注册一操作一 Tool。  
- **后置：** 调研 Unity/Unreal/Godot 等官方或社区 MCP 目录，再决定是否调粒度。

#### 3.3.1 Agent 如何知道「能做什么」· 能力真源

**忧虑（正确）：** 若仅在 MCP Server 里手写「支持哪些 `domain`/`action`」，Server 不是代码/资源真源，目录会过时。

**分层（两本「目录」不要混）：**

| 目录 | 给谁看 | 真源应在哪 |
|------|--------|------------|
| **MCP 粗 Tools**（`editor_query` / `edit` / `verify`…） | Agent / Cursor | 适配层契约，可长期稳定；**不必**随每个 Scene op 改 |
| **细能力**（`scene.set_property`、`scene.get_hierarchy`…） | Tool 参数 enum / Resource / description | **Maximum（Editor）** — 与 L1 域服务注册表同源 |

**推荐模型（P0 即拉 Host，禁止 TS 独真源）：**

```text
Maximum 启动
  → CapabilityRegistry（唯一能力真源；可先在 C++ 内登记 Scene MVP 全表）
  → Host 暴露 query: editor.get_capabilities

minEngine MCP Server 连接后
  → 拉取并缓存 capabilities
  → 填充 editor_query/edit/verify 的说明 / 校验
  → tools/list 仍返回粗 Tools
```

- P0 **不允许**「仅在 TS 手写 action 枚举当唯一真源」。若 Host 未就绪，connect 失败，不假装有能力表。  
- 执行时 Host 再校验：未知 `op` / `KindOpMismatch`。  
- `editor_get_state`：保留为独立粗 Tool（发现性）；语义 ≡ `editor_query(domain=editor, action=get_state)`；合并与否后置。

**业界对照（摘要，后置可深挖）：**

| 做法 | 谁维护工具表 | 典型 |
|------|--------------|------|
| A. MCP 进程即产品，工具表写死在 Server | Server | 多数 GitHub/文件系统类 MCP |
| B. 编辑器内注册表 + 薄 Relay 只转发 | **Editor** | Unity 官方 MCP：`McpToolRegistry` 在 Editor；relay 经 IPC 暴露 |
| C. 社区桥：Server 有一份工具列表，执行仍进 Editor | 常双份，易漂 | 部分 Unity 社区 MCP |
| D. MCP `tools/list` + `listChanged` | Server **对 Agent** 的广告可动态变 | 协议能力；广告内容仍应来自 B 的注册表 |

我们目标对齐 **B + 粗 Tool 外观**：对 Agent 广告稳定粗 Tools；细能力广告/校验来自 Editor。

MVP 缓解：Tool description；连接后必拉 get_capabilities；**禁止** TS 独有能力真源。

帧：length-prefix JSON；**默认 RPC 超时 30s**（可配置）。

### 3.4 连接

`	ext
spawn → CLI-F02 → wait ready-file（Listen 之后才出现）→ pipe connect
      → get_capabilities + get_state → Ready
attach → connect(pipeName) → 同上 → Ready | 可读失败
`

### 3.5 editor_disconnect

MVP：**可选**；进程退出即断。Spawn 子进程建议在 Server 退出时尽量杀掉（实现细节）。

---

## 4) 备选

| 选项 | 结论 |
|------|------|
| Maximum* 产品名 | 否决 |
| 细 Tool 目录为默认 | 否决（可后调） |
| Server 内业务逻辑 | 否决 |
| TS 独有 capabilities 真源 | **否决** |

---

## 5) 风险

| 风险 | 缓解 |
|------|------|
| Agent 不知道 action | 拉 capabilities + description |
| 与 Editor 语义漂移 | 禁止 TS 业务；只译协议 |
| ready 竞态 | 遵守 Listen→ready-file |

---

## 6) 验收标准

- [ ] 粗 Tools 可被 Cursor 列出且 description 可读
- [ ] Spawn 与 Attach 各一路径（或 P0-alpha 先通一种并文档声明）
- [ ] 连接后 capabilities 来自 Host
- [ ] Scene query→edit→verify→save 不经 TS 业务分支
- [ ] 目录/包名 = minEngine MCP Server

---

## 7) 仍可议

1. npm 名：@minengine/mcp-server？  
2. 业界 MCP 对照时间点  
3. 是否合并 editor_get_state 进 editor_query

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-17 | Draft |
| 2026-09-17 | 粗 Tools + Spawn/Attach + 产品名 |
| 2026-09-17 | L3 无业务；能力真源；业界对照后置 |
| 2026-09-17 | **评审修补：** 禁 TS 独真源、超时、get_state 保留、链接 §7.5；待审批 |
