#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Transform.h"
#include "Runtime/Function/Render/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RHIBuffer.h"
#include "Runtime/Function/Render/Material.h"

namespace minEngine
{

    class StaticMeshSceneProxy : public PrimitiveSceneProxy
    {
    public:
        StaticMeshSceneProxy() = default;
        virtual ~StaticMeshSceneProxy() = default;

        Transform m_Transform;

        VertexBuffer* m_VertexBuffer = nullptr;
        VertexDefinition* m_VertexDefinition = nullptr;
        IndexBuffer* m_IndexBuffer = nullptr;

        Material* m_Material = nullptr;
    };
}