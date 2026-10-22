# RND-F24 — 纹理采样模式资产化（过滤 / 环绕可配置）— Design Spec

## Meta
- **ID:** `RND-F24` · **Status:** `Draft`（待主人审批） · **Complexity:** `L2`
- **输入依据:** minCraft 反馈「Texture sampler modes」（`TEXTURE_SAMPLER_MODES`）；前次反馈 `TEXTURE_FILTER_AND_ATLAS_PADDING` 只交付了机制
- **行号基线:** 2026-10-21 工作树。

## Problem

| # | 事实 | 位置 |
|---|------|------|
| P1 | 采样器默认 `MinFilter/MagFilter=Linear`、`AddressU/V=Repeat` | `RHISampler.h:29-32` |
| P2 | `Texture2D::m_PreferredSamplerDesc{}` 即该默认；材质渲染代理直接取用 | `Texture.h:65`、`MaterialRenderProxy.cpp:34` |
| P3 | **导入选项与 `.metex` 资产没有 filter / address 字段** → 导入后无法声明像素画所需的最近邻与夹边 | `Texture2DImporter.cpp` 无相关选项 |
| P4 | 于是每个消费方各自 `SetPreferredSamplerDesc` 一遍（minCraft 的 `BlockTextureAtlas` 就是这么绕过的） | 消费方代码 |

**性质**：上一轮已完成**机制**（`RHISamplerDesc` + `SetPreferredSamplerDesc` + 图集 dilate）；缺的是**资产声明路径**。

## 现状里必须避免的坑

`Texture2D` 已有 `m_Filtering` 与 `m_Wrapping`，但它们是**从 `m_PreferredSamplerDesc` 派生的镜像**（`Texture.cpp:35-36` 在设置 desc 时同步）。因此本设计**不得**再造第三份真相。

## 目标

| 目标 | 说明 |
|------|------|
| 资产级可声明 | `.metex` 内保存过滤与环绕；冷加载后仍是设定值 |
| 可编辑 | Inspector 中作为 `ME_PROPERTY` 暴露，改完保存即生效 |
| 单一真相 | `m_PreferredSamplerDesc` 是唯一渲染依据；作者字段改动 → 重算 desc → 失效缓存 |
| 默认不变 | 导入默认 **Linear + Repeat**（主人已定），不改变既有资产观感 |

## 设计

### 作者字段（`Texture2D`）

```cpp
ME_PROPERTY()
RHISamplerFilter  m_SamplerFilter  { RHISamplerFilter::Linear };   // 已是 ME_ENUM，可直接反射
ME_PROPERTY()
RHISamplerAddress m_SamplerAddress { RHISamplerAddress::Repeat };  // 同上
```

两个字段是**唯一真源**。`m_PreferredSamplerDesc` 是派生缓存：`RHISamplerDesc` 只有五个字段，全部由这两轴决定（filter → Min/Mag，address → U/V/W），没有额外信息，因此缓存可完整重建。

### 写入接口（两个独立 setter）

```cpp
void SetSamplerFilter(RHISamplerFilter filter);     // 只换过滤轴，保留当前 address
void SetSamplerAddress(RHISamplerAddress address);  // 只换环绕轴，保留当前 filter
private:
void RebuildPreferredSamplerDesc();                 // 由两个字段重建缓存 + 失效采样器
void SetPreferredSamplerDesc(const RHISamplerDesc&);// 收为 private，仅供内部（加载/导入）使用
```

拆成两个 setter 的理由：反射与 Inspector 按字段逐项编辑，一次只改一轴；合成一个方法会强迫调用方先读另一轴，反而制造出错机会。

**`SetPreferredSamplerDesc` 收为 private** 是为了杜绝外部绕过真源。代价是**移除一个已导出符号**，消费仓（`BlockTextureAtlas`、`MobVisual` 共 8 处）需同步迁移到两个新 setter —— 消费方已确认接受。

### 派生与失效

```
SetSamplerModes(filter, address)
  → 重算 m_PreferredSamplerDesc（Min/Mag = filter，AddressU/V = address）
  → 重建采样器缓存（既有 SetPreferredSamplerDesc 已有该路径）
  → 标记资产脏（编辑器保存 .metex）
```

### `.metex` 序列化

- 头部追加两字节（filter / address）；**版本号 +1**。
- 旧文件无该字段 → 取 Linear + Repeat（与今天完全一致，零迁移）。
- 沿用既有追加型字段规则：**不重排既有字段**。

### 导入选项

`asset.import` 的 texture 选项新增 `filter` / `address`，缺省 Linear / Repeat；导入写入 `.metex` 头部。

### 消费路径

`ImageComponent` 与 `SpriteComponent` 都已经过 `MaterialRenderProxy` 取 `GetPreferredSamplerDesc()`，因此**无需改组件**；只需保证那份 desc 已由作者字段派生。

## 切片

| 片 | 内容 | 结束仍绿 | 验收 |
|----|------|----------|------|
| **S01** | `Texture2D` 两个 `ME_PROPERTY` 字段 + 派生 + 缓存失效 + 单元测试 | 默认值下行为与今天一致 | 设置 nearest/clamp 后 `GetPreferredSamplerDesc()` 正确；linear/repeat 回到今天行为 |
| **S02** | `.metex` 头部两字段 + 版本 +1 + 旧文件默认 + 往返测试 | 旧资产照常加载 | 冷加载后设置不丢；旧文件读出 Linear/Repeat |
| **S03** | 导入选项透传 + Inspector 可编辑（保存即生效） | 不改选项的导入行为不变 | 导入时声明 nearest/clamp 生效；Inspector 改后保存重载仍正确 |

## Failure Model

| # | 失效 | 观测 | 处理 |
|---|------|------|------|
| F1 | 作者字段改了但 desc 没重算 | 改了不生效 | 唯一入口 `SetSamplerModes`；构造与反序列化都走它 |
| F2 | 采样器缓存未失效 | 旧采样器被复用 | 缓存 key 已含 `RHISamplerDesc` 哈希（`RHISampler.h:102`），验证后加断言 |
| F3 | `.metex` 版本升级后旧引擎读新文件 | 读取失败 | 旧引擎按既有未知字段跳过策略处理；必要时拒绝并给明确错误 |
| F4 | Mip 与过滤交互（Nearest + mip） | 远处闪烁 | 本轮不改 mip 策略；记为已知限制 |

## Verification

1. 单元：字段 → desc 派生（四种组合全覆盖）。
2. 单元：`.metex` 往返（新文件、旧文件、缺字段三种）。
3. 单元：改字段后采样器缓存失效（旧句柄不再被复用）。
4. **目视（无法自动断言，明确标注）**：2×2 棋盘图 nearest/clamp 放大 4× 边界锐利、四边无渗色；linear/repeat 保持今天观感。
5. 既有 `screen-ui-*` / sprite 套件全绿。

## 待审批决策

| # | 决策 | 推荐 | 理由 | 推翻条件 |
|---|------|------|------|----------|
| D1 | 暴露粒度 | **两个枚举字段**（filter / address） | 采样器 desc 有十余个字段，全暴露会污染 Inspector；其余保持默认 | 需要 anisotropy / border color 可配 |
| D2 | 默认值 | **Linear + Repeat** | 主人已定；零兼容风险 | 希望新导入开箱像素友好（则按导入时间分裂，需接受） |
| D3 | `m_Filtering`/`m_Wrapping` | **删除**，改由派生函数提供 | 避免三份真相；它们已是镜像 | 若有外部消费方直接引用这两个 getter |
| D4 | 导入是否把默认值写死进 `.metex` | **写死**（Linear/Repeat） | 显式胜过隐式；将来改默认不影响老资产 | 希望省略默认值以省空间 |