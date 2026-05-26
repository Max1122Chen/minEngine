#include "Shell/ViewportClientRegistry.h"

#include "Shell/IEditorContext.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"

namespace minEngine
{
    SceneEditingViewportClient& ViewportClientRegistry::GetOrCreateSceneEditingViewportClient(
        const std::string& viewportId,
        const std::string& viewportTitle)
    {
        const std::string key = viewportId.empty() ? viewportTitle : viewportId;
        if (auto iter = m_Viewports.find(key); iter != m_Viewports.end() && iter->second)
        {
            return static_cast<SceneEditingViewportClient&>(*iter->second);
        }

        auto client = std::make_unique<SceneEditingViewportClient>(viewportTitle.empty() ? key : viewportTitle);
        SceneEditingViewportClient* created = client.get();
        created->SetEditorContext(m_Context);
        if (RHI* rhi = RenderSystem::Get().GetRHI())
        {
            created->InitializeSceneViewport(rhi, 1920, 1080);
        }
        m_Viewports[key] = std::move(client);
        return *created;
    }

    MaterialEditorViewportClient& ViewportClientRegistry::GetOrCreateMaterialEditorViewportClient(
        const std::string& viewportId,
        const std::string& viewportTitle)
    {
        const std::string key = viewportId.empty() ? viewportTitle : viewportId;
        if (auto iter = m_Viewports.find(key); iter != m_Viewports.end() && iter->second)
        {
            return static_cast<MaterialEditorViewportClient&>(*iter->second);
        }

        auto client = std::make_unique<MaterialEditorViewportClient>(viewportTitle.empty() ? key : viewportTitle);
        MaterialEditorViewportClient* created = client.get();
        created->SetEditorContext(m_Context);
        if (RHI* rhi = RenderSystem::Get().GetRHI())
        {
            created->InitializeEditorSceneViewport(rhi, 512, 512);
            created->SetupDefaultPreviewCamera();
        }
        m_Viewports[key] = std::move(client);
        return *created;
    }

    EditorViewportClient* ViewportClientRegistry::FindViewportClient(const std::string& viewportId)
    {
        const auto iter = m_Viewports.find(viewportId);
        return iter != m_Viewports.end() ? iter->second.get() : nullptr;
    }

    const EditorViewportClient* ViewportClientRegistry::FindViewportClient(const std::string& viewportId) const
    {
        const auto iter = m_Viewports.find(viewportId);
        return iter != m_Viewports.end() ? iter->second.get() : nullptr;
    }

    SceneEditingViewportClient* ViewportClientRegistry::FindSceneEditingViewportClient(const std::string& viewportId)
    {
        return dynamic_cast<SceneEditingViewportClient*>(FindViewportClient(viewportId));
    }

    MaterialEditorViewportClient* ViewportClientRegistry::FindMaterialEditorViewportClient(const std::string& viewportId)
    {
        return dynamic_cast<MaterialEditorViewportClient*>(FindViewportClient(viewportId));
    }

    void ViewportClientRegistry::RemoveViewportClient(const std::string& viewportId)
    {
        m_Viewports.erase(viewportId);
    }

    void ViewportClientRegistry::Clear()
    {
        m_Viewports.clear();
    }
}
