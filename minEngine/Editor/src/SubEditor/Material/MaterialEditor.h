#pragma once

#include "Core.h"
#include "Preview/PreviewScene.h"
#include "MaterialEditorSession.h"
#include "MaterialEditorInspectorSource.h"
#include "Commands/EditorSetObjectPropertyTarget.h"
#include "Commands/Material/MaterialTopologyTypes.h"
#include "Commands/Scene/EditorSetObjectPropertyCommand.h"
#include "Shell/EditorSubModule.h"
#include "Runtime/Core/GUID/GUID.h"

#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Resource/AssetMeta.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class MaterialEdGraphNode;
    class IEditorContext;
    class MEObject;

    /** Material editing SubModule: session, graph, preview, compile. */
    class MaterialEditor : public EditorSubModule, public EditorSetObjectPropertyTarget
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

        bool ApplySetObjectProperty(const GUID& ownerGuid,
                                    const std::string& ownerClassName,
                                    const std::string& propertyPath,
                                    const std::vector<uint8_t>& valueBlob) override;
        void SubmitSetObjectProperty(IEditorContext& context,
                                     const GUID& ownerGuid,
                                     const std::string& ownerClassName,
                                     const std::string& propertyPath,
                                     std::vector<uint8_t> beforeValue,
                                     std::vector<uint8_t> afterValue,
                                     bool applyOnFirstExecute = true,
                                     EditorSetObjectPropertySideEffects sideEffects = {});

        void TryCapturePropertyUndoActivated(const MEObject& owner, const std::string& propertyPath);
        void TryCommitPropertyUndoAfterEdit(const MEObject& owner, const std::string& propertyPath);
        void TryCommitAllPropertyUndoAfterEdit(const MEObject& owner);
        void StorePropertyUndoBefore(const MEObject& owner, const std::string& propertyPath);
        void ClearPropertyUndoBefore(const MEObject& owner, const std::string& propertyPath);
        void CommitStoredPropertyUndo(const MEObject& owner, const std::string& propertyPath);

        bool ApplyAddNode(const std::string& nodeDefClassName,
                          float editorPosX,
                          float editorPosY,
                          GUID* outCreatedNodeDefGuid = nullptr);
        void SubmitAddNode(IEditorContext& context,
                           const std::string& nodeDefClassName,
                           float editorPosX,
                           float editorPosY);

        bool ApplyRemoveNodeByGuid(const GUID& nodeDefGuid);
        bool TryCaptureRemoveNode(const GUID& nodeDefGuid,
                                  std::vector<uint8_t>& outSnapshot,
                                  std::vector<MaterialNodeInboundLink>& outInboundLinks) const;
        bool ApplyRestoreNodeFromSnapshot(const std::vector<uint8_t>& snapshot,
                                          const std::vector<MaterialNodeInboundLink>& inboundLinks);
        void SubmitRemoveNode(IEditorContext& context, const GUID& nodeDefGuid);

        bool ApplyConnectPins(const GUID& fromNodeDefGuid,
                              int32_t fromOutputIndex,
                              const GUID& toNodeDefGuid,
                              int32_t toInputIndex);
        void SubmitConnectPins(IEditorContext& context,
                               const GUID& fromNodeDefGuid,
                               int32_t fromOutputIndex,
                               const GUID& toNodeDefGuid,
                               int32_t toInputIndex);

        bool ApplyDisconnectInput(const GUID& toNodeDefGuid, int32_t toInputIndex);
        void SubmitDisconnectInput(IEditorContext& context,
                                   const GUID& toNodeDefGuid,
                                   int32_t toInputIndex,
                                   const GUID& fromNodeDefGuid,
                                   int32_t fromOutputIndex);

        void NotifyGraphChanged();
        void InvalidateGraphCanvas(bool rebindGraph = true);

        /** Multi-document: stash/restore by asset path without destroying other sessions. */
        void StashActiveSession();
        bool ActivateStoredSession(const std::string& assetKey);
        void DiscardStoredSession(const std::string& assetKey);
        bool IsStoredSessionDirty(const std::string& assetKey) const;

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

        PreviewScene& GetPreviewScene() { return m_PreviewScene; }
        const PreviewScene& GetPreviewScene() const { return m_PreviewScene; }

        IEditorContext* GetEditorContext() const { return m_Context; }

    private:
        void OnEnterMode();
        void OnExitMode();
        void EnsureDefaultSession();
        void ApplySessionToPreview();
        void ScheduleDebouncedCompile();
        void FlushPendingCompile();

        struct MaterialOutputLinkRecord
        {
            uint64_t ToNodeDefHigh = 0;
            uint64_t ToNodeDefLow = 0;
            int32_t InputIndex = 0;
            uint64_t FromNodeDefHigh = 0;
            uint64_t FromNodeDefLow = 0;
            int32_t OutputIndex = 0;
        };

        struct PendingPropertyUndoField
        {
            std::string PropertyPath;
            std::vector<uint8_t> BeforeValue;
        };

        struct PendingPropertyUndo
        {
            GUID OwnerGuid;
            std::string OwnerClassName;
            std::vector<PendingPropertyUndoField> Fields;
        };

        void RefreshGraphAfterMutation();
        void CaptureMaterialOutputLinks(std::vector<MaterialOutputLinkRecord>& outLinks) const;
        void RestoreMaterialOutputLinks(const std::vector<MaterialOutputLinkRecord>& links);
        void SubmitCapabilityProperty(const std::string& propertyPath, const std::function<void()>& assignAfterValue);
        MaterialEdGraphNode* FindEdNodeByNodeDefGuid(const GUID& nodeDefGuid) const;
        bool SerializeOwnedProperty(const MEObject& owner, const std::string& propertyPath, std::vector<uint8_t>& outBlob) const;

        IEditorContext* m_Context = nullptr;
        MaterialEditorInspectorSource m_InspectorSource;
        MaterialEditorSession m_Session;
        std::unordered_map<std::string, MaterialEditorSession> m_SessionsByKey;
        PreviewScene m_PreviewScene;
        std::vector<const AssetMeta*> m_MaterialMetas;
        int m_SelectedMaterialIndex = -1;
        bool m_GraphCanvasInvalidated = false;
        bool m_GraphCanvasRebindPending = false;
        MaterialEdGraphNode* m_SelectedEdNode = nullptr;
        bool m_CompilePending = false;
        float m_CompileDebounceTimer = 0.0f;
        std::unordered_map<uint32_t, PendingPropertyUndo> m_PropertyUndoBeforeByEditId;
    };
}
