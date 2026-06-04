#pragma once
#include "Core.h"
#include "RenderPassBase.h"
#include "Math/Math.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

namespace minEngine
{
    class RHI;
    class MeshDrawCommand;
    class UniformBuffer;
    class FrameBuffer;
    class RHITexture2DArray;
    class RHIShaderLegacy;



    class ShadowPass : public RenderPassBase
    {
    public:
        ShadowPass() = default;
        virtual ~ShadowPass() = default;    

        void Initialize();

        virtual void Execute() override;
    
    private:
        virtual void Render() override;

    public:
        UniformBuffer* m_LightViewProjUniformBuffer;
        std::vector<MeshDrawCommand> m_OpaqueQueue; // We only do depth test for opaque objects in the shadow pass

        std::vector<ShadowDrawCommand> m_ShadowDrawCommands;

    private:
        std::shared_ptr<RHIShaderLegacy> m_DepthOnlyShader; // A simple shader that only outputs depth, used for shadow pass

        void UpdateLightViewProjBuffer(Matrix4 inMatrix);
        void RenderDirectionalShadow(RHI& rhi, const ShadowDrawCommand& shadowCommand);
        void RenderSpotShadow(RHI& rhi, const ShadowDrawCommand& shadowCommand);
        void RenderPointShadow(RHI& rhi, const ShadowDrawCommand& shadowCommand);
    };
}