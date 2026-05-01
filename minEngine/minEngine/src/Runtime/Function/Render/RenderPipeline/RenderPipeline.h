#pragma once
#include "Core.h"
#include "RenderPasses/ShadowPass.h"
#include "RenderPasses/BasePass.h"
#include "RenderPasses/TranslucencyPass.h"
#include "RenderPasses/PostProcessPass.h"
#include "RenderPasses/PresentPass.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/SpotLightSceneProxy.h"
#include "Shadow/ShadowResourceManager.h"


namespace minEngine
{
    class UniformBuffer;
    class FrameBuffer;
    class RHITexture2D;

    /**
     * UBO binding point layout:
     * - 0: Per-frame data (view/proj matrices, camera position, etc.)
     * - 1: Light data (directional light, point lights, spot lights, etc.)
     */

    constexpr uint32_t MAX_POINT_LIGHTS = 16;
    constexpr uint32_t MAX_SPOT_LIGHTS = 16;
    constexpr uint32_t MAX_CASCADES = 4;

    // Data structure for per-frame uniform buffer
    struct PerFrameData
    {
        Matrix4 View;
        Matrix4 Proj;
        Matrix4 ViewProj;

        Vector4 CameraPos;
    };
    
    // Data structure for light uniform buffer
    struct DirectionalLightData
    {
        Vector4 Direction; 
        Vector4 Color;     // w component can be used for intensity
        Vector4 Params;    // w component can be used for shadow map index
    };

    struct PointLightData
    {
        Vector4 Position;  // w component can be used for radius. we dont have radius for point light, but we can use it to do some distance-based attenuation in shader
        Vector4 Color;     // w component can be used for intensity
        Vector4 Params;    // w component can be used for shadow map index 
    };

    struct SpotLightData
    {
        Vector4 Direction;
        Vector4 Position;
        Vector4 Color;      // w component can be used for intensity
        Vector4 Params;     // x = inner cone angle in degrees, y = outer cone angle in degrees, w component can be used for shadow map index
    };

    struct LightsData   // Be careful about the std140 layout requirement for this structure, we need to make sure the data is aligned properly.
    {
        DirectionalLightData DirectionalLight;
        PointLightData PointLights[MAX_POINT_LIGHTS];
        SpotLightData SpotLights[MAX_SPOT_LIGHTS];

        uint32_t PointLightsCount;
        uint32_t SpotLightsCount;
    };

    // CSM cascade split data structure
    struct CascadeSplit
    {
        float Near;
        float Far;
    };

    struct Frustum
    {
        Vector4 Corners[8];
    };

    struct DirShadowCommandBuildResult
    {
        std::vector<ShadowDrawCommand> Commands;
        std::vector<float> CascadeFarPlaneVS;
    };

    class RenderPipeline
    {
    public:
        RenderPipeline() = default;
        virtual ~RenderPipeline() = default;

        void Initialize();
        void Shutdown();
        void Execute();
        void ResizeSceneTargets(uint32_t width, uint32_t height);

        void SetPresentPassEnabled(bool enabled)
        {
            m_EnablePresentPass = enabled;
        }

        const std::shared_ptr<RHITexture2D>& GetSceneColorTexture() const
        {
            return m_SceneColorTexture;
        }

        Vector2 GetSceneBufferSize() const
        {
            return Vector2(m_SceneBufferWidth, m_SceneBufferHeight);
        }

        uint32_t GetSceneBufferWidth() const
        {
            return m_SceneBufferWidth;
        }

        uint32_t GetSceneBufferHeight() const
        {
            return m_SceneBufferHeight;
        }

    private:
        std::shared_ptr<UniformBuffer> m_LightViewProjUniformBuffer; // Uniform buffer for light view projection matrices used in shadow pass
        std::shared_ptr<UniformBuffer> m_PerFrameUniformBuffer;
        std::shared_ptr<UniformBuffer> m_LightDataUniformBuffer;

        std::shared_ptr<UniformBuffer> m_DirLightViewProjUniformBuffer; // Uniform buffer for directional light view projection matrix used in base pass for CSM
        std::shared_ptr<UniformBuffer> m_CascadeFarPlaneUniformBuffer; // Uniform buffer for CSM cascade far plane distances used in base pass for CSM

        std::shared_ptr<FrameBuffer> m_ShadowBuffer;
        std::shared_ptr<FrameBuffer> m_SceneBuffer;
        
        std::shared_ptr<RHITexture2D> m_SceneColorTexture;
        std::shared_ptr<RHITexture2D> m_SceneDepthTexture;

        std::vector<ShadowRequest> m_ShadowRequests;
        std::vector<ShadowDrawCommand> m_ShadowDrawCommands;

        ShadowResourceHandle m_DirectionalShadowHandle;
        Matrix4 m_DirectionalLightViewProj = Matrix4(1.0f);

        ShadowPass m_ShadowPass;
        BasePass m_BasePass;
        TranslucencyPass m_TranslucentPass;
        std::vector<PostProcessPass> m_PostProcessPasses;
        PresentPass m_PresentPass;

        std::vector<MeshDrawCommand> m_OpaqueQueue;
        std::vector<MeshDrawCommand> m_TranslucentQueue;

        ShadowResourceManager m_ShadowResourceManager;
        uint64_t m_FrameIndex = 0;
        bool m_EnablePresentPass = true;
        uint32_t m_SceneBufferWidth = 0;
        uint32_t m_SceneBufferHeight = 0;

    private:
        void UpdatePerFrameUBO();
        void UpdateLightUBO();
        void CollectShadowRequests();
        void BuildShadowDrawCommands();
        void BuildRenderQueue();

        // Directional shadow command building 
        DirShadowCommandBuildResult BuildDirectionalShadowDrawCommands(const ShadowRequest& shadowRequest, 
                                                            const ShadowResourceHandle& handle, 
                                                            const DirectionalLightSceneProxy* lightProxy,
                                                            uint32_t cascadeCount);
        
        std::vector<CascadeSplit> CalculateCascadeSplits(float nearPlane, float farPlane, uint32_t cascadeCount);
        void ExpandCascadeZForShadowCasters(Math::Geometry::AABB& frustumAABB, const Matrix4& lightView);
    };
}