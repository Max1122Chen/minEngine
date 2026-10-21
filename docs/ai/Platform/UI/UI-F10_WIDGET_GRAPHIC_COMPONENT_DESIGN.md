# UI-F10 — Widget 图形组件抽象（去具体类型依赖 + 多图形裁决 + Canvas 失效）— Design Spec

## Meta
- **ID:** `UI-F10` · **Status:** `Draft`（待主人审批，未授权实现）
- **Complexity:** `L2`（Components 层 + 渲染代理装配 + 命中；含一次基类引入与三处消费点）
- **Related:** `UI-F09`（表达力，排在本 feat 之后）· `BUG-CORE-007`（Canvas 失效，本 feat 的 S03）
- **行号基线:** 断言取自 2026-10-21 工作树；实现前按符号名重定位。

## Problem

| # | 事实 | 证据 |
|---|------|------|
| P1 | 一个 UIGO 只画一个图形是对的（对齐 Unity），但**额外的图形被静默丢弃**：有文字就直接 return，图片连判断都轮不到 | `WidgetComponent::FillSceneProxy`（文字分支 `return`） |
| P2 | `WidgetComponent` 认识两个具体子类，并替它们保管材质参数 | `WidgetComponent.h:13-14` 前向声明、`:84-85` `FindSiblingImage/FindSiblingText`、`:103` `SyncMaterialParameters(ImageComponent*)` |
| P3 | Canvas 组件失活不弄脏其后代 Widget，UI 保留最后一帧 | `BUG-CORE-007`；组件级失效只覆盖同 GO 兄弟 |

## Scope

**做**：引入图形抽象基类；Widget 只认基类；多图形时确定性裁决 + 告警；绘制与命中同源；Canvas 失活即时失效。
**不做**：代理下移到图形组件（单图形已足够）；编辑期检查；`BuildScreenUIQueue` 的 canvas 设置取数；表达力字段（属 `UI-F09`）。

## 接口设计

反射不支持多继承，用**抽象基类**而非接口（`ME_CLASS(Abstract)` 已有先例 `SceneComponent.h:18`）。

```cpp
// Framework/Components/WidgetGraphicComponent.h
ME_CLASS(Abstract)
class WidgetGraphicComponent : public Component      // 抽象，不可 Create
{
    ME_GENERATED_BODY()
public:
    /** 含自身活动态：关掉的图形不参与裁决。 */
    virtual bool CanContribute() const = 0;

    /** 产出一次绘制的数据；widget 只搬运，不懂来源。widgetSizePx 供文字重建网格用。 */
    virtual void BuildDrawData(const Vector2& widgetSizePx, WidgetDrawData& out) const = 0;

    /** 把自己的参数写进给定材质；材质实例仍由 widget 持有。 */
    virtual void ApplyMaterialParameters(Material& material) const = 0;
};

struct WidgetDrawData            // 纯数据，不含任何 Component 反向指针
{
    bool         bLocalPixelMesh = false;   // true: 用图形自带的顶点缓冲；false: 共享 quad
    RHIBuffer*   VertexBuffer = nullptr;
    RHIBuffer*   IndexBuffer = nullptr;
    RHIVertexInputLayout* VertexInputLayout = nullptr;
    Texture2D*   Texture = nullptr;
    Vector4      Color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4      UVRect{0.0f, 0.0f, 1.0f, 1.0f};
};
```

`ImageComponent : WidgetGraphicComponent`、`TextComponent : WidgetGraphicComponent`。
字段不动、序列化不动（基类不新增持久字段）。

Widget 侧唯一入口（命名按主人意见，不含 Primary/Secondary 暗示）：

```cpp
WidgetGraphicComponent* GetGraphic() const;   // 组件列表顺序中第一个 CanContribute() 为真的图形
bool HasGraphic() const { return GetGraphic() != nullptr; }
```

## 数据流（游戏线程内，单线程完成）

```
WidgetComponent::FillSceneProxy
  GetGraphic() ──null──> 空 widget：不装配（代理 m_bVisible=false），命中侧同样为假
  │
  └─> graphic->BuildDrawData(widgetSizePx, data)       // 数据从图形流向 widget
      graphic->ApplyMaterialParameters(material)       // 参数由来源提供，widget 不解释
      widget 把 data 搬进 WidgetSceneProxy（代理仍在 widget 上）
```

命中侧与绘制侧**共用 `GetGraphic()`**，杜绝「画的是 A、命中的是 B」。渲染线程仍只读代理位。

## 多图形裁决与告警

- 裁决：**组件列表顺序中第一个 `CanContribute()` 为真的图形**（列表顺序可由作者用 `ReorderComponents` 调整）。
- 告警：存在 ≥2 个图形组件时，每次代理重建打一条 `ME_LOG(Warn)`，列出【选中谁】【忽略了谁】【如何调整顺序】。主人已确认重复告警可接受。

## Canvas 失效（`BUG-CORE-007`）

`SetActive` 不是虚函数，但已有虚钩子：`CanvasComponent` 重写 `OnActivate()/OnDeactivate()`，遍历 `GetOwner()` 子树把所有 `WidgetComponent` 标脏。只做失效，不写作者化字段。

## 切片

| 片 | 内容 | 结束仍绿的理由 | 验收 |
|----|------|----------------|------|
| **S01** | 抽象基类 + Image/Text 继承 + Widget 改走 `GetGraphic()`/`BuildDrawData`/`ApplyMaterialParameters`；删除两处 `FindSibling*` 与具体类型重载 | 行为等价：单图形时选中的就是原来那个 | 现有 `screen-ui-*` 全套绿；`WidgetComponent.h` 不再出现 `ImageComponent`/`TextComponent`（grep=0）；导出 1283 |
| **S02** | 多图形裁决 + 去重告警 + 绘制/命中同源 | 单图形场景不变 | 用例：Image+Text → 只画第一个可贡献者、命中一致、重建不改变选择；新增/删除图形时告警出现 |
| **S03** | Canvas `OnActivate/OnDeactivate` → 弄脏后代 Widget（`BUG-CORE-007`） | 只加失效传播 | 关 Canvas 当帧不再绘制/命中；恢复后可绘制且作者化 flag 未变；**反向验证**移除传播会红 |

## Failure Model

| # | 失效 | 观测 | 处理 |
|---|------|------|------|
| F1 | 选中的图形 `CanContribute()` 为真但 `BuildDrawData` 产出空数据（如字体无效） | 代理 `m_bVisible=false`，但 `GetGraphic()` 仍返回它 → 命中与绘制可能不一致 | `BuildDrawData` 返回 `bool`；空数据时 Widget 退化为「无图形」，命中侧同样为假 |
| F2 | 图形组件在 widget 之后创建（顺序反了） | 选择结果突然换人 | 属作者问题，由告警暴露；不做自动仲裁 |
| F3 | Canvas 子树很大，激活时一次遍历 | 一次性 O(子树) 开销 | 只发生在激活变更，不进每帧路径 |
| F4 | 基类被误当可创建类型 | `NewObject` 到抽象类 | 反射 `Abstract` 标记阻止；补一条用例断言不可创建 |

## Verification（可自动断言）

1. `WidgetComponent.h` 与 `.cpp` 中 `ImageComponent`/`TextComponent` 出现次数 = **0**（grep 门禁，可加进 verify）。
2. Image+Text 同 GO：绘制源、命中源均等于同一组件（同一断言对象）。
3. 关掉 Image 的 active → Text 成为选中者（`CanContribute` 参与裁决）。
4. Canvas 失活当帧：Widget 不绘制、不命中；恢复后作者化 flag 值未变。
5. 抽象基类不可创建。
6. 导出 1283 不变；`verify.ps1` 为唯一门禁。

## 待审批决策

| # | 决策 | 推荐 | 理由 | 推翻条件 |
|---|------|------|------|----------|
| D1 | 基类放置 | `Framework/Components/WidgetGraphicComponent.h` | 与 Image/Text 同层，反射扫描同目录 | 若希望 UI 组件集中到子目录 |
| D2 | `GetGraphic()` 选「第一个存在的」还是「第一个可贡献的」 | **第一个可贡献的** | 否则关掉的 Image 会挡住 Text，与「关掉即不生效」矛盾 | 要求关掉的图形仍占据选中位（那样关掉 Image 得手工删组件） |
| D3 | `BuildDrawData` 返回 `bool`（F1） | 是 | 让「选中但画不出」可被绘制与命中一致处理 | 认为可用 `CanContribute` 覆盖全部情形 |
| D4 | 材质实例归属 | **留在 widget** | 一次绘制一份实例，图形只提供参数；若移到图形，多图形仍只有一个代理，反而要合并 | 将来代理下移到图形时一并搬走 |
| D5 | 告警粒度 | 每次代理重建一条 | 实现最简单；主人已确认刷屏可接受 | 日志被证明淹没真问题 → 改为集合变化时告警 |