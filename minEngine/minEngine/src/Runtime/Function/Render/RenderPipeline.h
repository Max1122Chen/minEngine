#pragma once
#include "Core.h"
#include "RenderPasses/BasePass.h"
#include "RenderPasses/TranslucencyPass.h"
#include "RenderPasses/PresentPass.h"


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
    };

    struct PointLightData
    {
        Vector4 Position;  // w component can be used for radius. we dont have radius for point light, but we can use it to do some distance-based attenuation in shader
        Vector4 Color;     // w component can be used for intensity
    };

    struct SpotLightData
    {
        Vector4 Direction;
        Vector4 Position;
        Vector4 Color;      // w component can be used for intensity
        Vector4 ConeAngles; // x = inner cone angle in degrees, y = outer cone angle in degrees
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
        std::shared_ptr<UniformBuffer> m_PerFrameUniformBuffer;
        std::shared_ptr<UniformBuffer> m_LightUniformBuffer;

        std::shared_ptr<FrameBuffer> m_SceneBuffer;
        
        std::shared_ptr<RHITexture2D> m_SceneColorTexture;
        std::shared_ptr<RHITexture2D> m_SceneDepthTexture;

        BasePass m_BasePass;
        TranslucencyPass m_TranslucentPass;
        PresentPass m_PresentPass;

        std::vector<MeshDrawCommand> m_OpaqueQueue;
        std::vector<MeshDrawCommand> m_TranslucentQueue;

    private:
        void UpdatePerFrameUBO();
        void UpdateLightUBO();
        void BuildRenderQueue();
    };
}