#pragma once

#include "Core.h"
#include "SubEditor/Material/MaterialEditorViewportClient.h"
#include "SubEditor/Scene/SceneEditingViewportClient.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace minEngine
{
    class EditorViewportClient;
    class IEditorContext;

    class ViewportClientRegistry
    {
    public:
        void SetContext(IEditorContext* context) { m_Context = context; }

        SceneEditingViewportClient& GetOrCreateSceneEditingViewportClient(
            const std::string& viewportId,
            const std::string& viewportTitle = "Viewport");
        MaterialEditorViewportClient& GetOrCreateMaterialEditorViewportClient(
            const std::string& viewportId,
            const std::string& viewportTitle = "Material Editor Viewport");

        EditorViewportClient* FindViewportClient(const std::string& viewportId);
        const EditorViewportClient* FindViewportClient(const std::string& viewportId) const;
        SceneEditingViewportClient* FindSceneEditingViewportClient(const std::string& viewportId);
        MaterialEditorViewportClient* FindMaterialEditorViewportClient(const std::string& viewportId);

        void RemoveViewportClient(const std::string& viewportId);
        void Clear();

    private:
        IEditorContext* m_Context = nullptr;
        std::unordered_map<std::string, std::unique_ptr<EditorViewportClient>> m_Viewports;
    };
}
