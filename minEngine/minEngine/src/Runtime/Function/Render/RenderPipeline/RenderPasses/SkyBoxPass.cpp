#include "SkyBoxPass.h"

#include "../../Environment/EngineIBLEnvironment.h"
#include "../../RenderCamera.h"
#include "../../Shader.h"
#include "../../SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Texture.h"

#include <glad/glad.h>

namespace minEngine
{
    namespace
    {
        constexpr float kCubeVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,

             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,

            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,

            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
        };

        constexpr int kSkyboxTextureUnit = 0;
    }

    void SkyBoxPass::Initialize(RHI& rhi, const std::filesystem::path& engineDefaultAssetsRoot)
    {
        Shutdown();

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<Shader> skyShader = Shader::CreateFromFiles(
            rhi,
            shaderDirectory / "background.vert",
            shaderDirectory / "background.frag");
        if (!skyShader || !skyShader->IsValid())
        {
            ME_CORE_ERROR("SkyBoxPass: failed to compile background shader.");
            return;
        }

        m_SkyShader = skyShader->GetRHIShader();

        const uint32_t vertexByteSize = static_cast<uint32_t>(sizeof(kCubeVertices));
        const uint32_t vertexCount = vertexByteSize / (3 * sizeof(float));
        m_CubeVertexBuffer =
            rhi.CreateVertexBuffer(const_cast<float*>(kCubeVertices), vertexByteSize, vertexCount);
        m_CubeVertexDefinition = rhi.CreateVertexDefinition({
            {"a_Position", VertexElementType::Float3, false},
        });
    }

    void SkyBoxPass::Shutdown()
    {
        m_SkyShader.reset();
        m_CubeVertexBuffer.reset();
        m_CubeVertexDefinition.reset();
    }

    void SkyBoxPass::Execute(
        const RenderCamera& camera,
        const SkyBoxSceneProxy& skyBox,
        const EngineIBLEnvironment& iblEnvironment) const
    {
        if (!m_SkyShader || !m_CubeVertexBuffer || !m_CubeVertexDefinition)
        {
            return;
        }

        if (!skyBox.m_Enabled)
        {
            return;
        }

        const TextureCube* environment = iblEnvironment.GetEnvironment();
        if (!environment || !environment->GetRHITexture())
        {
            return;
        }

        GLint previousDepthFunc = GL_LESS;
        GLboolean previousDepthMask = GL_TRUE;
        GLint previousCullFace = GL_BACK;
        glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
        glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);

        m_SkyShader->Use();
        m_SkyShader->UploadUniformMat4("u_Projection", camera.GetProjectionMatrix());
        m_SkyShader->UploadUniformMat4("u_View", camera.GetViewMatrix());
        m_SkyShader->UploadUniformFloat("u_SkyIntensity", skyBox.m_SkyIntensity);

        environment->GetRHITexture()->Bind(kSkyboxTextureUnit);
        m_SkyShader->UploadUniformInt("u_Skybox", kSkyboxTextureUnit);

        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glCullFace(GL_FRONT);

        m_CubeVertexDefinition->Bind();
        m_CubeVertexBuffer->Bind();
        glDrawArrays(
            GL_TRIANGLES,
            0,
            static_cast<GLsizei>(m_CubeVertexBuffer->GetNumVertices()));

        environment->GetRHITexture()->Unbind();

        glDepthFunc(previousDepthFunc);
        glDepthMask(previousDepthMask);
        glCullFace(previousCullFace);
    }
}
