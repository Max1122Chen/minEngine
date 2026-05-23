#pragma once

#include "Core.h"

#include "MaterialEditorPreview.h"
#include "MaterialEditorSession.h"

#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Resource/AssetMeta.h"

#include <vector>

namespace minEngine
{
    class MaterialEdGraphNode;
    class Editor;

    /** Session + commands for material editing (no ImGui). */
    class MaterialEditor
    {
    public:
        static constexpr const char* kPreviewViewportPanelId = "material_editor_preview";
        static constexpr float kCompileDebounceSeconds = 0.3f;

        explicit MaterialEditor(Editor& editor);

        void OnEnterMode();
        void OnExitMode();
        void Shutdown();

        void Tick(float deltaTime);

        /** Called when MaterialPreviewWindow attaches its viewport client. */
        void OnPreviewViewHostReady();

        void RefreshMaterialList();
        void OpenSession(const AssetMeta* meta);
        void CompileActiveMaterial();
        bool SaveActiveMaterial();
        void SetShadingModel(MaterialShadingModel model);
        void SetBlendMode(MaterialBlendMode blendMode);

        /** Graph edit: Finalize + debounced Compile + dirty. Does not reset node-editor pan/zoom. */
        void NotifyGraphChanged();

        /**
         * Refresh node-editor state. rebindGraph=true when switching .memtl (clears bound graph);
         * false after add/remove nodes (keeps pan/zoom and selection).
         */
        void InvalidateGraphCanvas(bool rebindGraph = true);

        void SetSelectedEdNode(MaterialEdGraphNode* node) { m_SelectedEdNode = node; }
        MaterialEdGraphNode* GetSelectedEdNode() const { return m_SelectedEdNode; }
        void ClearSelectedEdNode() { m_SelectedEdNode = nullptr; }

        bool ConsumeGraphCanvasInvalidation(bool& outRebindGraph)
        {
            outRebindGraph = m_GraphCanvasRebindPending;
            const bool invalidated = m_GraphCanvasInvalidated;
            m_GraphCanvasInvalidated = false;
            m_GraphCanvasRebindPending = false;
            return invalidated;
        }

        const MaterialEditorSession& GetSession() const { return m_Session; }
        MaterialEditorSession& GetSession() { return m_Session; }

        const std::vector<const AssetMeta*>& GetMaterialMetas() const { return m_MaterialMetas; }
        int GetSelectedMaterialIndex() const { return m_SelectedMaterialIndex; }

        MaterialEditorPreview& GetPreview() { return m_Preview; }
        const MaterialEditorPreview& GetPreview() const { return m_Preview; }

    private:
        void EnsureDefaultSession();
        void ApplySessionToPreview();
        void ScheduleDebouncedCompile();
        void FlushPendingCompile();

        Editor& m_Editor;
        MaterialEditorSession m_Session;
        MaterialEditorPreview m_Preview;
        std::vector<const AssetMeta*> m_MaterialMetas;
        int m_SelectedMaterialIndex = -1;
        bool m_GraphCanvasInvalidated = false;
        bool m_GraphCanvasRebindPending = false;
        MaterialEdGraphNode* m_SelectedEdNode = nullptr;
        bool m_CompilePending = false;
        float m_CompileDebounceTimer = 0.0f;
    };
}
