#include "EnvMapCapture.h"

#include "../EngineShaderUtils.h"
#include "../EnginePassUniforms.h"
#include "../TextureCubeLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIClipSpace.h"
#include "Runtime/Function/Render/RHI/RHIClipSpaceCapabilities.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIPipelineLayout.h"
#include "Runtime/Function/Render/RHI/RHIRenderPass.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <algorithm>
#include <vector>

// Offline bake path: modern RHI only (CommandList / PSO / SRV / GenerateMips).

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

        struct EnvMapMeshResources
        {
            RHIBufferRef VertexBuffer;
            RHIVertexInputLayoutRef VertexLayout;
            uint32_t VertexCount = 0;
        };

        /**
         * One bake is a self-contained burst of draws that has to have run before the bake returns:
         * the caller hands back the cubemap this burst wrote and never sees the commands.
         *
         * In a recording topology a list only accepts commands between BeginRecording and Replay, so
         * the burst opens that window here and closes it where the bake says it is done. Immediate
         * mode runs each command as it is issued and the window is a no-op.
         *
         * The burst stays on this thread's command buffer rather than borrowing a pooled list: a
         * pooled list is submitted to the RHI role and replayed there, so the caller would get a
         * cubemap back before anything had drawn into it.
         */
        class ScopedBakeRecording
        {
        public:
            explicit ScopedBakeRecording(RHICommandList& cmdList)
                : m_CmdList(cmdList)
                , m_bImmediate(cmdList.IsImmediate())
            {
                if (!m_bImmediate)
                {
                    m_CmdList.BeginRecording();
                }
            }

            ~ScopedBakeRecording() { Finish(); }

            ScopedBakeRecording(const ScopedBakeRecording&) = delete;
            ScopedBakeRecording& operator=(const ScopedBakeRecording&) = delete;

            /**
             * Runs the burst. Explicit at the point the bake stops recording, because what follows
             * it - handing the result over, submitting the command buffer - must not overtake the
             * commands; the destructor is only there for a path that returns before this call.
             */
            void Finish()
            {
                if (m_bFinished)
                {
                    return;
                }
                m_bFinished = true;
                if (m_bImmediate)
                {
                    return;
                }
                m_CmdList.FinishRecording();
                m_CmdList.Replay();
            }

        private:
            RHICommandList& m_CmdList;
            bool m_bImmediate = true;
            bool m_bFinished = false;
        };

        /**
         * Why a handle this bake has to draw into cannot be used, and whether trying again helps.
         *
         * A handle created on the graphics role is complete when the call returns, so a caller that
         * still sees one of these states is drawing with something the create path deferred: naming
         * it in a draw would name nothing, and the cubemap would come back empty with no error.
         */
        std::string DescribeUnusableHandle(
            const char* what,
            const RHIDeferredCreateTarget* handle,
            EEnvMapCaptureStatus& outStatus)
        {
            if (handle == nullptr)
            {
                outStatus = EEnvMapCaptureStatus::Failed;
                return std::string(what) + " was not created: the device returned no object for it";
            }
            if (handle->HasNativeCreateFailed())
            {
                outStatus = EEnvMapCaptureStatus::Failed;
                return std::string(what) + " has no native object: the device refused to create it";
            }
            outStatus = EEnvMapCaptureStatus::RetryLater;
            return std::string(what) + " is not created yet: it is still queued for the graphics role";
        }

        EnvMapMeshResources CreateEnvMapMesh(RHICommandList& cmdList)
        {
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

        /**
         * Draws the capture cube through a packet instead of raw binds.
         *
         * A bake records into a stream when the frame roles are running, and a recorded command
         * captures the address it was given, not the object behind it. The packet carries the handles
         * for the geometry the draw names - the same way it carries them for the pipeline state and
         * the bindings - so the stream is what keeps the cube alive until it is replayed.
         */
        void SubmitEnvMapMesh(
            RHICommandList& cmdList,
            const EnvMapMeshResources& mesh,
            const RHIGraphicsPipelineStateRef& pipelineState,
            const RHIShaderBindingSetRef& bindingSet)
        {
            MeshDrawPacket packet;
            packet.PipelineState = pipelineState;
            packet.ShaderBindingSets[EngineShaderBindings::kSetEnvCapture] = bindingSet;
            packet.VertexBufferRef = mesh.VertexBuffer;
            packet.VertexBuffer = mesh.VertexBuffer.get();
            packet.VertexInputLayoutRef = mesh.VertexLayout;
            packet.VertexInputLayout = mesh.VertexLayout.get();
            packet.VertexCount = mesh.VertexCount;
            cmdList.SubmitMeshDrawPacket(packet);
        }

        void SubmitVulkanImmediateCommandsBeforeResourceDestroy(RHI& rhi)
        {
            if (RHIBackendSelection::IsVulkan())
            {
                rhi.RHIEndImmediateCommands();
            }
        }

        void TransitionTextureToShaderRead(RHICommandList& cmdList, RHITexture* texture)
        {
            if (texture == nullptr)
            {
                return;
            }

            RHITextureTransitionInfo transition{};
            transition.Texture = texture;
            cmdList.Transition(transition);
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
            // Bake cubemap faces without the scene-path Vulkan Y-flip; otherwise ±Y faces
            // end up rotated/split relative to samplerCube conventions.
            cmdList.SetViewport(0, 0, viewportSize, viewportSize, RHIViewportConvention::CubeMapFace);
        }

        struct EnvCaptureDrawResources
        {
            RHIGraphicsPipelineStateRef PipelineState;
            RHIPipelineLayoutRef PipelineLayout;
            RHIShaderBindingSetLayoutRef BindingLayout;
        };

        // Keep per-face UBOs + descriptor sets alive until the immediate CB has submitted.
        // Destroying them mid-loop (while still referenced by the recorded CB) causes DEVICE_LOST.
        struct EnvCapturePendingBindings
        {
            std::vector<RHIBufferRef> FrameUniformBuffers;
            std::vector<RHIShaderBindingSetRef> BindingSets;
        };

        EnvCaptureDrawResources CreateEnvCaptureDrawResources(
            RHICommandList& cmdList,
            RHIShader* shader,
            RHIVertexInputLayout* vertexLayout,
            uint32_t sourceTextureUnit)
        {
            EnvCaptureDrawResources resources;

            resources.BindingLayout = cmdList.CreateShaderBindingSetLayout({
                {EngineShaderBindings::kEnvCapture_SourceSRV,
                 RHIShaderBindingType::TextureSRV,
                 sourceTextureUnit,
                 RHIGraphicsShaderStage::Pixel},
                {EngineShaderBindings::kEnvCapture_FrameData,
                 RHIShaderBindingType::UniformBuffer,
                 EngineShaderBindings::kGL_EnvCaptureFrameDataUBO,
                 RHIGraphicsShaderStage::Vertex},
            });
            resources.PipelineLayout = cmdList.CreatePipelineLayout({resources.BindingLayout.get()});

            RHIGraphicsPSODesc psoDesc;
            psoDesc.PipelineLayout = resources.PipelineLayout.get();
            psoDesc.VertexShader = shader;
            psoDesc.PixelShader = shader;
            psoDesc.VertexInputLayout = vertexLayout;
            psoDesc.DepthStencilState.bDepthTestEnabled = true;
            psoDesc.DepthStencilState.bDepthWriteEnabled = true;
            psoDesc.BlendState.bBlendEnabled = false;
            resources.PipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
            return resources;
        }

        /** The set for one face, as the handle the packet and the pending list both hold. */
        RHIShaderBindingSetRef AppendEnvCaptureBindingSet(
            RHICommandList& cmdList,
            EnvCaptureDrawResources& drawResources,
            EnvCapturePendingBindings& pending,
            RHIShaderResourceView* sourceSrv,
            const EnvCaptureFrameUBO& frameData)
        {
            if (!drawResources.BindingLayout || sourceSrv == nullptr)
            {
                return nullptr;
            }

            RHIBufferCreateDesc frameDesc;
            frameDesc.Usage = RHIBufferUsage::Uniform;
            frameDesc.ByteSize = sizeof(EnvCaptureFrameUBO);
            RHIBufferRef frameUniformBuffer = cmdList.CreateBuffer(frameDesc, &frameData);
            if (!frameUniformBuffer)
            {
                return nullptr;
            }

            RHIShaderBindingSetRef bindingSet = cmdList.CreateShaderBindingSet(
                drawResources.BindingLayout.get(),
                {
                    {RHIShaderBindingType::TextureSRV, nullptr, sourceSrv},
                    {RHIShaderBindingType::UniformBuffer, frameUniformBuffer.get(), nullptr},
                });
            if (!bindingSet)
            {
                return nullptr;
            }

            pending.FrameUniformBuffers.push_back(std::move(frameUniformBuffer));
            pending.BindingSets.push_back(bindingSet);
            return bindingSet;
        }

        RHIShaderResourceViewRef CreateSourceSRV(RHICommandList& cmdList, RHITexture* sourceTexture)
        {
            RHITextureSRVDesc srvDesc;
            srvDesc.Texture = sourceTexture;
            return cmdList.CreateShaderResourceView(srvDesc);
        }
    }

    std::shared_ptr<TextureCube> EnvMapCapture::EquirectToCubemap(
        RHI& rhi,
        RHITexture& equirectTexture,
        const std::filesystem::path& engineDefaultAssetsRoot,
        uint32_t faceSize,
        std::string* outError,
        EEnvMapCaptureStatus* outStatus)
    {
        auto reportError = [&](const std::string& message, EEnvMapCaptureStatus status) -> std::shared_ptr<TextureCube>
        {
            ME_LOG(LogRender, Error, "EnvMapCapture: {}", message);
            if (outError)
            {
                *outError = message;
            }
            if (outStatus)
            {
                *outStatus = status;
            }
            SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
            return nullptr;
        };

        if (faceSize == 0)
        {
            return reportError("faceSize must be > 0.", EEnvMapCaptureStatus::Failed);
        }

        const TextureFormat cubeFormat = TextureFormat::RGB16F;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> captureShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
            rhi,
            shaderDirectory / "equirect_to_cubemap.vert",
            shaderDirectory / "equirect_to_cubemap.frag",
            outError);
        if (!captureShader)
        {
            return reportError(
                "failed to load equirect_to_cubemap SPIR-V shader.",
                EEnvMapCaptureStatus::Failed);
        }
        if (captureShader->HasNativeCreateFailed())
        {
            return reportError(
                "the driver refused the equirect_to_cubemap SPIR-V shader: " + captureShader->GetCompileLog(),
                EEnvMapCaptureStatus::Failed);
        }
        if (!captureShader->IsReady())
        {
            // The capture records its draws and reads the result back in one call, so it cannot wait
            // for a program that is only queued: that is a capture to retry later, not a shader that
            // failed to load.
            return reportError(
                "the equirect_to_cubemap program is not created yet; the capture has to run after the "
                "graphics role has created it.",
                EEnvMapCaptureStatus::RetryLater);
        }

        const uint32_t environmentMipCount =
            RHIBackendSelection::IsVulkan() ? 1u : ComputeTextureMipCount(faceSize);
        RHITextureRef environmentCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat, environmentMipCount);
        if (GetDeferredUse(environmentCube.get()) != RHIDeferredUse::Usable)
        {
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Failed;
            const std::string reason = DescribeUnusableHandle("the environment cubemap", environmentCube.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (GetDeferredUse(depthTarget.get()) != RHIDeferredUse::Usable)
        {
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Failed;
            const std::string reason = DescribeUnusableHandle("the capture depth texture", depthTarget.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }

        // Creating the mesh and the pipeline objects drives the device directly and is synchronous
        // on this thread; the recording window below is what the draws need.
        RHICommandList cmdList(&rhi);
        const EnvMapMeshResources mesh = CreateEnvMapMesh(cmdList);
        if (GetDeferredUse(mesh.VertexBuffer.get()) != RHIDeferredUse::Usable
            || GetDeferredUse(mesh.VertexLayout.get()) != RHIDeferredUse::Usable)
        {
            // The draw names this cube through its handles, and a handle whose native object is still
            // queued would name nothing: the bake would come back with an empty map instead of the
            // frame it skipped.
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::RetryLater;
            const std::string reason =
                DescribeUnusableHandle("the capture cube mesh", mesh.VertexBuffer.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }
        RHIShaderResourceViewRef sourceSrv = CreateSourceSRV(cmdList, &equirectTexture);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            captureShader.get(),
            mesh.VertexLayout.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);
        EnvCapturePendingBindings pendingBindings;

        const Matrix4 captureProjection = RHIClipSpace::MakePerspective(
            glm::radians(90.0f),
            1.0f,
            0.1f,
            10.0f);

        ScopedBakeRecording recording(cmdList);
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
            const RHIShaderBindingSetRef bindingSet = AppendEnvCaptureBindingSet(
                cmdList, drawResources, pendingBindings, sourceSrv.get(), frameData);
            SubmitEnvMapMesh(cmdList, mesh, drawResources.PipelineState, bindingSet);
            cmdList.EndRenderPass();
        }

        TransitionTextureToShaderRead(cmdList, environmentCube.get());
        cmdList.GenerateMips(environmentCube.get());
        recording.Finish();

        auto environment = TextureCubeLoader::WrapTextureCube(std::move(environmentCube), faceSize, channels);
        SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
        if (outStatus)
        {
            *outStatus = EEnvMapCaptureStatus::Succeeded;
        }
        return environment;
    }

    std::shared_ptr<TextureCube> EnvMapCapture::ConvolveIrradiance(
        RHI& rhi,
        RHITexture& environmentCube,
        const std::filesystem::path& engineDefaultAssetsRoot,
        uint32_t faceSize,
        std::string* outError,
        EEnvMapCaptureStatus* outStatus)
    {
        auto reportError = [&](const std::string& message, EEnvMapCaptureStatus status) -> std::shared_ptr<TextureCube>
        {
            ME_LOG(LogRender, Error, "EnvMapCapture: {}", message);
            if (outError)
            {
                *outError = message;
            }
            if (outStatus)
            {
                *outStatus = status;
            }
            SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
            return nullptr;
        };

        if (faceSize == 0)
        {
            return reportError("irradiance faceSize must be > 0.", EEnvMapCaptureStatus::Failed);
        }

        const TextureFormat cubeFormat = environmentCube.GetDesc().Format;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError(
                "environment cubemap must be RGB16F or RGBA16F.",
                EEnvMapCaptureStatus::Failed);
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> irradianceShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
            rhi,
            shaderDirectory / "irradiance_convolution.vert",
            shaderDirectory / "irradiance_convolution.frag",
            outError);
        if (!irradianceShader)
        {
            return reportError(
                "failed to load irradiance_convolution SPIR-V shader.",
                EEnvMapCaptureStatus::Failed);
        }
        if (irradianceShader->HasNativeCreateFailed())
        {
            return reportError(
                "the driver refused the irradiance_convolution SPIR-V shader: " + irradianceShader->GetCompileLog(),
                EEnvMapCaptureStatus::Failed);
        }
        if (!irradianceShader->IsReady())
        {
            // Queued rather than refused: a capture that runs later succeeds, and the caller backs
            // off instead of treating the map as broken.
            return reportError(
                "the irradiance_convolution program is not created yet; the capture has to run after the "
                "graphics role has created it.",
                EEnvMapCaptureStatus::RetryLater);
        }

        RHITextureRef irradianceCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat);
        if (GetDeferredUse(irradianceCube.get()) != RHIDeferredUse::Usable)
        {
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Failed;
            const std::string reason = DescribeUnusableHandle("the irradiance cubemap", irradianceCube.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (GetDeferredUse(depthTarget.get()) != RHIDeferredUse::Usable)
        {
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Failed;
            const std::string reason = DescribeUnusableHandle("the capture depth texture", depthTarget.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }

        // Creating the mesh and the pipeline objects drives the device directly and is synchronous
        // on this thread; the recording window below is what the draws need.
        RHICommandList cmdList(&rhi);
        const EnvMapMeshResources mesh = CreateEnvMapMesh(cmdList);
        if (GetDeferredUse(mesh.VertexBuffer.get()) != RHIDeferredUse::Usable
            || GetDeferredUse(mesh.VertexLayout.get()) != RHIDeferredUse::Usable)
        {
            // The draw names this cube through its handles, and a handle whose native object is still
            // queued would name nothing: the bake would come back with an empty map instead of the
            // frame it skipped.
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::RetryLater;
            const std::string reason =
                DescribeUnusableHandle("the capture cube mesh", mesh.VertexBuffer.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }
        RHIShaderResourceViewRef sourceSrv = CreateSourceSRV(cmdList, &environmentCube);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            irradianceShader.get(),
            mesh.VertexLayout.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);
        EnvCapturePendingBindings pendingBindings;

        const Matrix4 captureProjection = RHIClipSpace::MakePerspective(
            glm::radians(90.0f),
            1.0f,
            0.1f,
            10.0f);

        ScopedBakeRecording recording(cmdList);
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
            const RHIShaderBindingSetRef bindingSet = AppendEnvCaptureBindingSet(
                cmdList, drawResources, pendingBindings, sourceSrv.get(), frameData);
            SubmitEnvMapMesh(cmdList, mesh, drawResources.PipelineState, bindingSet);
            cmdList.EndRenderPass();
        }

        TransitionTextureToShaderRead(cmdList, irradianceCube.get());
        recording.Finish();

        ME_LOG(LogRender, Info, 
            "EnvMapCapture: convolved irradiance cubemap {}x{} from environment.",
            faceSize,
            faceSize);

        auto irradiance = TextureCubeLoader::WrapTextureCube(std::move(irradianceCube), faceSize, channels);
        SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
        if (outStatus)
        {
            *outStatus = EEnvMapCaptureStatus::Succeeded;
        }
        return irradiance;
    }

    std::shared_ptr<TextureCube> EnvMapCapture::PrefilterEnvironment(
        RHI& rhi,
        RHITexture& environmentCube,
        const std::filesystem::path& engineDefaultAssetsRoot,
        uint32_t faceSize,
        std::string* outError,
        EEnvMapCaptureStatus* outStatus)
    {
        auto reportError = [&](const std::string& message, EEnvMapCaptureStatus status) -> std::shared_ptr<TextureCube>
        {
            ME_LOG(LogRender, Error, "EnvMapCapture: {}", message);
            if (outError)
            {
                *outError = message;
            }
            if (outStatus)
            {
                *outStatus = status;
            }
            SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
            return nullptr;
        };

        if (faceSize == 0)
        {
            return reportError("prefilter faceSize must be > 0.", EEnvMapCaptureStatus::Failed);
        }

        const TextureFormat cubeFormat = environmentCube.GetDesc().Format;
        const uint32_t channels = ChannelsFromFormat(cubeFormat);
        if (channels == 0)
        {
            return reportError(
                "environment cubemap must be RGB16F or RGBA16F.",
                EEnvMapCaptureStatus::Failed);
        }

        const uint32_t environmentResolution = environmentCube.GetDesc().Width;
        if (environmentResolution == 0)
        {
            return reportError(
                "environment cubemap has zero width.",
                EEnvMapCaptureStatus::Failed);
        }

        const std::filesystem::path shaderDirectory =
            engineDefaultAssetsRoot / "Shaders" / "EnvMap";
        std::shared_ptr<RHIShader> prefilterShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
            rhi,
            shaderDirectory / "prefilter.vert",
            shaderDirectory / "prefilter.frag",
            outError);
        if (!prefilterShader)
        {
            return reportError(
                "failed to load prefilter SPIR-V shader.",
                EEnvMapCaptureStatus::Failed);
        }
        if (prefilterShader->HasNativeCreateFailed())
        {
            return reportError(
                "the driver refused the prefilter SPIR-V shader: " + prefilterShader->GetCompileLog(),
                EEnvMapCaptureStatus::Failed);
        }
        if (!prefilterShader->IsReady())
        {
            // Queued rather than refused: a capture that runs later succeeds, and the caller backs
            // off instead of treating the map as broken.
            return reportError(
                "the prefilter program is not created yet; the capture has to run after the "
                "graphics role has created it.",
                EEnvMapCaptureStatus::RetryLater);
        }

        const uint32_t maxMipLevel = kMaterialPBRMaxReflectionLod;
        RHITextureRef prefilterCube =
            TextureCubeLoader::CreateRenderTargetCube(rhi, faceSize, cubeFormat, maxMipLevel + 1);
        if (GetDeferredUse(prefilterCube.get()) != RHIDeferredUse::Usable)
        {
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Failed;
            const std::string reason = DescribeUnusableHandle("the prefilter cubemap", prefilterCube.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }

        RHITextureRef depthTarget = rhi.RHICreateTexture2D(MakeDepthTextureDesc(faceSize, faceSize), nullptr);
        if (GetDeferredUse(depthTarget.get()) != RHIDeferredUse::Usable)
        {
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::Failed;
            const std::string reason = DescribeUnusableHandle("the capture depth texture", depthTarget.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }

        // Creating the mesh and the pipeline objects drives the device directly and is synchronous
        // on this thread; the recording window below is what the draws need.
        RHICommandList cmdList(&rhi);
        const EnvMapMeshResources mesh = CreateEnvMapMesh(cmdList);
        if (GetDeferredUse(mesh.VertexBuffer.get()) != RHIDeferredUse::Usable
            || GetDeferredUse(mesh.VertexLayout.get()) != RHIDeferredUse::Usable)
        {
            // The draw names this cube through its handles, and a handle whose native object is still
            // queued would name nothing: the bake would come back with an empty map instead of the
            // frame it skipped.
            EEnvMapCaptureStatus status = EEnvMapCaptureStatus::RetryLater;
            const std::string reason =
                DescribeUnusableHandle("the capture cube mesh", mesh.VertexBuffer.get(), status);
            return reportError("nothing can be baked: " + reason + ".", status);
        }
        RHIShaderResourceViewRef sourceSrv = CreateSourceSRV(cmdList, &environmentCube);
        EnvCaptureDrawResources drawResources = CreateEnvCaptureDrawResources(
            cmdList,
            prefilterShader.get(),
            mesh.VertexLayout.get(),
            EngineShaderBindings::kGL_EnvCaptureSourceUnit);
        EnvCapturePendingBindings pendingBindings;

        const Matrix4 captureProjection = RHIClipSpace::MakePerspective(
            glm::radians(90.0f),
            1.0f,
            0.1f,
            10.0f);

        ScopedBakeRecording recording(cmdList);
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
                const RHIShaderBindingSetRef bindingSet = AppendEnvCaptureBindingSet(
                    cmdList, drawResources, pendingBindings, sourceSrv.get(), frameData);
                SubmitEnvMapMesh(cmdList, mesh, drawResources.PipelineState, bindingSet);
                cmdList.EndRenderPass();
            }
        }

        TransitionTextureToShaderRead(cmdList, prefilterCube.get());
        recording.Finish();

        ME_LOG(LogRender, Info, 
            "EnvMapCapture: prefiltered environment cubemap {}x{} ({} mips) from environment.",
            faceSize,
            faceSize,
            PrefilterMipLevelCount());

        auto prefilter = TextureCubeLoader::WrapTextureCube(std::move(prefilterCube), faceSize, channels);
        SubmitVulkanImmediateCommandsBeforeResourceDestroy(rhi);
        if (outStatus)
        {
            *outStatus = EEnvMapCaptureStatus::Succeeded;
        }
        return prefilter;
    }
}
