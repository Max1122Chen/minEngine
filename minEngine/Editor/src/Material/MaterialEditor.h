#pragma once

#include "Core.h"
#include "MaterialEditorPreview.h"
#include "MaterialEditorSession.h"
#include "MaterialEditorInspectorSource.h"
#include "Shell/EditorSubModule.h"

#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Resource/AssetMeta.h"

#include <vector>

namespace minEngine
{
    class MaterialEdGraphNode;
    class IEditorContext;

    /** Material editing SubModule: session, graph, preview, compile. */
    class MaterialEditor : public EditorSubModule
    {
    public:
        static constexpr const char* kModuleId = "Material";
        static constexpr const char* kPreviewViewportPanelId = "material_editor_preview";
        static constexpr float kCompileDebounceSeconds = 0.3f;

        MaterialEditor();

        std::string_view GetModuleId() const override { return kModuleId; }
        std::string_view GetDisplayName() const override { return "Material"; }

        void Register(IEditorContext& context) override;
        void Shutdown() override;

        void OnActivate(IEditorContext& context) override;
        void OnDeactivate(IEditorContext& context) override;
        void RegisterCommands(IEditorContext& context) override;
        void UnregisterCommands(IEditorContext& context) override;
        void Tick(float deltaTime) override;

        void ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId) override;

        IEditorInspectorSource* GetInspectorSource() override { return &m_InspectorSource; }
        const IEditorInspectorSource* GetInspectorSource() const override { return &m_InspectorSource; }

        bool CanOpenAsset(const AssetMeta& meta) const override;
        bool OpenAsset(const AssetMeta& meta) override;
        bool RouteViewportInput(EditorViewportClient& client) override;

        void OnPreviewViewHostReady();

        void RefreshMaterialList();
        void OpenSession(const AssetMeta* meta);
        void CompileActiveMaterial();
        bool SaveActiveMaterial();
        void SetShadingModel(MaterialShadingModel model);
        void SetBlendMode(MaterialBlendMode blendMode);

        void NotifyGraphChanged();
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

        IEditorContext* GetEditorContext() const { return m_Context; }

    private:
        void OnEnterMode();
        void OnExitMode();
        void EnsureDefaultSession();
        void ApplySessionToPreview();
        void ScheduleDebouncedCompile();
        void FlushPendingCompile();

        IEditorContext* m_Context = nullptr;
        MaterialEditorInspectorSource m_InspectorSource;
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
