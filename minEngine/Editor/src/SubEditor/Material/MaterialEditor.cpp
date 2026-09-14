#include "MaterialEditor.h"

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

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
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

        material.m_ShadingModel = model;
        MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(material);
        m_Session.Dirty = true;
        MaterialGraphIds::Reset();
        InvalidateGraphCanvas(false);
        NotifyGraphChanged();
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

        material.m_BlendMode = blendMode;
        MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(material);
        m_Session.Dirty = true;
        MaterialGraphIds::Reset();
        InvalidateGraphCanvas(false);
        NotifyGraphChanged();
    }
}
