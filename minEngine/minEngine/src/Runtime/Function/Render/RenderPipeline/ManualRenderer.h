#pragma once

#include "Runtime/Function/Render/RenderPipeline/ForwardRenderer.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <array>

namespace minEngine
{
    /**
     * RND-F13 diagnostic renderer: reuses ForwardRenderer scene/shadow/UBO setup but
     * executes Shadow -> Base -> Present manually (no RenderGraph).
     * EXPERIMENTAL — not a production rendering path.
     */
    class ManualRenderer : public ForwardRenderer
    {
    public:
        ManualRenderer() = default;
        ~ManualRenderer() override = default;

        void Execute(const SceneDrawDesc& desc) override;
        void Shutdown() override;

    private:
        RHITextureRef m_ManualSceneColor;
        RHITextureRef m_ManualSceneDepth;
        RHITextureRef m_ManualDirShadowAtlas;
        std::array<RHITextureRef, MAX_SPOT_SHADOW_MAPS> m_ManualSpotShadowMaps{};
        std::array<RHITextureRef, MAX_POINT_SHADOW_MAPS> m_ManualPointShadowMaps{};
        uint32_t m_ManualWidth = 0;
        uint32_t m_ManualHeight = 0;

        bool EnsureManualTextures(RHI& rhi, uint32_t width, uint32_t height);
        void BindManualShadowTextures(SceneRenderContext& ctx);
        void ExecuteManualShadowPasses(RHICommandList& cmdList, SceneRenderContext& ctx);
        /** When EnableSkyBox: clear SceneColor/Depth (and draw sky if ready). Returns whether targets were cleared. */
        bool ExecuteManualSkyBoxPass(
            RHICommandList& cmdList,
            const SceneDrawDesc& desc,
            const SceneRenderContext& ctx,
            uint32_t width,
            uint32_t height);
        void ExecuteManualBasePass(
            RHICommandList& cmdList,
            const SceneDrawDesc& desc,
            const SceneRenderContext& ctx,
            bool clearScene,
            uint32_t width,
            uint32_t height);
    };
}
