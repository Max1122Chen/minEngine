#pragma once
#include "Core.h"
#include "RenderPasses/ShadowPass.h"
#include "RenderPasses/BasePass.h"
#include "RenderPasses/TranslucencyPass.h"
#include "RenderPasses/PresentPass.h"
#include "LightSceneProxies/LightSceneProxy.h"
#include "LightSceneProxies/DirectionalLightSceneProxy.h"
#include "LightSceneProxies/PointLightSceneProxy.h"
#include "LightSceneProxies/SpotLightSceneProxy.h"


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

    struct ShadowEntry
    {
        Matrix4 LightViewProjMatrix;

        uint32_t Resolution; // e.g., 1024 for a 1024x1024 shadow map

        virtual Matrix4 CalculateLightViewProjMatrix() const = 0;
    };

    struct DirLightShadowEntry : public ShadowEntry
    {
        DirectionalLightSceneProxy* LightProxy;
        // Currently we only generate one shadow map for the directional light, but we can extend this to support cascaded shadow maps in the future
        std::vector<std::shared_ptr<RHITexture2D>> CascadeShadowMaps; // One shadow map per cascade

        virtual Matrix4 CalculateLightViewProjMatrix() const override;

    };

    constexpr uint32_t MAX_POINT_LIGHTS = 16;
    constexpr uint32_t MAX_SPOT_LIGHTS = 16;

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

    struct LightsData
    {
        DirectionalLightData DirectionalLight;
        PointLightData PointLights[MAX_POINT_LIGHTS];
        SpotLightData SpotLights[MAX_SPOT_LIGHTS];

        uint32_t PointLightsCount;
        uint32_t SpotLightsCount;
    };

    class RenderPipeline
    {
    public:
        RenderPipeline() = default;
        virtual ~RenderPipeline() = default;

        void Initialize();
        void Shutdown();
        void Execute();

    private:
        std::shared_ptr<UniformBuffer> m_LightViewProjUniformBuffer; // Uniform buffer for light view projection matrices, used in shadow pass
        std::shared_ptr<UniformBuffer> m_PerFrameUniformBuffer;
        std::shared_ptr<UniformBuffer> m_LightUniformBuffer;

        std::shared_ptr<FrameBuffer> m_ShadowBuffer;
        std::shared_ptr<FrameBuffer> m_SceneBuffer;
        
        std::shared_ptr<RHITexture2D> m_SceneColorTexture;
        std::shared_ptr<RHITexture2D> m_SceneDepthTexture;

        std::vector<DirLightShadowEntry> m_DirLightShadowEntries;

        ShadowPass m_ShadowPass;
        BasePass m_BasePass;
        TranslucencyPass m_TranslucentPass;
        PresentPass m_PresentPass;

        std::vector<MeshDrawCommand> m_OpaqueQueue;
        std::vector<MeshDrawCommand> m_TranslucentQueue;

    private:
        void UpdatePerFrameUBO();
        void UpdateLightUBO();
        void BuildShadowEntries();
        void BuildRenderQueue();
    };
}