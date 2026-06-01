# Math

数学模块提供引擎内统一的**向量、矩阵**类型与少量**几何**工具。

## 第三方库：GLM

`Math.h` 将 [OpenGL Mathematics (GLM)](https://github.com/g-truc/glm) 映射为引擎别名，而非自研线性代数：

| 别名 | GLM 类型 |
|------|----------|
| `Vector2` / `Vector3` / `Vector4` | `glm::vec2` / `vec3` / `vec4` |
| `Matrix3` / `Matrix4` | `glm::mat3` / `mat4` |

常用工具函数（如 `radians`）直接转发到 `glm::*`。渲染与场景代码应优先使用这些别名，避免在业务层混用裸 `glm::` 类型。

## 引擎自有补充

| 内容 | 说明 |
|------|------|
| `Color` | 颜色工具（`Math/Color.h`，非 GLM 内置类型封装） |
| `Geometry/AABB` | 轴对齐包围盒，变换与射线检测等 |
| `Geometry/Ray` | 射线 |

几何实现中会调用 GLM（如 `glm::min` / `glm::max`）。

## 与反射

`Vector2/3/4` 在反射/序列化中多按**标量组合或 primitive 策略**处理（见反射文档），编辑器展示可能较 UE 的 `FVector` 结构体属性更简化。

**入口：** `Runtime/Core/Math/Math.h`, `Runtime/Core/Math/Geometry/`
