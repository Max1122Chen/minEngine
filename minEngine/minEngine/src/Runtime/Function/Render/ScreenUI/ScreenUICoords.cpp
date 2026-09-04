#include "ScreenUICoords.h"

#include "Runtime/Function/Render/RHI/RHIClipSpace.h"

#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    Matrix4 ScreenUICoords::MakePixelOrthoProjection(float viewportWidth, float viewportHeight)
    {
        const float width = viewportWidth > 0.0f ? viewportWidth : 1.0f;
        const float height = viewportHeight > 0.0f ? viewportHeight : 1.0f;
        // top=0, bottom=height → pixel Y increases downward.
        return RHIClipSpace::MakeOrthographic(0.0f, width, height, 0.0f, -1.0f, 1.0f);
    }

    Matrix4 ScreenUICoords::MakeWidgetModelMatrix(const Vector2& topLeftPx, const Vector2& sizePx)
    {
        const float width = sizePx.x;
        const float height = sizePx.y;
        const Vector3 center(topLeftPx.x + width * 0.5f, topLeftPx.y + height * 0.5f, 0.0f);
        const Matrix4 translation = glm::translate(Matrix4(1.0f), center);
        const Matrix4 scale = glm::scale(Matrix4(1.0f), Vector3(width, height, 1.0f));
        return translation * scale;
    }
}
