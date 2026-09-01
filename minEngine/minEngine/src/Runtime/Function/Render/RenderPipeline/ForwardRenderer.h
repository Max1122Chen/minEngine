#pragma once
#include "Core.h"
#include "Runtime/Function/Render/SceneRenderer.h"
#include "Runtime/Function/Render/EngineRenderLimits.h"
#include "RenderPasses/ShadowPass.h"
#include "RenderPasses/ShadowGraphPass.h"
#include "RenderPasses/BasePass.h"
#include "RenderPasses/TranslucencyPass.h"
#include "RenderPasses/PostProcessPass.h"
#include "RenderPasses/PresentPass.h"
#include "RenderPasses/SkyBoxPass.h"
#include "RenderPasses/DebugDrawPass.h"
#include "Runtime/Function/Render/EnginePipelineLayouts.h"
#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/SpotLightSceneProxy.h"
#include "Shadow/ShadowTypes.h"
#include "Shadow/ShadowUniformBuffers.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Runtime/Function/Render/SceneRenderContext.h"
#include "Runtime/Function/Render/SceneRenderTarget.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RenderGraph/RenderGraph.h"
#include "Runtime/Function/Render/RenderGraph/RDGTypes.h"

#include <unordered_map>


namespace minEngine
{
    class RenderCamera;

    /**
     * UBO binding point layout:
     * - 0: Per-frame data (view/proj matrices, camera position, etc.)
     * - 1: Light data (directional light, point lights, spot lights, etc.)
     */

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
        Vector4 Position;  // xyz = position; w = attenuation radius
        Vector4 Color;     // w = intensity
        Vector4 Params;    // x = falloff exponent; z = shadow far plane; w = shadow map index
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

    class ForwardRenderer : public SceneRenderer
    {
    public:
        ForwardRenderer() = default;
        ~ForwardRenderer() override = default;

        void Initialize() override;
        void Shutdown() override;
        void Execute(const SceneDrawDesc& desc) override;

        void SetPresentPassEnabled(bool enabled) override { m_EnablePresentPass = enabled; }

        void LoadEngineRenderingAssets(const std::string& engineDefaultAssetsRoot) override;

        RHIBuffer* GetPerFrameUniformBuffer() const { return m_PerFrameUniformBuffer.get(); }
        RHIBuffer* GetPerObjectUniformBuffer() const { return m_PerObjectUniformBuffer.get(); }
        uint32_t GetPerObjectSlotStride() const { return m_PerObjectSlotStride; }
        EngineSceneBindingSets& GetSceneBindings() { return m_SceneBindings; }
        const EngineSceneBindingSets& GetSceneBindings() const { return m_SceneBindings; }
        const EnginePipelineLayouts& GetPipelineLayouts() const { return m_PipelineLayouts; }

    protected:
        RHIBufferRef m_PerFrameUniformBuffer;
        RHIBufferRef m_LightDataUniformBuffer;
        RHIBufferRef m_PerObjectUniformBuffer;
        uint32_t m_PerObjectSlotStride = 256;

        ShadowUniformBuffers m_ShadowUniformBuffers;

        ShadowPass m_ShadowPass;
        BasePass m_BasePass;
        DebugDrawPass m_DebugDrawPass;
        PresentPass m_PresentPass;

        EngineSceneBindingSets m_SceneBindings;
        EnginePipelineLayouts m_PipelineLayouts;
        uint64_t m_FrameIndex = 0;
        bool m_EnablePresentPass = true;

        void BindSceneRenderTarget(SceneRenderTarget& target);
        void UpdatePerFrameUBO(const SceneRenderContext& ctx);
        void UpdateLightUBO(const SceneRenderContext& ctx);
        void CollectShadowRequests(SceneRenderContext& ctx);
        void BuildShadowDrawCommands(SceneRenderContext& ctx);
        void BuildRenderQueue(SceneRenderContext& ctx);
        void ClearUnusedShadowViewProjSlots(const SceneRenderContext& ctx);

        SkyBoxPass m_SkyBoxPass;

    private:
        TranslucencyPass m_TranslucentPass;
        std::vector<PostProcessPass> m_PostProcessPasses;

        std::string m_EngineDefaultAssetsRoot;

        RHIBufferRef m_ScreenQuadVertexBuffer;
        RHIVertexInputLayoutRef m_ScreenQuadVertexLayout;

        RenderGraph m_FrameRenderGraph;
        RHITextureRef m_PostBufferTexture;
        std::vector<std::unique_ptr<ShadowGraphPass>> m_ShadowGraphPasses;
        std::vector<RenderPass*> m_ShadowGraphPassPtrs;
        bool m_ConfiguredEnablePostProcess = false;
        bool m_ConfiguredPresentToBackBuffer = false;
        bool m_ConfiguredEnableDebugDraw = false;
        RenderPass* m_SceneSkyGraphPass = nullptr;
        RenderPass* m_SceneOpaqueGraphPass = nullptr;
        RenderPass* m_SceneTranslucentGraphPass = nullptr;
        RenderPass* m_SceneDebugGraphPass = nullptr;
        RenderPass* m_PostFxaaGraphPass = nullptr;
        RenderPass* m_PostSharpenGraphPass = nullptr;
        RenderPass* m_PresentGraphPass = nullptr;
        bool m_FrameRenderGraphBuilt = false;
        uint32_t m_PostBufferWidth = 0;
        uint32_t m_PostBufferHeight = 0;

        void BuildFrameRenderGraph(bool enablePostProcess, bool presentToBackBuffer, bool enableDebugDraw);
        void AssignShadowGraphPassCommands(const SceneRenderContext& ctx);
        static size_t GetFixedShadowGraphPassIndex(const ShadowDrawCommand& command);
        static ShadowGraphPermanentOutput MakePermanentShadowOutput(size_t passIndex);
        void EnsurePostBufferTexture(RHI* rhi, uint32_t width, uint32_t height);
        void SetupFrameRenderGraph(
            RHICommandList& cmdList,
            const SceneDrawDesc& desc,
            SceneRenderContext& ctx);
        void BindGraphShadowTextures(SceneRenderContext& ctx);
        void EnqueueFrameRenderGraph(RHICommandList& cmdList, SceneRenderTarget* sceneTarget);
        ShadowResourceHandle MakeDirectionalShadowBinding(const ShadowRequest& req, uint32_t cascadeCount) const;
        ShadowResourceHandle MakeSpotShadowBinding(const ShadowRequest& req, int slotIndex) const;
        ShadowResourceHandle MakePointShadowBinding(const ShadowRequest& req, int slotIndex) const;

        DirShadowCommandBuildResult BuildDirectionalShadowDrawCommands(const ShadowRequest& shadowRequest, 
                                                            const ShadowResourceHandle& handle, 
                                                            const DirectionalLightSceneProxy* lightProxy,
                                                            uint32_t cascadeCount,
                                                            RenderCamera* camera,
                                                            const std::vector<MeshDrawCommand>& opaqueQueue);
        
        std::vector<CascadeSplit> CalculateCascadeSplits(float nearPlane, float farPlane, uint32_t cascadeCount);
        void ExpandCascadeZForShadowCasters(Math::Geometry::AABB& frustumAABB,
                                            const Matrix4& lightView,
                                            const std::vector<MeshDrawCommand>& opaqueQueue);

        ShadowDrawCommand BuildSpotShadowDrawCommand(const ShadowRequest& shadowRequest,
                                                      const ShadowResourceHandle& handle,
                                                      const SpotLightSceneProxy* lightProxy);

        std::vector<ShadowDrawCommand> BuildPointShadowDrawCommands(const ShadowRequest& shadowRequest,
                                                                     const ShadowResourceHandle& handle,
                                                                     const PointLightSceneProxy* lightProxy);
    };
}
