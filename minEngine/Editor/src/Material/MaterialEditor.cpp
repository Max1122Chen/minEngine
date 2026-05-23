#include "MaterialEditor.h"

#include "Editor.h"
#include "Material/MaterialGraphIds.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>

namespace minEngine
{
    MaterialEditor::MaterialEditor(Editor& editor)
        : m_Editor(editor)
    {
    }

    void MaterialEditor::OnEnterMode()
    {
        RefreshMaterialList();

        if (RHI* rhi = RenderSystem::Get().GetRHI())
        {
            m_Preview.EnsureInitialized(rhi, 512, 512);
        }

        m_Preview.EnsureSceneBuilt();
        EnsureDefaultSession();
        ApplySessionToPreview();
    }

    void MaterialEditor::OnExitMode()
    {
        FlushPendingCompile();
        ClearSelectedEdNode();
        m_Editor.RemoveViewportClient(kPreviewViewportPanelId);
    }

    void MaterialEditor::Shutdown()
    {
        m_Preview.Shutdown();
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
        m_Editor.GetOrCreateMaterialPreviewViewportClient(kPreviewViewportPanelId, "Material Preview");

        if (RHI* rhi = RenderSystem::Get().GetRHI())
        {
            m_Preview.EnsureInitialized(rhi, 512, 512);
        }

        // Scene content is built in OnEnterMode (after asset registry is ready).
        if (m_Preview.IsSceneReady())
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

        std::string finalizeError;
        if (!m_Session.MaterialAsset->FinalizeGraphAfterLoad(&finalizeError))
        {
            ME_CORE_WARN("MaterialEditor: graph finalize failed: {}", finalizeError);
        }

        ScheduleDebouncedCompile();
    }

    void MaterialEditor::RefreshMaterialList()
    {
        const std::string materialClassName = Reflection::GetClassName<Material>();
        const std::vector<AssetMeta*> materials = AssetManager::Get().FindAssetMetasByType(materialClassName);
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

        if (!meta)
        {
            m_Session.Clear();
            m_SelectedMaterialIndex = -1;
            ClearSelectedEdNode();
            ApplySessionToPreview();
            InvalidateGraphCanvas();
            return;
        }

        std::shared_ptr<Material> material = AssetManager::Get().LoadAsset<Material>(meta->AssetPath);
        if (!material)
        {
            ME_CORE_ERROR("MaterialEditor: failed to load material '{}'.", meta->AssetPath);
            return;
        }

        std::string finalizeError;
        if (!material->FinalizeGraphAfterLoad(&finalizeError))
        {
            ME_CORE_WARN("MaterialEditor: FinalizeGraphAfterLoad failed for '{}': {}", meta->AssetPath, finalizeError);
        }

        material->Compile();

        m_Session.MaterialAsset = material;
        m_Session.AssetPath = meta->AssetPath;
        m_Session.Dirty = false;
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
        if (!m_Preview.IsSceneReady())
        {
            return;
        }

        if (m_Session.HasOpenMaterial())
        {
            m_Preview.SetMaterial(m_Session.MaterialAsset);
        }
        else
        {
            m_Preview.SetMaterial(nullptr);
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
        }
        else
        {
            ME_CORE_ERROR("MaterialEditor: Save failed for '{}'.", m_Session.AssetPath);
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
