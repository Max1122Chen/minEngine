# BUG-ASSET-003 — 编辑器启动重写 MyMEProject 的 *.meta（Load 期字段补齐 + 全结构重序列化）

## Meta
- **ID:** BUG-ASSET-003
- **Status:** Open
- **Severity:** S3
- **Owner:** project maintainer
- **Found:** 2026-10-20
- **Last updated:** 2026-10-20
- **Affects:** `minEngine/MyMEProject/Assets/**/*.meta`（实测一次启动改写 15 个）；任何 meta 缺 `RuntimeClassName` 的项目/夹具；所有经 `AssetManager::WriteMetaFile` 落盘的 meta（键序 + 尾换行）
- **Related Feature/Slice:** BUILD-F04 S04（编辑器启动冒烟）· ASSET-F10（`AssetMeta::RuntimeClassName` 缺字段两档策略 / Save 统一写入）
- **Design:** —（本条为缺陷记录；修复方向见「修复」，无独立 Design Spec）

## TL;DR
启动时编辑器按需 Load 资产，`ValidateOrHealRuntimeClassNameAfterLoad` 对**任何 `RuntimeClassName` 为空的 meta** 就地把整个 `AssetMeta` 全结构重序列化回盘（`AssetManager.cpp:622-687` → `:578` → `:742`）。副作用有三层：补出 `RuntimeClassName`（真实值）、补出 `SourcePath: ""`、以及因 `Json = nlohmann::json`（std::map 有序）导致键序变成字典序（`$engineVersion` 跑到 `$schemaVersion` 前）且丢掉文件尾换行。**GUID 不变、`$schemaVersion` 数值不变（仍是 1）**。一次启动的 15 个文件改完后，第二次启动不再写（0 次 heal / 0 个 mtime 变化）——即这是一次性迁移，不是每次启动都写；噪声之所以「一直存在」，是因为迁移结果从未提交，工作区一被 revert/checkout 就重新触发。

---

## 症状
1. 任一次编辑器启动后 `git status --porcelain -- minEngine/MyMEProject/Assets` 出现 **15 个 ` M`**：
   `Environment/DefaultEnvironment.meenv.meta`、`Fonts/Inter_18pt-Regular.ttf.meta`、
   `Materials/DefaultMaterial.memtl.meta`、`Meshes/BasicShapes/cube.obj.meta`、
   `Scenes/test.mescene.meta`、`Textures/albedo.jpg.meta` …（完整列表见「复现」备注）。
2. 每个文件 diff 形状完全一致（`git diff --stat`：15 files changed, 75 insertions(+), 45 deletions(-)，即每文件 5 增 3 删）：
   ```diff
   -    "$schemaVersion": 1,
        "$engineVersion": "0.0.9",
   +    "$schemaVersion": 1,
        "AssetName": "DefaultMaterial",
   ...
   -    }
   -}
   +    },
   +    "RuntimeClassName": "minEngine::Material",
   +    "SourcePath": ""
   +}
   \ No newline at end of file
   ```
3. 编辑器日志同一次启动内有 15 条：
   `[Asset] [Warn] LoadAsset: '<path>' missing RuntimeClassName; rewriting meta as 'minEngine::<Class>'. (AssetManager.cpp:658)`
4. 对多代理/多会话协作的干扰：任何干净检出（或 `git checkout -- .`）后第一次开编辑器都会重新长出这 15 个 diff，容易被误当成自己的改动或误提交。

## 期望
1. 启动只读：不改变磁盘字节的启动路径不应产生 tracked 文件 diff（或至少只在 schema/字段真正落后时才写一次，并把结果显式落库）。
2. 需要写回时**只写变化**：不要因为补一个字段就把整份 meta 重排键序、补 `"SourcePath": ""`、删掉文件尾换行。
3. 序列化确定：同一语义的 meta 反复落盘应字节稳定（键序固定、尾换行固定）。

## 复现
前置：干净工作区。
```bash
cd /d/Dev/GitRepo/minEngine
git status --porcelain -- minEngine/MyMEProject/Assets   # 期望为空
```

1. 启动并正常退出编辑器（自动化管道，比手工开窗口可控）：
   ```bash
   python scripts/smoke_editor_automation.py --project minEngine/MyMEProject/MyMEProject.meproject
   ```
   实测输出：6 个 PASS + `smoke PASS`，退出前打印 `ready: AUTOMATION_READY pipe=...`。
2. 确认字节确实变了、变了哪些文件：
   ```bash
   git status --porcelain -- minEngine/MyMEProject/Assets
   git diff --stat      -- minEngine/MyMEProject/Assets
   git diff             -- minEngine/MyMEProject/Assets/Materials/DefaultMaterial.memtl.meta
   ```
   期望（实测）：15 行 ` M`；`15 files changed`；diff 形状即上文「症状 2」。
3. 判定「是不是 GUID 被重写」：
   ```bash
   git diff -U0 -- minEngine/MyMEProject/Assets | grep -E '^[-+].*(Guid|High|Low)' || echo 'NO GUID LINES'
   ```
   实测输出：`NO GUID LINES`（GUID 未被改写）。
4. 幂等性（第二次启动还写不写）：
   ```bash
   find minEngine/MyMEProject/Assets -name '*.meta' -printf '%T@ %p\n' | sort > /tmp/mt1.txt
   python scripts/smoke_editor_automation.py --project minEngine/MyMEProject/MyMEProject.meproject
   find minEngine/MyMEProject/Assets -name '*.meta' -printf '%T@ %p\n' | sort > /tmp/mt2.txt
   diff /tmp/mt1.txt /tmp/mt2.txt     # 无输出 = 第二次启动没有任何 meta 被重写
   L=$(ls -t minEngine/bin/Logs/*.log | head -1); grep -c 'missing RuntimeClassName' "$L"   # 实测 0
   ```
5. 清理（只还原本条涉及的 .meta，别用全仓 checkout）：
   ```bash
   git checkout -- $(git status --porcelain -- minEngine/MyMEProject/Assets | awk '{print $2}')
   git status --porcelain      # 期望只剩与本条无关的他人改动
   ```

**实测受影响的 15 个文件（`AssetType` → 写入的 `RuntimeClassName`）：**
| # | 文件（相对 `minEngine/MyMEProject/Assets`） | RuntimeClassName |
|---|---|---|
| 1 | `Environment/DefaultEnvironment.meenv.meta` | `minEngine::EnvironmentMap` |
| 2 | `Fonts/Inter_18pt-Regular.ttf.meta` | `minEngine::Font` |
| 3 | `Fonts/Inter_18pt-SemiBold.ttf.meta` | `minEngine::Font` |
| 4 | `Materials/DefaultMaterial.memtl.meta` | `minEngine::Material` |
| 5 | `Materials/MaterialIRSmoke.memtl.meta` | `minEngine::Material` |
| 6 | `Meshes/BasicShapes/cube.obj.meta` | `minEngine::StaticMesh` |
| 7 | `Meshes/BasicShapes/plane.obj.meta` | `minEngine::StaticMesh` |
| 8 | `Meshes/BasicShapes/sphere.obj.meta` | `minEngine::StaticMesh` |
| 9 | `Scenes/test.mescene.meta` | `minEngine::Scene` |
| 10 | `Textures/IBL/brdf_lut.png.meta` | `minEngine::Texture2D` |
| 11 | `Textures/albedo.jpg.meta` | `minEngine::Texture2D` |
| 12 | `Textures/container.jpg.meta` | `minEngine::Texture2D` |
| 13 | `Textures/marble_cliff_03_ao_1k.jpg.meta` | `minEngine::Texture2D` |
| 14 | `Textures/marble_cliff_03_diff_1k.jpg.meta` | `minEngine::Texture2D` |
| 15 | `Textures/marble_cliff_03_nor_gl_1k.jpg.meta` | `minEngine::Texture2D` |

其余 24 个 meta（`capsule/cone/cylinder/convexmesh.obj`、`armadillo/bunny/dragon`、`Audios/*.wav`、`Scripts/*.lua`、`Prefabs/Cube.meprefab`、`Animations/MinSkinnedStick.glb` 等）**不受影响**——启动流程没有 Load 它们，heal 不触发。

## 环境
`master` 工作区（`D:\Dev\GitRepo\minEngine`），Windows，`minEngine/bin/MaximumEditor.exe`（2026-10-20 构建），项目 `minEngine/MyMEProject/MyMEProject.meproject`，`kDiskSchemaVersion = 1`（`Runtime/Core/EngineVersion.h:36`），meta 内 `$engineVersion: "0.0.9"`。复现经 `scripts/smoke_editor_automation.py`（ED-F17 自动化管道），非 headless，但会正常初始化并退出。

## 根因
### 1. 触发点：Load 期 heal 把「缺字段」当成需要写盘的事件
`AssetManager::LoadAssetByMeta_Internal` 在每次成功加载后调用 heal：
- `minEngine/minEngine/src/Runtime/Resource/AssetManager.cpp:1462` — `ValidateOrHealRuntimeClassNameAfterLoad(meta, *asset, outErrorMessage)`
- `AssetManager.cpp:622-687` — 该函数本体；`AssetManager.h:357` 声明
- `AssetManager.cpp:635-650` — **门禁只看「字段是否为空/是否相符」**：`meta.RuntimeClassName` 非空且等于加载类的名字 → 直接 `return true`（不写盘）；为空 → 进入写回分支
- `AssetManager.cpp:652-656` — 例外档：`RequiresRuntimeClassNameForLoad(assetType)`（仅 `DataAsset`，见 `AssetManager.cpp:583-586`）时直接报错失败，不写盘
- `AssetManager.cpp:658` — 日志 `LoadAsset: '<path>' missing RuntimeClassName; rewriting meta as '<Class>'.`（本次 15 条即此）
- `AssetManager.cpp:676-677` — `liveMeta->RuntimeClassName = className; WriteOrUpdateMetaFile(*liveMeta)`

**为什么要求写**：仓库内 39 个 meta 都是 ASSET-F10 引入 `RuntimeClassName` **之前**写下的，因此每个被 Load 的资产第一眼都是「空字段」，都要补。

### 2. 写回点：全结构重序列化（键序 + `SourcePath` + 尾换行都被顺带改掉）
- `AssetManager.cpp:578-581` — `WriteOrUpdateMetaFile` → `WriteMetaFile`
- `AssetManager.cpp:742-767` — `WriteMetaFile`：新建 `JsonWriterArchive`，用 `Serializer::ToFile(metaPath, &meta, ...)` **整份 `AssetMeta` 重新序列化**，没有「新旧字节比较」也没有「只补差异字段」
- `minEngine/minEngine/src/Runtime/Resource/AssetMeta.h:12-31`（反射顺序）— 结构体字段是 `AssetName/AssetPath/AssetType/RuntimeClassName/SourcePath/Guid`；`WriteMetaFile` 一写就把 `SourcePath`（原生资产为空串）也写出去
- **键序**：`Runtime/Core/Serialization/Json.h:3` `using Json = nlohmann::json;` → 底层 `std::map`，**键按字典序输出**。于是 `$engineVersion` 排在 `$schemaVersion` 之前。库里现有 meta 的键序（`$schemaVersion` 在首）不是本 writer 产生的，一旦被本 writer 重写就永久翻转。
- **`$schemaVersion` 只挪位置、不改值**：`JsonArchive.cpp:53-62` `ApplyRootSchemaVersion` 把 `$schemaVersion` 直接 `m_Root[kSchemaVersionKey] = schemaVersion`（`Serializer.cpp:37-47` 在写前调用），值仍取 `options.schemaVersion` 默认 `kDiskSchemaVersion = 1`（`SerializationTypes.h:24-27`、`EngineVersion.h:36`）。
- **尾换行丢失**：`JsonArchive.cpp:248` `output << m_Root.dump(4);` —— 只 dump，不补 `"\n"`。

### 3. 为什么「看起来每次启动都写」
三条独立事实叠在一起造成了误判：
1. heal 的门禁只有「字段空/相符」，与 `$schemaVersion` 无关 —— 所以它不是「schema 落后才迁移」，而是「字段空就写」。（这不是 bug 本身，但意味着没有版本闸门可以依赖。）
2. 写回是**全量重序列化**，于是补 1 个字段产生 5 增 3 删的 diff，噪声远大于信息量。
3. 迁移结果**从未提交**：工作区每次被还原（`git checkout`、干净克隆、别的会话/代理清理工作区）后，15 个 meta 又回到「空字段」状态，下一次启动再写一遍。**实测同一工作区内第二次启动 0 次写**（`grep -c 'missing RuntimeClassName'` → 0；39 个 meta 的 mtime 全部未变），所以本条**不是**每次启动都重写的持续抖动，而是一次性迁移 + 结果不落库导致的反复回归。

### 4. GUID 与 schema 的归属判定（回答「到底是哪一种」）
| 候选现象 | 是否发生 | 证据 |
|---|---|---|
| `$schemaVersion` **版本升级**（值变化） | **否** | diff 里 `"$schemaVersion": 1` 只是换了位置，值仍为 1；`EngineVersion.h:36` `kDiskSchemaVersion = 1u` |
| `$schemaVersion` **字段位置变化** | **是** | nlohmann `std::map` 字典序（`Json.h:3`）+ `JsonArchive.cpp:53-62` 注入；`$engineVersion` 反超到首位 |
| 字段补齐 `RuntimeClassName` | **是**（15/15） | `AssetManager.cpp:676-677`；日志 15 条 `AssetManager.cpp:658`；写入真实类名 |
| 字段补齐 `SourcePath` | **是**（15/15，均为 `""`） | 全结构重序列化 `AssetManager.cpp:742-767` + `AssetMeta.h:25-27` |
| `m_Guid` / `Guid` 被重写 | **否** | `git diff -U0 ... | grep -E '^[-+].*(Guid|High|Low)'` 无输出；代码上也只在 Guid 为 0（`AssetManager.cpp:503-507`）或跨资产冲突（`:515-537`）时才动 GUID |
| 文件尾换行丢失 | **是**（15/15） | `JsonArchive.cpp:248` |

**结论**：本次现象是「**字段补齐 + 全结构重序列化副作用（键序 / `SourcePath` / 尾换行）**」，**不是 schema 升级，也不是 GUID 重写**。任务书里「编辑器会重写 *.meta 的 m_Guid」的说法对 `Assets/**/*.meta` 不成立（GUID 逐字节未变）。

### 5. 与「`Content/Vanilla/*.medata` 的 m_Guid 每次载入被重写」的关系
**不同根因，同一类病（身份/字段在两处存储且加载期不做收敛）。判断依据：**
1. 本条的写回目标只有 `.meta`（`AssetManager::WriteMetaFile` → `BuildMetaAbsolutePath` = 资产路径 + `.meta`，`AssetManager.cpp:271-275`）。heal 分支只改 `liveMeta->RuntimeClassName`，**从不重写资产本体**。
2. 本仓库内**没有 Load 期重写资产本体**的路径：`Loaders/*.cpp` 里的 `SaveRegisteredAsset` / `WriteMetaFile` 调用全部落在 `Create` 函数内（`DataAssetLoader.cpp:168`、`MaterialLoader.cpp:131`、`PrefabLoader.cpp:99`、`SceneLoader.cpp:90`、`SpriteLoader.cpp:127,182` 等），Load 分支只读。
3. `.medata` 的 `m_Guid` 漂移是**存量数据不一致**：实测 minCraft 三个 Vanilla 文件，body 根 `m_Guid` 与 `<file>.meta` 的 `Guid` **全部不同**（例：`Blocks.medata` body `{High:13489655955538330085, Low:9574216157506369178}` vs meta `{High:15698141613781110115, Low:12913956067495925274}`）。而 Load 会以 meta 为准覆盖内存对象（`DataAssetLoader.cpp:82` → `AssetManager::ApplyMetaIdentity`，`AssetManager.cpp:689-696`），因此**下一次 Save** 必然把 body 的 `m_Guid` 写成 meta 的 Guid —— 表现为「碰过就变一次」，而不是「载入即写」。
4. 附带发现（不属本条，另立记录）：上述 Vanilla meta 里 `RuntimeClassName` 为 `""`，而 `DataAsset` 走严格档（`AssetManager.cpp:583-586` + `:652-656`）→ heal 不会补写、且报 `DataAsset load requires RuntimeClassName in meta`。说明「Load 期 heal」这条路对 DataAsset 根本不生效，与「一次性迁移工具」的设计意图（`docs/ai/Asset/ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md:270`、`:392-393`）脱节。

## 修复
**Status = Open，以下只是建议方向，尚未实现。**

1. **门禁从「字段空」改成「字段空 且 需要迁移」并显式化**（`AssetManager.cpp:635-650`）：保留只读判定，把写盘动作从 Load 里摘出去，改成一次可 dry-run 的迁移 pass（或在 heal 前检查 < 当前 schema 才写）。Load 保持纯读，符合「启动不应制造 diff」。参照 Design 里已写下的「一次性迁移、不在 Load 里猜」原则（`ASSET-F10_DATAASSET_POLYMORPHIC_LOAD_DESIGN.md:270`）。
2. **最小 diff 写回**（`AssetManager.cpp:742-767` / `:578-581`）：若保留 heal，先序列化到内存 buffer 与磁盘现有字节比对，语义等价则跳过；或对「仅补字段」场景做字段级 patch（在 `AssetMeta` 的 JSON 末尾插入 `RuntimeClassName`/`SourcePath`），避免整份重排。
3. **序列化确定性**（`Json.h:3` + `JsonArchive.cpp:53-73`）：meta 用 `nlohmann::ordered_json`，或为 `$schemaVersion`/`$engineVersion` 固定注入顺序（在字段序列化前先建根对象并插入 meta 键），使键序与历史文件一致；同时把 `$engineVersion` 排在 `$schemaVersion` 之后。
4. **补尾换行**（`JsonArchive.cpp:248`）：`output << m_Root.dump(4) << '\n';`，消除每次落盘的 `No newline at end of file` 噪声（也避免以后每次 Save 都产生 1 行 diff）。
5. **迁移结果落库**（流程层）：迁移 pass 跑完后把 15 个 meta 作为独立提交入库，否则任何干净检出都会重新触发本条——这是「看起来每次启动都写」的真正成因，代码改动本身无法替代它。
6. **一致性（可选，与 §5 联动）**：迁移 pass 里同时校验 body 根 `m_Guid` 与 meta `Guid`，不一致时按 meta 纠正 body（或明确以 body 为准），避免 Save 期静默改写 `.medata`。

## 回归验证
修复后应满足（本条当前 **Open**，未验证）：
- [ ] 干净工作区 + 一次编辑器启动 → `git status --porcelain -- minEngine/MyMEProject/Assets` 为空（或只出现一次显式迁移提交，之后恒为空）。
- [ ] `git diff -U0 | grep -E '^[-+].*(Guid|High|Low)'` 仍无输出（GUID 永不因迁移而变）。
- [ ] 迁移后连续两次启动：`grep -c 'missing RuntimeClassName'` 为 0，39 个 meta 的 mtime 不变。
- [ ] 新建资产（`Create` 路径）写出的 meta 键序 / 尾换行与迁移后存量 meta 一致（不再出现两种格式并存）。
- [ ] 已有测试不回归：`minEngineTests.exe test asset-manager`、`test data-asset`。

**本次实测已确认的基线事实（可复用的判据）：**
- [x] 一次启动改写 15 个 meta，每文件 5 增 3 删；GUID 无变化；`$schemaVersion` 值恒为 1。
- [x] 第二次启动 0 次写（heal 日志 0 条、mtime 无变化）→ 一次性迁移，非持续抖动。
- [x] 15 个文件与「启动时被 Load 的资产集合」完全对应；39 − 15 = 24 个未 Load 的 meta 未被触碰。
- [x] `git checkout -- <15 个 .meta>` 后 `git status --porcelain` 仅剩 `M docs/ai/Platform/Core/CORE-F38_TASK_SYSTEM_DESIGN.md`（他人未提交改动，未触碰）。

## 兼容影响
1. **正向**：补齐 `RuntimeClassName` 后，Load 会按真实类名校验（`AssetManager.cpp:635-650`），错类时能早失败；`DataAsset` 严格档才是唯一入口依赖它。
2. **字节层面**：迁移后的 meta 与历史 meta 键序不同（`$engineVersion` 前移）、多两字段、无尾换行——只要**同一次提交**把这些文件一起入库，后续所有会话都稳定；不提交就会反复回归（本条核心）。
3. **GUID 不变** ⇒ 场景/预制体里的资产引用、Registry 键、跨仓引用全部不受影响；不需要用户做任何重连。
4. **消费仓（minCraft 等）**：其 meta 已由当前 writer 写成新格式（实测 Vanilla 三个 meta 已含 `RuntimeClassName`/`SourcePath`，`$engineVersion` 在前），因此不受本条「键序翻转」影响；但见 §5 的 `.medata` body `m_Guid` 漂移，与本条无共同根因。
5. **协作影响**：本条不改语义，只改字节表示；对多代理/多会话的实际伤害是「干净检出后必然出现 15 文件 diff」，修复后（迁移入库 + 确定性序列化）归零。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-20 | 登记：BUILD-F04 S04 启动冒烟中发现 15 个 meta 被改写；编号取 `BUG-ASSET-*` 下一个空闲号 003 |
| 2026-10-20 | 实测判定归属：字段补齐（`RuntimeClassName`/`SourcePath`）+ 键序/尾换行副作用；**非** schema 升级、**非** GUID 重写 |
| 2026-10-20 | 幂等性实测：第二次启动 0 次写（日志 0 条、mtime 无变化）→ 严重度定 S3，非「每次启动都写」 |
| 2026-10-20 | 写回路径定位：`AssetManager.cpp:1462 → :622-687(:677) → :578 → :742`；键序 `Json.h:3`、尾换行 `JsonArchive.cpp:248` |
| 2026-10-20 | 与 `Content/Vanilla/*.medata` m_Guid 漂移区分：不同根因（Load 不写 body；漂移来自 body/meta Guid 存量不一致 + Save 以 meta 覆写） |
