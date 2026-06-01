# Render

> 文档建设中。

**代码目录：** `minEngine/minEngine/src/Runtime/Function/Render/`

渲染子系统负责将场景代理、材质与相机数据提交到 GPU，经 RenderPipeline 与各 Pass 输出最终画面。

## 管线概览

`RenderPipeline` 组织 BasePass、阴影、半透明、后处理、Present 等阶段。Pass 顺序与视口恢复是扩展时的常见风险点。

## RHI 与 OpenGL

当前 RHI 实现以 OpenGL 为主（`Function/Render/OpenGL/`），负责缓冲、纹理、着色器与绘制调用。

## 材质

材质 IR、编译器与编辑器图数据在 `Function/Render/Material/`；运行时 `Material` 与场景代理协作完成绘制。

### 相关入口（占位）

后续将补充 MIR、编译器与 Property 工具链的专页链接。
