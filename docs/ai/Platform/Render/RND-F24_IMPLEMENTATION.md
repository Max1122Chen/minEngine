# RND-F24 — 纹理采样模式资产化 — 实现记录

## 落点

| 内容 | 位置 |
|------|------|
| 两个作者字段（`ME_PROPERTY` + Setter/Getter meta） | `Render/Texture.h:66-70` |
| 两个独立 setter 声明 / 派生缓存读取 | `Render/Texture.h:53,55,58` |
| 派生实现、缓存 sink | `Render/Texture.cpp:8,14,20,31` |
| `private` 段：`SetPreferredSamplerDesc` + `RebuildPreferredSamplerDesc` | `Render/Texture.h:78-84`（子类也调不到） |
| `m_Wrapping`/`m_Filtering` 与两个 getter | 删除；`TextureWrapping`/`TextureFiltering` 保留给 `TextureCube`（头部已注明遗留） |
| .metex 头部两字段 + 版本 | `Loaders/Texture2DLoader.cpp:397,404-409,514-519` |
| 加载把模式落到对象 | `Texture2DLoader.cpp:611-612` |
| 导入选项 `filter`/`address` | `Importers/Texture2DImporter.cpp:40,55,244,252` |
| 唯一引擎内调用点迁移（模式不变） | `UI/Font/FontGlyphAtlas.cpp:319-320`（Linear + ClampToEdge） |
| 用例 | `Tests/Suites/TextureSamplerTest.cpp`（suite `texture-sampler`，7 例全 `[smoke]`） |

## 行为

- 真源是两个字段；`GetPreferredSamplerDesc()` 每次由两轴重建（filter → Min/Mag，address → U/V/W），不存在第三份状态。
- 每个 setter 只写自己那一轴再重建；写一轴不会回填另一轴。
- 采样器缓存由 RHI 按 `RHISamplerDesc` 键控（`RHISamplerDescHash`），字段变化即换 key，因此不会复用旧采样器。
- Inspector 写入走反射 Setter thunk，直落两个 setter，因此改完即重算 desc。

## .metex 格式

- 版本 1 → **2**（`Texture2DCookTypes.h` 的 `kMetexVersion`）；`version` 由写入方统一盖章，不再是调用方可传的状态。
- 头部**末尾追加**两个整数键 `samplerFilter` / `samplerAddress`（枚举字节值），既有键顺序不变。
- 旧键 `wrapU`/`wrapV`/`filter` 仍原样写出（由两轴派生），读侧不再解析；`Texture2DCookSettings` 因此只有一份采样状态。
- 读侧：`version < 2` 或头部缺键 → Linear + Repeat；读取先把 settings 清回默认再赋值，调用方的旧值不会被当成"文件里的值"。

## 导入

- `asset.import` 的 texture 选项新增 `filter`（Linear｜Nearest）与 `address`（Repeat｜MirroredRepeat｜ClampToEdge｜ClampToBorder）；缺省 Linear / Repeat。
- 未知取值直接失败并列出允许值，不静默回退。
- 覆盖导入沿用既有产品的设置，只有显式给出的选项才覆盖。

## 导出契约

- 两个新 setter 以 `MINENGINE_API` 导出（`minEngine_exports.def` 未改，白名单只减不增）；导出 1283 → 1285，基线未改。
- `SetPreferredSamplerDesc` 收为 private（与 `RebuildPreferredSamplerDesc` 同段）：源级不可外部调用，但 .def 白名单仍列该符号，所以**导出表没有减少**，`--assert-no-removals-vs` 0 遗漏、PASS；基线因此零差异。
- 消费仓 `BlockTextureAtlas`/`MobVisual` 共 8 处需迁移到 `SetSamplerFilter` + `SetSamplerAddress`；现在是编译期报错，不是链接期。

## 验证

- `scripts/verify.ps1` exit 0；smoke 12 → 13 套件（`kSmokeOrder` + `InSmoke=true`）。
- 三项反向验证（互相覆盖 / 去掉缓存失效 / 旧文件不取默认）均使对应用例变红，已复原。

## 已知限制

- mip 与过滤的交互未改（Nearest + mip 远处仍按既有策略）。

## 后续可选优化（本批不做）

- 字体图集可考虑改为 nearest：字形覆盖率存在 alpha 通道里，nearest 采样可避免相邻字形插值渗色；但那是渲染外观变更，不属于采样配置资产化，需要单独评估与目视确认。
