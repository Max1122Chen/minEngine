# 编辑器右键菜单系统设计

## 核心思想

右键菜单本质上不应该只是 UI 逻辑，而应该是：

> 基于 Context 查询可执行 Action 的系统。

即：

```txt
右键 -> 构建 Context
     -> 查询匹配 Action
     -> 生成菜单
     -> 执行 Command
```

UI（ImGui）只是最终的表现层。

---

# 一、推荐架构

建议拆分为：

```txt
EditorContext
EditorAction
EditorActionRegistry
EditorMenuBuilder
Command
```

整体流程：

```txt
右键 -> 构建 Context
     -> 查询匹配 Action
     -> 生成菜单
     -> 执行 Command
```

---

# 二、Context System（最关键）

不同窗口右键时的上下文完全不同：

## Hierarchy

可能：

- 点中 Entity
- 点中多个 Entity
- 点中空白

## Inspector

可能：

- 点中 Property
- 点中 Component
- 点中 Object

## Content Browser

可能：

- 点中 Asset
- 点中文件夹
- 多选

因此不能简单用 enum 表示上下文。

应该使用：

# “上下文对象集合”

---

## 推荐实现

不要做：

```cpp
struct EditorContext
{
    Entity* Entity;
    Asset* Asset;
    Property* Property;
};
```

因为后期会无限膨胀。

更推荐：

```cpp
class EditorContext
{
public:

    template<typename T>
    void Add(T*);

    template<typename T>
    T* Get();

    template<typename T>
    std::vector<T*> GetAll();
};
```

使用方式：

```cpp
ctx.Add(selectedEntity);
ctx.Add(clickedComponent);
ctx.Add(asset);
```

查询：

```cpp
if (ctx.Get<Entity>())
{
}
```

---

# 三、Action System

每个菜单项抽象成：

```cpp
class EditorAction
{
public:

    virtual String GetName() = 0;

    virtual bool CanExecute(const EditorContext&) = 0;

    virtual void Execute(const EditorContext&) = 0;
};
```

示例：

```cpp
class DeleteEntityAction : public EditorAction
{
public:

    bool CanExecute(const EditorContext& ctx) override
    {
        return ctx.Get<Entity>() != nullptr;
    }

    void Execute(const EditorContext& ctx) override
    {
        auto entity = ctx.Get<Entity>();

        GEditor->ExecuteCommand(
            new DeleteEntityCommand(entity)
        );
    }
};
```

---

# 四、Action Registry（非常关键）

不要在窗口里直接写：

```cpp
if(entity)
{
    MenuItem("Delete");
}
```

而是：

```cpp
for (auto action : ActionRegistry::GetActions(ctx))
{
    DrawMenuItem(action);
}
```

Registry：

```cpp
class EditorActionRegistry
{
public:

    void Register(EditorAction*);

    std::vector<EditorAction*> Query(
        const EditorContext& ctx
    );
};
```

这样：

# 新增菜单项不需要修改任何窗口代码

这是扩展性的核心。

---

# 五、Menu Builder（UI层）

窗口只负责：

- 提供 Context
- 请求菜单构建

例如：

```cpp
if (ImGui::BeginPopupContextItem())
{
    auto ctx = BuildHierarchyContext();

    auto actions = GActionRegistry->Query(ctx);

    for (auto action : actions)
    {
        if (ImGui::MenuItem(action->GetName()))
        {
            action->Execute(ctx);
        }
    }

    ImGui::EndPopup();
}
```

重点：

# Hierarchy 不知道具体有哪些菜单项

它只负责提供 Context。

---

# 六、分类与子菜单

后期一定会需要：

```txt
Create
 ├── Empty
 ├── Camera
 ├── Light

Asset
 ├── Rename
 ├── Reimport
```

因此 Action 建议支持：

```cpp
virtual String GetCategory();
```

例如：

```cpp
"Create"
"Asset"
"Transform"
```

MenuBuilder 自动生成子菜单。

---

# 七、动态菜单（非常重要）

例如：

```txt
Add Component
    -> 自动列出所有 Component 类
```

因此 Action 不应该全是静态的。

推荐支持：

```cpp
using ActionProvider = Function<void(
    const EditorContext&,
    MenuBuilder&
)>;
```

例如：

```cpp
registry.RegisterProvider(
    [](ctx, builder)
    {
        for(auto cls : ComponentClasses)
        {
            builder.AddAction(
                MakeAddComponentAction(cls)
            );
        }
    });
```

这会极大提升扩展性。

---

# 八、Command 集成

推荐：

# Action 不直接修改数据

而是：

```txt
Execute()
    -> 创建 Command
    -> Push Undo Stack
```

例如：

```txt
DeleteEntityAction
    -> DeleteEntityCommand

RenameAssetAction
    -> RenameAssetCommand
```

这样：

- 菜单
- Toolbar
- 快捷键
- Inspector 按钮

都能复用同一套逻辑。

这是现代编辑器的典型设计。

---

# 九、应该避免的设计

## 1. 把菜单写死在窗口里

错误：

```cpp
HierarchyWindow::DrawContextMenu()
{
    if(entity)
    {
        MenuItem("Delete");
        MenuItem("Rename");
    }
}
```

问题：

- 无法插件扩展
- 高耦合
- if 地狱

---

## 2. Context 用 enum

错误：

```cpp
enum ContextType
{
    Hierarchy,
    Inspector,
    ContentBrowser
};
```

因为一个菜单可能同时涉及：

- Entity
- Component
- Asset
- Folder

Context 本质上是：

# “一组对象”

不是单一类型。

---

## 3. Action 直接操作 UI

Action 不应该：

```cpp
ImGui::OpenPopup()
```

Action 只负责逻辑。

UI 属于 MenuBuilder。

---

# 十、推荐的最终架构

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

这样后期很容易支持：

- Toolbar
- 快捷键
- 命令搜索
- 插件扩展
- Lua / Python Editor Script
- 动态菜单
- 拖拽菜单

---

# 十一、反射系统的进一步扩展

你已经有反射系统。

未来甚至可以支持：

```cpp
UFUNCTION(EditorAction)
void Delete();
```

自动注册为菜单项。

甚至：

```cpp
UPROPERTY(ContextMenu)
```

自动生成 Inspector 菜单。

这也是 UE 编辑器很多能力的来源之一。

---

# 十二、推荐实现顺序

## 第一阶段（MVP）

实现：

- EditorContext
- EditorAction
- ActionRegistry
- MenuBuilder

支持：

- Delete
- Rename
- Duplicate

即可。

---

## 第二阶段

增加：

- Category
- SubMenu
- MultiSelection
- Dynamic Action

---

## 第三阶段

接入：

- Undo / Redo
- Shortcut
- Toolbar

统一成完整 Command System。

---

# 十三、最终总结

不要让：

```txt
右键菜单 == UI代码
```

而应该让：

```txt
右键菜单 == Context 上的 Action 查询结果
```

这是编辑器架构中非常重要的一步。
