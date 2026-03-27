#pragma once
#include "Core.h"
#include "RenderPassBase.h"
#include "Math/Math.h"

namespace minEngine
{
    class MeshDrawCommand;
    class UniformBuffer;
    class FrameBuffer;
    class RHITexture2D;
    class DirLightShadowEntry;
    class RHIShader;



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

        std::vector<DirLightShadowEntry> m_DirLightShadowEntries;
        // std::vector<PointLightShadowEntry> m_PointLightShadowEntries;
        // std::vector<SpotLightShadowEntry> m_SpotLightShadowEntries;

    private:
        std::shared_ptr<RHIShader> m_DepthOnlyShader; // A simple shader that only outputs depth, used for shadow pass

        void UpdateLightViewProjBuffer(Matrix4 inMatrix);
    };
}