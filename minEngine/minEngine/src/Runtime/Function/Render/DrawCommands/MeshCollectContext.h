#pragma once

#include "EngineAPI.h"
#include "Math/Geometry/AABB.h"
#include "Math/Geometry/Frustum.h"

#include <cstdint>

namespace minEngine
{
    /** Why this Collect ran — drives whether a main-view frustum may be supplied. */
    enum class MeshCollectPurpose : uint8_t
    {
        /** Opaque / translucent (and shadow draws from the same main-visible primitive). */
        MainView,
        /** Shadow-only candidates (failed main frustum; still casting). */
        ShadowOnly,
    };

    /**
     * Frame-local hints for PrimitiveSceneProxy::GetMeshBatches.
     * ViewFrustum is non-owning and valid only for the synchronous Collect call.
     * Null frustum = do not sub-cull (default proxies ignore; fine-grained proxies emit all).
     */
    struct MINENGINE_API MeshCollectContext
    {
        MeshCollectPurpose Purpose = MeshCollectPurpose::MainView;
        const Math::Geometry::Frustum* ViewFrustum = nullptr;

        /** Fail-open: null frustum or invalid bounds → emit. */
        bool AllowsWorldBounds(const Math::Geometry::AABB& worldBounds) const
        {
            if (ViewFrustum == nullptr)
            {
                return true;
            }
            if (!worldBounds.IsValid())
            {
                return true;
            }
            return ViewFrustum->Intersect(worldBounds);
        }
    };
}
