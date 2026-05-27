#pragma once

#include "Core.h"
#include "Preview/PreviewScene.h"
#include "Runtime/Function/Render/SceneViewport.h"

#include <memory>

namespace minEngine
{
    class AssetMeta;
    class Material;
    class RHITexture2D;
    class RHI;

    enum class PreviewDisplayKind
    {
        None,
        Scene3D,
    };

    /** Read-only asset preview for Inspector: PreviewScene + SceneViewport bridge (no ViewportClient). */
    class InspectorAssetInspection
    {
    public:
        void SetInspectionTarget(const AssetMeta* meta);
        void ClearInspectionTarget();

        PreviewDisplayKind GetDisplayKind() const { return m_DisplayKind; }
        bool HasPreviewContent() const;

        void RenderInspection(uint32_t squareSize);
        const std::shared_ptr<RHITexture2D>& GetSceneColorTexture() const;

        void Shutdown();

    private:
        PreviewDisplayKind ResolveDisplayKind(const AssetMeta& meta) const;
        void EnsureSceneViewport(uint32_t squareSize);
        void ShutdownSceneViewport();
        void RebuildScene3DPreview(const AssetMeta& meta);
        void SetupDefaultPreviewCamera();
        void SyncPreviewCameraAspect();

        PreviewDisplayKind m_DisplayKind = PreviewDisplayKind::None;
        PreviewScene m_World;
        SceneViewport m_Viewport;
        bool m_ViewportInitialized = false;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;

        std::shared_ptr<Material> m_MaterialAsset;
    };
}

