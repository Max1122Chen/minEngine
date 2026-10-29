#pragma once

#include "Core.h"
#include "PrimitiveSceneProxy.h"

namespace minEngine
{
    class Texture2D;

    class SpriteSceneProxy : public PrimitiveSceneProxy
    {
    public:
        SpriteSceneProxy() = default;
        virtual ~SpriteSceneProxy() = default;

        Texture2D* m_Texture = nullptr;
        Vector4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 m_UVRect{ 0.0f, 0.0f, 1.0f, 1.0f };
        Vector2 m_Size{ 1.0f, 1.0f };

        /** World matrix including Size scale (unit quad → world). */
        Matrix4 m_ModelMatrix{ 1.0f };
        bool m_bNeedsTranslucentPass = false;

        void GetMeshBatches(MeshBatchCollector& collector, const MeshCollectContext& context) const override;
    };
}
