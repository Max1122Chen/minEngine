# UE 编辑器 Context Menu 系统设计分析

# 一、UE 的整体设计思路

UE 的现代编辑器菜单系统本质上是：

```txt
UI
 └── ToolMenus
      └── ToolMenu
            └── ToolMenuSection
                  └── ToolMenuEntry

Context
 └── ToolMenuContext

行为
 └── UICommand

对象抽象
 └── TypedElementFramework
```

其中最关键的是：

# ToolMenuContext

它决定：

- 当前有哪些菜单项
- 哪些 Action 可执行
- 菜单是否显示
- 是否可删除 / 重命名 / 复制

UE 的核心思想是：

# 菜单依赖 Context，而不是依赖窗口

---

# 二、为什么 UE 要使用 Context

因为很多编辑器窗口都需要右键菜单：

- ContentBrowser
- SceneOutliner（Hierarchy）
- BlueprintEditor
- MaterialEditor
- GraphEditor
- AssetEditor

但：

很多操作本身是共通的：

```txt
Delete
Rename
Duplicate
Copy
Paste
Save
```

UE 不希望：

```txt
每个窗口自己写一遍菜单逻辑
```

因此：

# 菜单项只依赖 Context

窗口只负责：

```txt
提供 Context
```

---

# 三、UE 的 ToolMenuContext 本质

UE 的：

```cpp
FToolMenuContext
```

本质上是：

# “异构对象容器”

类似：

```cpp
class FToolMenuContext
{
public:

    template<typename T>
    void AddObject(TSharedRef<T>);

    template<typename T>
    T* FindContext() const;
};
```

使用：

```cpp
Context.AddObject(SelectedAssetContext);
Context.AddObject(SceneOutlinerContext);
Context.AddObject(LevelEditorContext);
```

查询：

```cpp
auto* AssetContext =
    Context.FindContext<UContentBrowserAssetContextMenuContext>();
```

这与推荐的：

```cpp
EditorContext
    -> Add<T>()
    -> Find<T>()
```

是高度一致的。

---

# 四、UE 的关键设计：Context Object

很多人会这样设计：

```cpp
struct EditorContext
{
    Entity* Entity;
    Asset* Asset;
    Property* Property;
};
```

UE 不喜欢这种设计。

因为后期会变成：

```txt
越来越胖
越来越耦合
```

最终：

```cpp
if (ctx.Asset && ctx.Entity)
```

到处飞。

---

# 五、UE 的真正做法

UE 更倾向：

# “多个小型 Typed Context Object 的组合”

例如：

## ContentBrowser

```cpp
UContentBrowserAssetContextMenuContext
```

内部：

```cpp
SelectedAssets
CurrentPath
CanRename
CanDelete
```

---

## SceneOutliner

```cpp
USceneOutlinerMenuContext
```

内部：

```cpp
SelectedActors
World
Folder
```

---

## Inspector

可能提供：

```cpp
InspectedObject
HoveredProperty
```

---

然后：

```cpp
ToolMenuContext.AddObject(...)
```

把它们组合起来。

---

# 六、推荐模仿 UE 的 EditorContext 设计

不要：

```cpp
class EditorContext
{
    Entity* Entity;
    Asset* Asset;
};
```

推荐：

```cpp
class EditorContext
{
public:

    template<typename T>
    void Add(TSharedPtr<T>);

    template<typename T>
    T* Find() const;
};
```

---

## 示例

## HierarchyContext

```cpp
class HierarchyContext
{
public:

    Array<Entity*> SelectedEntities;
};
```

---

## InspectorContext

```cpp
class InspectorContext
{
public:

    Object* InspectedObject;

    Property* HoveredProperty;
};
```

---

## ContentBrowserContext

```cpp
class ContentBrowserContext
{
public:

    Array<AssetHandle> SelectedAssets;

    Path CurrentFolder;
};
```

---

使用：

```cpp
EditorContext ctx;

ctx.Add(MakeShared<HierarchyContext>());
ctx.Add(MakeShared<SelectionContext>());
```

查询：

```cpp
auto* hierarchy =
    ctx.Find<HierarchyContext>();
```

---

# 七、UE 更高级的设计：Capability 思想

UE 后期逐渐不再关注：

```txt
对象是什么类型
```

而开始关注：

```txt
对象有什么能力
```

例如：

```cpp
class IDeleteProvider
{
public:
    virtual void Delete() = 0;
};
```

Context 中提供：

```txt
DeleteProvider
RenameProvider
DuplicateProvider
```

菜单只判断：

```txt
当前是否支持 Delete
```

而不是：

```txt
当前是不是 Asset
```

这是非常高级的编辑器架构思想。

---

# 八、UE5 的 TypedElementFramework

UE5 后来发现：

很多完全不同的对象：

- Actor
- Component
- Asset
- Foliage
- MeshInstance

都需要：

- Selection
- Delete
- Duplicate
- Transform

因此 UE5 引入：

# TypedElementFramework

核心：

```cpp
TypedElementHandle
```

通过接口查询能力：

```cpp
CanDelete()
CanDuplicate()
GetTransform()
```

这实际上已经非常接近：

# Editor ECS

思想。

---

# 九、现阶段最适合的实现方案

现阶段不建议直接实现 TypedElementFramework。

太复杂。

更适合：

---

# 第一层：Typed Context Object

```cpp
EditorContext
    -> Add<T>()
    -> Find<T>()
```

---

# 第二层：Action Registry

```cpp
CanExecute(ctx)
```

---

# 第三层：Command System

```cpp
Execute()
Undo()
Redo()
```

---

# 第四层（未来）

Capability System：

```cpp
IDeletable
IRenameable
ITransformable
```

---

# 十、UE 中值得重点学习的部分

## 1. ToolMenus

重点：

```txt
UToolMenus
FToolMenuContext
FToolMenuSection
```

这是现代 UE 菜单系统核心。

---

## 2. SceneOutliner

重点：

```txt
SceneOutlinerMenuContext
```

这是 Hierarchy 右键菜单。

---

## 3. ContentBrowser

重点：

```txt
ContentBrowserAssetContextMenuContext
```

这是资产右键菜单。

---

## 4. TypedElementFramework（后期）

重点：

```txt
TypedElementHandle
TypedElementSelectionSet
```

---

# 十一、一个非常重要的经验

不要：

# “菜单决定能不能操作”

例如：

```cpp
if (asset)
{
    ShowDelete();
}
```

更好的：

```cpp
if (CanDelete(ctx))
```

再进一步：

```cpp
if (ctx.Has<IDeletable>())
```

即：

# 对象自己声明支持什么操作

这是现代编辑器架构中非常重要的思想。

---

# 十二、最终推荐的整体架构

```txt
Editor
 ├── ActionRegistry
 ├── CommandManager
 ├── SelectionSystem
 ├── ContextSystem
 ├── ToolMenuSystem
```

窗口：

```txt
只负责：
- 提供 Context
- 请求菜单构建
```

Action：

```txt
只负责：
- 查询 Context
- 判断 CanExecute
- 执行 Command
```

Command：

```txt
只负责：
- Execute
- Undo
- Redo
```

这样未来可以自然扩展：

- Toolbar
- Shortcut
- Search Command
- Lua / Python Editor Script
- 插件系统
- 动态菜单
- Editor Automation

