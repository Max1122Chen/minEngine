# minEngine

## Engine Design

refs：

[How To Make a Game Engine: The Easy Way](https://gamedesigning.org/learn/make-a-game-engine/)

[从零手写游戏引擎 - 知乎](https://www.zhihu.com/column/c_1352653422265643008)







## Project Setting



## Entry Point

我们有一个Application作为engine的中心组件

EntryPoint（main）设置在Engine中，外部传入Application的子类对象来指导Engine的运作

Engine会被制作成dll来灵活加载



## Logging

使用spdlog

通过git submodule add 把spdlog的仓库导入为一个submodule

我们需要为引擎包装spdlog，提供引擎层面的接口，这样即使我们后续不想用spdlog作为引擎log的支持了，也不会太严重地破坏已有的东西

我们只需要定义一个简单的Log类，将其和所使用的log库对接起来，并且自定义Log宏



## Event System

我们的按键、鼠标等都会产生某种“事件”

我们会拥有一个游戏窗口/引擎编辑器窗口，一个”Window“

我们需要将Window收集到的事件发送给Application，让Application处理它们

但是我们不想让Windows依赖Application，不想让它察觉到Application的存在

我们会用callback来让Window把事件发回去给Application



这只是一个很简单的Event System，我们暂时不会使用事件队列去优化事件处理机制

任何一个新事件的出现都会阻塞当前的程序，it‘s blocking



## Window



## Abstract RHI into classes

尽管我们可以只使用单一的RHI，但是显然基于一定的可拓展性更有趣

我们至少需要把OpenGL的API抽象成自定义C++类

