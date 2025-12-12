#pragma once
#include "Core.h"
#include "PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RHI/RHIBuffer.h"
#include "Runtime/Function/Render/Material.h"

namespace minEngine
{
    struct Transform;

    class StaticMeshSceneProxy : public PrimitiveSceneProxy
    {
    public:
        StaticMeshSceneProxy() = default;
        virtual ~StaticMeshSceneProxy() = default;

        VertexBuffer* m_VertexBuffer = nullptr;
        VertexDefinition* m_VertexDefinition = nullptr;
        IndexBuffer* m_IndexBuffer = nullptr;

        Material* m_Material = nullptr;
    };
}