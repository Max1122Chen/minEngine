#pragma once
#include "Core.h"
#include "PrimitiveSceneProxy.h"

#include <vector>

namespace minEngine
{
    class Material;
    class RHIBuffer;
    class RHIVertexInputLayout;

    class SkeletalMeshSceneProxy : public PrimitiveSceneProxy
    {
    public:
        SkeletalMeshSceneProxy() = default;
        ~SkeletalMeshSceneProxy() override = default;

        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;
        Material* m_Material = nullptr;

        std::vector<Matrix4> m_BonePalette;
    };
}
