#pragma once

#include "Core.h"
#include "Runtime/Function/Render/MaterialPreviewViewport.h"

#include <memory>

namespace minEngine
{
    class Material;
    class RHI;

    /** Preview world owned by MaterialEditor (RT + sphere scene + material binding). */
    class MaterialEditorPreview
    {
    public:
        void EnsureInitialized(RHI* rhi, uint32_t width, uint32_t height);
        void EnsureSceneBuilt();
        void SetMaterial(const std::shared_ptr<Material>& material);
        void Shutdown();

        bool IsInitialized() const { return m_Initialized; }
        bool IsSceneReady() const { return m_Preview.IsContentReady(); }

        MaterialPreviewViewport& GetPreview() { return m_Preview; }
        const MaterialPreviewViewport& GetPreview() const { return m_Preview; }

        SceneViewport& GetSceneViewport();
        const SceneViewport& GetSceneViewport() const;

    private:
        MaterialPreviewViewport m_Preview;
        bool m_Initialized = false;
    };
}
