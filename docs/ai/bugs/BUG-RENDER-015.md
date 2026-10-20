# BUG-RENDER-015 — ScreenUI Image 渲染上下颠倒（world Sprite 与 UI Image 朝向不一致）

## Meta
- **ID:** BUG-RENDER-015
- **Status:** Fixed (pending commit + Editor/PIE visual verify)
- **Severity:** P0 / S1
- **Owner:** project maintainer
- **Found:** 2026-09-09
- **Last updated:** 2026-09-09
- **Affects:** `ScreenUI` 全部 `ImageComponent`（`Raw` 与 `Sprite` 两种 SourceMode）、消费方 HUD / 容器面板
- **Related Feature/Slice:** UI-F06（ScreenUI Image/Raw）、RND-F16（2D Rendering Foundation）
- **Probe asset:** [BUG-RENDER-015_uvprobe_2x2.png](./BUG-RENDER-015_uvprobe_2x2.png)

## TL;DR
UI Image 走的是与 world Sprite **同一个** unit quad（`SpriteQuadMesh`）与**同一套** UV 约定（v=0 = 纹理底），
但 ScreenUI 的像素正交投影是 Y 向下（左上原点），而 widget 模型矩阵只做了 `scale(width, height)` 没有镜像 Y →
quad 的"纹理底"被放到 widget 顶边，整体上下颠倒。
修复：`ScreenUICoords::MakeWidgetModelMatrix` 把 Y 缩放取负（单点，一行），把 quad 模型空间镜像到 UI 的 Y-down 像素空间。

---

## 症状
1. 编辑器进 Play/PIE 后，Canvas 下所有 `ImageComponent` 图像整体上下颠倒（内容上下镜像，位置矩形本身正确）。
2. 两种 SourceMode 都复现：`Raw`（直接给 Texture2D）与 `Sprite`（给 Sprite 资产）。
3. 同一张纹理、同一个 2×2 四色探针：world `SpriteComponent` 显示 红 绿 / 蓝 黄（红在左上），
   UI `ImageComponent` 显示 蓝 黄 / 红 绿（蓝在左上）。
4. 不入 Play 时编辑器 UI 预览正常——预览由 ImGui 绘制，与 ScreenUI 渲染无关，所以问题只在 Play/PIE 出现。

## 期望
**唯一 oracle：同一纹理下，world `SpriteComponent` 与 UI `ImageComponent` 的屏幕朝向必须一致。**
即纹理第 0 行（图像顶部）在两者中都必须出现在屏幕上方；不允许 UI 侧存在第二套 UV 或翻转开关。

## 复现
1. 用探针 `BUG-RENDER-015_uvprobe_2x2.png`（2×2：左上红 / 右上绿 / 左下蓝 / 右下黄）导入为 Texture2D。
2. world 侧：新建 Scene，放 `SpriteComponent`（Texture = 探针，Size = 200×200），摆在相机前方可见处。
3. UI 侧：同一场景加 `Canvas`（ScaleMode = None 或 Letterbox 均可），其下 `Widget`（200×200）+ `ImageComponent`，
   SourceMode = `Raw`，Texture = 同一探针；再加第三个 Image 换成 SourceMode = `Sprite`、指向同一探针做的 Sprite。
4. 进 Play：对比屏幕上的 world 精灵与两块 UI 图像。

## 环境
`master`；Editor GL 为主验证路径，VK 路径同一套模型矩阵与 clip space 适配（`RHIClipSpace`）同样生效。

## 根因

**证据链（逐环，全部复核到 file:line）**

| # | 环 | 事实 | 位置 | 判定 |
|---|----|------|------|------|
| 1 | 纹理上载 | `ImageLoader::Load(path, pixels, /*flipVertical=*/true)`：CPU 像素 row 0 = 图像**底**行；`Texture2DLoader::CreateFromPixels` 原样上传，不再翻转 → GL `v=0` = 图像**底** | `Runtime/Resource/Loaders/Texture2DLoader.cpp:508`、`:148-150`；默认值 `Runtime/Resource/Loaders/ImageLoader.h:41` | **不动**：GL/引擎统一约定（`SpriteAtlasPacker.h:37` 亦记录 "row 0 = image bottom"），改它会把 3D 全部贴图一起翻 |
| 2 | quad 顶点 | pos.y=`-0.5` ↔ uv.v=0；pos.y=`+0.5` ↔ uv.v=1 → 模型空间 -Y 边携带"纹理底" | `Runtime/Function/Render/Sprite/SpriteQuadMesh.cpp:12-18` | **不动**：这是 world Sprite 正确朝向的来源（world 为 Y-up，-Y 在屏幕下方） |
| 3 | UV rect | `PixelRectToUvRectTopLeft`：左上原点像素矩形 → OpenGL UV（`v0=1-(y+h)/th`、`v1=1-y/th`），即 `v=1` 对应图像顶 | `Runtime/Function/Render/Sprite/Sprite.cpp:100-105`；消费方 `SpriteComponent.cpp:88`、`ImageComponent.cpp:99` | **不动**：与环 1 的 GL 上载约定自洽；UI-F06 明确"直接用 `GetUVRectNormalized()`，禁止第二套转换"（`docs/ai/Platform/UI/UI-F06_SCREENUI_IMAGE_SPRITE_RAW_DESIGN.md:48`） |
| 4 | **空间映射** | ScreenUI 像素正交投影是 **Y 向下**（`MakeOrthographic(0, width, /*bottom=*/height, /*top=*/0, ...)`）；widget 模型矩阵却用 **正** Y 缩放 `Vector3(width, height, 1)` → quad 模型 +Y 指向屏幕**下方**，环 2/3 的"纹理底(uv.v=0)"落到 widget **顶边** | `Runtime/Function/Render/ScreenUI/ScreenUICoords.cpp:15`（投影）与 `:24`（修复前的 `scale(width, height, 1)`） | **唯一根因环** |

**为什么只有环 4 是根因**
1. 环 1/2/3 对 world 与 UI **完全共享**：两条路径取同一个 `SpriteQuadMesh` 单例（`SpriteComponent.cpp:183-189`、`WidgetComponent.cpp:378-384`）、
   同一套 uvRect（`SpriteComponent.cpp:84-92` / `ImageComponent.cpp:99`）、同一个 SpriteUnlit 着色器图
   （`ScreenUIMaterialFactory.cpp:18-21` → `SpriteMaterialFactory::CreateInstance`；shader 只有 `uv=lerp(uvMin,uvMax,attrib)`，无任何 V 翻转，`SpriteMaterialFactory.cpp:86-98`）。
   共享环不可能只让一边翻。
2. 两条路径**唯一**的差异就是"把模型空间放进屏幕空间"这一步：world 用相机 view-proj（Y 向上），UI 用 `MakePixelOrthoProjection`（Y 向下）。
   朝向不一致只能由这一步产生。
3. 该步的所有权就在这里：`ScreenUICoords` 的职责正是"widget 局部空间 ↔ 屏幕像素"换算（`ScreenUICoords.h:12-25`），
   且 `MakeWidgetModelMatrix` 是 image quad 唯一的 model 矩阵来源（`ForwardRenderer.cpp:1280`），
   所以在这一处镜像 Y 即覆盖全部 UI Image（Raw/Sprite 共用同一条绘制路径，不存在 SourceMode 分支）。
4. 反证：同一批 UI 文本走**另一条**路径——`TextMeshBuilder` 直接在 widget-local 像素（Y 向下）建网格，
   配 `MakeWidgetLocalPixelModelMatrix`（纯平移，`ScreenUICoords.cpp:32-36`，`ForwardRenderer.cpp:1275-1277`），
   字形 UV 为"v0 = cell 顶行"（`FontGlyphAtlas.cpp:237-241`）→ 文本一直是正的。
   即引擎本来就存在"Y-down 几何 + v=0 顶"与"Y-up 几何 + v=0 底"两套自洽约定；UI Image 错在把后者的几何放进了前者的空间。

## 修复

**单点改动（一行语义）：** `Runtime/Function/Render/ScreenUI/ScreenUICoords.cpp:18-30`

    const Matrix4 scale = glm::scale(Matrix4(1.0f), Vector3(width, -height, 1.0f));  // 原: (width, height, 1.0f)

- widget 仍占据 `topLeftPx .. topLeftPx+sizePx`（中心与半长不变：`-height` 只换轴方向，包围盒完全一致）。
- quad 的 -Y 边（uv.v=0，纹理底）现在落在 widget **底边**；uv.v=1（图像顶）落在顶边 → 与 world Sprite 一致。
- **不是** SourceMode 分支补丁：Raw/Sprite 共用 `ResolveDrawUVRect` → 同一个 model 矩阵，一处生效。
- **不是** `FlipV` 之类的开关：没有新增开关或配置项，UV 约定保持唯一（UI-F06:48 仍然成立）。
- **不是** UI 侧再叠一层例外：没有第二套 UV 转换，没有在材质/着色器里加 flip 参数。
- 绕序影响：ScreenUI pass 的 PSO 显式关闭背面剔除（`EnginePipelineLayouts.cpp:129-137`：`bCullEnabled=false`、`CullMode=None`），
  Y 镜像导致的绕序反转对 UI 绘制无影响。

**辅助改动（零行为变化，只为让 oracle 绑定真源）：** `SpriteQuadMesh.h/.cpp` 把顶点/索引数组从 `EnsureInitialized`
的局部变量提到文件作用域，新增 `GetInterleavedVertexData()` / `GetIndexData()` 与 `VertexCount` / `FloatsPerVertex` /
`TexCoordFloatOffset`；`EnsureInitialized` 改用同一份数组（字节内容一字未变）。测试因此读取**真实 quad 定义**，而不是把四组顶点抄一遍。

**未采用的替代方案**

| 方案 | 否决理由 |
|------|----------|
| 改 `SpriteQuadMesh` 的 uv（pos.y=-0.5 ↔ v=1） | 会把 **world Sprite 一起**翻过来，等于把 bug 搬到 3D 侧 |
| 改 `PixelRectToUvRectTopLeft` 不翻 V | 破坏 GL 上载约定（环 1），3D 贴图全错；且 UI-F06 明令禁止第二套 UV 转换 |
| 在 UI 的 uvRect 上再翻一次 V | UI 侧例外层，违反任务书与 UI-F06:48；Sprite/Atlas 子矩形会算错 |
| 像素正交投影改 Y 向上，用平移/负偏移摆位 | 改动面最大：命中测试（`UISystem`/`ScreenUIHitTester`）、文本网格、letterbox 映射全靠"像素 Y 向下"约定 |
| 纹理加载不翻转（flipVertical=false） | 同时改变 3D 全部贴图朝向，且与 `SpriteAtlasPacker` 既有约定冲突 |

## 验证

### 1. 自动化回归 oracle（新增套件 `screen-ui-orientation`）
`D:\Dev\GitRepo\minEngine\minEngine\Tests\Suites\ScreenUIOrientationTest.cpp`（6 用例 / 23 断言）：
1. `UI widget keeps texture bottom at screen bottom` —— 640×360 视口下 widget 像素矩形 x:[40,160] y:[20,80]，
   纹理底(v=0)边必须落在 y=80px、纹理顶(v=1)边落在 y=20px。
2. `world sprite keeps texture bottom at screen bottom` —— world 侧 model = 单位 quad（Sprite 在原点、Size=1）+ 标准 Y-up
   view-proj（`RHIClipSpace::MakeOrthographic(-1,1,-1,1,...)`，即相机提供的那一支）。
3. `world sprite and UI image agree on screen orientation` —— **唯一 oracle 本身**：两条路径朝向符号必须相等。
4. `widget model mirrors the shared quad Y axis` —— 直接钉住修复点：`model*(0,-0.5,0).y == 40`、`model*(0,+0.5,0).y == 0`。
5. `local pixel text matrix stays unmirrored` —— 反向保护：文本 local-pixel 矩阵不得被镜像（Y 仍向下）。
6. `2x2 probe top-left cell maps to GL upper half` —— 钉住纹理空间契约：2×2 左上像素格 (0,0,1,1) → uv `v∈[0.5,1]`。

**退化说明（为何不是像素断言）：** `minEngine` 测试面没有可用的离屏帧缓冲回读路径
（`Tests/` 下无任何 `glReadPixels`/帧抓取工具；`ScreenUIPass` 依赖 `RenderSystem` 的 RG 图与真实交换链），
所以按任务书退化为"几何/UV/顶点数据数值断言"这一层，像素层由下面的手动点验覆盖。

**通过命令与尾行**

    cd D:DevGitRepominEngineminEnginein
    .minEngineTests.exe test screen-ui-orientation

    [doctest] test cases:  6 |  6 passed | 0 failed | 214 skipped
    [doctest] assertions: 23 | 23 passed | 0 failed |
    [doctest] Status: SUCCESS!
    [12:23:03] [Test] TestRunner: suite 'screen-ui-orientation' PASSED.

**反向验证（oracle 真的会红）：** 把 `-height` 临时改回 `height` 并重建后，
`UI widget keeps texture bottom at screen bottom` 立即失败：`CHECK( -1 == Approx( 1 ) )`（`OrientationSign` = -1，即翻转），
suite FAILED。恢复修复后重新转绿。

### 2. 相关既有套件（无回归）

    screen-ui-coords PASSED         screen-ui-image PASSED         screen-ui-text PASSED
    screen-ui-hit PASSED            screen-ui-button PASSED        screen-ui-pointer PASSED
    screen-ui-drag PASSED           ui-layout PASSED               sprite-translucency PASSED
    sprite-atlas PASSED             sprite-runtime-consume PASSED  sprite-authoring PASSED
    mesh-draw-sort PASSED           render-graph PASSED

（注：`test smoke` 只跑 `TestSuiteRegistry.cpp:49-55` 里固定的 5 个核心套件，UI/sprite 套件属于 full 集合，
因此上面按 suite id 逐个跑。）

### 3. 手动点验（像素层，**待人工确认**）
探针资产：[BUG-RENDER-015_uvprobe_2x2.png](./BUG-RENDER-015_uvprobe_2x2.png)（2×2：**左上红 / 右上绿 / 左下蓝 / 右下黄**）
1. 把探针 PNG 拷进项目内容目录，在内容浏览器导入为 Texture2D（Filtering 选 Nearest 更清楚；Linear 也能判向）。
2. 用该纹理做一个 Sprite 资产（SourceRect 全图）。
3. Scene 里放 world 精灵：GameObject + `SpriteComponent`，Texture = 探针，Size = 200×200，摆在相机正前方可见处。
4. 同一 Scene 加 `Canvas` → 其下 `Widget`(200×200) + `ImageComponent`(SourceMode=`Raw`, Texture=探针)；
   再加第二个 Widget + `ImageComponent`(SourceMode=`Sprite`, Sprite=步骤 2 的 Sprite)。
5. 进 Play（PIE）。
6. **看什么**：三块图上四色的相对位置。
7. **期望**：三块**完全一致**，都是

       红 绿
       蓝 黄

   修复前：world 精灵如此，而两块 UI Image 是 蓝 黄 / 红 绿（上下颠倒）。
8. 附加检查：Canvas 的 ScaleMode 切到 `Letterbox` 再进 Play，结果不变（镜像在 model 矩阵里，与 letterbox 无关）。
9. 附加检查：UI 文本不应有任何镜像变化（`screen-ui-text` 套件 + 目视）。

> 编辑器自动截图 / MCP 取证：本仓有 Automation Host（`--automation-pipe`），但未暴露"抓 Play 帧缓冲"的能力；
> 本次未走该路径，故标注 **待人工点验**。

## 兼容影响
1. **既有场景 / Prefab / `.meta`：** 无格式、无序列化字段变化，不需要迁移；`.meta` 与 GUID 完全不受影响（没碰资产层）。
2. **会不会把"已经凑合对"的场景反过来？** 不会自动翻转任何东西，但有一种情况需要作者处置：修复前 UI Image 对任何
   **非对称内容**都是上下镜像的（2×2 四色即可证明），所以"凑合对"只可能来自作者**手工预翻转**过的资产
   （贴图本身预先垂直翻转过，或 Sprite 的 SourceRect 做了非满图裁切来抵消）。这类资产修复后**会重新变成镜像**。
   排查与处置：找出贴图预翻转或 `SourceRectPx` 非满图的 UI 资产，把预翻转撤掉即可（一次性、纯资产侧）。
   满图、未预翻转的资产（绝大多数）**自动变正确**。
3. **文本 / 纯色块：** 文本走另一条几何路径，本次未动 → 无影响；纯色（无贴图）Image 上下镜像不可见 → 无感知差异。
   9-slice / FillAmount 尚未实现（P1 待办），不受影响。
4. **命中测试 / 布局 / 拖拽：** 全部基于参考分辨率像素矩形（`ScreenUICoords::LetterboxMapping` 等），与 model 矩阵
   Y 方向无关 → 行为不变（`screen-ui-hit` / `ui-layout` / `screen-ui-drag` / `screen-ui-pointer` 全绿）。
5. **消费方 minCraft：** HUD / 容器面板图像从倒置变为正立；若其资产做过预翻转，按第 2 条回退即可。
   消费方仓本次**未改动**（只读参考）。
6. **glTF / 3D 贴图 / cubemap / HDR：** 完全不受影响（未碰 `ImageLoader` / `Texture2DLoader`）。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-09 | 登记；四方事实复核到 file:line；根因定位到 `MakeWidgetModelMatrix` 缺 Y 镜像 |
| 2026-09-09 | 单点修复 + 新增 `screen-ui-orientation` oracle 套件（含反向验证：去掉修复即红） |
| 2026-09-09 | 产出 2×2 四色探针资产与手动点验步骤；像素层待人工确认 |
