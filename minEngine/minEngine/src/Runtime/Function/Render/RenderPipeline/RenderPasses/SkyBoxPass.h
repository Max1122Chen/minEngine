#pragma once

#include "Core.h"

#include <filesystem>
#include <memory>

namespace minEngine
{
    class RHI;
    class RHIShaderLegacy;
    class RenderCamera;
    class EngineIBLEnvironment;
    class SkyBoxSceneProxy;
    class Shader;
    class VertexBuffer;
    class VertexDefinition;

    class SkyBoxPass
    {
    public:
        void Initialize(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot);
        void Shutdown();

        void Execute(
            const RenderCamera& camera,
            const SkyBoxSceneProxy& skyBox,
            const EngineIBLEnvironment& iblEnvironment) const;

        bool IsReady() const { return m_SkyShader != nullptr && m_CubeVertexBuffer != nullptr; }

    private:
        std::shared_ptr<RHIShaderLegacy> m_SkyShader;
        std::shared_ptr<VertexBuffer> m_CubeVertexBuffer;
        std::shared_ptr<VertexDefinition> m_CubeVertexDefinition;
    };
}
