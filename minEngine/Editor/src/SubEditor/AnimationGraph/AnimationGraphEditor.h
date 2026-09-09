#pragma once

#include "Core.h"
#include "AnimationGraphEditorSession.h"
#include "AnimGraphInspectorSource.h"
#include "Shell/EditorSubModule.h"

#include "Runtime/Resource/AssetMeta.h"

#include <vector>

namespace minEngine
{
    class IEditorContext;

    /** Animation Graph editing SubModule: session, canvas, details, parameters. */
    class AnimationGraphEditor : public EditorSubModule
    {
    public:
        static constexpr const char* kModuleId = "AnimationGraph";

        AnimationGraphEditor();

        std::string_view GetModuleId() const override { return kModuleId; }
        std::string_view GetDisplayName() const override { return "Animation Graph"; }

        void Register(IEditorContext& context) override;
        void Shutdown() override;

        void OnActivate(IEditorContext& context) override;
        void OnDeactivate(IEditorContext& context) override;
        void RegisterCommands(IEditorContext& context) override;
        void UnregisterCommands(IEditorContext& context) override;

        void Tick(float deltaTime) override { (void)deltaTime; }

        void ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId) override;

        IEditorInspectorSource* GetInspectorSource() override { return &m_InspectorSource; }
        const IEditorInspectorSource* GetInspectorSource() const override { return &m_InspectorSource; }

        bool CanOpenAsset(const AssetMeta& meta) const override;
        bool OpenAsset(const AssetMeta& meta) override;

        void RefreshGraphList();
        void OpenSession(const AssetMeta* meta);
        bool SaveActiveGraph();
        bool ValidateActiveGraph(std::string* outError = nullptr) const;

        void NotifyGraphChanged();
        void InvalidateGraphCanvas(bool rebindGraph = true);

        bool ConsumeGraphCanvasInvalidation(bool& outRebindGraph)
        {
            outRebindGraph = m_GraphCanvasRebindPending;
            const bool invalidated = m_GraphCanvasInvalidated;
            m_GraphCanvasInvalidated = false;
            m_GraphCanvasRebindPending = false;
            return invalidated;
        }

        void ClearSelection() { m_Session.Selection.Clear(); }
        void SetSelection(AnimGraphSelection selection) { m_Session.Selection = std::move(selection); }

        const AnimationGraphEditorSession& GetSession() const { return m_Session; }
        AnimationGraphEditorSession& GetSession() { return m_Session; }

        const std::vector<const AssetMeta*>& GetGraphMetas() const { return m_GraphMetas; }
        int GetSelectedGraphIndex() const { return m_SelectedGraphIndex; }

        IEditorContext* GetEditorContext() const { return m_Context; }

        std::string MakeUniqueStateName(std::string_view baseName) const;
        bool AddStateAt(float editorPosX, float editorPosY, std::string* outName = nullptr);
        bool RemoveStateByName(std::string_view stateName);
        bool RenameState(std::string_view oldName, std::string_view newName, std::string* outError = nullptr);
        bool AddTransition(std::string_view fromState, std::string_view toState, std::string* outError = nullptr);
        bool RemoveTransitionAt(size_t transitionIndex);
        bool RemoveAnyStateTransitionAt(size_t anyTransitionIndex);
        bool ReverseTransition();

    private:
        void OnEnterMode();
        void OnExitMode();
        void EnsureDefaultSession();

        AnimGraphInspectorSource m_InspectorSource;
        IEditorContext* m_Context = nullptr;
        AnimationGraphEditorSession m_Session;
        std::vector<const AssetMeta*> m_GraphMetas;
        int m_SelectedGraphIndex = -1;
        bool m_GraphCanvasInvalidated = false;
        bool m_GraphCanvasRebindPending = false;
    };
}
