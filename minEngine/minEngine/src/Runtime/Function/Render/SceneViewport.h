#pragma once

#include "Core.h"
#include "SceneDrawDesc.h"
#include "Runtime/Core/Math/Math.h"
#include "Render/RHI/RHITexture.h"

#include <memory>

namespace minEngine
{
    class RenderScene;
    class RenderCamera;
    class SceneRenderTarget;
    class RHI;

    /** Main editor viewport (P2). P3: owned by EditorViewportClient instead of SceneManager. */
    class SceneViewport
    {
    public:
        void Initialize(RHI* rhi, uint32_t width, uint32_t height);
        void Shutdown();

        void SetObservedScene(RenderScene* scene) { m_ObservedScene = scene; }
        RenderScene* GetObservedScene() const { return m_ObservedScene; }

        void SetActiveCamera(std::shared_ptr<RenderCamera> camera) { m_Camera = std::move(camera); }
        RenderCamera* GetCamera() { return m_Camera.get(); }
        const RenderCamera* GetCamera() const { return m_Camera.get(); }

        SceneRenderTarget* GetRenderTarget() { return m_RenderTarget.get(); }
        const SceneRenderTarget* GetRenderTarget() const { return m_RenderTarget.get(); }
        const RHITextureRef& GetColorTexture() const;

        Math::Vector2 GetBufferSize() const;

        void RequestResizeByRatio(float widthRatio, float heightRatio);
        void ApplyPendingResize(RHI* rhi);

        SceneDrawDesc BuildDrawDesc(SceneDrawFlags flags = SceneDrawFlags::Default) const;

    private:
        RenderScene* m_ObservedScene = nullptr;
        std::shared_ptr<RenderCamera> m_Camera;
        std::shared_ptr<SceneRenderTarget> m_RenderTarget;

        bool m_HasPendingResize = false;
        uint32_t m_PendingWidth = 0;
        uint32_t m_PendingHeight = 0;
    };
}
