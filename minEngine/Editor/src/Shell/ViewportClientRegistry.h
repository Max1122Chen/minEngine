#pragma once

#include "Core.h"
#include "Viewport/EditorViewportClient.h"
#include "Viewport/MaterialPreviewViewportClient.h"
#include "Viewport/SceneEditingViewportClient.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace minEngine
{
    class IEditorContext;

    class ViewportClientRegistry
    {
    public:
        void SetContext(IEditorContext* context) { m_Context = context; }

        SceneEditingViewportClient& GetOrCreateSceneEditingViewportClient(
            const std::string& viewportId,
            const std::string& viewportTitle = "Scene");
        MaterialPreviewViewportClient& GetOrCreateMaterialPreviewViewportClient(
            const std::string& viewportId,
            const std::string& viewportTitle = "Material Preview");

        EditorViewportClient* FindViewportClient(const std::string& viewportId);
        const EditorViewportClient* FindViewportClient(const std::string& viewportId) const;
        SceneEditingViewportClient* FindSceneEditingViewportClient(const std::string& viewportId);
        MaterialPreviewViewportClient* FindMaterialPreviewViewportClient(const std::string& viewportId);

        void RemoveViewportClient(const std::string& viewportId);
        void Clear();

    private:
        IEditorContext* m_Context = nullptr;
        std::unordered_map<std::string, std::unique_ptr<EditorViewportClient>> m_Viewports;
    };
}
