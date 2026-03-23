#pragma once
#include "Core.h"
#include "PrimitiveSceneProxy.h"

namespace minEngine
{
    class VertexBuffer;
    class VertexDefinition;
    class IndexBuffer;
    class Material;

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