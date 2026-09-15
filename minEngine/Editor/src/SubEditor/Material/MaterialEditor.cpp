#include "MaterialEditor.h"

#include "Commands/EditorObjectPropertyApply.h"
#include "Commands/Material/EditorAddMaterialNodeCommand.h"
#include "Commands/Material/EditorConnectMaterialPinsCommand.h"
#include "Commands/Material/EditorDisconnectMaterialPinCommand.h"
#include "Commands/Material/EditorRemoveMaterialNodeCommand.h"
#include "Commands/Scene/EditorSetObjectPropertyCommand.h"
#include "EditorGUIManager.h"
#include "SubEditor/Material/MaterialGraphIds.h"
#include "Shell/EditorDockLayout.h"
#include "Shell/EditorInputHub.h"
#include "Shell/IEditorContext.h"
#include "Shell/ViewportClientRegistry.h"

#include "imgui.h"

#include "UI/EditorWindows/MaterialGraphWindow.h"
#include "UI/EditorWindows/MaterialEditorViewportWindow.h"
#include "SubEditor/Material/MaterialEditorViewportClient.h"

#include "Runtime/Resource/AssetMeta.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>

namespace minEngine
{
    MaterialEditor::MaterialEditor()
        : m_InspectorSource(*this)
    {
    }

    void MaterialEditor::Register(IEditorContext& context)
    {
        m_Context = &context;
        EditorGUIManager& gui = context.GetGUIManager();
        gui.RegisterWindow(std::make_unique<MaterialGraphWindow>(context));
        gui.RegisterWindow(std::make_unique<MaterialEditorViewportWindow>(context));
    }

    void MaterialEditor::OnActivate(IEditorContext& context)
    {
        (void)context;
        OnEnterMode();
    }

    void MaterialEditor::OnDeactivate(IEditorContext& context)
    {
        (void)context;
        OnExitMode();
    }

    void MaterialEditor::RegisterCommands(IEditorContext& context)
    {
        EditorCommandBinding saveMaterialCommand;
        saveMaterialCommand.Name = "Save Material";
        saveMaterialCommand.Chord = { ImGuiKey_S, true, false, false };
        saveMaterialCommand.CanExecute = [this]() { return m_Session.HasOpenMaterial(); };
        saveMaterialCommand.Execute = [this]() { SaveActiveMaterial(); };
        context.GetInputHub().RegisterActiveSubModuleCommand(std::move(saveMaterialCommand));
    }

    void MaterialEditor::UnregisterCommands(IEditorContext& context)
    {
        context.GetInputHub().ClearActiveSubModuleCommands();
    }

    void MaterialEditor::ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId)
    {
        (void)context;
        EditorDockLayout::BuildMaterialEditingLayout(dockspaceId);
    }

    bool MaterialEditor::CanOpenAsset(const AssetMeta& meta) const
    {
        return meta.AssetType == "Material";
    }

    bool MaterialEditor::OpenAsset(const AssetMeta& meta)
    {
        OpenSession(&meta);
        return true;
    }

    bool MaterialEditor::RouteViewportInput(EditorViewportClient& client)
    {
        (void)client;
        return dynamic_cast<MaterialEditorViewportClient*>(&client) != nullptr;
    }

    void MaterialEditor::OnEnterMode()
    {
        RefreshMaterialList();

        if (!m_PreviewScene.IsContentReady())
        {
            m_PreviewScene.BuildDefaultSphereScene();
        }
        // DocumentHost owns which materials are open; do not auto-open the first asset.
        ApplySessionToPreview();
    }

    void MaterialEditor::OnExitMode()
    {
        // Keep material sessions/preview alive for multi-document host (ED-F11).
        StashActiveSession();
        FlushPendingCompile();
        ClearSelectedEdNode();
    }

    void MaterialEditor::StashActiveSession()
    {
        if (!m_Session.HasOpenMaterial())
        {
            return;
        }
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool MaterialEditor::ActivateStoredSession(const std::string& assetKey)
    {
        if (m_Session.HasOpenMaterial() && m_Session.AssetPath == assetKey)
        {
            return true;
        }

        StashActiveSession();
        const auto it = m_SessionsByKey.find(assetKey);
        if (it == m_SessionsByKey.end())
        {
            return false;
        }

        m_Session = it->second;
        ClearSelectedEdNode();
        ApplySessionToPreview();
        InvalidateGraphCanvas();
        return m_Session.HasOpenMaterial();
    }

    void MaterialEditor::DiscardStoredSession(const std::string& assetKey)
    {
        m_SessionsByKey.erase(assetKey);
        if (m_Session.AssetPath == assetKey)
        {
            m_Session.Clear();
            ClearSelectedEdNode();
            ApplySessionToPreview();
            InvalidateGraphCanvas();
        }
    }

    bool MaterialEditor::IsStoredSessionDirty(const std::string& assetKey) const
    {
        if (m_Session.AssetPath == assetKey)
        {
            return m_Session.Dirty;
        }
        const auto it = m_SessionsByKey.find(assetKey);
        return it != m_SessionsByKey.end() && it->second.Dirty;
    }

    void MaterialEditor::Shutdown()
    {
        m_PreviewScene.Shutdown();
        m_Context = nullptr;
    }

    void MaterialEditor::Tick(float deltaTime)
    {
        if (!m_CompilePending || !m_Session.HasOpenMaterial())
        {
            return;
        }

        m_CompileDebounceTimer -= deltaTime;
        if (m_CompileDebounceTimer <= 0.0f)
        {
            FlushPendingCompile();
        }
    }

    void MaterialEditor::OnPreviewViewHostReady()
    {
        if (!m_Context)
        {
            return;
        }

        m_Context->GetViewportRegistry().GetOrCreateMaterialEditorViewportClient(
            kPreviewViewportPanelId, "Material Editor Viewport");

        if (m_PreviewScene.IsContentReady())
        {
            ApplySessionToPreview();
        }

        InvalidateGraphCanvas();
    }

    void MaterialEditor::ScheduleDebouncedCompile()
    {
        m_CompilePending = true;
        m_CompileDebounceTimer = kCompileDebounceSeconds;
    }

    void MaterialEditor::FlushPendingCompile()
    {
        m_CompilePending = false;
        m_CompileDebounceTimer = 0.0f;
        CompileActiveMaterial();
    }

    void MaterialEditor::InvalidateGraphCanvas(bool rebindGraph)
    {
        m_GraphCanvasInvalidated = true;
        if (rebindGraph)
        {
            m_GraphCanvasRebindPending = true;
        }
    }

    void MaterialEditor::NotifyGraphChanged()
    {
        if (!m_Session.HasOpenMaterial())
        {
            return;
        }

        m_Session.Dirty = true;
        if (!m_Session.AssetPath.empty())
        {
            m_SessionsByKey[m_Session.AssetPath] = m_Session;
        }

        std::string finalizeError;
        if (!m_Session.MaterialAsset->FinalizeGraphAfterLoad(&finalizeError))
        {
            ME_LOG(LogEditor, Warn, "MaterialEditor: graph finalize failed: {}", finalizeError);
        }

        ScheduleDebouncedCompile();
    }

    void MaterialEditor::RefreshMaterialList()
    {
        const std::vector<const AssetMeta*> materials =
            AssetManager::Get().FindAssetMetasByType("Material");
        m_MaterialMetas.assign(materials.begin(), materials.end());
        std::sort(
            m_MaterialMetas.begin(),
            m_MaterialMetas.end(),
            [](const AssetMeta* lhs, const AssetMeta* rhs)
            {
                return lhs->AssetPath < rhs->AssetPath;
            });

        m_SelectedMaterialIndex = -1;
        if (m_Session.HasOpenMaterial())
        {
            for (size_t i = 0; i < m_MaterialMetas.size(); ++i)
            {
                if (m_MaterialMetas[i]->AssetPath == m_Session.AssetPath)
                {
                    m_SelectedMaterialIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    void MaterialEditor::OpenSession(const AssetMeta* meta)
    {
        FlushPendingCompile();
        StashActiveSession();

        if (!meta)
        {
            m_Session.Clear();
            m_SelectedMaterialIndex = -1;
            ClearSelectedEdNode();
            ApplySessionToPreview();
            InvalidateGraphCanvas();
            return;
        }

        if (ActivateStoredSession(meta->AssetPath))
        {
            for (size_t i = 0; i < m_MaterialMetas.size(); ++i)
            {
                if (m_MaterialMetas[i] == meta ||
                    (m_MaterialMetas[i] != nullptr && m_MaterialMetas[i]->AssetPath == meta->AssetPath))
                {
                    m_SelectedMaterialIndex = static_cast<int>(i);
                    break;
                }
            }
            return;
        }

        std::shared_ptr<Material> material = AssetManager::Get().LoadAsset<Material>(meta->AssetPath);
        if (!material)
        {
            ME_LOG(LogEditor, Error, "MaterialEditor: failed to load material '{}'.", meta->AssetPath);
            return;
        }

        std::string finalizeError;
        if (!material->FinalizeGraphAfterLoad(&finalizeError))
        {
            ME_LOG(LogEditor, Warn, "MaterialEditor: FinalizeGraphAfterLoad failed for '{}': {}", meta->AssetPath, finalizeError);
        }

        material->Compile();

        m_Session.MaterialAsset = material;
        m_Session.AssetPath = meta->AssetPath;
        m_Session.Dirty = false;
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
        ClearSelectedEdNode();

        for (size_t i = 0; i < m_MaterialMetas.size(); ++i)
        {
            if (m_MaterialMetas[i] == meta)
            {
                m_SelectedMaterialIndex = static_cast<int>(i);
                break;
            }
        }

        ApplySessionToPreview();
        InvalidateGraphCanvas();
    }

    void MaterialEditor::ApplySessionToPreview()
    {
        if (!m_PreviewScene.IsContentReady())
        {
            return;
        }

        if (m_Session.HasOpenMaterial())
        {
            m_PreviewScene.SetPreviewMaterial(m_Session.MaterialAsset);
        }
        else
        {
            m_PreviewScene.SetPreviewMaterial(nullptr);
        }
    }

    void MaterialEditor::EnsureDefaultSession()
    {
        if (m_Session.HasOpenMaterial())
        {
            return;
        }

        if (m_MaterialMetas.empty())
        {
            return;
        }

        OpenSession(m_MaterialMetas.front());
    }

    void MaterialEditor::CompileActiveMaterial()
    {
        if (!m_Session.HasOpenMaterial())
        {
            return;
        }

        m_CompilePending = false;
        m_CompileDebounceTimer = 0.0f;

        m_Session.MaterialAsset->Compile();
        ApplySessionToPreview();
    }

    bool MaterialEditor::SaveActiveMaterial()
    {
        if (!m_Session.HasOpenMaterial())
        {
            return false;
        }

        FlushPendingCompile();

        const bool saved = AssetManager::Get().SaveAsset<Material>(
            m_Session.AssetPath,
            *m_Session.MaterialAsset);
        if (saved)
        {
            m_Session.Dirty = false;
            if (!m_Session.AssetPath.empty())
            {
                m_SessionsByKey[m_Session.AssetPath] = m_Session;
            }
        }
        else
        {
            ME_LOG(LogEditor, Error, "MaterialEditor: Save failed for '{}'.", m_Session.AssetPath);
        }

        return saved;
    }

    void MaterialEditor::SetShadingModel(MaterialShadingModel model)
    {
        if (!m_Session.HasOpenMaterial())
        {
            return;
        }

        Material& material = *m_Session.MaterialAsset;
        if (material.m_ShadingModel == model)
        {
            return;
        }

        SubmitCapabilityProperty("m_ShadingModel", [&material, model]() { material.m_ShadingModel = model; });
    }

    void MaterialEditor::SetBlendMode(MaterialBlendMode blendMode)
    {
        if (!m_Session.HasOpenMaterial())
        {
            return;
        }

        Material& material = *m_Session.MaterialAsset;
        if (material.m_BlendMode == blendMode)
        {
            return;
        }

        SubmitCapabilityProperty("m_BlendMode", [&material, blendMode]() { material.m_BlendMode = blendMode; });
    }

    bool MaterialEditor::ApplySetObjectProperty(const GUID& ownerGuid,
                                                const std::string& ownerClassName,
                                                const std::string& propertyPath,
                                                const std::vector<uint8_t>& valueBlob)
    {
        if (!EditorObjectPropertyApply::ApplyBlob(
                ownerGuid,
                ownerClassName,
                propertyPath,
                valueBlob,
                EditorObjectPropertyApply::MakeDefaultOptions()))
        {
            return false;
        }

        RefreshGraphAfterMutation();
        return true;
    }

    void MaterialEditor::SubmitSetObjectProperty(IEditorContext& context,
                                                 const GUID& ownerGuid,
                                                 const std::string& ownerClassName,
                                                 const std::string& propertyPath,
                                                 std::vector<uint8_t> beforeValue,
                                                 std::vector<uint8_t> afterValue,
                                                 bool applyOnFirstExecute,
                                                 EditorSetObjectPropertySideEffects sideEffects)
    {
        if (beforeValue == afterValue)
        {
            return;
        }

        context.GetCommandStack().Execute(std::make_unique<EditorSetObjectPropertyCommand>(
            *this,
            ownerGuid,
            ownerClassName,
            propertyPath,
            std::move(beforeValue),
            std::move(afterValue),
            applyOnFirstExecute,
            std::move(sideEffects)));
    }

    void MaterialEditor::StorePropertyUndoBefore(const MEObject& owner, const std::string& propertyPath)
    {
        if (ImGui::GetItemID() == 0)
        {
            return;
        }

        const Reflection::MEClass* ownerClass = owner.GetClass();
        if (ownerClass == nullptr)
        {
            return;
        }

        std::vector<uint8_t> beforeValue;
        if (!SerializeOwnedProperty(owner, propertyPath, beforeValue))
        {
            return;
        }

        const uint32_t editId = static_cast<uint32_t>(ImGui::GetItemID());
        PendingPropertyUndo& pending = m_PropertyUndoBeforeByEditId[editId];
        pending.OwnerGuid = owner.GetGuid();
        pending.OwnerClassName = ownerClass->GetName();

        for (PendingPropertyUndoField& field : pending.Fields)
        {
            if (field.PropertyPath == propertyPath)
            {
                field.BeforeValue = std::move(beforeValue);
                return;
            }
        }

        PendingPropertyUndoField field;
        field.PropertyPath = propertyPath;
        field.BeforeValue = std::move(beforeValue);
        pending.Fields.push_back(std::move(field));
    }

    void MaterialEditor::TryCapturePropertyUndoActivated(const MEObject& owner, const std::string& propertyPath)
    {
        if (!ImGui::IsItemActivated())
        {
            return;
        }

        StorePropertyUndoBefore(owner, propertyPath);
    }

    void MaterialEditor::ClearPropertyUndoBefore(const MEObject& owner, const std::string& propertyPath)
    {
        if (ImGui::GetItemID() == 0)
        {
            return;
        }

        const uint32_t editId = static_cast<uint32_t>(ImGui::GetItemID());
        const auto pendingIter = m_PropertyUndoBeforeByEditId.find(editId);
        if (pendingIter == m_PropertyUndoBeforeByEditId.end())
        {
            return;
        }

        PendingPropertyUndo& pending = pendingIter->second;
        if (pending.OwnerGuid != owner.GetGuid())
        {
            return;
        }

        for (auto fieldIter = pending.Fields.begin(); fieldIter != pending.Fields.end(); ++fieldIter)
        {
            if (fieldIter->PropertyPath != propertyPath)
            {
                continue;
            }

            pending.Fields.erase(fieldIter);
            break;
        }

        if (pending.Fields.empty())
        {
            m_PropertyUndoBeforeByEditId.erase(pendingIter);
        }
    }

    void MaterialEditor::CommitStoredPropertyUndo(const MEObject& owner, const std::string& propertyPath)
    {
        if (ImGui::GetItemID() == 0 || m_Context == nullptr)
        {
            return;
        }

        const uint32_t editId = static_cast<uint32_t>(ImGui::GetItemID());
        const auto pendingIter = m_PropertyUndoBeforeByEditId.find(editId);
        if (pendingIter == m_PropertyUndoBeforeByEditId.end())
        {
            return;
        }

        PendingPropertyUndo& pending = pendingIter->second;
        if (pending.OwnerGuid != owner.GetGuid())
        {
            return;
        }

        for (auto fieldIter = pending.Fields.begin(); fieldIter != pending.Fields.end(); ++fieldIter)
        {
            if (fieldIter->PropertyPath != propertyPath)
            {
                continue;
            }

            std::vector<uint8_t> afterValue;
            if (SerializeOwnedProperty(owner, propertyPath, afterValue))
            {
                SubmitSetObjectProperty(
                    *m_Context,
                    pending.OwnerGuid,
                    pending.OwnerClassName,
                    propertyPath,
                    std::move(fieldIter->BeforeValue),
                    std::move(afterValue),
                    false);
            }

            pending.Fields.erase(fieldIter);
            break;
        }

        if (pending.Fields.empty())
        {
            m_PropertyUndoBeforeByEditId.erase(pendingIter);
        }
    }

    void MaterialEditor::TryCommitPropertyUndoAfterEdit(const MEObject& owner, const std::string& propertyPath)
    {
        if (!ImGui::IsItemDeactivatedAfterEdit())
        {
            return;
        }

        CommitStoredPropertyUndo(owner, propertyPath);
    }

    void MaterialEditor::TryCommitAllPropertyUndoAfterEdit(const MEObject& owner)
    {
        if (!ImGui::IsItemDeactivatedAfterEdit() || ImGui::GetItemID() == 0 || m_Context == nullptr)
        {
            return;
        }

        const uint32_t editId = static_cast<uint32_t>(ImGui::GetItemID());
        const auto pendingIter = m_PropertyUndoBeforeByEditId.find(editId);
        if (pendingIter == m_PropertyUndoBeforeByEditId.end())
        {
            return;
        }

        PendingPropertyUndo pending = std::move(pendingIter->second);
        m_PropertyUndoBeforeByEditId.erase(pendingIter);
        if (pending.OwnerGuid != owner.GetGuid())
        {
            return;
        }

        for (PendingPropertyUndoField& field : pending.Fields)
        {
            std::vector<uint8_t> afterValue;
            if (!SerializeOwnedProperty(owner, field.PropertyPath, afterValue))
            {
                continue;
            }

            SubmitSetObjectProperty(
                *m_Context,
                pending.OwnerGuid,
                pending.OwnerClassName,
                field.PropertyPath,
                std::move(field.BeforeValue),
                std::move(afterValue),
                false);
        }
    }

    void MaterialEditor::RefreshGraphAfterMutation()
    {
        MaterialGraphIds::Reset();
        InvalidateGraphCanvas(false);
        NotifyGraphChanged();
    }

    void MaterialEditor::CaptureMaterialOutputLinks(std::vector<MaterialOutputLinkRecord>& outLinks) const
    {
        outLinks.clear();
        if (!m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph)
        {
            return;
        }

        MaterialEdGraph& graph = *m_Session.MaterialAsset->m_Graph;
        for (const std::shared_ptr<MaterialEdGraphNode>& nodePtr : graph.m_Nodes)
        {
            if (!nodePtr)
            {
                continue;
            }

            MaterialGraphNodeDef* nodeDef = nodePtr->GetNodeDef();
            if (nodeDef == nullptr || !nodeDef->IsMaterialOutputNode())
            {
                continue;
            }

            for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = nodeDef->GetInput(inputIndex);
                 ++inputIndex)
            {
                if (input == nullptr || !input->IsConnected() || input->NodeDef == nullptr)
                {
                    continue;
                }

                MaterialOutputLinkRecord record;
                record.ToNodeDefHigh = nodeDef->GetGuid().High;
                record.ToNodeDefLow = nodeDef->GetGuid().Low;
                record.InputIndex = inputIndex;
                record.FromNodeDefHigh = input->NodeDef->GetGuid().High;
                record.FromNodeDefLow = input->NodeDef->GetGuid().Low;
                record.OutputIndex = input->OutputIndex;
                outLinks.push_back(record);
            }
        }
    }

    void MaterialEditor::RestoreMaterialOutputLinks(const std::vector<MaterialOutputLinkRecord>& links)
    {
        if (!m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph)
        {
            return;
        }

        Material& material = *m_Session.MaterialAsset;
        MaterialEdGraph& graph = *material.m_Graph;
        for (const MaterialOutputLinkRecord& record : links)
        {
            MaterialEdGraphNode* toNode = FindEdNodeByNodeDefGuid(GUID(record.ToNodeDefHigh, record.ToNodeDefLow));
            MaterialEdGraphNode* fromNode =
                FindEdNodeByNodeDefGuid(GUID(record.FromNodeDefHigh, record.FromNodeDefLow));
            if (toNode == nullptr || fromNode == nullptr)
            {
                continue;
            }

            graph.ConnectPins(
                *fromNode,
                record.OutputIndex,
                *toNode,
                record.InputIndex,
                material.m_ShadingModel,
                material.m_BlendMode);
        }
    }

    void MaterialEditor::SubmitCapabilityProperty(const std::string& propertyPath,
                                                  const std::function<void()>& assignAfterValue)
    {
        if (!m_Session.HasOpenMaterial() || m_Context == nullptr || !assignAfterValue)
        {
            return;
        }

        Material& material = *m_Session.MaterialAsset;
        const GUID ownerGuid = material.GetGuid();
        const Reflection::MEClass* ownerClass = material.GetClass();
        if (ownerClass == nullptr)
        {
            return;
        }

        const std::string ownerClassName = ownerClass->GetName();
        std::vector<uint8_t> beforeValue;
        if (!SerializeOwnedProperty(material, propertyPath, beforeValue))
        {
            return;
        }

        std::vector<MaterialOutputLinkRecord> outputLinks;
        CaptureMaterialOutputLinks(outputLinks);
        assignAfterValue();

        std::vector<uint8_t> afterValue;
        const bool serializedAfter = SerializeOwnedProperty(material, propertyPath, afterValue);
        EditorObjectPropertyApply::ApplyBlob(
            ownerGuid,
            ownerClassName,
            propertyPath,
            beforeValue,
            EditorObjectPropertyApply::MakeDefaultOptions());
        if (!serializedAfter)
        {
            return;
        }

        EditorSetObjectPropertySideEffects sideEffects;
        sideEffects.AfterExecute = [this]() {
            if (m_Session.HasOpenMaterial())
            {
                MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(*m_Session.MaterialAsset);
                RefreshGraphAfterMutation();
            }
        };
        sideEffects.AfterUndo = [this, outputLinks]() { RestoreMaterialOutputLinks(outputLinks); RefreshGraphAfterMutation(); };

        SubmitSetObjectProperty(
            *m_Context,
            ownerGuid,
            ownerClassName,
            propertyPath,
            std::move(beforeValue),
            std::move(afterValue),
            true,
            std::move(sideEffects));
    }

    MaterialEdGraphNode* MaterialEditor::FindEdNodeByNodeDefGuid(const GUID& nodeDefGuid) const
    {
        if (!m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph)
        {
            return nullptr;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& nodePtr : m_Session.MaterialAsset->m_Graph->m_Nodes)
        {
            if (!nodePtr || nodePtr->GetNodeDef() == nullptr)
            {
                continue;
            }

            if (nodePtr->GetNodeDef()->GetGuid() == nodeDefGuid)
            {
                return nodePtr.get();
            }
        }

        return nullptr;
    }

    bool MaterialEditor::SerializeOwnedProperty(const MEObject& owner,
                                                const std::string& propertyPath,
                                                std::vector<uint8_t>& outBlob) const
    {
        const Reflection::MEClass* ownerClass = owner.GetClass();
        if (ownerClass == nullptr)
        {
            return false;
        }

        return EditorObjectPropertyApply::SerializeBlob(
            owner.GetGuid(),
            ownerClass->GetName(),
            propertyPath,
            outBlob,
            EditorObjectPropertyApply::MakeDefaultOptions());
    }

    bool MaterialEditor::ApplyAddNode(const std::string& nodeDefClassName,
                                      float editorPosX,
                                      float editorPosY,
                                      GUID* outCreatedNodeDefGuid)
    {
        if (!m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph || nodeDefClassName.empty())
        {
            return false;
        }

        const Reflection::MEClass* nodeDefClass = Reflection::ReflectionSystem::Get().FindClass(nodeDefClassName);
        if (nodeDefClass == nullptr || !nodeDefClass->IsA(MaterialGraphNodeDef::StaticClass()))
        {
            return false;
        }

        MaterialEdGraphNode& newNode =
            m_Session.MaterialAsset->m_Graph->AddNode(nodeDefClass, editorPosX, editorPosY);
        MaterialGraphNodeDef* nodeDef = newNode.GetNodeDef();
        if (nodeDef == nullptr)
        {
            return false;
        }

        if (outCreatedNodeDefGuid != nullptr)
        {
            *outCreatedNodeDefGuid = nodeDef->GetGuid();
        }

        SetSelectedEdNode(&newNode);
        RefreshGraphAfterMutation();
        return true;
    }

    void MaterialEditor::SubmitAddNode(IEditorContext& context,
                                       const std::string& nodeDefClassName,
                                       float editorPosX,
                                       float editorPosY)
    {
        context.GetCommandStack().Execute(std::make_unique<EditorAddMaterialNodeCommand>(
            *this,
            nodeDefClassName,
            editorPosX,
            editorPosY));
    }

    bool MaterialEditor::ApplyRemoveNodeByGuid(const GUID& nodeDefGuid)
    {
        MaterialEdGraphNode* node = FindEdNodeByNodeDefGuid(nodeDefGuid);
        if (node == nullptr || !m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph)
        {
            return false;
        }

        MaterialGraphNodeDef* nodeDef = node->GetNodeDef();
        if (nodeDef != nullptr && nodeDef->IsMaterialOutputNode())
        {
            return false;
        }

        if (GetSelectedEdNode() == node)
        {
            ClearSelectedEdNode();
        }

        if (!m_Session.MaterialAsset->m_Graph->RemoveNode(*node))
        {
            return false;
        }

        RefreshGraphAfterMutation();
        return true;
    }

    bool MaterialEditor::TryCaptureRemoveNode(const GUID& nodeDefGuid,
                                              std::vector<uint8_t>& outSnapshot,
                                              std::vector<MaterialNodeInboundLink>& outInboundLinks) const
    {
        outSnapshot.clear();
        outInboundLinks.clear();

        MaterialEdGraphNode* node = FindEdNodeByNodeDefGuid(nodeDefGuid);
        if (node == nullptr || node->GetNodeDef() == nullptr || !m_Session.HasOpenMaterial()
            || !m_Session.MaterialAsset->m_Graph)
        {
            return false;
        }

        if (node->GetNodeDef()->IsMaterialOutputNode())
        {
            return false;
        }

        const Reflection::MEClass* nodeClass = node->GetClass();
        if (nodeClass == nullptr)
        {
            return false;
        }

        const Serialization::SerializeResult serializeResult =
            Serialization::Serializer::SerializeObjectToBuffer(nodeClass, node, outSnapshot);
        if (!serializeResult.ok)
        {
            return false;
        }

        MaterialGraphNodeDef* removedDef = node->GetNodeDef();
        for (const std::shared_ptr<MaterialEdGraphNode>& otherNodePtr : m_Session.MaterialAsset->m_Graph->m_Nodes)
        {
            if (!otherNodePtr || otherNodePtr.get() == node || otherNodePtr->GetNodeDef() == nullptr)
            {
                continue;
            }

            MaterialGraphNodeDef* otherDef = otherNodePtr->GetNodeDef();
            for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = otherDef->GetInput(inputIndex);
                 ++inputIndex)
            {
                if (input == nullptr || input->NodeDef != removedDef)
                {
                    continue;
                }

                MaterialNodeInboundLink link;
                link.ToNodeDefHigh = otherDef->GetGuid().High;
                link.ToNodeDefLow = otherDef->GetGuid().Low;
                link.ToInputIndex = inputIndex;
                link.FromOutputIndex = input->OutputIndex;
                outInboundLinks.push_back(link);
            }
        }

        return true;
    }

    bool MaterialEditor::ApplyRestoreNodeFromSnapshot(const std::vector<uint8_t>& snapshot,
                                                      const std::vector<MaterialNodeInboundLink>& inboundLinks)
    {
        if (!m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph || snapshot.empty())
        {
            return false;
        }

        const Reflection::MEClass* nodeClass = MaterialEdGraphNode::StaticClass();
        std::shared_ptr<void> instanceVoid = nodeClass->CreateDefaultInstance();
        if (!instanceVoid)
        {
            return false;
        }

        std::shared_ptr<MaterialEdGraphNode> edNode = std::static_pointer_cast<MaterialEdGraphNode>(instanceVoid);
        edNode->SetOuter(m_Session.MaterialAsset->m_Graph.get());

        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        const Serialization::SerializeResult deserializeResult =
            Serialization::Serializer::DeserializeObjectFromBuffer(
                nodeClass,
                edNode.get(),
                snapshot,
                unresolvedRefs,
                EditorObjectPropertyApply::MakeDefaultOptions());
        if (!deserializeResult.ok)
        {
            return false;
        }

        ObjectManager::Get().RegisterObject(std::static_pointer_cast<MEObject>(edNode));
        if (!unresolvedRefs.empty())
        {
            Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
        }

        if (edNode->GetNodeDef() != nullptr)
        {
            ObjectManager::Get().RegisterObject(std::static_pointer_cast<MEObject>(edNode->m_NodeDef));
        }

        m_Session.MaterialAsset->m_Graph->m_Nodes.push_back(edNode);
        m_Session.MaterialAsset->FinalizeGraphAfterLoad(nullptr);

        Material& material = *m_Session.MaterialAsset;
        for (const MaterialNodeInboundLink& link : inboundLinks)
        {
            MaterialEdGraphNode* toNode = FindEdNodeByNodeDefGuid(GUID(link.ToNodeDefHigh, link.ToNodeDefLow));
            if (toNode == nullptr || edNode->GetNodeDef() == nullptr)
            {
                continue;
            }

            material.m_Graph->ConnectPins(
                *edNode,
                link.FromOutputIndex,
                *toNode,
                link.ToInputIndex,
                material.m_ShadingModel,
                material.m_BlendMode);
        }

        SetSelectedEdNode(edNode.get());
        RefreshGraphAfterMutation();
        return true;
    }

    void MaterialEditor::SubmitRemoveNode(IEditorContext& context, const GUID& nodeDefGuid)
    {
        std::vector<uint8_t> snapshot;
        std::vector<MaterialNodeInboundLink> inboundLinks;
        if (!TryCaptureRemoveNode(nodeDefGuid, snapshot, inboundLinks))
        {
            return;
        }

        context.GetCommandStack().Execute(std::make_unique<EditorRemoveMaterialNodeCommand>(
            *this,
            nodeDefGuid.High,
            nodeDefGuid.Low,
            std::move(snapshot),
            std::move(inboundLinks)));
    }

    bool MaterialEditor::ApplyConnectPins(const GUID& fromNodeDefGuid,
                                          int32_t fromOutputIndex,
                                          const GUID& toNodeDefGuid,
                                          int32_t toInputIndex)
    {
        if (!m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph)
        {
            return false;
        }

        MaterialEdGraphNode* fromNode = FindEdNodeByNodeDefGuid(fromNodeDefGuid);
        MaterialEdGraphNode* toNode = FindEdNodeByNodeDefGuid(toNodeDefGuid);
        if (fromNode == nullptr || toNode == nullptr)
        {
            return false;
        }

        Material& material = *m_Session.MaterialAsset;
        if (!material.m_Graph->ConnectPins(
                *fromNode,
                fromOutputIndex,
                *toNode,
                toInputIndex,
                material.m_ShadingModel,
                material.m_BlendMode))
        {
            return false;
        }

        RefreshGraphAfterMutation();
        return true;
    }

    void MaterialEditor::SubmitConnectPins(IEditorContext& context,
                                           const GUID& fromNodeDefGuid,
                                           int32_t fromOutputIndex,
                                           const GUID& toNodeDefGuid,
                                           int32_t toInputIndex)
    {
        MaterialEdGraphNode* toNode = FindEdNodeByNodeDefGuid(toNodeDefGuid);
        if (toNode == nullptr || toNode->GetNodeDef() == nullptr)
        {
            return;
        }

        MaterialGraphNodeDefInput* input = toNode->GetNodeDef()->GetInput(toInputIndex);
        bool hadPrevious = false;
        GUID previousFromGuid;
        int32_t previousFromOutput = 0;
        if (input != nullptr && input->IsConnected() && input->NodeDef != nullptr)
        {
            hadPrevious = true;
            previousFromGuid = input->NodeDef->GetGuid();
            previousFromOutput = input->OutputIndex;
        }

        context.GetCommandStack().Execute(std::make_unique<EditorConnectMaterialPinsCommand>(
            *this,
            fromNodeDefGuid.High,
            fromNodeDefGuid.Low,
            fromOutputIndex,
            toNodeDefGuid.High,
            toNodeDefGuid.Low,
            toInputIndex,
            previousFromGuid.High,
            previousFromGuid.Low,
            previousFromOutput,
            hadPrevious));
    }

    bool MaterialEditor::ApplyDisconnectInput(const GUID& toNodeDefGuid, int32_t toInputIndex)
    {
        MaterialEdGraphNode* toNode = FindEdNodeByNodeDefGuid(toNodeDefGuid);
        if (toNode == nullptr || !m_Session.HasOpenMaterial() || !m_Session.MaterialAsset->m_Graph)
        {
            return false;
        }

        m_Session.MaterialAsset->m_Graph->DisconnectInput(*toNode, toInputIndex);
        RefreshGraphAfterMutation();
        return true;
    }

    void MaterialEditor::SubmitDisconnectInput(IEditorContext& context,
                                               const GUID& toNodeDefGuid,
                                               int32_t toInputIndex,
                                               const GUID& fromNodeDefGuid,
                                               int32_t fromOutputIndex)
    {
        context.GetCommandStack().Execute(std::make_unique<EditorDisconnectMaterialPinCommand>(
            *this,
            toNodeDefGuid.High,
            toNodeDefGuid.Low,
            toInputIndex,
            fromNodeDefGuid.High,
            fromNodeDefGuid.Low,
            fromOutputIndex));
    }
}
