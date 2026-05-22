#include "MaterialEditorPreview.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/SceneViewport.h"

namespace minEngine
{
    void MaterialEditorPreview::EnsureInitialized(RHI* rhi, uint32_t width, uint32_t height)
    {
        if (m_Initialized || !rhi)
        {
            return;
        }

        m_Preview.Initialize(rhi, width, height);
        m_Initialized = true;
    }

    void MaterialEditorPreview::EnsureSceneBuilt()
    {
        if (!m_Initialized)
        {
            ME_CORE_WARN("MaterialEditorPreview: EnsureSceneBuilt before Initialize.");
            return;
        }

        if (!m_Preview.IsContentReady())
        {
            m_Preview.BuildPreviewScene();
        }
    }

    void MaterialEditorPreview::SetMaterial(const std::shared_ptr<Material>& material)
    {
        m_Preview.SetPreviewMaterial(material);
    }

    void MaterialEditorPreview::Shutdown()
    {
        m_Preview.Shutdown();
        m_Initialized = false;
    }

    SceneViewport& MaterialEditorPreview::GetSceneViewport()
    {
        return m_Preview.GetSceneViewport();
    }

    const SceneViewport& MaterialEditorPreview::GetSceneViewport() const
    {
        return m_Preview.GetSceneViewport();
    }
}
