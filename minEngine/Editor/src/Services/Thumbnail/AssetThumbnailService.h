#pragma once

#include "Core.h"
#include "Preview/PreviewScene.h"
#include "Runtime/Function/Render/SceneViewport.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "imgui.h"

namespace minEngine
{
    class AssetMeta;
    class Material;
    class Texture2D;
    class RHITexture;

    enum class ThumbnailBackendKind : uint8_t
    {
        None = 0,
        Texture2DDirect,
        Scene3D
    };

    enum class ThumbnailState : uint8_t
    {
        Empty = 0,
        Pending,
        Ready,
        Failed
    };

    struct ThumbnailView
    {
        ThumbnailState State = ThumbnailState::Empty;
        ImTextureID TextureId = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        ThumbnailBackendKind BackendKind = ThumbnailBackendKind::None;
    };

    /**
     * Asset thumbnail service (Inspector + Content Browser tiles).
     * - Texture2D: direct RHI texture id (no scene submit).
     * - Material / StaticMesh Scene3D previews: disabled (TD-027); CB/Inspector fall back to icons.
     */
    class AssetThumbnailService
    {
    public:
        void SetInspectionTarget(const AssetMeta* meta);
        void ClearInspectionTarget();

        bool HasPreviewContent() const;
        ThumbnailBackendKind GetBackendKind() const { return m_BackendKind; }
        const Texture2D* GetTexture2DPreviewAsset() const { return m_TextureAsset.get(); }

        ThumbnailView RequestThumbnail();
        ThumbnailView RequestThumbnailForAsset(const AssetMeta& meta);

        void Shutdown();

    private:
        struct Scene3DThumbnailEntry
        {
            bool bDirty = true;
            bool bViewportInitialized = false;
            uint32_t ViewportWidth = 0;
            uint32_t ViewportHeight = 0;
            int LastAccessFrame = 0;

            SceneViewport Viewport;
            ThumbnailView CachedView{};

            // Keep assets alive for this thumbnail.
            std::shared_ptr<Material> MaterialAsset;
            std::shared_ptr<StaticMesh> MeshAsset;
        };

        ThumbnailBackendKind ResolveBackendKind(const AssetMeta& meta) const;
        void RebuildTexture2DTarget(const AssetMeta& meta);
        void RebuildScene3DTarget(const AssetMeta& meta);

        void EnsureSceneViewport();
        void ShutdownSceneViewport();
        void SetupDefaultPreviewCamera();
        void SyncPreviewCameraAspect();

        ThumbnailView BuildScene3DThumbnailIfDirty();
        ThumbnailView BuildTexture2DThumbnail();
        ThumbnailView BuildTexture2DThumbnailFromAssetPath(std::string_view assetPath);
        ThumbnailView BuildScene3DThumbnailForAsset(const AssetMeta& meta);
        ThumbnailView BuildScene3DThumbnailForEntry(const AssetMeta& meta, Scene3DThumbnailEntry& entry);
        void EnsureEntryViewport(Scene3DThumbnailEntry& entry);
        void ResetPerFrameBudgetIfNeeded();
        void EvictScene3DCacheIfNeeded();

    private:
        static constexpr uint32_t kScene3DRenderSize = 256;
        static constexpr int kMaxScene3DThumbnailsPerFrame = 1;
        static constexpr size_t kMaxScene3DCacheEntries = 64;

        ThumbnailBackendKind m_BackendKind = ThumbnailBackendKind::None;
        bool m_bDirty = true;

        // Inspector target preview state (single active target).
        PreviewScene m_InspectorPreviewScene;
        SceneViewport m_InspectorViewport;
        bool m_ViewportInitialized = false;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;

        std::shared_ptr<Material> m_MaterialAsset;
        std::shared_ptr<Texture2D> m_TextureAsset;
        std::string m_TargetAssetPath;

        ThumbnailView m_CachedView{};

        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_TextureThumbnailCache;

        // Content Browser Scene3D thumbnail cache.
        PreviewScene m_TilePreviewScene;
        std::unordered_map<std::string, Scene3DThumbnailEntry> m_Scene3DThumbnailCache;

        int m_LastBudgetFrame = -1;
        int m_BuiltScene3DThisFrame = 0;
    };
}

