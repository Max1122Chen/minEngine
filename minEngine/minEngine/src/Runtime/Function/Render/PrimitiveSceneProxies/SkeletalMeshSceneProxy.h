#pragma once
#include "Core.h"
#include "PrimitiveSceneProxy.h"

#include <vector>

namespace minEngine
{
    class SkeletalMeshSceneProxy : public PrimitiveSceneProxy
    {
    public:
        SkeletalMeshSceneProxy() = default;
        ~SkeletalMeshSceneProxy() override = default;

        std::vector<Matrix4> m_BonePalette;

        void GetMeshBatches(class MeshBatchCollector& collector) const override;
    };
}
