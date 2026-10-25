# RND-F28 — Texture2DRenderTarget（可渲染 Texture2D 子类）+ Offscreen 渲入

## Meta
- **ID:** `RND-F28`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** agent + @mikumikumikumiku
- **Last updated:** 2026-10-25
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [RND-F25](./RND-F25_RENDER_TO_TEXTURE_DESIGN.md) · [RND-F27](./RND-F27_RDG_REGISTER_EXTERNAL_DESIGN.md)（**硬依赖**） · [ASSET-F05](../Asset/ASSET-F05_TEXTURE_COOKED_ASSET_DESIGN.md) · minCraft `ENGINE_FEEDBACK_PROMPT_OFFSCREEN_TEXTURE2D_WRAP.md`
- **Branch:** `feat/render`
- **命名说明：** 产品类型名为 **`Texture2DRenderTarget`**（主人选定）。UE 对应物为 `UTextureRenderTarget2D`（`Engine/.../TextureRenderTarget2D.h`）——同为「可采样且拥有 RT 的 Texture」，标识符词序不同。

## TL;DR

minCraft 需要从 `OffscreenRenderer` 得到**每块独立、可采样、可长期持有**的贴图，且游戏侧不碰 RHI、不共享 RDG 池化 color。本 Feature 引入 **`Texture2DRenderTarget : Texture2D`**、分配 `RenderTarget|ShaderResource` 的工厂，以及经 RND-F27 绑定的 **`SetColorTarget` + 仍返回 `RHITextureRef` 的 `RenderOnce`**。无公开 `Wrap*` API。

## Pre-flight

| 项 | 结论 |
|----|------|
| 前置 | F25 Offscreen（**健全**）；F27 RegisterExternal（**必需，实现前须 Done**） |
| 技术债风险 | 若无 F27 仅 blit 上线 → **中**；双路径永久并存 → **高** |
| WIP | F28 编码排在 F27 S01 之后 |
| 哲学 | Texture2D LSP 服务 UI + Offscreen 薄策略；Core 无玩法类型 |
| 建议 | **有条件 Go：** 设计可定；编码等 F27 S01 绿。Done 定义不含永久 blit。 |

## 问题

1. `GetColorTexture()` / `RenderOnce()` 返回的是 RDG Publish 的池化 SceneColor；再渲会覆写旧 icon。
2. `ImageComponent::SetTexture` 等要 `Texture2D*`，不要 RHI。
3. 今日 `Texture2D` 偏加载/像素创建（多为 `ShaderResource`），没有一等「同时也是颜色 RT」类型。
4. 反馈中的 Wrap/Adopt 不作产品叙事（见方案对比）。

## 需求

- R1：`Texture2DRenderTarget` **is-a** `Texture2D`（反射 + `shared_ptr<Texture2D>` / `Cast` 可供 UI）。
- R2：工厂创建带 `RenderTarget | ShaderResource` 的 GPU 纹理；默认 RGBA8；宽高由调用方指定。
- R3：采样 API（`SetSamplerFilter` / `SetSamplerAddress`）与基类一致（icon 用 Nearest+Clamp）。
- R4：`OffscreenRenderer` 可渲入调用方持有的 `Texture2DRenderTarget`；`RenderOnce` 后该对象 `GetRHITexture()` 含绘制结果；同一 Offscreen 再渲**另一**目标时**不得**改写前一目标的纹素。
- R5：游戏 catalog 可按 block 持有 `shared_ptr<Texture2D>`（或子类），无池化别名串色。
- R6：未 `SetColorTarget` 时行为与 F25 一致（图分配 SceneColor + `GetColorTexture`），缩略图等可继续工作。
- R7：运行时 icon RT **不必** cook `.metex` / 进 AssetManager（无磁盘身份）；但仍是 **`MEObject`/`Asset` 实例**，须经 **`NewObject`** 创建并登记 ObjectManager（见「对象创建」）。

## 约束

- C1：**依赖 RND-F27** 做零拷贝 SceneColor；不得在永久 blit 双路径下宣称 F28 Done。
- C2：Depth 可继续由图分配（F27 Depth External 可选）；产品面是 color。
- C3：`feat/render` 上向前演进；无 Wrap 兼容层。
- C4：源码注释不含 Feature/Slice ID。

## 偏好

- P1：类名 `Texture2DRenderTarget`。
- P2：公开工厂 `Texture2DRenderTarget::Create`，不是 `WrapRHITexture`。
- P3：Offscreen 用 `SetColorTarget`；`RenderOnce` **保持**现有返回类型以兼容 Thumbnail（见 API，评审锁定）。

## 假设

- A1：icon 尺寸小（如 64–256）；N 张 RT 显存可接受。
- A2：使用 SetColorTarget 时，`Initialize(w,h)` 与目标尺寸一致，且 ForwardRenderer/图的 backbuffer 尺寸解析与之一致（Offscreen 须 `SetBackbufferDimensions` 与 Initialize 对齐——实现时核对现路径）。
- A3：子类可写基类 protected 的 `m_RHITexture` / 尺寸，或经引擎内部 protected 助手（**非**游戏面 Wrap）。

## 非目标

- 完整 UE `ConstructTexture2D` 编辑器烘培资产管线。
- Godot 式不拥有像素的 live `ViewportTexture`（可另开 Feature）。
- 公开 `Texture2D::WrapRHITexture`。
- TextureCubeRenderTarget（Cube 维持现有 EnvMap 路径）。
- 改 ImageComponent API（基类 LSP 即可）。
- 本 Feature 内把 Texture2DRenderTarget 做成 Content Browser 资产类型。

## 当前架构

- `Texture2D : Asset` — 尺寸、采样、`m_RHITexture`、`CreateFromRGBA8` / `CreateSolidRGBA`。
- `OffscreenRenderer` 持有 `SceneViewport` → 自 RDG SceneColor Publish。
- UI 采样 `GetRHITexture()` + preferred sampler。
- **现有调用方：** `AssetThumbnailService` 使用 `const RHITextureRef& t = offscreen.RenderOnce();` —— **不可破坏。**

## 方案架构

### 类型

```cpp
ME_CLASS()
class MINENGINE_API Texture2DRenderTarget : public Texture2D
{
    ME_GENERATED_BODY()
public:
    static std::shared_ptr<Texture2DRenderTarget> Create(
        RHI& rhi,
        uint32_t width,
        uint32_t height,
        TextureFormat format = TextureFormat::RGBA8);

    bool Resize(RHI& rhi, uint32_t width, uint32_t height);

    TextureFormat GetRenderTargetFormat() const { return m_RenderTargetFormat; }

protected:
    TextureFormat m_RenderTargetFormat = TextureFormat::RGBA8;
};
```

创建：`RHICreateTexture2D`，flags = `RenderTarget | ShaderResource`（对齐 `SceneRenderTarget::MakeColorDesc` / UE RenderTargetable|ShaderResource）。

### 基类小改动（评审锁定）

- 在 `Texture2D` 增加 `const RHITextureRef& GetRHITextureShared() const`（或返回拷贝 `RHITextureRef`），供 RegisterExternal / Offscreen 使用，避免 friendship 蔓延。

### Offscreen（评审锁定 API）

```cpp
class OffscreenRenderer
{
    void SetColorTarget(const std::shared_ptr<Texture2DRenderTarget>& target);
    void ClearColorTarget();
    const std::shared_ptr<Texture2DRenderTarget>& GetColorTarget() const;

    /** 保持 F25 签名：返回颜色 RHI ref（有 target 时即该 target 的 GPU 纹理）。 */
    const RHITextureRef& RenderOnce();

    const RHITextureRef& GetColorTexture() const;
    const RHITextureRef& GetDepthTexture() const;
};
```

**不要**把 `RenderOnce` 改成返回 `shared_ptr<Texture2DRenderTarget>`（会破坏 Thumbnail）。游戏取产品对象用 `GetColorTarget()` / 自己持有的 `Create` 结果。

**有 target 时的控制路径：**

1. `Initialize` 尺寸 == target 尺寸，否则失败。
2. Offscreen 把 `SceneDrawDesc.ExternalSceneColor = target->GetRHITextureShared()`（字段名可定）。
3. ForwardRenderer 在 Bake 之后、SetupAttachments 之前：`RegisterExternalTexture(kRDGSceneColor, desc.ExternalSceneColor)`；帧末或绘制后 `Unregister`（**每帧 RenderOnce 后注销**，避免无 target 时残留）。
4. 绘制完成后像素已在 target 内；Publish 到 SRT 可指向同一 ref（无害）。
5. `RenderOnce` / `GetColorTexture` 返回该纹理的 `RHITextureRef`。

### 尺寸契约（评审补充）

- `SetColorTarget` 前须已 `Initialize(w,h)` 且 `w,h` 与 target 一致。
- Offscreen / SceneViewport 须保证本视图 Bake 得到的 SceneColor **物理尺寸**等于 `(w,h)`（沿用 F25 已设 backbuffer/viewport 尺寸的路径；实现时加断言或 Error）。
- 不一致 → `RenderOnce` 失败（空 ref + Error），不部分绘制。

### 游戏用法（minCraft）

```cpp
auto icon = Texture2DRenderTarget::Create(rhi, 128, 128);
icon->SetSamplerFilter(RHISamplerFilter::Nearest);
icon->SetSamplerAddress(RHISamplerAddress::Clamp);

offscreen.Initialize(128, 128);
offscreen.SetColorTarget(icon);
offscreen.SetScene(preview.GetRenderScene());
offscreen.SetClearColor(0, 0, 0, 0);
offscreen.RenderOnce();  // 仍返回 RHITextureRef；icon 已被写入

catalog.Set(blockId, std::static_pointer_cast<Texture2D>(icon));
image->SetTexture(catalog.Get(blockId));
```

## 职责

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `Texture2DRenderTarget` | 分配/Resize 可 RT 的 GPU 纹；作为 Texture2D 采样面 | Offscreen / RDG |
| `Texture2D` | 采样 + GetRHITexture(Shared) | RT 标志策略 |
| `OffscreenRenderer` | 设置 target、尺寸检查、填 ExternalSceneColor、提交 | icon catalog / 玩法 |
| `ForwardRenderer` | 按 Desc 调用 F27 Register/Unregister | 产品类型 |
| `RenderGraph`（F27） | External physical slot | 产品类型 |

## 边界

- **允许：** 游戏 → Texture2DRenderTarget → ImageComponent（基类）；Offscreen → Desc → F27。
- **禁止：** 游戏为 bake 去 include RDG；文档把「只返回 RHI」写成新主路径；公开 Wrap。

## 依赖

- **依赖：** `RND-F27` S01 Done，才能做 F28 S02（渲入）。
- **依赖：** `RND-F25` Done。
- **被依赖：** minCraft BlockItemIconRenderer 迁移（消费仓）。

## 所有权与寿命

- Catalog / 游戏拥有 `shared_ptr<Texture2DRenderTarget>`（或基类指针）。
- Offscreen 在 Set 期间持有 `shared_ptr m_ColorTarget`。
- 销毁 Offscreen 不得销毁游戏仍持有的 target。
- 对**当前已绑定**的 target 做 Resize → **非法**；实现应 ClearColorTarget 或直接拒绝 Resize。

## 数据流

`Create RT → SetColorTarget → Desc.ExternalSceneColor → RegisterExternal(SceneColor) → 绘制 → 像素在 RT → UI 按 Texture2D 采样`

## 控制流

与 F25 相同的同步 `SubmitSceneDraw`。Register 挂在 ForwardRenderer、紧贴 SetupAttachments（经 SceneDrawDesc），游戏不直接调 RDG。

## 状态与不变量

- I1：Create/Resize 成功后 RHI flags 含 RenderTarget|ShaderResource。
- I2：有 color target 时，RenderOnce 后内容属于该对象；再渲另一 target 不改前一对象纹素。
- I3：仅 Create 为 RT 的对象可 `dynamic_pointer_cast` 为 Texture2DRenderTarget；cook 的 Texture2D 不是 RT。
- I4：LSP——只采样的 `Texture2D*` 调用方对子类行为正确（不假设可写）。

## API 语义

**`Texture2DRenderTarget::Create`**

- 创建运行时对象 + GPU 资源；失败返回 nullptr。
- 默认不 Register AssetManager（无 `.metex` / 无 Content 路径）。
- **对象创建（评审修订锁定）：`NewObject<Texture2DRenderTarget>(debugName)`**（可带生成 GUID），再写入 RHI 尺寸/flags。  
  - **禁止** `std::make_shared<Texture2DRenderTarget>` 作为产品路径。  
  - 理由：`Texture2D`/`Asset`/`MEObject` 须走 ObjectManager（`SetClass`、Guid 登记、`FindObject`、GC/weak 表）；`make_shared` 会得到「像 MEObject、却不在表里」的野实例。  
  - **勿抄** `Texture2D::CreateFromRGBA8` / `CreateSolidRGBA` 的 `make_shared`——那是历史运行时捷径（字图集/白贴/默认参数），本身已脱离 ObjectManager，**不是**新正式子类的范本；若日后收债应另开卫生项，不在本 Feature 扩大债务。

**`SetColorTarget`**

- 存 shared_ptr；nullptr 清除。
- 本身不渲染。

**`RenderOnce`**

- 签名不变：`const RHITextureRef&`。
- 有 target：尺寸检查 → 注册 External → 提交 → 返回 target 的 RHI ref。
- 无 target：遗留行为；返回图 Publish 的 color。

## 失败模型

| 失败 | 行为 |
|------|------|
| Create 时 w/h 为 0 | nullptr |
| target 尺寸 ≠ Initialize | RenderOnce 失败；空/静态 empty ref + Error |
| F27 Register / 校验失败 | RenderOnce 失败；target 内容保持不变 |
| scene/camera 为空 | 同 F25 |

## 方案对比

### 方案 A（推荐）— 子类 + F27 渲入

- 优点：符合产品意图；UI LSP；每 icon 所有权清晰；对齐 UE「RT 即贴图」。
- 缺点：新反射类型；依赖 F27。

### 方案 B — 仅 `Texture2D::CreateRenderTarget` 无子类

- 未作主方案：主人要求正式子类；且便于日后扩展 RT 专用字段。

### 方案 C — WrapRHITexture

- 拒绝：叙事错误；易别名池化纹理。

### 方案 D — 每帧从池化 SceneColor blit

- 仅当 F27 阻塞时的应急；须记技术债且退出条件 = F27。**不是** F28 Done 定义。

## 集成影响

- 新类（或扩 `Texture.h/.cpp`）+ 反射 codegen。
- `OffscreenRenderer`、`SceneDrawDesc`、`ForwardRenderer` Register 钩子。
- 测试套件；F25 文档交叉链接；`MINENGINE_API` 导出。

## 兼容与迁移

- F25 API 保留；`RenderOnce` 签名保留；新路径经 `SetColorTarget` 选择加入。
- 无 Wrap shim。
- Thumbnail 可选日后 SetColorTarget——非本 Feature 必做。

## 验证策略

- 套件名建议：`texture-render-target`（或扩 offscreen 套件）：
  1. Create → Nearest/Clamp → GetRHITexture 非空；flags 含 RenderTarget。
  2. 两张 RT A/B；依次渲入；readback（或现有离屏读回手段）证明 A 在渲 B 后不变。
  3. 以 `shared_ptr<Texture2D>` 交给 ImageComponent 或等价采样检查。
  4. 尺寸不匹配 → 失败关闭。
  5. **回归：** 未 SetColorTarget 时 Thumbnail/既有 offscreen 用例；`RenderOnce` 仍返回可用 `RHITextureRef`。
- 消费方 minCraft 迁移在引擎 DoD 之后（消费仓跟踪）。

## 开放问题（评审后状态）

1. NewObject vs make_shared？— **`NewObject`（主人指出后修订）。** 已锁定。  
2. 基类 `GetRHITextureShared`？— **要。** 已接受。  
3. SceneDrawDesc.ExternalSceneColor vs Offscreen 直调图？— **SceneDrawDesc + ForwardRenderer。** 已接受。  
4. AssetRegistry 类型串？— **F28 不做。** 已接受。

## 设计评审

- **Verdict:** `Ready with deferred items`
- **Reviewer / date:** agent · 2026-10-25 · Mode A（与 F27 一并）
- **摘要：** 产品形状正确；硬依赖 F27。关键修正：`RenderOnce` 保持 `RHITextureRef&`；Desc 接线；**创建用 `NewObject`（非 make_shared）**；`GetRHITextureShared`；每帧 Unregister。Defer：缩略图 SetColorTarget、CB 资产类型。
- **阻塞实现？** **是（编码）**——须等 F27 S01 Done；设计本身可在主人确认后升 Planned，S01（仅类型+Create 测试）可与 F27 后期并行。

## 实现就绪

- [x] 需求 / 架构 / API / 失败 / 验证
- [x] 开放问题已锁定
- [ ] 主人确认 → Planned
- [ ] F27 S01 Done 后开 S02

**可否大规模开工？** F27 S01 之前：**仅 S01（类型）**；渲入：**否**。

## 建议切片

| Slice | 内容 | 依赖 |
|-------|------|------|
| S01 | `Texture2DRenderTarget` + Create/Resize + 反射 + 创建单测 + `GetRHITextureShared` | — |
| S02 | SceneDrawDesc.ExternalSceneColor + ForwardRenderer Register + Offscreen SetColorTarget | F27 S01 |
| S03 | 双目标隔离 + 采样/UI 冒烟 | S02 |
| S04 | 文档 / F25 交叉链接 | S02 |

## 验收清单

- [x] 设计评审完成（本文）
- [x] 主人确认 / Planned（与 F27 同批授权实现）
- [x] F27 S01 Done 先于 / 同批 F28 S02
- [x] S01：`Texture2DRenderTarget` Create/Resize + 反射 stub + Create 单测 + `GetRHITextureShared`
- [x] S02：`SceneDrawDesc.ExternalSceneColor` + Forward Register + Offscreen `SetColorTarget`
- [x] `Texture2D::CreateFromRGBA8` / `CreateSolidRGBA` 改 `NewObject`
- [x] `minEngineTests.exe test render-graph` PASS（含 Create / RegisterExternal）
- [x] Progress / Registry 同步
- [ ] S03（可选）：双 Offscreen 目标隔离 + UI 采样冒烟
- [ ] S04：交叉链到 F25（可选文档抛光）
