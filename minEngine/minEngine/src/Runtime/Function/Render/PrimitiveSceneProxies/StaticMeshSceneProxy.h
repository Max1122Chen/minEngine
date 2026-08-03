#pragma once
#include "Core.h"
#include "PrimitiveSceneProxy.h"

namespace minEngine
{
    class Material;
    class RHIBuffer;
    class RHIVertexInputLayout;

    class StaticMeshSceneProxy : public PrimitiveSceneProxy
    {
    public:
        StaticMeshSceneProxy() = default;
        virtual ~StaticMeshSceneProxy() = default;

        // Non-owning; GPU resources are owned by StaticMesh (asset).
        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        Material* m_Material = nullptr;
    };
}
