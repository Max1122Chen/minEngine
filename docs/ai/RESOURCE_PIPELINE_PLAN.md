# Resource Pipeline — 基础设施方案

Last updated: 2026-05-23  
Status: **设计稿 — 待评审/实现**  
关联：[MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md)（IBL / HDR cubemap）、[MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md](./MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md)

---

## 0) 目标

1. **职责分离**：`AssetManager` 只做「注册表 + 元数据 + 缓存 + 按类型分发」，不再堆叠具体文件格式解析。
2. **Image 先行**：`ImageLoader`（Resource 层，stb 后端）承接全部 2D/HDR 像素 IO；`AssetManager` **不再暴露** `LoadImage` / `FreeImage`。
3. **对齐 LearnOpenGL**：HDR 环境贴图走 **stbi_loadf → float 2D → GPU cubemap capture**（非 stb 内置 cubemap 转换）。
4. **可扩展**：为 `StaticMesh` / `Material` / `Shader` / `Scene` 等预留独立 Loader，逐步从 `AssetManager.cpp` 迁出。

---

## 1) 现状与问题

| 现状 | 问题 |
|------|------|
| `AssetManager::LoadImage` / `FreeImage`（private，仅 `LoadAsset_Impl<Texture2D>` 用） | 名字像「资产管理」，实际是 stb 封装；`TextureCubeLoader` 曾被迫直接 `#include stb_image` 或 friend |
| `LoadAsset_Impl<StaticMesh>` 内嵌整段 Assimp | ~270 行挤在 `AssetManager.cpp`，难测、难复用 |
| `LoadAsset_Impl<Material>` / `Scene` / `Shader` 各一套序列化/读文件 | 同上 |
| IBL 需要 HDR + cubemap | 无 float 纹理格式、无 equirect→cube capture；与资源层未分层 |

**原则（拍板）：**

- **IO / 解码** → `*Loader`（Resource 或 Render 工具，视是否依赖 RHI）
- **资产对象 + GUID + Meta** → `AssetManager` + `MEObject`
- **GPU 资源** → `RHI` / `Texture2D` / 专用 `*Uploader` 或 Render 子系统

---

## 2) 目标架构

```text
                    ┌─────────────────────────────────────┐
                    │           AssetManager               │
                    │  ScanAssets / Meta / Cache / Load<T> │
                    └──────────────┬──────────────────────┘
                                   │ LoadAsset_Impl<T> 委托
         ┌─────────────────────────┼─────────────────────────┐
         ▼                         ▼                         ▼
  ImageLoader              MeshLoader (未来)          MaterialLoader (未来)
  (stb LDR/HDR)            (Assimp)                   (JsonArchive + .memtl)
         │                         │                         │
         ▼                         ▼                         ▼
  ImagePixels /              MeshImportData            Material 磁盘结构
  float buffer               (CPU 几何)                 (尚未 MEObject)
         │
         ├──────────────────────────────────────┐
         ▼                                      ▼
  Texture2DLoader (未来，或留在 Impl 内)    EnvMapCapture (Render)
  Meta → Texture2D + RHI                   HDR 2D → Cubemap (LearnOpenGL)
```

**依赖方向（硬规则）：**

```text
Core  ←  Resource (ImageLoader, *Loader, AssetManager)  ←  Function/Render (RHI, TextureCubeLoader, EnvMapCapture)
```

- `Resource` **不得** `#include RenderSystem` / `RHI`（当前 `LoadAsset_Impl<Texture2D>` 违反此条，迁移时一并修）。
- `TextureCubeLoader`、`EngineIBLEnvironment` 在 **Render**，调用 `ImageLoader` 读盘，再调 RHI / Capture。

---

## 3) Phase R1 — `ImageLoader`（本阶段必做）

### 3.1 位置与后端

```text
minEngine/minEngine/src/Runtime/Resource/
  ImageLoader.h
  ImageLoader.cpp    // 唯一编译单元定义 STB_IMAGE_IMPLEMENTATION（从 AssetManager.cpp 迁出）
```

- 后端：**stb_image**（`stbi_load` / `stbi_loadf` / `stbi_image_free` / `stbi_is_hdr`）。
- **不**做 cubemap 数学、不做 RHI 上传。

### 3.2 类型与 API（建议）

```cpp
namespace minEngine {

enum class ImageStorage : uint8_t { UInt8, Float32 };

struct ImagePixels {
    ImageStorage Storage = ImageStorage::UInt8;
    union {
        unsigned char* U8  = nullptr;
        float*         F32 = nullptr;
    };
    int Width = 0, Height = 0, Channels = 0;

    bool IsValid() const;
    void Reset();  // Free + 清零
};

class ImageLoader {
public:
    // LDR：png/jpg/bmp… → 8-bit
    static bool LoadLdr(const std::string& path, ImagePixels& out, bool flipVertical = true,
                        std::string* outError = nullptr);

    // HDR：.hdr → float（stbi_loadf）
    static bool LoadHdr(const std::string& path, ImagePixels& out, bool flipVertical = false,
                        std::string* outError = nullptr);

    // 按扩展名 / stbi_is_hdr 自动选择
    static bool Load(const std::string& path, ImagePixels& out, bool flipVertical = true,
                     std::string* outError = nullptr);

    static void Free(ImagePixels& pixels);
    static bool IsHdrPath(const std::string& path);
};

}
```

**约定：**

| 调用方 | `flipVertical` |
|--------|----------------|
| 普通 albedo/normal（`Texture2D` 资产） | `true`（与现 `AssetManager` 一致） |
| Cubemap 六面 PNG | `false` |
| Equirect HDR | `false`（与 LearnOpenGL 一致，必要时再调） |

**错误处理：** 失败写 `outError` + `ME_CORE_ERROR`（与现 Log 风格一致）；`out.Reset()` 保证无泄漏。

### 3.3 `AssetManager` 变更

- **删除** `LoadImage` / `FreeImage` 声明与实现。
- `LoadAsset_Impl<Texture2D>` 改为：
  1. `ImageLoader::Load(path, pixels, true)`
  2. 调用 **`Texture2DLoader::CreateFromPixels(RHI&, pixels, desc)`**（新建，见下）或内联 RHI 创建（R1 可暂留 Impl 内，但经 `ImageLoader` 取像素）。

**R1 最小 `Texture2DLoader`（推荐同批引入，解开 Resource→Render 环）：**

```text
Runtime/Function/Render/Texture2DLoader.h/.cpp
  CreateFromPixels(RHI&, const ImagePixels&, RHITextureDesc) → shared_ptr<Texture2D>
```

- `AssetManager::LoadAsset_Impl<Texture2D>` 只负责：`Meta` → `ImageLoader` → `Texture2DLoader` → 填 GUID/Meta。
- `AssetManager.cpp` **移除** `#include RenderSystem`（若 Texture2DLoader 在 Render，则 Impl 可移到 `Texture2DLoader.cpp` +  thin wrapper 在 AssetManager，或 `LoadAsset_Impl` 特化放到 Render 侧 `.cpp` — 实现时二选一，以「AssetManager 不依赖 RHI」为准）。

### 3.4 其它调用方迁移

| 调用方 | R1 改法 |
|--------|---------|
| `TextureCubeLoader::LoadFaceSetFromFiles` | `ImageLoader::LoadLdr`，`flipVertical=false` |
| `EngineIBLEnvironment` | 仍用 `TextureCubeLoader`；HDR 路径在 R2 |
| 未来 Editor 缩略图 | 直接 `ImageLoader` |

### 3.5 R1 验收

- [ ] 全仓库无 `AssetManager::LoadImage` / `FreeImage`。
- [ ] 仅 **一处** `STB_IMAGE_IMPLEMENTATION`（`ImageLoader.cpp`）。
- [ ] 项目内贴图加载行为不变（含 1/3/4 channel、flip）。
- [ ] `Editor.exe --material-ir-test` exit 0。
- [ ] `TextureCubeLoader` 不直接 `#include stb_image.h`。

---

## 4) Phase R2 — LearnOpenGL 式 HDR → Cubemap（与 IBL 衔接）

> stb **只**提供 equirect 浮点像素；cubemap 必须在 **GPU** 生成（与教程一致）。

### 4.1 管线（教程对齐）

```text
environment.hdr
    → ImageLoader::LoadHdr → ImagePixels (F32, W×H×3)
    → RHITexture2D RGB16F（或 RGB32F）  // 需扩展 TextureFormat + OpenGLTexture
    → EnvMapCapture::EquirectToCubemap(equirect, faceSize) → RHITextureCube
    →（P4.2+）IrradianceConvolve / Prefilter / BrdfLut（额外 pass 或离线）
```

### 4.2 组件划分

| 组件 | 层 | 职责 |
|------|-----|------|
| `ImageLoader` | Resource | 读 HDR 文件 |
| `EnvMapCapture` | Render | cubemap FBO、capture shader、`EquirectToCubemap` |
| `TextureCubeLoader` | Render | 六面 PNG、solid color、**组装** RHI cube |
| `EngineIBLEnvironment` | Render | 加载策略：png 六面 > `.hdr` capture > fallback |

**Shaders（EngineDefault）：**

```text
Shaders/EnvMap/
  equirect_to_cubemap.vert
  equirect_to_cubemap.frag   // 采样 equirect，输出当前 face
```

（可与 LearnOpenGL 相同数学：`vec2 uv = SphericalUV(normalize(Position));`）

### 4.3 RHI 扩展（R2 前置）

在 `TextureFormat` 增加例如：

- `RGB16F` / `RGBA16F`（IBL 推荐）
- 可选 `RGB32F`

`OpenGLTexture2D` / `OpenGLTextureCube` 的 `ResolveOpenGLTextureFormat` 映射到 `GL_RGB16F` 等；upload 使用 `GL_FLOAT`。

### 4.4 `EngineIBLEnvironment` 加载顺序（更新）

```text
1. irradiance_posx.png … negz.png
2. environment.hdr（或任意 *.hdr）→ EnvMapCapture
3. validation 六色 cube
```

P4.1 临时可把 capture 结果同时绑到 irradiance + prefilter alias；P4.2 再拆三张图。

### 4.5 R2 验收

- [ ] 单张 `.hdr` 放入 `EngineDefault/Textures/IBL/` 即可启动，日志非 validation fallback。
- [ ] Cubemap 目视合理（天空无严重接缝；可选后续 seam 修复）。
- [ ] 不回归 R1 的 LDR 贴图资产。

---

## 5) 后续 Loader 路线图（堆逻辑迁出 AssetManager）

| Loader | 层 | 输入 | 输出 | 从 AssetManager 迁出内容 |
|--------|-----|------|------|-------------------------|
| **ImageLoader** | Resource | 路径 | `ImagePixels` | `LoadImage` / stb ✅ R1 |
| **Texture2DLoader** | Render | `ImagePixels` + desc | `Texture2D` | `LoadAsset_Impl<Texture2D>` 的 RHI 部分 |
| **TextureCubeLoader** | Render | 六面路径 / solid / capture 结果 | `TextureCube` | 已独立；间接用 ImageLoader |
| **MeshLoader** | Resource 或 Render | 路径 (fbx/obj…) | `MeshImportData`（顶点/索引/AABB） | Assimp 循环 |
| **MaterialLoader** | Resource | `.memtl` | `Material` 或中间 DTO | JsonArchive 读图 |
| **ShaderLoader** | Resource | 源文件路径 | `string` 或 `ShaderSource` | 读文件 + include |
| **SceneLoader** | Resource | `.mescene` | `Scene` | 已有部分逻辑 |

**`AssetManager::LoadAsset_Impl<T>` 目标形态（长期）：**

```cpp
// 伪代码
template<> shared_ptr<Texture2D> LoadAsset_Impl<Texture2D>(const AssetMeta& meta) {
    ImagePixels px;
    if (!ImageLoader::Load(meta.AssetPath, px)) return nullptr;
    auto tex = Texture2DLoader::CreateFromPixels(GetRHIForAssetLoad(), px, DefaultSrgbDesc());
    AssignMeta(tex, meta);
    return tex;
}
```

**Mesh / Material 同理**，`AssetManager` 不超过 ~30 行/类型。

---

## 6) 实施顺序（建议）

```text
R1  ImageLoader + 去掉 AssetManager::LoadImage + TextureCubeLoader 改用 ImageLoader
    + Texture2DLoader（解开 AssetManager→RHI）
    + 文档 / --material-ir-test

R2  RHI float 格式 + EnvMapCapture + EngineIBLEnvironment 读 .hdr
    + IBL README 更新

R3  MeshLoader 抽出（Assimp）
R4  MaterialLoader / SceneLoader 抽出（按编辑器痛点排）
```

**与 Material Phase 4 关系：**

- P4.1（绑定）已完成 → **R1 不挡 P4.2**。
- P4.2（`MaterialIBL.glslinc`）可与 **R2** 并行：shader 可先绑六面 PNG，HDR 作为资源便利项。

---

## 7) 非目标（本方案不做）

- DDS / KTX2 容器
- 异步 IO / 流式加载
- 资产热重载
- `TextureCube` 正式 `Asset` 类型与 `.meta`（可单列 backlog）
- CPU equirect→cube（与「对齐 LearnOpenGL」冲突，不采用为主路径）

---

## 8) 风险与对策

| 风险 | 对策 |
|------|------|
| `LoadAsset_Impl<Texture2D>` 依赖 `RenderSystem::Get()` | R1 引入 `Texture2DLoader`；或 `AssetLoadContext` 注入 `RHI*` |
| 双处 `STB_IMAGE_IMPLEMENTATION` | 仅从 `AssetManager.cpp` 删除，保留 `ImageLoader.cpp` |
| HDR 曝光/伽马 | R2 capture shader 与 P4 tone-mapping 一致；先线性 float，display 再议 |
| 启动时 capture 耗时 | 日志 + 可选缓存六面 PNG（工具链二期） |

---

## 9) 评审清单（请你拍板）

1. **R1 是否同意** `Texture2DLoader` 与 `ImageLoader` 同批（避免 AssetManager 仍碰 RHI）？
2. **R2 float 格式** 默认 `RGB16F` 是否 OK？
3. **Loader 放 Resource vs Render**：Mesh 几何放 Resource（`MeshImportData`），GPU buffer 创建放 Render — 是否同意？
4. **EnvMapCapture** 是否放在 `Runtime/Function/Render/Environment/`（与 `EngineIBLEnvironment` 同目录）？

确认后可按 **R1 → R2** 开实现任务。
