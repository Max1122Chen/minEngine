#include "Services/Thumbnail/AssetThumbnailService.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    void AssetThumbnailService::ClearInspectionTarget()
    {
        SetInspectionTarget(nullptr);
    }

    void AssetThumbnailService::SetInspectionTarget(const AssetMeta* meta)
    {
        if (meta == nullptr)
        {
            if (m_TargetAssetPath.empty())
            {
                return;
            }

            m_TargetAssetPath.clear();
            m_MaterialAsset.reset();
            m_TextureAsset.reset();
            m_BackendKind = ThumbnailBackendKind::None;
            m_bDirty = true;
            m_CachedView = ThumbnailView{};

            if (m_ViewportInitialized)
            {
                m_InspectorOffscreen.SetScene(nullptr);
            }

            m_InspectorPreviewScene.Shutdown();
            ShutdownSceneViewport();
            return;
        }

        if (meta->AssetPath == m_TargetAssetPath)
        {
            return;
        }

        m_TargetAssetPath = meta->AssetPath;
        m_MaterialAsset.reset();
        m_TextureAsset.reset();
        m_BackendKind = ResolveBackendKind(*meta);
        m_bDirty = true;
        m_CachedView = ThumbnailView{};

        if (m_BackendKind == ThumbnailBackendKind::Scene3D)
        {
            RebuildScene3DTarget(*meta);
            return;
        }

        if (m_BackendKind == ThumbnailBackendKind::Texture2DDirect)
        {
            RebuildTexture2DTarget(*meta);
            return;
        }

        if (m_ViewportInitialized)
        {
            m_InspectorOffscreen.SetScene(nullptr);
        }

        m_InspectorPreviewScene.Shutdown();
        ShutdownSceneViewport();
    }

    ThumbnailBackendKind AssetThumbnailService::ResolveBackendKind(const AssetMeta& meta) const
    {
        if (meta.AssetType == "Texture2D")
        {
            return ThumbnailBackendKind::Texture2DDirect;
        }

        // Scene3D thumbnails safe when RDG physical caches are keyed by ViewId+SRT.
        if (meta.AssetType == "Material" || meta.AssetType == "StaticMesh")
        {
            return ThumbnailBackendKind::Scene3D;
        }

        return ThumbnailBackendKind::None;
    }

    bool AssetThumbnailService::HasPreviewContent() const
    {
        if (m_BackendKind == ThumbnailBackendKind::Texture2DDirect)
        {
            return true;
        }

        return m_BackendKind == ThumbnailBackendKind::Scene3D && m_InspectorPreviewScene.IsContentReady();
    }

    void AssetThumbnailService::RebuildTexture2DTarget(const AssetMeta& meta)
    {
        if (m_ViewportInitialized)
        {
            m_InspectorOffscreen.SetScene(nullptr);
        }

        m_InspectorPreviewScene.Shutdown();
        ShutdownSceneViewport();

        m_TextureAsset = ([&]() -> std::shared_ptr<Texture2D> { std::string _loadAssetError; return std::dynamic_pointer_cast<Texture2D>(AssetManager::Get().LoadAssetByPath(meta.AssetPath, _loadAssetError)); })();
        if (!m_TextureAsset)
        {
            ME_LOG(LogEditor, Warn, "AssetThumbnailService: failed to load texture '{}'.", meta.AssetPath);
        }
    }

    void AssetThumbnailService::RebuildScene3DTarget(const AssetMeta& meta)
    {
        if (!m_InspectorPreviewScene.IsContentReady())
        {
            m_InspectorPreviewScene.BuildDefaultSphereScene();
        }

        if (!m_InspectorPreviewScene.IsContentReady())
        {
            m_BackendKind = ThumbnailBackendKind::None;
            return;
        }

        if (meta.AssetType == "Material")
        {
            m_MaterialAsset = ([&]() -> std::shared_ptr<Material> { std::string _loadAssetError; return std::dynamic_pointer_cast<Material>(AssetManager::Get().LoadAssetByPath(meta.AssetPath, _loadAssetError)); })();
            if (!m_MaterialAsset)
            {
                ME_LOG(LogEditor, Warn, "AssetThumbnailService: failed to load material '{}'.", meta.AssetPath);
                m_BackendKind = ThumbnailBackendKind::None;
                return;
            }

            m_InspectorPreviewScene.ResetPreviewMeshToDefaultSphere();
            m_InspectorPreviewScene.SetPreviewMaterial(m_MaterialAsset);
            return;
        }

        if (meta.AssetType == "StaticMesh")
        {
            std::shared_ptr<StaticMesh> mesh = ([&]() -> std::shared_ptr<StaticMesh> { std::string _loadAssetError; return std::dynamic_pointer_cast<StaticMesh>(AssetManager::Get().LoadAssetByPath(meta.AssetPath, _loadAssetError)); })();
            if (!mesh)
            {
                ME_LOG(LogEditor, Warn, 
                    "AssetThumbnailService: failed to load static mesh '{}'.",
                    meta.AssetPath);
                m_BackendKind = ThumbnailBackendKind::None;
                return;
            }

            if (!m_InspectorPreviewScene.EnsureStaticMeshPreviewMaterial())
            {
                ME_LOG(LogEditor, Warn, 
                    "AssetThumbnailService: static mesh preview material is not ready ('{}').",
                    PreviewScene::kStaticMeshPreviewMaterialPath);
                m_BackendKind = ThumbnailBackendKind::None;
                return;
            }

            m_InspectorPreviewScene.SetPreviewMesh(mesh);
            m_InspectorPreviewScene.SetPreviewMaterial(m_InspectorPreviewScene.GetStaticMeshPreviewMaterial());
        }
    }

    void AssetThumbnailService::EnsureSceneViewport()
    {
        if (!m_ViewportInitialized)
        {
            m_InspectorOffscreen.Initialize(kScene3DRenderSize, kScene3DRenderSize);
            m_InspectorOffscreen.SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            m_InspectorOffscreen.SetFlags(SceneDrawFlags::None);
            m_ViewportInitialized = true;
            SetupDefaultPreviewCamera();
            return;
        }

        if (m_InspectorOffscreen.GetWidth() == kScene3DRenderSize
            && m_InspectorOffscreen.GetHeight() == kScene3DRenderSize)
        {
            return;
        }

        // Size mismatch: rebuild at the fixed thumbnail size.
        m_InspectorOffscreen.Shutdown();
        m_InspectorOffscreen.Initialize(kScene3DRenderSize, kScene3DRenderSize);
        m_InspectorOffscreen.SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        m_InspectorOffscreen.SetFlags(SceneDrawFlags::None);
        m_ViewportInitialized = true;
        SetupDefaultPreviewCamera();
    }

    void AssetThumbnailService::ShutdownSceneViewport()
    {
        if (!m_ViewportInitialized)
        {
            return;
        }

        m_InspectorOffscreen.Shutdown();
        m_ViewportInitialized = false;
        m_ViewportWidth = 0;
        m_ViewportHeight = 0;
    }

    void AssetThumbnailService::SetupDefaultPreviewCamera()
    {
        RenderCamera* camera = m_InspectorOffscreen.EnsureInternalCamera();
        if (!camera)
        {
            return;
        }

        const Vector3 eye(1.15f, 0.8f, 1.15f);
        const Vector3 target(0.0f, 0.0f, 0.0f);
        const Vector3 up(0.0f, 1.0f, 0.0f);

        camera->m_Position = eye;
        camera->m_zNear = 0.1f;
        camera->m_zFar = 200.0f;
        camera->m_FOV = 45.0f;
        camera->m_ViewMatrix = glm::lookAt(eye, target, up);
        camera->UpdateProjectionMatrix();
        camera->UpdateViewProjMatrix();
    }

    void AssetThumbnailService::SyncPreviewCameraAspect()
    {
        RenderCamera* camera = m_InspectorOffscreen.EnsureInternalCamera();
        if (!camera)
        {
            return;
        }

        const Vector2 bufferSize(static_cast<float>(m_InspectorOffscreen.GetWidth()), static_cast<float>(m_InspectorOffscreen.GetHeight()));
        if (bufferSize.x > 0.0f && bufferSize.y > 0.0f)
        {
            camera->m_AspectRatio = bufferSize.x / bufferSize.y;
            camera->UpdateProjectionMatrix();
            camera->UpdateViewProjMatrix();
        }
    }

    ThumbnailView AssetThumbnailService::BuildScene3DThumbnailIfDirty()
    {
        if (m_BackendKind != ThumbnailBackendKind::Scene3D || !m_InspectorPreviewScene.IsContentReady())
        {
            return ThumbnailView{};
        }

        if (!m_bDirty && m_CachedView.State == ThumbnailState::Ready && m_CachedView.TextureId != 0)
        {
            return m_CachedView;
        }

        EnsureSceneViewport();
        if (!m_ViewportInitialized)
        {
            return ThumbnailView{};
        }

        m_InspectorOffscreen.SetScene(m_InspectorPreviewScene.GetRenderScene());
        SyncPreviewCameraAspect();
        m_InspectorPreviewScene.RefreshRenderScene();

        const RHITextureRef& displayTexture = m_InspectorOffscreen.RenderOnce();
        const RHITextureCreateDesc& displayDesc = displayTexture ? displayTexture->GetDesc() : RHITextureCreateDesc{};
        if (!displayTexture || displayDesc.Width == 0 || displayDesc.Height == 0)
        {
            return ThumbnailView{};
        }

        m_CachedView.State = ThumbnailState::Ready;
        m_CachedView.BackendKind = ThumbnailBackendKind::Scene3D;
        m_CachedView.Width = displayDesc.Width;
        m_CachedView.Height = displayDesc.Height;
        m_CachedView.TextureId = static_cast<ImTextureID>(GetRHINativeTextureHandle(displayTexture.get()));

        m_bDirty = false;
        return m_CachedView;
    }

    ThumbnailView AssetThumbnailService::BuildTexture2DThumbnail()
    {
        ThumbnailView view{};
        view.BackendKind = ThumbnailBackendKind::Texture2DDirect;

        const Texture2D* textureAsset = m_TextureAsset.get();
        if (!textureAsset)
        {
            return view;
        }

        const RHITexture* rhiTexture = textureAsset->GetRHITexture();
        if (!rhiTexture || GetRHINativeTextureHandle(rhiTexture) == 0)
        {
            return view;
        }

        uint32_t textureWidth = rhiTexture->GetDesc().Width;
        uint32_t textureHeight = rhiTexture->GetDesc().Height;
        if (textureWidth == 0 || textureHeight == 0)
        {
            textureWidth = textureAsset->GetWidth();
            textureHeight = textureAsset->GetHeight();
        }

        if (textureWidth == 0 || textureHeight == 0)
        {
            return view;
        }

        view.State = ThumbnailState::Ready;
        view.Width = textureWidth;
        view.Height = textureHeight;
        view.TextureId = static_cast<ImTextureID>(GetRHINativeTextureHandle(rhiTexture));
        return view;
    }

    ThumbnailView AssetThumbnailService::BuildTexture2DThumbnailFromAssetPath(std::string_view assetPath)
    {
        ThumbnailView view{};
        view.BackendKind = ThumbnailBackendKind::Texture2DDirect;

        if (assetPath.empty())
        {
            return view;
        }

        std::shared_ptr<Texture2D> textureAsset;
        auto it = m_TextureThumbnailCache.find(std::string(assetPath));
        if (it != m_TextureThumbnailCache.end())
        {
            textureAsset = it->second;
        }
        else
        {
            textureAsset = ([&]() -> std::shared_ptr<Texture2D> { std::string _loadAssetError; return std::dynamic_pointer_cast<Texture2D>(AssetManager::Get().LoadAssetByPath(std::string(assetPath), _loadAssetError)); })();
            m_TextureThumbnailCache.emplace(std::string(assetPath), textureAsset);
        }

        if (!textureAsset)
        {
            return view;
        }

        const RHITexture* rhiTexture = textureAsset->GetRHITexture();
        if (!rhiTexture || GetRHINativeTextureHandle(rhiTexture) == 0)
        {
            return view;
        }

        uint32_t textureWidth = rhiTexture->GetDesc().Width;
        uint32_t textureHeight = rhiTexture->GetDesc().Height;
        if (textureWidth == 0 || textureHeight == 0)
        {
            textureWidth = textureAsset->GetWidth();
            textureHeight = textureAsset->GetHeight();
        }

        if (textureWidth == 0 || textureHeight == 0)
        {
            return view;
        }

        view.State = ThumbnailState::Ready;
        view.Width = textureWidth;
        view.Height = textureHeight;
        view.TextureId = static_cast<ImTextureID>(GetRHINativeTextureHandle(rhiTexture));
        return view;
    }

    ThumbnailView AssetThumbnailService::RequestThumbnail()
    {
        if (m_BackendKind == ThumbnailBackendKind::Scene3D)
        {
            return BuildScene3DThumbnailIfDirty();
        }

        if (m_BackendKind == ThumbnailBackendKind::Texture2DDirect)
        {
            return BuildTexture2DThumbnail();
        }

        return ThumbnailView{};
    }

    ThumbnailView AssetThumbnailService::RequestThumbnailForAsset(const AssetMeta& meta)
    {
        const ThumbnailBackendKind kind = ResolveBackendKind(meta);
        if (kind == ThumbnailBackendKind::Texture2DDirect)
        {
            return BuildTexture2DThumbnailFromAssetPath(meta.AssetPath);
        }

        if (kind == ThumbnailBackendKind::Scene3D)
        {
            return BuildScene3DThumbnailForAsset(meta);
        }

        return ThumbnailView{};
    }

    void AssetThumbnailService::ResetPerFrameBudgetIfNeeded()
    {
        const int frame = ImGui::GetFrameCount();
        if (frame != m_LastBudgetFrame)
        {
            m_LastBudgetFrame = frame;
            m_BuiltScene3DThisFrame = 0;
        }
    }

    void AssetThumbnailService::EnsureEntryViewport(Scene3DThumbnailEntry& entry)
    {
        if (!entry.bViewportInitialized)
        {
            entry.Offscreen.Initialize(kScene3DRenderSize, kScene3DRenderSize);
            entry.Offscreen.SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            entry.Offscreen.SetFlags(SceneDrawFlags::None);
            entry.bViewportInitialized = true;
            entry.ViewportWidth = kScene3DRenderSize;
            entry.ViewportHeight = kScene3DRenderSize;

            RenderCamera* camera = entry.Offscreen.EnsureInternalCamera();
            if (camera)
            {
                const Vector3 eye(1.15f, 0.8f, 1.15f);
                const Vector3 target(0.0f, 0.0f, 0.0f);
                const Vector3 up(0.0f, 1.0f, 0.0f);
                camera->m_Position = eye;
                camera->m_zNear = 0.1f;
                camera->m_zFar = 200.0f;
                camera->m_FOV = 45.0f;
                camera->m_ViewMatrix = glm::lookAt(eye, target, up);
                camera->UpdateProjectionMatrix();
                camera->UpdateViewProjMatrix();
            }

            return;
        }
    }

    ThumbnailView AssetThumbnailService::BuildScene3DThumbnailForEntry(const AssetMeta& meta, Scene3DThumbnailEntry& entry)
    {
        if (!m_TilePreviewScene.IsContentReady())
        {
            m_TilePreviewScene.BuildDefaultSphereScene();
        }

        if (!m_TilePreviewScene.IsContentReady())
        {
            return ThumbnailView{};
        }

        if (meta.AssetType == "Material")
        {
            entry.MaterialAsset = ([&]() -> std::shared_ptr<Material> { std::string _loadAssetError; return std::dynamic_pointer_cast<Material>(AssetManager::Get().LoadAssetByPath(meta.AssetPath, _loadAssetError)); })();
            if (!entry.MaterialAsset)
            {
                return ThumbnailView{};
            }

            m_TilePreviewScene.ResetPreviewMeshToDefaultSphere();
            m_TilePreviewScene.SetPreviewMaterial(entry.MaterialAsset);
        }
        else if (meta.AssetType == "StaticMesh")
        {
            entry.MeshAsset = ([&]() -> std::shared_ptr<StaticMesh> { std::string _loadAssetError; return std::dynamic_pointer_cast<StaticMesh>(AssetManager::Get().LoadAssetByPath(meta.AssetPath, _loadAssetError)); })();
            if (!entry.MeshAsset)
            {
                return ThumbnailView{};
            }

            if (!m_TilePreviewScene.EnsureStaticMeshPreviewMaterial())
            {
                return ThumbnailView{};
            }

            m_TilePreviewScene.SetPreviewMesh(entry.MeshAsset);
            m_TilePreviewScene.SetPreviewMaterial(m_TilePreviewScene.GetStaticMeshPreviewMaterial());
        }
        else
        {
            return ThumbnailView{};
        }

        EnsureEntryViewport(entry);
        if (!entry.bViewportInitialized)
        {
            return ThumbnailView{};
        }

        entry.Offscreen.SetScene(m_TilePreviewScene.GetRenderScene());

        RenderCamera* camera = entry.Offscreen.EnsureInternalCamera();
        if (camera)
        {
            const float bufferWidth = static_cast<float>(entry.Offscreen.GetWidth());
            const float bufferHeight = static_cast<float>(entry.Offscreen.GetHeight());
            if (bufferWidth > 0.0f && bufferHeight > 0.0f)
            {
                camera->m_AspectRatio = bufferWidth / bufferHeight;
                camera->UpdateProjectionMatrix();
                camera->UpdateViewProjMatrix();
            }
        }

        m_TilePreviewScene.RefreshRenderScene();

        const RHITextureRef& displayTexture = entry.Offscreen.RenderOnce();
        const RHITextureCreateDesc& displayDesc = displayTexture ? displayTexture->GetDesc() : RHITextureCreateDesc{};
        if (!displayTexture || displayDesc.Width == 0 || displayDesc.Height == 0)
        {
            return ThumbnailView{};
        }

        entry.CachedView.State = ThumbnailState::Ready;
        entry.CachedView.BackendKind = ThumbnailBackendKind::Scene3D;
        entry.CachedView.Width = displayDesc.Width;
        entry.CachedView.Height = displayDesc.Height;
        entry.CachedView.TextureId = static_cast<ImTextureID>(GetRHINativeTextureHandle(displayTexture.get()));

        entry.bDirty = false;
        return entry.CachedView;
    }

    void AssetThumbnailService::EvictScene3DCacheIfNeeded()
    {
        if (m_Scene3DThumbnailCache.size() <= kMaxScene3DCacheEntries)
        {
            return;
        }

        // Evict least-recently-accessed entries until under cap.
        while (m_Scene3DThumbnailCache.size() > kMaxScene3DCacheEntries)
        {
            auto victimIt = m_Scene3DThumbnailCache.begin();
            for (auto it = m_Scene3DThumbnailCache.begin(); it != m_Scene3DThumbnailCache.end(); ++it)
            {
                if (it->second.LastAccessFrame < victimIt->second.LastAccessFrame)
                {
                    victimIt = it;
                }
            }
            m_Scene3DThumbnailCache.erase(victimIt);
        }
    }

    ThumbnailView AssetThumbnailService::BuildScene3DThumbnailForAsset(const AssetMeta& meta)
    {
        ResetPerFrameBudgetIfNeeded();

        const int frame = ImGui::GetFrameCount();
        Scene3DThumbnailEntry& entry = m_Scene3DThumbnailCache[meta.AssetPath];
        entry.LastAccessFrame = frame;

        if (!entry.bDirty && entry.CachedView.State == ThumbnailState::Ready && entry.CachedView.TextureId != 0)
        {
            EvictScene3DCacheIfNeeded();
            return entry.CachedView;
        }

        if (m_BuiltScene3DThisFrame >= kMaxScene3DThumbnailsPerFrame)
        {
            ThumbnailView pending{};
            pending.State = ThumbnailState::Pending;
            pending.BackendKind = ThumbnailBackendKind::Scene3D;
            EvictScene3DCacheIfNeeded();
            return pending;
        }

        ++m_BuiltScene3DThisFrame;
        ThumbnailView built = BuildScene3DThumbnailForEntry(meta, entry);
        EvictScene3DCacheIfNeeded();
        return built;
    }

    void AssetThumbnailService::Shutdown()
    {
        m_TargetAssetPath.clear();
        m_MaterialAsset.reset();
        m_TextureAsset.reset();
        m_BackendKind = ThumbnailBackendKind::None;
        m_bDirty = true;
        m_CachedView = ThumbnailView{};
        m_TextureThumbnailCache.clear();

        m_InspectorPreviewScene.Shutdown();
        ShutdownSceneViewport();

        m_TilePreviewScene.Shutdown();
        for (auto& [path, entry] : m_Scene3DThumbnailCache)
        {
            if (entry.bViewportInitialized)
            {
                entry.Offscreen.Shutdown();
            }
        }
        m_Scene3DThumbnailCache.clear();
    }
}

