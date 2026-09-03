# ANIM-F01 — Skeletal Mesh Pipeline — Design Spec

## Meta
- **ID:** `ANIM-F01`
- **Type:** Feature
- **Status:** Review
- **Owner:** project maintainer
- **Last updated:** 2026-09-03（竖切目视通过；下一焦点 ASSET-F01）
- **Branch:** `feat/animation`
- **Related:**
  - [Implementation](./ANIM-F01_SKELETAL_MESH_PIPELINE_IMPLEMENTATION.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - Follow-ups: [ANIM-F02 Clip Playback](./ANIM-F02_CLIP_PLAYBACK_DESIGN.md) · [ANIM-F03 Animation Graph](./ANIM-F03_ANIMATION_GRAPH_DESIGN.md)
  - Input brief (reference): [`docs/external/minEngine — 3D Animation System Development Brief.md`](../../external/minEngine%20—%203D%20Animation%20System%20Development%20Brief.md)
  - Code anchors: `StaticMeshLoader` / `StaticMesh` / `StaticMeshComponent` / `StaticMeshSceneProxy` / `ForwardRenderer::BuildRenderQueue` / `GLSLMaterialShellAssemblerImpl`
  - Reference (sibling trees): UE `UStaticMesh` / `USkeletalMesh` / `USkinnedAsset`；Godot `Mesh` + `Skin` + `Skeleton3D`
- **Depends on:** `CORE-F05` Play Mode MVP（Done）、`CORE-F06` Component Activate（Done）；复用现有 Asset / RHI / Forward 路径
- **Supersedes:** 旧占位 `ANIM-F01_ANIMATION_SYSTEM_DESIGN.md`（本文件取代）

## TL;DR
打通 **骨骼网格体竖切**：Assimp 导入 Skeleton + 蒙皮网格 → Runtime `Pose` / skinning palette → GPU skinning 绘制。  
产品形态上与 **`StaticMesh` 平行复用**（Asset / Loader / Component / Proxy / Material 壳变体），差异收敛在「蒙皮顶点布局 + palette + Pose」；**不**把蒙皮塞进 `StaticMesh`。  
本 Feature **不包含** Clip / Animator / Event；验收以「导入角色在 Bind / 手工 Pose 下正确变形」为准。

## Scope
- **In:**
  - Runtime 类型：`Skeleton`、`Bone`、`Pose`、local→global、skinning palette
  - 资产：`Skeleton`、`SkeletalMesh`（独立于 `StaticMesh`）
  - 导入：从 FBX/glTF 抽取层级、bind / inverse-bind、每顶点 ≤4 influences（indices + weights）
  - 命名澄清：现有 `MeshLoader`（仅静态几何 Assimp→CPU）→ 归入 **Static** 命名空间/类型（见 §2.2）；平行增加 `SkeletalMeshLoader`
  - 渲染：`SkeletalMeshComponent` + SceneProxy；材质壳按 **Mesh 变形模式** 选 VS 模板；per-draw bone palette
  - 验证：层次 / palette 单测 + Editor/Playground 可视冒烟
- **Out:**
  - `AnimationClip`、Player、State Machine / Animation Graph、Animation Event（→ F02 / F03；Event 暂不排期）
  - IK、Root Motion、Retarget、Layer / Additive / Mask、Blend Tree、压缩、Anim Job / LOD
  - Cooked 二进制网格（短期仍「meta + 源文件再导入」；记债）
  - Godot 式「单一 Mesh 资产 + 可选骨头属性」合并产品类型（见 §2.1）
  - 过早抽象重量级 `UMeshComponent` / 共享 LOD 框架（F01 只做薄复用，见 §2.1）
  - 完美 multi-section / 每 section 材质（若与 StaticMesh 同债，可 TD，不挡竖切）

## Reader quick start
1. 本文件：边界、**Static∥Skeletal 平行地基**、数据流、**§2.7 数据结构/接口**、Render/Asset 契约
2. 实现切片：[Implementation Plan](./ANIM-F01_SKELETAL_MESH_PIPELINE_IMPLEMENTATION.md)
3. 代码入口：`Loaders/StaticMeshLoader.*`、`AssimpMeshImportUtil.*`、`StaticMesh*`、`StaticMeshSceneProxy.*`；Animation 核：`Runtime/Function/Animation/`

---

## 1) 背景与目标

### 1.1 现状
- Assimp 仅导入 Position / UV / Normal / Tangent；无 `aiBone`、无权重。
- `StaticMesh` → `StaticMeshSceneProxy` → `MeshDrawPacket`；VS 为刚体 `u_Model * position`。
- Registry 有 `ANIM-F01` 占位；**无**动画 Runtime 代码。

### 1.2 目标
形成可复用的 **Pose → Palette → GPU** 管道，使后续 F02（Clip）只接在 Pose 生成端，而不重做渲染。

**成功标准：** 同一套 Skeleton + SkeletalMesh，在 Bind Pose 与「单骨人工偏移」下，屏幕上变形正确；Assimp 不进入 Runtime 帧循环。

### 1.3 系列定位

| Feature | 职责 |
|---------|------|
| **ANIM-F01**（本文件） | 资产 + 蒙皮渲染地基 |
| **ANIM-F02** | Clip 求值 + Player |
| **ANIM-F03** | Animation Graph MVP（SM + Params + Transition Blend） |
| Event / IK / Root Motion / Retarget | 不排期或后续 Feature |

---

## 2) 方案

### 2.1 StaticMesh ∥ SkeletalMesh：平行产品线（地基）

现状已是 **UE 形** 半条链路：`StaticMesh` → `StaticMeshComponent` → `StaticMeshSceneProxy` → `MeshDrawPacket`。  
F01 应补齐对称的另一条，而不是把蒙皮做成 `StaticMesh` 的可选字段（Godot 的「一个 Mesh + 可选 bones」在编辑器一体性上更省，但会把 layout / PSO / 导入语义缠死；minEngine 规模下 **平行类型更清晰**）。

#### 参考结论

| 来源 | 可借 | 刻意不借（本期） |
|------|------|------------------|
| **UE** | 平行资产（`UStaticMesh` vs `USkeletalMesh` / `USkinnedAsset`）；平行 Component / SceneProxy；蒙皮差异在 vertex factory + bone buffer，几何/section/材质索引同构 | `USkinnedAsset` 大抽象、异步编译、per-section BoneMap 优化、Morph/Cloth |
| **Godot** | **Skeleton / Pose 与 Mesh 解耦**（`Skeleton3D` + `Skin` vs `Mesh`）；蒙皮是「接线」不是第二种几何语言 | 单一 `ArrayMesh` 同时承载 static/skinned；场景里用 path 挂 Skeleton |

**minEngine 默认：** UE 的产品线切分 + Godot 的「Pose/Skeleton 不塞进 Mesh 资产当每帧状态」。

#### 同构（应复用 / 镜像）

```text
                 Static                         Skeletal
Asset            StaticMesh                     SkeletalMesh (+ Skeleton 引用)
Import CPU       StaticMeshImport*              SkeletalMeshImport*
GPU build        StaticMeshLoader               SkeletalMeshLoader
Component        StaticMeshComponent            SkeletalMeshComponent
SceneProxy       StaticMeshSceneProxy           SkeletalMeshSceneProxy
Draw             MeshDrawCommand/Packet         同左（额外 skinning bind）
Material         Material 资产                  同左（编译时选 VS 变体）
```

| 层 | 共性 | 差异（Skeletal 侧） |
|----|------|---------------------|
| **资源管线** | `.meta` + 源文件；Assimp 只在 Import/Load；`AssetTypeRegistry` 登记；sections / AABB / 材质槽位概念 | 多产出 `Skeleton`；顶点含 bone index/weight；inverse bind |
| **Component** | 挂 Mesh + Material；`CreateSceneProxy`；bounds；随 Transform | 持有 / 写入 `Pose`；每帧（或脏时）生成 palette |
| **Proxy / Queue** | 非拥有 VB/IB/layout；进 `BuildRenderQueue`；走 Base/Shadow/Translucency | 携带 palette 或句柄；layout 含蒙皮属性 |
| **材质** | 同一 `Material` 图 / 着色模型 / 纹理绑定；FS 侧基本不变 | **VS 壳**按变形模式分支：rigid vs skinning（见下） |
| **RHI** | Buffer、VertexInputLayout、Descriptor 习惯 | Bone palette UBO/SSBO；可能更大 stride |

#### 材质 / Shader 模板（关键设计点）

今日 `GLSLMaterialShellAssemblerImpl::BuildVertexIoBlock` **写死** rigid 属性（P/UV/N/T）。地基应显式引入：

```text
MeshDeformationMode = Rigid | Skinned   // 编译环境或 shell 参数
```

- **同一 Material 资产**可挂在 Static 或 Skeletal Component 上；**编译 / PSO key** 必须包含 deformation 模式（或等价：vertex layout 签名）。
- FS / 光照 / IBL **共享**；仅 VS 入口：是否 `Σ wᵢ Palette[i]` 再乘 `u_Model`。
- ShadowPass **必须**使用同一 skinned VS 变体，避免剪影与主 pass 不一致。
- F01 不做「运行时同一 Material 动态在两种 layout 间切换」的花活：Component 类型决定需要的变体；缺变体则编译或报错。

可选后续（**非 F01 必做**）：薄基类 `MeshComponent` 抽「Material + bounds + proxy 生命周期」——仅当两份 Component 出现真实重复时再抽，避免空抽象。

#### 分层与所有权（数据流）

```text
Import (Assimp, tool/load path only)
        ↓
Skeleton Asset  ·  SkeletalMesh Asset
        ↓
Gameplay / 调试：写 Pose（F01：Bind 或手工；F02+：Clip/Graph）
        ↓
Pose (local TRS per bone)
        ↓
Skeleton::LocalToGlobal → Global pose
        ↓
SkinningPalette[i] = Global[i] * InverseBind[i]
        ↓
SkeletalMeshSceneProxy / MeshDraw*
        ↓
RHI + Skinned vertex shader（Material shell 变体）
```

| 模块 | 负责 | 不负责 |
|------|------|--------|
| Animation / Pose 核 | Pose、层次、palette 数学 | Pass、PSO、GL/VK |
| Asset / Import | 源格式 → Runtime 资产 | 每帧求值 |
| Render | VB/IB/layout、palette 绑定、skinned VS | Clip / 状态机 |
| Component | 持有 mesh 引用、每帧提交 palette、SceneProxy | 解析 FBX |

### 2.2 导入命名与资产模型

#### Loader 命名（澄清现状）

当前职责分裂，名字却偏「通用 Mesh」：

| 现状类型 | 实际职责 |
|----------|----------|
| `MeshLoader` | Assimp → **仅静态** `MeshImportData`（P/UV/N/T） |
| `StaticMeshLoader` | ImportData → GPU `StaticMesh` |

**决策：**
1. 将 `MeshLoader` / `MeshImportData` / `MeshImportVertex` **收束到 Static 命名**（推荐：`StaticMeshImportData` + `StaticMeshLoader::ImportFromFile` 或独立 `StaticMeshImporter`，与现有 `CreateFromImportData` / `LoadFromAssetMeta` 同文件或同目录并列）。
2. 新增平行线：`SkeletalMeshImportData`（几何 + influences）+ `SkeletalMeshLoader`（Import → GPU `SkeletalMesh`，并可产出/填充 `Skeleton`）。
3. **共享**的只有真正与变形无关的 Assimp 工具（三角化、切线、坐标系约定）——可放 `AssimpMeshImportUtil` 之类，**不要**再叫泛化的 `MeshLoader`。
4. 改名可放在 F01 早期切片（与 S01 导入一起），避免骨骼导入继续挂在误导性 API 下。

#### 资产

**`Skeleton`（Asset）**
- 有序 `Bone[]`：`Name`、`ParentIndex`（根 = -1）、`LocalBind`（TRS 或 matrix）、`InverseBindMatrix`
- Import 期完成 `Name → Index`；Runtime 查找以 Index 为主，Name 仅调试 / 工具
- 可被多个 `SkeletalMesh` / 后续 Clip 引用

**`SkeletalMesh`（Asset）**
- 引用 `Skeleton`（GUID / 强类型 Asset 引用，跟进现有 Asset 惯例）
- 顶点：现有 PBR 属性 + `BoneIndices`（uint8×4 或 uint16×4）+ `BoneWeights`（float×4，归一化）
- Index buffer、sections（可先单材质全量 draw，与 StaticMesh 行为对齐）
- `InverseBind` 以 Skeleton 为准；若源文件 per-mesh bind 与 skeleton 不一致，Import 期校验 / 失败并报错

**与 `StaticMesh` 关系：** 平行类型，**不**让 StaticMesh「可选蒙皮」。几何导入工具可共享；类型、Import DTO、GPU layout、Loader 入口分离。

### 2.3 Pose 与 Skinning 契约

```text
Pose.local[i]     // Transform relative to parent
Global[i]         // parentGlobal * local[i]  （根用 Component/Actor model 或单位，见下）
Palette[i]        // Global[i] * InverseBind[i]
```

**Model / Component 变换：**
- Palette 在 **mesh bind 空间** 内计算；Component 的 `GetWorldMatrix()` 仍作 **整体** `u_Model`（或等价），与现有 StaticMesh 一致。
- 即：skinning 在「资源绑定空间」完成，再乘 Entity 变换——避免把 Actor 平移 bake 进每骨矩阵（除非后续 Root Motion 另议；本 Feature Out）。

**Influences：** 硬上限 **4**；Import 时超出则保留权重最大的 4 并重归一化，打 warning。

**F01 的 Pose 来源（仅本 Feature）：**
1. Bind / rest Pose（从 Skeleton local bind 填充）
2. 调试 API：按 bone index 覆盖 local（单测 / 临时 Demo）

不引入 Player。

### 2.4 导入管线

平行于 Static 线，经 `SkeletalMeshLoader`（Assimp 仅此处）：

| 步骤 | 产出 |
|------|------|
| 遍历 `aiNode` 建骨层级 | `Skeleton` |
| `aiMesh::mBones` → 权重表 | per-vertex indices/weights |
| offset matrix → InverseBind | 写入 Bone |
| 几何 + tangent 管线 | 复用 `AssimpMeshImportUtil`（自原 Static 导入逻辑抽出） |
| 坐标系 / 单位 | 与当前 StaticMesh 导入一致；差异记风险表 |

**AssetTypeRegistry：** 新增 `Skeleton`、`SkeletalMesh`。  
扩展名：继续 `.fbx` / `.gltf`；`.glb` 若未注册可本 Feature 顺带补登记（小项）。

**加载策略（MVP）：** 与 StaticMesh 相同——`.meta` + 源文件，Load 时再 Import 上传 GPU。Cooked 格式 → TECH_DEBT，不挡验收。

**同一源文件多资产：** 一次 Import 可写出 Skeleton + SkeletalMesh 两个 meta/GUID（具体 UX 跟现有 `ImportAsset`；若只能单类型，先「导入为 SkeletalMesh 并内嵌/旁路生成 Skeleton」——实现计划里定，Design 要求 **逻辑上 Skeleton 可独立引用**）。

### 2.5 渲染接入

1. **Vertex layout（skinned）：**  
   `Position, TexCoord, Normal, Tangent, BoneIndices, BoneWeights`  
   材质壳 `BuildVertexIoBlock` 按 `MeshDeformationMode::Skinned` 追加蒙皮 attribute。

2. **VS：**  
   `skinMatrix = Σ wᵢ * Palette[indexᵢ]`；变换 P/N/T；再乘 `u_Model`（及现有 view-proj 路径）。

3. **Palette 上传：**  
   每 draw（或每 proxy）绑定 bone matrix 数组（UBO/SSBO 按 RHI 能力选型；骨数上限初版可固定，例如 128/256，超限 Import 失败或截断并报错）。  
   （UE 的 per-section BoneMap 压缩属优化，F01 用全骨架 palette 即可。）

4. **Queue：**  
   `SkeletalMeshSceneProxy` 进入 `BuildRenderQueue`（与 Static **并列分支**，镜像现有 Static 路径）；复用 `MeshDrawPacket` / 材质 bind；差异在 layout + skinning set + PSO key。

5. **阴影 / 透明：** 与 BasePass 同 proxy 类型入队；skinned VS 同步用于 ShadowPass（避免 silhouette 错误）。

### 2.6 Component

- `SkeletalMeshComponent`：引用 `SkeletalMesh` + Material（镜像 `StaticMeshComponent` API 面）
- 持有或缓存当前 `Pose` / palette（F01 由组件或轻量 helper 填 Bind / debug）
- `CreateSceneProxy()` 提交 VB/IB/layout/material + palette 句柄或 CPU 快照（跟现有 proxy 寿命模型对齐）

不在本 Feature 引入 `Animator` 组件。

### 2.7 数据结构与接口（详设）

> 下列为 **契约草图**（C++ 风格示意，非最终头文件）。命名与成员前缀遵循工程惯例（`m_`）；实现时可微调，语义不变。

#### 2.7.1 常量与枚举

```cpp
enum class MeshDeformationMode : uint8_t
{
    Rigid = 0,
    Skinned = 1,
};

// F01 defaults — Import / palette 校验用
constexpr int32_t kMaxBoneInfluences = 4;
constexpr int32_t kMaxBonesPerSkeleton = 256; // palette / UBO 上限；超限 Import 失败
```

- `MeshDeformationMode` 进入 Material 编译环境与 PSO key。
- Bone index 在 GPU 侧：优先 `Int4`（现有 `VertexElementType` 已有）；若后续补 `UByte4`/`UShort4` 再收紧带宽。

#### 2.7.2 Skeleton / Bone / Pose

```cpp
struct SkeletonBone
{
    std::string Name;                 // import / 调试；Runtime 热路径不用
    int32_t ParentIndex = -1;         // -1 = root
    Transform LocalBind;              // 相对父骨的 bind local TRS（复用现有 Transform）
    Matrix4 InverseBindPose;          // mesh-bind → bone 空间；蒙皮用
};

class Skeleton : public Asset
{
public:
    int32_t GetBoneCount() const;
    const SkeletonBone& GetBone(int32_t index) const;
    int32_t FindBoneIndex(std::string_view name) const; // 冷路径 / 工具

    // 用 LocalBind 填满 Pose（F01 默认 rest）
    void FillBindPose(Pose& outPose) const;

    // local Pose → model-space global matrices（与 Component u_Model 无关）
    void LocalToGlobal(const Pose& localPose, std::vector<Matrix4>& outGlobal) const;

    // outPalette[i] = global[i] * InverseBindPose[i]
    void BuildSkinningPalette(const Pose& localPose, std::vector<Matrix4>& outPalette) const;

private:
    std::vector<SkeletonBone> m_Bones;
    // optional: unordered_map name→index built at import
};

struct Pose
{
    // size == Skeleton::GetBoneCount()；存 **local** TRS
    std::vector<Transform> LocalTransforms;

    void ResetToIdentity(size_t boneCount);
    Transform& At(int32_t boneIndex);
    const Transform& At(int32_t boneIndex) const;
};
```

**不变量：**
- `Pose` **只**表达 local；global / palette 是派生量，由 `Skeleton` 计算。
- Clip（F02）与手工调试都只写 `Pose`，不直接写 palette。
- `InverseBindPose` 只读自 Skeleton；SkeletalMesh 不复制一份互相打架的 inv-bind。

#### 2.7.3 SkeletalMesh 资产与 CPU 顶点

```cpp
struct SkeletalMeshSectionInfo  // 对齐 StaticMeshSectionInfo
{
    int32_t MaterialIndex = 0;
    uint32_t FirstIndex = 0;
    uint32_t NumIndices = 0;
};

struct SkeletalMeshVertex  // Import / 交错打包前的逻辑布局
{
    Vector3 Position;
    Vector2 TexCoord;
    Vector3 Normal;
    Vector4 Tangent;              // xyz + sign
    uint16_t BoneIndices[4];      // CPU 用 16-bit；上传可扩成 Int4
    Vector4 BoneWeights;          // 归一化，Σ ≈ 1
};

class SkeletalMesh : public Asset
{
public:
    Skeleton* GetSkeleton() const;
    void SetSkeleton(const std::shared_ptr<Skeleton>& skeleton);

    // GPU（与 StaticMesh 同构字段名，便于 Proxy 镜像）
    RHIBufferRef m_VertexBuffer;
    RHIVertexInputLayoutRef m_VertexInputLayout;
    RHIBufferRef m_IndexBuffer;
    Math::Geometry::AABB m_BoundingBox;
    std::vector<SkeletalMeshSectionInfo> m_Sections;
    std::vector<std::shared_ptr<Material>> m_Materials; // F01 可只用 [0]

private:
    std::shared_ptr<Skeleton> m_Skeleton;
};
```

**Skinned layout（location 建议）：**

| Loc | Attribute | Type |
|-----|-----------|------|
| 0 | `a_Position` | Float3 |
| 1 | `a_TexCoord` | Float2 |
| 2 | `a_Normal` | Float3 |
| 3 | `a_Tangent` | Float4 |
| 4 | `a_BoneIndices` | Int4 |
| 5 | `a_BoneWeights` | Float4 |

#### 2.7.4 Import DTO 与 Loader API

```cpp
struct SkeletalMeshImportData
{
    std::vector<SkeletalMeshVertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<SkeletalMeshSectionInfo> Sections;
    Math::Geometry::AABB BoundingBox;
    // 并行产出：骨表（可先建 Skeleton 再填 mesh）
    std::vector<SkeletonBone> Bones;
    bool IsValid() const;
};

class SkeletalMeshLoader
{
public:
    static bool ImportFromFile(
        const std::string& path,
        SkeletalMeshImportData& outMesh,
        std::string* outError = nullptr);

    static std::shared_ptr<Skeleton> CreateSkeletonFromImport(
        const AssetMeta& meta,
        const SkeletalMeshImportData& data);

    static std::shared_ptr<SkeletalMesh> CreateFromImportData(
        const AssetMeta& meta,
        SkeletalMeshImportData& data,
        const std::shared_ptr<Skeleton>& skeleton);

    static std::shared_ptr<SkeletalMesh> LoadFromAssetMeta(const AssetMeta& meta);
};
```

Static 侧对称（改名后）：

```cpp
// 原 MeshImport* → StaticMeshImport*
class StaticMeshLoader
{
    static bool ImportFromFile(...);           // 原 MeshLoader::ImportFromFile
    static std::shared_ptr<StaticMesh> CreateFromImportData(...);
    static std::shared_ptr<StaticMesh> LoadFromAssetMeta(...);
};
```

共享：`AssimpMeshImportUtil`（三角化 flags、切线、UV fallback、坐标约定）—— **无** bone API。

#### 2.7.5 Component / Proxy 接口

```cpp
class SkeletalMeshComponent : public PrimitiveComponent
{
public:
    void SetMesh(const std::shared_ptr<SkeletalMesh>& mesh);
    SkeletalMesh* GetMesh() const;

    void SetMaterial(const std::shared_ptr<Material>& material);
    Material* GetMaterial() const;

    // Pose 写入（F01：Bind / 调试；F02：Player 写入）
    void SetLocalPose(const Pose& pose);
    const Pose& GetLocalPose() const;
    void ResetToBindPose();

    // 调试：改单骨 local，标脏
    void SetBoneLocalTransform(int32_t boneIndex, const Transform& local);

    Skeleton* GetSkeleton() const; // 经 Mesh

    Math::Geometry::AABB GetBoundingBox() const override;
    PrimitiveSceneProxy* CreateSceneProxy() override;

private:
    std::shared_ptr<SkeletalMesh> m_Mesh;
    std::shared_ptr<Material> m_Material;
    Pose m_LocalPose;
    std::vector<Matrix4> m_SkinningPalette; // 或仅在 CreateSceneProxy / 同步时构建
    bool m_bPoseDirty = true;

    void RebuildPaletteIfNeeded();
};

class SkeletalMeshSceneProxy : public PrimitiveSceneProxy
{
public:
    RHIBuffer* m_VertexBuffer = nullptr;
    RHIVertexInputLayout* m_VertexInputLayout = nullptr;
    RHIBuffer* m_IndexBuffer = nullptr;
    Material* m_Material = nullptr;

    // 每帧或脏时由 Component 推送的拷贝 / 上传句柄
    std::vector<Matrix4> m_BonePalette; // MVP：CPU 旁路；后续可改 RHIBuffer
};
```

**帧时序（契约）：**
1. 写 `Pose`（F01 手工 / F02 Player）  
2. `RebuildPaletteIfNeeded`（`Skeleton::BuildSkinningPalette`）  
3. `CreateSceneProxy` / 同步 proxy 带上 palette  
4. Draw：bind palette + skinned PSO + `u_Model`

F02 **只替换步骤 1 的 Pose 来源**，不改 2–4。

#### 2.7.6 建议源码布局

```text
Runtime/Function/Animation/     // Pose 核（无 Assimp、无 RHI）
  Skeleton.h/.cpp
  Pose.h
Runtime/Function/Render/
  SkeletalMesh.h/.cpp
  PrimitiveSceneProxies/SkeletalMeshSceneProxy.*
Runtime/Function/Framework/Components/
  SkeletalMeshComponent.*
Runtime/Resource/Loaders/
  AssimpMeshImportUtil.*
  StaticMeshLoader.*          // 含原 MeshLoader 导入
  SkeletalMeshLoader.*
```

### 2.8 测试与 Demo

| 层级 | 内容 |
|------|------|
| 单测 | 小层级 local→global；palette = global * invBind；权重归一化 |
| 可视 | 导入角色 Bind Pose；偏移单骨看网格跟随 |
| 回归 | 现有 `StaticMesh` 路径与 `verify.ps1` smoke 不受影响 |

---

## 3) 备选方案

| 选项 | 说明 | 结论 |
|------|------|------|
| A. 扩展 `StaticMesh` 可选蒙皮（偏 Godot） | 少类型，污染刚体路径与 layout / PSO | **拒绝** |
| B. 独立 `SkeletalMesh` + `Skeleton`（偏 UE） | 与现有 Static 栈对称；F02+ 引用自然 | **选用** |
| C. F01 同时做 Clip | 竖切变长，蒙皮 bug 难隔离 | **拒绝**（→ F02） |
| D. MVP 即 cooked 二进制 | 正确但拖慢第一竖切 | **Defer**（TD） |
| E. 保留泛化名 `MeshLoader` | 继续暗示「唯一网格导入」 | **拒绝**；收束为 Static 命名 + 平行 Skeletal Loader |

---

## 4) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Assimp 层级 / bind 与引擎坐标不一致 | 网格炸裂、错位 | 与 StaticMesh 同约定；加 Bind Pose 可视化对照；失败即报错 |
| 材质壳双路径（rigid/skinned）漏改 | 编译错或阴影不蒙皮 | `MeshDeformationMode` 进 PSO/编译 key；Shadow 同步变体 |
| Loader 改名漏改调用点 | 编译失败 | 与 S01 同 PR；全库 rename + smoke |
| 骨数 / UBO 限制 | 大骨架失败 | 明确上限；Import 校验 |
| 多 section 材质 | 角色 look 不完整 | 单材质先过；TD 对齐 StaticMesh section draw |
| palette 每帧上传成本 | 后期才重要 | F01 正确优先；实例化 / 缓冲池后置 |
| `.fbx` 当 StaticMesh 登记 | 类型与 Source 混淆 | **共识移交 [`ASSET-F01`](../Asset/ASSET-F01_IMPORT_PIPELINE_DESIGN.md)**；F01 目视用外部脚本最小资源 |

---

## 5) 验收标准

- [x] `Skeleton` / `SkeletalMesh` 类型注册并可经 Asset 路径加载（meta + 源；`.fbx` 默认仍 Static，显式 `SkeletalMesh` / `.glb`）
- [x] Import 产出合法层级、inverse bind、≤4 influences（代码路径；真实资产目视待勾）
- [x] Bind Pose 下角色外观正确（相对源 DCC / 参考图可接受） → **最小 stick 目视通过**（非人型）
- [ ] 调试偏移单骨 → 网格对应变形（交互 UX 未做；API 已有）
- [x] OpenGL 路径可玩；Vulkan 若工作量可控则同切片或紧随（实现计划标明） → **OpenGL 已验**
- [x] `StaticMesh` 回归：既有 smoke / asset-manager 无回退
- [x] Assimp **不**链接进「每帧动画更新」模块
- [x] `MeshLoader` 已收束为 Static 命名；Skeletal 走独立 Loader；无残留「唯一 MeshLoader」语义
- [x] Material 壳支持 Rigid/Skinned VS 变体（主 Pass）；Shadow skinned **Deferred**（Component 默认不投阴影）
- [x] Design / Registry / ACTIVE_WORK / Progress 与实现状态一致；Impl Plan 切片已落地（目视未完） → **已对齐 Review**

---

## 6) 建议切片（预览；以 Impl Plan 为准）

| Slice | 目标 |
|-------|------|
| S00 | `Bone` / `Skeleton` / `Pose` / local→global / palette + 单测 |
| S00b | （可并入 S01）`MeshLoader` → Static 命名；抽出 `AssimpMeshImportUtil` |
| S01 | `SkeletalMeshLoader`：Assimp 抽骨 + 权重 → CPU / GPU 资产数据 |
| S02 | GPU layout + Material `MeshDeformationMode::Skinned` + palette bind |
| S03 | `SkeletalMeshComponent` + SceneProxy + Forward/Shadow 入队（镜像 Static） |
| S04 | AssetType 注册 + 加载/导入接线 + 可视验收 |

---

## 7) Status note

| 字段 | 内容 |
|------|------|
| Status | **Planned** — Impl Plan 已开；实现自 S00 |
| Branch | `feat/animation`（已存在） |
| Next | `ANIM-F01-S00` Pose/Skeleton 核 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | Registry 占位（旧「Animation system」） |
| 2026-09-01 | 分支 `feat/animation`；依赖 CORE-F05/F06 |
| 2026-09-03 | 正式 Draft：收窄为 Skeletal Mesh Pipeline；系列拆 F02/F03；Event 不排期 |
| 2026-09-03 | 增补 §2.1 平行产品线（UE∥Godot 取舍）、Loader 命名、`MeshDeformationMode` 材质变体 |
| 2026-09-03 | 增补 §2.7 数据结构与接口详设（Skeleton/Pose/Mesh/Loader/Component/Proxy） |
| 2026-09-03 | Status → **Planned**；链接 Implementation Plan；开工 S00 |
| 2026-09-03 | S00b：Loader 命名已落地（见 Impl）；验收项 MeshLoader 收束勾选 |
