#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

#include <filesystem>
#include <memory>

namespace minEngine
{
    class RHI;
    class RHICommandList;
    class RHIBindingLayout;
    class RHIPipelineLayout;
    class RHIBindingSet;
    class RHIGraphicsPipelineState;
    class RHIShader;
    class RHIShaderResourceView;
    class RenderCamera;
    class SkyBoxSceneProxy;
    class RHIBuffer;
    class RHIVertexInputLayout;
    class TextureCube;

    class SkyBoxPass
    {
    public:
        void Initialize(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot);
        void Shutdown();

        void Execute(
            RHICommandList& cmdList,
            const RenderCamera& camera,
            const SkyBoxSceneProxy& skyBox) const;

        bool IsReady() const
        {
            return m_SkyShader != nullptr && m_CubeVertexBuffer != nullptr && m_EnvironmentCube != nullptr;
        }

    private:
        std::shared_ptr<RHIShader> m_SkyShader;
        RHIBufferRef m_CubeVertexBuffer;
        RHIVertexInputLayoutRef m_CubeVertexLayout;
        std::shared_ptr<RHIGraphicsPipelineState> m_SkyPipelineState;
        std::shared_ptr<RHIBindingLayout> m_SkyBindingLayout;
        std::shared_ptr<RHIPipelineLayout> m_SkyPipelineLayout;
        std::shared_ptr<TextureCube> m_EnvironmentCube;
        mutable std::shared_ptr<RHIShaderResourceView> m_EnvironmentSRV;
        mutable std::shared_ptr<RHIBindingSet> m_SkyBindingSet;
        mutable RHIBufferRef m_SkyFrameUniformBuffer;
    };
}
