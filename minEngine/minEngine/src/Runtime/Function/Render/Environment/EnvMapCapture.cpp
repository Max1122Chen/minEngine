#include "EnvMapCapture.h"

#include "../EngineShaderUtils.h"
#include "../EnginePassUniforms.h"
#include "../TextureCubeLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <glad/glad.h>

#include <algorithm>

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

        void AllocateRgb16fCubemapMipChain(
            GLuint textureId,
            uint32_t baseFaceSize,
            uint32_t maxMipLevel)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
            for (uint32_t mip = 0; mip <= maxMipLevel; ++mip)
            {
                const uint32_t mipSize = std::max(1u, baseFaceSize >> mip);
                for (uint32_t face = 0; face < 6; ++face)
                {
                    glTexImage2D(
                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                        static_cast<GLint>(mip),
                        GL_RGB16F,
                        static_cast<GLsizei>(mipSize),
                        static_cast<GLsizei>(mipSize),
                        0,
                        GL_RGB,
                        GL_FLOAT,
                        nullptr);
                }
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(maxMipLevel));
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }

        struct EnvMapMeshResources
        {
            RHIBufferRef VertexBuffer;
            RHIVertexInputLayoutRef VertexLayout;
            uint32_t VertexCount = 0;
        };

        EnvMapMeshResources CreateEnvMapMesh(RHI& rhi)
        {
            RHICommandList cmdList(&rhi);
            EnvMapMeshResources resources;
            const uint32_t vertexByteSize = static_cast<uint32_t>(sizeof(kCubeVertices));
            resources.VertexCount = vertexByteSize / (3 * sizeof(float));

            RHIBufferCreateDesc vbDesc;
            vbDesc.Usage = RHIBufferUsage::Vertex;
            vbDesc.ByteSize = vertexByteSize;
            vbDesc.Stride = 3 * sizeof(float);
            vbDesc.ElementCount = resources.VertexCount;
            resources.VertexBuffer = cmdList.CreateBuffer(vbDesc, kCubeVertices);
            resources.VertexLayout = cmdList.CreateVertexInputLayout({
                {"a_Position", VertexElementType::Float3, false},
            });
            return resources;
        }

        void DrawEnvMapMesh(RHICommandList& cmdList, const EnvMapMeshResources& mesh)
        {
            cmdList.SetVertexInputLayout(mesh.VertexLayout.get());
            cmdList.SetVertexBuffer(mesh.VertexBuffer.get());
            cmdList.Draw(mesh.VertexCount, 0);
        }

        void BeginCubeFaceRenderPass(
            RHICommandList& cmdList,
            RHITexture* colorCube,
            uint32_t faceIndex,
            uint8_t mipIndex,
            RHITexture* depthTexture,
            uint32_t viewportSize)
        {
            RHIRenderPassInfo passInfo;
            passInfo.ColorAttachments[0].RenderTarget = colorCube;
            passInfo.ColorAttachments[0].ArraySlice = static_cast<int32_t>(faceIndex);
            passInfo.ColorAttachments[0].MipIndex = mipIndex;
            passInfo.ColorAttachments[0].Action = RHIRenderTargetActions::ClearStore;
            passInfo.DepthStencil.DepthStencilTarget = depthTexture;
            passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;
            passInfo.ClearValue.Depth = 1.0f;
            cmdList.BeginRenderPass(passInfo);
            cmdList.SetViewport(0, 0, viewportSize, viewportSize);
        }

        struct EnvCaptureDrawResources
        {
            RHIGraphicsPipelineStateRef PipelineState;
            RHIBindingLayoutRef BindingLayout;
            RHIBufferRef FrameUniformBuffer;
        };

        EnvCaptureDrawResources CreateEnvCaptureDrawResources(
            RHICommandList& cmdList,
            RHIShader* shader,
            uint32_t sourceTextureUnit)
        {
            EnvCaptureDrawResources resources;
            RHIGraphicsPSODesc psoDesc;
            psoDesc.VertexShader = shader;
            psoDesc.PixelShader = shader;
            psoDesc.DepthStencilState.bDepthTestEnabled = true;
            psoDesc.DepthStencilState.bDepthWriteEnabled = true;
            psoDesc.BlendState.bBlendEnabled = false;
            resources.PipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);

            RHIBufferCreateDesc frameDesc;
            frameDesc.Usage = RHIBufferUsage::Uniform;
            frameDesc.ByteSize = sizeof(EnvCaptureFrameUBO);
            resources.FrameUniformBuffer = cmdList.CreateBuffer(frameDesc, nullptr);

            resources.BindingLayout = cmdList.CreateBindingLayout({
                {EngineShaderBindings::kEnvCapture_SourceSRV,
                 RHIBindingType::TextureSRV,
                 sourceTextureUnit,
                 RHIGraphicsShaderStage::Pixel},
                {EngineShaderBindings::kEnvCapture_FrameData,
                 RHIBindingType::UniformBuffer,
                 EngineShaderBindings::kGL_EnvCaptureFrameDataUBO,
                 RHIGraphicsShaderStage::Vertex},
            });
            return resources;
        }

        RHIBindingSetRef CreateEnvCaptureBindingSet(
            RHICommandList& cmdList,
            EnvCaptureDrawResources& drawResources,
            RHIShaderResourceView* sourceSrv,
            const EnvCaptureFrameUBO& frameData)
        {
            drawResources.FrameUniformBuffer->UpdateSubresource(&frameData, 0, sizeof(EnvCaptureFrameUBO));
            return cmdList.CreateBindingSet(
                drawResources.BindingLayout.get(),
                {
                    {RHIBindingType::TextureSRV, nullptr, sourceSrv},
                    {RHIBindingType::UniformBuffer, drawResources.FrameUniformBuffer.get(), nullptr},
                });
        }

        struct EnvMapSourceBinding
        {
            RHIShaderResourceViewRef SourceSRV;
        };

        EnvMapSourceBinding CreateSourceBinding(RHITexture* sourceTexture)
        {
            EnvMapSourceBinding binding;
            RHITextureSRVDesc srvDesc;
            srvDesc.Texture = sourceTexture;
            binding.SourceSRV = std::make_shared<OpenGLRHIShaderResourceView>(srvDesc);
            return binding;
        }
    }

    std::shared_ptr<TextureCube> EnvMapCapture::EquirectToCubemap(
        RHI& rhi,
        RHITexture& equirectTexture,
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

        const TextureFormat cubeFormat = equirectTexture.GetDesc().Format;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError("equirect texture must be RGB16F or RGBA16F.");
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> captureShader = EngineShaderUtils::CreateShaderFromFiles(
            rhi,
            shaderDirectory / "equirect_to_cubemap.vert",
            shaderDirectory / "equirect_to_cubemap.frag",
            outError);
        if (!captureShader || !captureShader->IsValid())
        {
            return reportError("failed to compile equirect_to_cubemap shader.");
        }

        RHITextureRef environmentCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat);
        if (!environmentCube)
        {
            return reportError("failed to allocate cubemap.");
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (!depthTarget)
        {
            return reportError("failed to allocate capture depth texture.");
        }

        const EnvMapMeshResources mesh = CreateEnvMapMesh(rhi);
        RHICommandList cmdList(&rhi);
        const EnvMapSourceBinding sourceBinding = CreateSourceBinding(&equirectTexture);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            captureShader.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);

        GLint previousViewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, previousViewport);

        const Matrix4 captureProjection =
            glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            const Matrix4 captureView = glm::lookAt(
                Vector3(0.0f),
                kCaptureTargets[faceIndex],
                kCaptureUps[faceIndex]);

            BeginCubeFaceRenderPass(
                cmdList,
                environmentCube.get(),
                faceIndex,
                0,
                depthTarget.get(),
                faceSize);

            EnvCaptureFrameUBO frameData{};
            frameData.Projection = captureProjection;
            frameData.View = captureView;
            cmdList.SetGraphicsPipelineState(drawResources.PipelineState.get());
            if (RHIBindingSetRef bindingSet = CreateEnvCaptureBindingSet(
                    cmdList, drawResources, sourceBinding.SourceSRV.get(), frameData))
            {
                cmdList.SetBindingSet(EngineShaderBindings::kSetEnvCapture, bindingSet.get());
            }
            DrawEnvMapMesh(cmdList, mesh);
            cmdList.EndRenderPass();
        }

        rhi.SetViewport(
            static_cast<uint32_t>(previousViewport[0]),
            static_cast<uint32_t>(previousViewport[1]),
            static_cast<uint32_t>(previousViewport[2]),
            static_cast<uint32_t>(previousViewport[3]));

        const GLuint cubeTextureId = GetOpenGLTextureId(environmentCube.get());
        if (cubeTextureId != 0)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTextureId);
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }

        return TextureCubeLoader::WrapTextureCube(std::move(environmentCube), faceSize, channels);
    }

    std::shared_ptr<TextureCube> EnvMapCapture::ConvolveIrradiance(
        RHI& rhi,
        RHITexture& environmentCube,
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
            return reportError("irradiance faceSize must be > 0.");
        }

        const TextureFormat cubeFormat = environmentCube.GetDesc().Format;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError("environment cubemap must be RGB16F or RGBA16F.");
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> convolutionShader = EngineShaderUtils::CreateShaderFromFiles(
            rhi,
            shaderDirectory / "irradiance_convolution.vert",
            shaderDirectory / "irradiance_convolution.frag",
            outError);
        if (!convolutionShader || !convolutionShader->IsValid())
        {
            return reportError("failed to compile irradiance_convolution shader.");
        }

        RHITextureRef irradianceCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat);
        if (!irradianceCube)
        {
            return reportError("failed to allocate irradiance cubemap.");
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (!depthTarget)
        {
            return reportError("failed to allocate capture depth texture.");
        }

        const EnvMapMeshResources mesh = CreateEnvMapMesh(rhi);
        RHICommandList cmdList(&rhi);
        const EnvMapSourceBinding sourceBinding = CreateSourceBinding(&environmentCube);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            convolutionShader.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);

        GLint previousViewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, previousViewport);

        const Matrix4 captureProjection =
            glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
        {
            const Matrix4 captureView = glm::lookAt(
                Vector3(0.0f),
                kCaptureTargets[faceIndex],
                kCaptureUps[faceIndex]);

            BeginCubeFaceRenderPass(
                cmdList,
                irradianceCube.get(),
                faceIndex,
                0,
                depthTarget.get(),
                faceSize);

            EnvCaptureFrameUBO frameData{};
            frameData.Projection = captureProjection;
            frameData.View = captureView;
            cmdList.SetGraphicsPipelineState(drawResources.PipelineState.get());
            if (RHIBindingSetRef bindingSet = CreateEnvCaptureBindingSet(
                    cmdList, drawResources, sourceBinding.SourceSRV.get(), frameData))
            {
                cmdList.SetBindingSet(EngineShaderBindings::kSetEnvCapture, bindingSet.get());
            }
            DrawEnvMapMesh(cmdList, mesh);
            cmdList.EndRenderPass();
        }

        rhi.SetViewport(
            static_cast<uint32_t>(previousViewport[0]),
            static_cast<uint32_t>(previousViewport[1]),
            static_cast<uint32_t>(previousViewport[2]),
            static_cast<uint32_t>(previousViewport[3]));

        const GLuint cubeTextureId = GetOpenGLTextureId(irradianceCube.get());
        if (cubeTextureId != 0)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTextureId);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }

        ME_CORE_INFO(
            "EnvMapCapture: convolved irradiance cubemap {}x{} from environment.",
            faceSize,
            faceSize);

        return TextureCubeLoader::WrapTextureCube(std::move(irradianceCube), faceSize, channels);
    }

    std::shared_ptr<TextureCube> EnvMapCapture::PrefilterEnvironment(
        RHI& rhi,
        RHITexture& environmentCube,
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
            return reportError("prefilter faceSize must be > 0.");
        }

        const TextureFormat cubeFormat = environmentCube.GetDesc().Format;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError("environment cubemap must be RGB16F or RGBA16F.");
        }

        const uint32_t environmentResolution = environmentCube.GetDesc().Width;
        if (environmentResolution == 0)
        {
            return reportError("environment cubemap has zero width.");
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> prefilterShader = EngineShaderUtils::CreateShaderFromFiles(
            rhi,
            shaderDirectory / "prefilter.vert",
            shaderDirectory / "prefilter.frag",
            outError);
        if (!prefilterShader || !prefilterShader->IsValid())
        {
            return reportError("failed to compile prefilter shader.");
        }

        const uint32_t maxMipLevel = kMaterialPBRMaxReflectionLod;
        RHITextureRef prefilterCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat, maxMipLevel + 1);
        if (!prefilterCube)
        {
            return reportError("failed to allocate prefilter cubemap.");
        }

        const GLuint prefilterTextureId = GetOpenGLTextureId(prefilterCube.get());
        if (prefilterTextureId == 0)
        {
            return reportError("prefilter cubemap has no GL texture id.");
        }

        AllocateRgb16fCubemapMipChain(prefilterTextureId, faceSize, maxMipLevel);

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (!depthTarget)
        {
            return reportError("failed to allocate capture depth texture.");
        }

        const EnvMapMeshResources mesh = CreateEnvMapMesh(rhi);
        RHICommandList cmdList(&rhi);
        const EnvMapSourceBinding sourceBinding = CreateSourceBinding(&environmentCube);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            prefilterShader.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);

        GLint previousViewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, previousViewport);

        const Matrix4 captureProjection =
            glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        for (uint32_t mip = 0; mip <= maxMipLevel; ++mip)
        {
            const uint32_t mipWidth = std::max(1u, faceSize >> mip);
            const float roughness =
                maxMipLevel > 0 ? static_cast<float>(mip) / static_cast<float>(maxMipLevel) : 0.0f;

            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                const Matrix4 captureView = glm::lookAt(
                    Vector3(0.0f),
                    kCaptureTargets[faceIndex],
                    kCaptureUps[faceIndex]);

                BeginCubeFaceRenderPass(
                    cmdList,
                    prefilterCube.get(),
                    faceIndex,
                    static_cast<uint8_t>(mip),
                    depthTarget.get(),
                    mipWidth);

                EnvCaptureFrameUBO frameData{};
                frameData.Projection = captureProjection;
                frameData.View = captureView;
                frameData.Roughness = roughness;
                frameData.EnvironmentResolution = static_cast<float>(environmentResolution);
                cmdList.SetGraphicsPipelineState(drawResources.PipelineState.get());
                if (RHIBindingSetRef bindingSet = CreateEnvCaptureBindingSet(
                        cmdList, drawResources, sourceBinding.SourceSRV.get(), frameData))
                {
                    cmdList.SetBindingSet(EngineShaderBindings::kSetEnvCapture, bindingSet.get());
                }
                DrawEnvMapMesh(cmdList, mesh);
                cmdList.EndRenderPass();
            }
        }

        rhi.SetViewport(
            static_cast<uint32_t>(previousViewport[0]),
            static_cast<uint32_t>(previousViewport[1]),
            static_cast<uint32_t>(previousViewport[2]),
            static_cast<uint32_t>(previousViewport[3]));

        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterTextureId);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        ME_CORE_INFO(
            "EnvMapCapture: prefiltered environment cubemap {}x{} ({} mips) from environment.",
            faceSize,
            faceSize,
            PrefilterMipLevelCount());

        return TextureCubeLoader::WrapTextureCube(std::move(prefilterCube), faceSize, channels);
    }
}
