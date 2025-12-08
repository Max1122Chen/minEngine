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



## LogSystem

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



## DeltaTime





## Abstract RHI into classes

尽管我们可以只使用单一的RHI，但是显然基于一定的可拓展性更有趣

我们至少需要把OpenGL的API抽象成自定义C++类



## RenderCommand

现代游戏引擎渲染场景的逻辑并不是逻辑层发生更新就立即渲染的，更“高阶”的做法是将逻辑层更新的内容收集起来，缓存起来，让渲染层可以对逻辑层一帧内的渲染需求进行一些渲染前的预处理，例如视锥外物体的剔除（也有可能在提交前由逻辑层处理），使用相同材质、相同资源的Drawcall的分组重排等等。

但是我们也并非在逻辑层中的每一次更新时就创建一个RenderCommand，因为物体在一帧中可能发生多次变化。每次发生微小变化就提交一次RenderCommand是不合理的，因为一帧内的逻辑层变化最终反映到视觉上也只是静态的一帧而已，我们要做的是在逻辑层中标记所有的变化，并且在帧末统一生成渲染指令并提交。





## Camera



## Texture

OpenGL的纹理有“位置”这个属性，为了可以在同一个shader中使用多个纹理



Texture就是一块“缓冲区”



## Shader







## Mesh

minEngine使用assimp库来导入模型，使用mingGW构建assimp库参考文献：[Windows环境下使用CMake+MinGW-w64编译模型加载库assimp | SIRLIS](https://sirlis.cn/posts/windows-mingw64-assimp/)
