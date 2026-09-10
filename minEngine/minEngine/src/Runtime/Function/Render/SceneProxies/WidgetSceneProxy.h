#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    class Material;
    class RHIBuffer;
    class RHIVertexInputLayout;
    class WidgetComponent;

    class WidgetSceneProxy
    {
    public:
        WidgetSceneProxy() = default;
        ~WidgetSceneProxy() = default;

        WidgetComponent* m_WidgetComponent = nullptr;

        Vector2 m_TopLeftPx{ 0.0f, 0.0f };
        Vector2 m_SizePx{ 100.0f, 100.0f };
        Vector4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 m_UVRect{ 0.0f, 0.0f, 1.0f, 1.0f };
        uint32_t m_StableOrder = 0;

        Texture2D* m_Texture = nullptr;
        Material* m_Material = nullptr;

        RHIBuffer* m_VertexBuffer = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;

        bool m_bVisible = false;
    };
}
