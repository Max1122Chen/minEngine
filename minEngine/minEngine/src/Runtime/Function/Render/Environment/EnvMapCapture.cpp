#include "EnvMapCapture.h"

#include "../Shader.h"
#include "../Texture.h"
#include "../TextureCubeLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

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

        const Vector3 kCaptureTargets[6] = {
            Vector3( 1.0f,  0.0f,  0.0f),
            Vector3(-1.0f,  0.0f,  0.0f),
            Vector3( 0.0f,  1.0f,  0.0f),
            Vector3( 0.0f, -1.0f,  0.0f),
            Vector3( 0.0f,  0.0f,  1.0f),
            Vector3( 0.0f,  0.0f, -1.0f),
        };

        const Vector3 kCaptureUps[6] = {
            Vector3(0.0f, -1.0f,  0.0f),
            Vector3(0.0f, -1.0f,  0.0f),
            Vector3(0.0f,  0.0f,  1.0f),
            Vector3(0.0f,  0.0f, -1.0f),
            Vector3(0.0f, -1.0f,  0.0f),
            Vector3(0.0f, -1.0f,  0.0f),
        };

        uint32_t ChannelsFromFormat(TextureFormat format)
        {
            if (format == TextureFormat::RGB16F)
            {
                return 3;
            }
            if (format == TextureFormat::RGBA16F)
            {
                return 4;
            }
            return 0;
        }
    }

    std::shared_ptr<TextureCube> EnvMapCapture::EquirectToCubemap(
        RHI& rhi,
        RHITexture2D& equirectTexture,
        const std::filesystem::path& engineDefaultAssetsRoot,
        uint32_t faceSize,
        std::string* outError)
    {
        auto reportError = [&](const std::string& message) -> std::shared_ptr<TextureCube>
        {
            ME_CORE_ERROR("EnvMapCapture: {}", message);
            if (outError)
            {
                *outError = message;
            }
            return nullptr;
        };

        if (faceSize == 0)
        {
            return reportError("faceSize must be > 0.");
        }

        const TextureFormat cubeFormat = equirectTexture.GetFormat();
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError("equirect texture must be RGB16F or RGBA16F.");
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        const std::filesystem::path vertexPath = shaderDirectory / "equirect_to_cubemap.vert";
        const std::filesystem::path fragmentPath = shaderDirectory / "equirect_to_cubemap.frag";

        std::shared_ptr<Shader> captureShader = Shader::CreateFromFiles(
            rhi,
            vertexPath,
            fragmentPath,
            outError);
        if (!captureShader || !captureShader->IsValid())
        {
            return reportError("failed to compile equirect_to_cubemap shader.");
        }

        std::shared_ptr<RHIShader> shader = captureShader->GetRHIShader();

        std::string cubeError;
        std::shared_ptr<RHITextureCube> environmentCube = TextureCubeLoader::CreateEmptyRenderTargetCube(
            rhi,
            faceSize,
            cubeFormat,
            &cubeError);
        if (!environmentCube)
        {
            return reportError(cubeError.empty() ? "failed to allocate cubemap." : cubeError);
        }

        std::shared_ptr<FrameBuffer> captureFramebuffer = rhi.CreateFrameBuffer(faceSize, faceSize);
        if (!captureFramebuffer)
        {
            return reportError("failed to create capture framebuffer.");
        }

        const uint32_t vertexByteSize = static_cast<uint32_t>(sizeof(kCubeVertices));
        const uint32_t vertexCount = vertexByteSize / (3 * sizeof(float));
        std::shared_ptr<VertexBuffer> cubeVertexBuffer =
            rhi.CreateVertexBuffer(const_cast<float*>(kCubeVertices), vertexByteSize, vertexCount);
        std::shared_ptr<VertexDefinition> cubeVertexDefinition = rhi.CreateVertexDefinition({
            {"a_Position", VertexElementType::Float3, false},
        });

        GLuint depthRenderbuffer = 0;
        glGenRenderbuffers(1, &depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, faceSize, faceSize);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        GLint previousViewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, previousViewport);

        const Matrix4 captureProjection =
            glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        captureFramebuffer->Bind();
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER,
            depthRenderbuffer);

        rhi.EnableDepthTest();
        rhi.SetDepthMask(true);
        rhi.DisableCullFace();

        shader->Use();
        shader->UploadUniformMat4("u_Projection", captureProjection);
        equirectTexture.Bind(0);
        shader->UploadUniformInt("u_EquirectangularMap", 0);

        for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            const Matrix4 captureView = glm::lookAt(
                Vector3(0.0f),
                kCaptureTargets[faceIndex],
                kCaptureUps[faceIndex]);

            captureFramebuffer->AttachColorCubeFace(environmentCube, faceIndex);
            captureFramebuffer->Bind();

            const GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
            {
                glDeleteRenderbuffers(1, &depthRenderbuffer);
                return reportError(
                    "capture framebuffer incomplete for face " + std::to_string(faceIndex) + ".");
            }

            rhi.SetViewport(0, 0, faceSize, faceSize);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader->UploadUniformMat4("u_View", captureView);
            cubeVertexDefinition->Bind();
            cubeVertexBuffer->Bind();
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
        }

        captureFramebuffer->Unbind();
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glDeleteRenderbuffers(1, &depthRenderbuffer);

        rhi.SetViewport(
            static_cast<uint32_t>(previousViewport[0]),
            static_cast<uint32_t>(previousViewport[1]),
            static_cast<uint32_t>(previousViewport[2]),
            static_cast<uint32_t>(previousViewport[3]));

        equirectTexture.Unbind();
        environmentCube->Unbind();

        return TextureCubeLoader::WrapRHITextureCube(std::move(environmentCube), faceSize, channels);
    }
}
