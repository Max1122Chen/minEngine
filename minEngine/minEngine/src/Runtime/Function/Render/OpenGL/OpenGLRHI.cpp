#include "OpenGLRHI.h"
#include "Runtime/Function/Render/WindowSystem.h"

#include "OpenGLRHIResources.h"
#include "OpenGLFunctionTable.h"

#include "Render/RHI/RHIShaderBinding.h"
#include "Log/LogSystem.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIResourceTransition.h"
#include "Render/RHI/RHIPipelineLayout.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Runtime/Core/Threading/ThreadIdentity.h"

#include "glad/glad.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <utility>

namespace minEngine
{
    namespace
    {
        /**
         * A draw names native objects, and a buffer whose GL name is 0 was never created: its creating
         * thread held no context, so the driver knows nothing about it. Binding such a name names no
         * data, and the driver answers the resulting draw by faulting rather than by raising an error.
         * Refusing the draw keeps that state away from the driver and reports it once per kind.
         */
        /**
         * Names the caller and its thread. Which thread drew is half the answer; the other half is
         * which thread created the nameless buffer, and that was reported once at creation with the
         * object pointer that appears here.
         */
        std::string DescribeDrawingThreadAndSite()
        {
            const ThreadIdentity identity = CaptureCurrentThreadIdentity();
            char header[256]{};
            std::snprintf(
                header,
                sizeof(header),
                " thread=%u role=%s",
                static_cast<unsigned>(identity.LogThreadId),
                identity.RoleName);

            std::string text(header);
            void* frames[kDiagnosticFrameCount]{};
            const std::size_t frameCount = CaptureDiagnosticFrames(frames, kDiagnosticFrameCount);
            // The first frames are this helper and the reporting function; the caller is the site.
            for (std::size_t frameIndex = 2; frameIndex < frameCount; ++frameIndex)
            {
                char location[320]{};
                FormatAddressForLog(location, sizeof(location), frames[frameIndex]);
                text += "\n    #" + std::to_string(frameIndex - 2) + " " + location;
            }
            return text;
        }

        void ReportDrawWithoutNativeBuffer(const char* kind, uint32_t count, const void* object)
        {
            static std::atomic<uint32_t> s_refusedDraws{ 0 };
            const uint32_t refused = s_refusedDraws.fetch_add(1, std::memory_order_relaxed) + 1;
            if (refused == 1 || refused % 100 == 0)
            {
                ME_LOG(
                    LogRHI,
                    Error,
                    "OpenGL draw skipped: the bound {} ({:p}) has no native name, so the draw would "
                    "name no data ({} requested). Such a buffer was created on a thread that held no "
                    "graphics context; the creation site was reported with its thread and call stack, "
                    "and the pointer here identifies that object. Drawing thread and call site:{} "
                    "{} draw(s) refused so far.",
                    kind,
                    object,
                    count,
                    DescribeDrawingThreadAndSite(),
                    refused);
            }
        }

        /**
         * A shader binding that names a resource whose native object does not exist yet.
         *
         * GL raises nothing for this: the binding simply leaves the unit pointing at whatever it
         * was pointing at before, so the draw samples the wrong image or reads a uniform block
         * that was never bound. The handle reports IsReady() false, and a resource created off the
         * graphics role is ready from the next render frame on, so the only way to land here is a
         * draw that used a handle between its creation and that frame start. It is reported
         * instead of being left to look like a rendering bug.
         */
        void ReportBindingWithoutNativeObject(const char* kind, const void* object)
        {
            static std::atomic<uint32_t> s_reported{ 0 };
            const uint32_t reported = s_reported.fetch_add(1, std::memory_order_relaxed) + 1;
            if (reported == 1 || reported % 100 == 0)
            {
                ME_LOG(
                    LogRHI,
                    Warn,
                    "OpenGL shader binding names a {} ({:p}) with no native object: the binding is "
                    "skipped and the draw keeps whatever that unit held. {} such binding(s) so far.{}",
                    kind,
                    object,
                    reported,
                    DescribeDrawingThreadAndSite());
            }
        }

        GLenum ToGLDepthFunc(RHIDepthCompareFunc compare)
        {
            switch (compare)
            {
            case RHIDepthCompareFunc::LessEqual:
                return GL_LEQUAL;
            case RHIDepthCompareFunc::Always:
                return GL_ALWAYS;
            case RHIDepthCompareFunc::Less:
            default:
                return GL_LESS;
            }
        }

        GLenum ToGLPrimitiveType(RHIPrimitiveType primitiveType)
        {
            switch (primitiveType)
            {
            case RHIPrimitiveType::LineList:
                return GL_LINES;
            case RHIPrimitiveType::TriangleStrip:
                return GL_TRIANGLE_STRIP;
            case RHIPrimitiveType::TriangleList:
            default:
                return GL_TRIANGLES;
            }
        }

        GLenum GetBoundPrimitiveType(RHIGraphicsPipelineState* boundPipeline)
        {
            if (boundPipeline == nullptr)
            {
                return GL_TRIANGLES;
            }

            if (auto* fallback = static_cast<RHIGraphicsPSOStateFallback*>(boundPipeline))
            {
                return ToGLPrimitiveType(fallback->GetDesc().PrimitiveType);
            }

            return GL_TRIANGLES;
        }

    }

    void OpenGLRHI::Initialize()
    {
        // The engine's glad table is initialized here for every path that drives rendering
        // through the RHI. Before glad left the export table that happened implicitly - the
        // consumer linked the engine's exported gladLoadGLLoader, so its load call initialized
        // THIS module's copy. See OpenGLFunctionTable.h for the contract.
        EnsureOpenGLFunctionTableLoaded();
        m_WindowSystem = &WindowSystem::Get();
        m_DefaultLinearSampler = RHIGetOrCreateSampler(MakeLinearRepeatSamplerDesc());
        ME_LOG(LogRHI, Info, "OpenGLRHI Initialized");
    }

    void OpenGLRHI::Shutdown()
    {
        // Waiting for another submit is a liveness rule, not a teardown rule: everything still
        // queued goes now, while the context this backend was drawing with may still be current.
        RHIDrainAllDeferredDestroys();
        m_DefaultLinearSampler.reset();
        m_SamplerCache.clear();
        m_WindowSystem = nullptr;
        ME_LOG(LogRHI, Info, "OpenGLRHI Shutdown");
    }

    void OpenGLRHI::RHISetBackbufferClearColor(const Vector3& color)
    {
        if (m_WindowSystem)
        {
            m_WindowSystem->SetClearColor(color);
        }
    }

    void OpenGLRHI::RHIClearBackbuffer()
    {
        if (m_WindowSystem)
        {
            m_WindowSystem->Clear();
        }
    }

    void OpenGLRHI::RHIPresent()
    {
        if (m_WindowSystem)
        {
            m_WindowSystem->SwapBuffers();
        }

        // This backend hands every command to the driver as it is issued, so the frame is with the
        // platform by the time SwapBuffers returns (and by now when there is no window). That - not
        // an elapsed frame count - is what makes the handles retired before it deletable. It also
        // runs on whichever thread owns the drawing context: the combined graphics thread in Multi,
        // the only frame thread in Single.
        RHISubmitBatchCommitted();
    }

    

    namespace
    {
        bool ShouldClearColor(RHIRenderTargetActions action)
        {
            return GetLoadAction(action) == RHIRenderTargetLoadAction::Clear;
        }

        bool ShouldClearDepth(RHIDepthStencilTargetActions action)
        {
            return action == RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil ||
                   action == RHIDepthStencilTargetActions::ClearDepthStencilDontStore;
        }
    }

    void OpenGLRHI::DestroyTransientFramebuffer()
    {
        EnsureOpenGLFunctionTableLoaded();
        if (m_OwnsTransientFramebuffer && m_TransientFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_TransientFramebuffer);
        }
        m_TransientFramebuffer = 0;
        m_OwnsTransientFramebuffer = false;
    }

    std::shared_ptr<RHITexture> OpenGLRHI::RHICreateTexture2D(const RHITextureCreateDesc& desc, const void* initialData)
    {
        // Off the graphics role the pixels are copied out of the caller's memory now and the
        // driver object is created at the start of the next render frame. On it, nothing changes.
        auto texture = RHIGraphicsRole::IsCurrent()
            ? std::make_shared<OpenGLRHITexture>(desc, initialData)
            : std::make_shared<OpenGLRHITexture>(desc, initialData, DeferredNativeCreate{});
        return ScheduleNativeCreate(std::move(texture), "openGL texture");
    }

    std::shared_ptr<RHIShaderResourceView> OpenGLRHI::RHICreateShaderResourceView(const RHITextureSRVDesc& desc)
    {
        if (!desc.Texture)
        {
            return nullptr;
        }
        return std::make_shared<OpenGLRHIShaderResourceView>(desc);
    }

    std::shared_ptr<RHIBuffer> OpenGLRHI::RHICreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData)
    {
        auto buffer = RHIGraphicsRole::IsCurrent()
            ? std::make_shared<OpenGLRHIBuffer>(desc, initialData)
            : std::make_shared<OpenGLRHIBuffer>(desc, initialData, DeferredNativeCreate{});
        return ScheduleNativeCreate(std::move(buffer), "openGL buffer");
    }

    namespace
    {
        /**
         * What a caller that could not see the driver gets to read. The program exists as a handle
         * only; its log is filled in by the graphics role, so an empty string here does not mean
         * "compiled fine" but "nothing has run yet".
         */
        const char* const kProgramQueuedLog =
            "Shader program queued for the graphics role; the driver log becomes readable on the "
            "handle once it has been created there.";

        void PublishCompileLog(const OpenGLRHIShader& shader, std::string* outCompileLog)
        {
            if (outCompileLog == nullptr)
            {
                return;
            }

            *outCompileLog = shader.GetCompileLog();
            if (outCompileLog->empty() && !shader.IsReady() && !shader.HasNativeCreateFailed())
            {
                *outCompileLog = kProgramQueuedLog;
            }
        }
    }

    std::shared_ptr<RHIShader> OpenGLRHI::RHICreateShader(
        const RHIShaderCreateDesc& desc,
        std::string* outCompileLog)
    {
        // Off the graphics role the bytecode is copied and both stages are specialized at the
        // start of the next render frame; on it, nothing changes and the result is final here.
        const bool bOnGraphicsRole = RHIGraphicsRole::IsCurrent();
        auto shader = bOnGraphicsRole
            ? std::make_shared<OpenGLRHIShader>(desc)
            : std::make_shared<OpenGLRHIShader>(desc, DeferredNativeCreate{});
        PublishCompileLog(*shader, outCompileLog);

        // A program that failed where it could be created is still a failure: no handle is handed
        // out for it. A queued one is a handle whose program does not exist yet.
        if (bOnGraphicsRole && !shader->IsValid())
        {
            return nullptr;
        }

        return ScheduleNativeCreate(std::move(shader), "openGL shader program");
    }

    std::shared_ptr<RHIShader> OpenGLRHI::RHICreateShader(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outCompileLog)
    {
        const bool bOnGraphicsRole = RHIGraphicsRole::IsCurrent();
        auto shader = bOnGraphicsRole
            ? std::make_shared<OpenGLRHIShader>(vertexSource, fragmentSource)
            : std::make_shared<OpenGLRHIShader>(vertexSource, fragmentSource, DeferredNativeCreate{});
        PublishCompileLog(*shader, outCompileLog);

        if (bOnGraphicsRole && !shader->IsValid())
        {
            return nullptr;
        }

        return ScheduleNativeCreate(std::move(shader), "openGL shader program");
    }

    std::shared_ptr<RHIGraphicsPipelineState> OpenGLRHI::RHICreateGraphicsPipelineState(const RHIGraphicsPSODesc& desc)
    {
        if (!desc.PipelineLayout)
        {
            ME_LOG(LogRHI, Warn, "RHICreateGraphicsPipelineState: PipelineLayout is null");
        }
        // This backend binds the pipeline piece by piece, so the value is a description and owns
        // no driver object: it is ready where it is built and never enters the create queue.
        return ScheduleNativeCreate(
            std::make_shared<RHIGraphicsPSOStateFallback>(desc),
            "openGL pipeline state");
    }

    std::shared_ptr<RHIShaderBindingSetLayout> OpenGLRHI::RHICreateShaderBindingSetLayout(const std::vector<RHIShaderBindingSetLayoutEntry>& entries)
    {
        // A table, not a driver object: ready at once, so the channel passes it straight through.
        return ScheduleNativeCreate(
            std::make_shared<OpenGLRHIShaderBindingSetLayout>(entries),
            "openGL shader binding set layout");
    }

    std::shared_ptr<RHIPipelineLayout> OpenGLRHI::RHICreatePipelineLayout(
        const std::vector<RHIShaderBindingSetLayout*>& setLayouts)
    {
        return std::make_shared<OpenGLRHIPipelineLayout>(setLayouts);
    }

    std::shared_ptr<RHIShaderBindingSet> OpenGLRHI::RHICreateShaderBindingSet(
        RHIShaderBindingSetLayout* layout,
        const std::vector<RHIShaderBinding>& resources)
    {
        return ScheduleNativeCreate(
            std::make_shared<OpenGLRHIShaderBindingSet>(layout, resources),
            "openGL shader binding set");
    }

    std::shared_ptr<RHIVertexInputLayout> OpenGLRHI::RHICreateVertexInputLayout(
        std::initializer_list<RHIVertexElement> elements)
    {
        auto layout = RHIGraphicsRole::IsCurrent()
            ? std::make_shared<OpenGLRHIVertexInputLayout>(elements)
            : std::make_shared<OpenGLRHIVertexInputLayout>(elements, DeferredNativeCreate{});
        return ScheduleNativeCreate(std::move(layout), "openGL vertex input layout");
    }

    void OpenGLRHI::ApplyGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        EnsureOpenGLFunctionTableLoaded();
        m_BoundPipeline = pipelineState;
        auto* fallback = static_cast<RHIGraphicsPSOStateFallback*>(pipelineState);
        if (!fallback)
        {
            return;
        }

        const RHIGraphicsPSODesc& desc = fallback->GetDesc();

        // Set shader program
        if (auto* vs = static_cast<OpenGLRHIShader*>(desc.VertexShader))
        {
            glUseProgram(vs->GetProgramId());
        }

        // Set vertex input layout
        if (desc.VertexInputLayout)
        {
            m_BoundVertexLayout = static_cast<OpenGLRHIVertexInputLayout*>(desc.VertexInputLayout);
            if (m_BoundVertexLayout)
            {
                glBindVertexArray(m_BoundVertexLayout->GetVertexArrayId());
            }
        }

        // Set blend state
        if (desc.BlendState.bBlendEnabled)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquation(GL_FUNC_ADD);
        }
        else
        {
            glDisable(GL_BLEND);
        }

        // Set depth test
        if (desc.DepthStencilState.bDepthTestEnabled)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(desc.DepthStencilState.bDepthWriteEnabled ? GL_TRUE : GL_FALSE);
        glDepthFunc(ToGLDepthFunc(desc.DepthStencilState.DepthCompare));

        // Set rasterizer state
        if (!desc.RasterizerState.bCullEnabled || desc.RasterizerState.CullMode == RHICullMode::None)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            switch (desc.RasterizerState.CullMode)
            {
            case RHICullMode::Front:
                glCullFace(GL_FRONT);
                break;
            case RHICullMode::Back:
            default:
                glCullFace(GL_BACK);
                break;
            }
        }

        if (desc.RasterizerState.DepthBiasSlopeScale != 0.0f
            || desc.RasterizerState.DepthBiasConstant != 0.0f)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(
                desc.RasterizerState.DepthBiasSlopeScale,
                desc.RasterizerState.DepthBiasConstant);
        }
        else
        {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    
        // Set primitive type
        switch (desc.PrimitiveType)
        {
        case RHIPrimitiveType::TriangleList:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        case RHIPrimitiveType::TriangleStrip:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        case RHIPrimitiveType::LineList:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        }

        ReapplyBoundShaderBindingSets();
    }

    void OpenGLRHI::ApplyShaderBindingSetResources(RHIShaderBindingSet* bindingSet)
    {
        EnsureOpenGLFunctionTableLoaded();
        auto* glSet = static_cast<OpenGLRHIShaderBindingSet*>(bindingSet);
        if (!glSet)
        {
            return;
        }

        const RHIShaderBindingSetLayout* layout = glSet->GetLayout();
        const std::vector<RHIShaderBinding>& resources = glSet->GetBindings();
        if (!layout)
        {
            return;
        }

        const std::vector<RHIShaderBindingSetLayoutEntry>& entries = layout->GetEntries();
        const size_t bindCount = std::min(resources.size(), entries.size());
        for (size_t i = 0; i < bindCount; ++i)
        {
            const RHIShaderBinding& resource = resources[i];
            const RHIShaderBindingSetLayoutEntry& entry = entries[i];

            if (resource.Type == RHIShaderBindingType::TextureSRV && resource.TextureSRV)
            {
                const RHITextureSRVDesc& srvDesc = resource.TextureSRV->GetCreateDesc();
                if (!srvDesc.Texture)
                {
                    continue;
                }

                GLuint texId = GetOpenGLTextureId(srvDesc.Texture);
                if (texId == 0)
                {
                    ReportBindingWithoutNativeObject("texture", srvDesc.Texture);
                    continue;
                }

                GLenum target = GL_TEXTURE_2D;
                if (auto* glTexture = static_cast<OpenGLRHITexture*>(srvDesc.Texture))
                {
                    target = glTexture->GetTextureTarget();
                }

                glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(entry.ShaderBinding));
                glBindTexture(target, texId);

                GLuint samplerId = 0;
                if (resource.Sampler != nullptr)
                {
                    if (auto* glSampler = static_cast<OpenGLRHISampler*>(resource.Sampler))
                    {
                        samplerId = glSampler->GetSamplerId();
                    }
                }
                else if (m_DefaultLinearSampler)
                {
                    if (auto* glSampler = static_cast<OpenGLRHISampler*>(m_DefaultLinearSampler.get()))
                    {
                        samplerId = glSampler->GetSamplerId();
                    }
                }
                glBindSampler(static_cast<GLuint>(entry.ShaderBinding), samplerId);
            }
            else if (resource.Type == RHIShaderBindingType::UniformBuffer && resource.Buffer)
            {
                auto* ubo = static_cast<OpenGLRHIBuffer*>(resource.Buffer);
                if (ubo)
                {
                    if (ubo->GetBufferId() == 0)
                    {
                        ReportBindingWithoutNativeObject("uniform buffer", ubo);
                        continue;
                    }
                    if (resource.BufferRange != 0)
                    {
                        glBindBufferRange(
                            GL_UNIFORM_BUFFER,
                            entry.ShaderBinding,
                            ubo->GetBufferId(),
                            static_cast<GLintptr>(resource.BufferOffset),
                            static_cast<GLsizeiptr>(resource.BufferRange));
                    }
                    else
                    {
                        glBindBufferBase(
                            GL_UNIFORM_BUFFER,
                            entry.ShaderBinding,
                            ubo->GetBufferId());
                    }
                }
            }
        }
    }

    void OpenGLRHI::ReapplyBoundShaderBindingSets()
    {
        for (uint32_t setIndex = 0; setIndex < kMaxShaderBindingSets; ++setIndex)
        {
            if (m_BoundShaderBindingSets[setIndex])
            {
                ApplyShaderBindingSetResources(m_BoundShaderBindingSets[setIndex]);
            }
        }
    }

    void OpenGLRHI::RHICmdBeginRenderPass(const RHIRenderPassInfo& info)
    {
        EnsureOpenGLFunctionTableLoaded();
        m_BoundShaderBindingSets = {};
        DestroyTransientFramebuffer();

        const RHIRenderPassInfo::ColorAttachment& color0 = info.ColorAttachments[0];
        const bool hasColor = color0.RenderTarget != nullptr;
        const bool hasDepth = info.DepthStencil.DepthStencilTarget != nullptr;

        GLuint fbo = 0;
        if (hasColor || hasDepth)
        {
            glGenFramebuffers(1, &fbo);
            m_TransientFramebuffer = fbo;
            m_OwnsTransientFramebuffer = true;
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            if (hasColor)
            {
                auto* colorTex = static_cast<OpenGLRHITexture*>(color0.RenderTarget);
                if (colorTex->GetTextureTarget() == GL_TEXTURE_2D_ARRAY &&
                    color0.ArraySlice >= 0)
                {
                    glFramebufferTextureLayer(
                        GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        colorTex->GetTextureId(),
                        color0.MipIndex,
                        color0.ArraySlice);
                }
                else if (colorTex->GetTextureTarget() == GL_TEXTURE_CUBE_MAP &&
                         color0.ArraySlice >= 0)
                {
                    const GLenum cubeFace =
                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(color0.ArraySlice);
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        cubeFace,
                        colorTex->GetTextureId(),
                        color0.MipIndex);
                }
                else
                {
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        colorTex->GetTextureTarget(),
                        colorTex->GetTextureId(),
                        color0.MipIndex);
                }
                GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
                glDrawBuffers(1, drawBuffers);
            }
            else
            {
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
            }

            if (hasDepth)
            {
                auto* depthTex = static_cast<OpenGLRHITexture*>(info.DepthStencil.DepthStencilTarget);
                if (depthTex->GetTextureTarget() == GL_TEXTURE_2D_ARRAY &&
                    info.DepthStencil.ArraySlice >= 0)
                {
                    glFramebufferTextureLayer(
                        GL_FRAMEBUFFER,
                        GL_DEPTH_ATTACHMENT,
                        depthTex->GetTextureId(),
                        info.DepthStencil.MipIndex,
                        info.DepthStencil.ArraySlice);
                }
                else if (depthTex->GetTextureTarget() == GL_TEXTURE_CUBE_MAP &&
                         info.DepthStencil.ArraySlice >= 0)
                {
                    const GLenum cubeFace =
                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(info.DepthStencil.ArraySlice);
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER,
                        GL_DEPTH_ATTACHMENT,
                        cubeFace,
                        depthTex->GetTextureId(),
                        info.DepthStencil.MipIndex);
                }
                else
                {
                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER,
                        GL_DEPTH_ATTACHMENT,
                        depthTex->GetTextureTarget(),
                        depthTex->GetTextureId(),
                        info.DepthStencil.MipIndex);
                }
            }

            const GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
            {
                ME_LOG(LogRHI, Error, 
                    "RHICmdBeginRenderPass: framebuffer incomplete (status=0x{:X}, color={}, depth={}).",
                    static_cast<unsigned int>(fboStatus),
                    hasColor,
                    hasDepth);
                DestroyTransientFramebuffer();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                return;
            }
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDrawBuffer(GL_BACK);
            glReadBuffer(GL_BACK);
        }

        GLbitfield clearMask = 0;
        if (hasColor && ShouldClearColor(color0.Action))
        {
            glClearColor(
                info.ClearValue.Color[0],
                info.ClearValue.Color[1],
                info.ClearValue.Color[2],
                info.ClearValue.Color[3]);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
        if (hasDepth && ShouldClearDepth(info.DepthStencil.Action))
        {
            // glClear depth is skipped when GL_DEPTH_WRITEMASK is false (e.g. after SkyBox / translucency).
            glDepthMask(GL_TRUE);
            glClearDepth(info.ClearValue.Depth);
            clearMask |= GL_DEPTH_BUFFER_BIT;
        }
        if (clearMask != 0)
        {
            glClear(clearMask);
        }
    }

    void OpenGLRHI::RHICmdEndRenderPass()
    {
        EnsureOpenGLFunctionTableLoaded();
        DestroyTransientFramebuffer();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDepthMask(GL_FALSE);
    }

    void OpenGLRHI::RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        ApplyGraphicsPipelineState(pipelineState);
    }

    void OpenGLRHI::RHICmdSetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet)
    {
        if (setIndex >= kMaxShaderBindingSets)
        {
            ME_LOG(LogRHI, Warn, "RHICmdSetShaderBindingSet: setIndex {} out of range", setIndex);
            return;
        }

        if (bindingSet && m_BoundPipeline)
        {
            if (auto* fallback = static_cast<RHIGraphicsPSOStateFallback*>(m_BoundPipeline))
            {
                RHIPipelineLayout* pipelineLayout = fallback->GetDesc().PipelineLayout;
                if (pipelineLayout && setIndex >= pipelineLayout->GetShaderBindingSetLayoutCount())
                {
                    ME_LOG(LogRHI, Warn, 
                        "RHICmdSetShaderBindingSet: setIndex {} exceeds pipeline layout set count {}",
                        setIndex,
                        pipelineLayout->GetShaderBindingSetLayoutCount());
                    return;
                }

                if (pipelineLayout)
                {
                    RHIShaderBindingSetLayout* expectedLayout = pipelineLayout->GetShaderBindingSetLayout(setIndex);
                    if (expectedLayout && bindingSet->GetLayout() != expectedLayout)
                    {
                        ME_LOG(LogRHI, Warn, "RHICmdSetShaderBindingSet: binding layout mismatch at set {}", setIndex);
                    }
                }
            }
        }

        m_BoundShaderBindingSets[setIndex] = bindingSet;
        ApplyShaderBindingSetResources(bindingSet);
    }

    void OpenGLRHI::RHICmdTransition(const RHITextureTransitionInfo& transition)
    {
        (void)transition;
    }

    void OpenGLRHI::RHICmdSetViewport(
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        bool flipY)
    {
        EnsureOpenGLFunctionTableLoaded();
        (void)flipY;
        glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }

    uint32_t OpenGLRHI::RHIGetMinUniformBufferOffsetAlignment() const
    {
        return 256;
    }


    void OpenGLRHI::RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot)
    {
        EnsureOpenGLFunctionTableLoaded();
        (void)slot;
        m_BoundVertexBuffer = static_cast<OpenGLRHIBuffer*>(vertexBuffer);
        if (!m_BoundVertexBuffer)
        {
            return;
        }

        if (m_BoundVertexLayout)
        {
            m_BoundVertexLayout->BindVertexBuffer(m_BoundVertexBuffer->GetBufferId());
            return;
        }

        glBindBuffer(m_BoundVertexBuffer->GetBindingTarget(), m_BoundVertexBuffer->GetBufferId());
    }

    void OpenGLRHI::RHICmdSetIndexBuffer(RHIBuffer* indexBuffer)
    {
        EnsureOpenGLFunctionTableLoaded();
        m_BoundIndexBuffer = static_cast<OpenGLRHIBuffer*>(indexBuffer);
        if (!m_BoundIndexBuffer)
        {
            return;
        }

        if (m_BoundVertexLayout)
        {
            glBindVertexArray(m_BoundVertexLayout->GetVertexArrayId());
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_BoundIndexBuffer->GetBufferId());
    }

    void OpenGLRHI::RHICmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        EnsureOpenGLFunctionTableLoaded();
        if (m_BoundIndexBuffer == nullptr || m_BoundIndexBuffer->GetBufferId() == 0)
        {
            ReportDrawWithoutNativeBuffer("index buffer", indexCount, m_BoundIndexBuffer);
            return;
        }

        const GLenum primitiveType = GetBoundPrimitiveType(m_BoundPipeline);
        const void* indices = reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex * sizeof(uint32_t)));
        glDrawElements(primitiveType, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, indices);
        (void)vertexOffset;
    }

    void OpenGLRHI::RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex)
    {
        EnsureOpenGLFunctionTableLoaded();
        if (m_BoundVertexBuffer != nullptr && m_BoundVertexBuffer->GetBufferId() == 0)
        {
            ReportDrawWithoutNativeBuffer("vertex buffer", vertexCount, m_BoundVertexBuffer);
            return;
        }

        const GLenum primitiveType = GetBoundPrimitiveType(m_BoundPipeline);
        glDrawArrays(primitiveType, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
    }

    void OpenGLRHI::RHICmdGenerateMips(RHITexture* texture)
    {
        EnsureOpenGLFunctionTableLoaded();
        if (texture == nullptr)
        {
            return;
        }

        auto* glTexture = static_cast<OpenGLRHITexture*>(texture);
        const GLuint textureId = glTexture->GetTextureId();
        const GLenum target = glTexture->GetTextureTarget();
        if (textureId == 0)
        {
            return;
        }

        glBindTexture(target, textureId);
        glGenerateMipmap(target);
        // sampling filter lives on RHISampler, not the texture object.
        glBindTexture(target, 0);
    }

    std::shared_ptr<RHISampler> OpenGLRHI::RHIGetOrCreateSampler(const RHISamplerDesc& desc)
    {
        const auto existing = m_SamplerCache.find(desc);
        if (existing != m_SamplerCache.end())
        {
            return existing->second;
        }

        // The cache holds the handle, not the driver object: a sampler asked for on the game role
        // and the one the graphics role later creates are the same entry, so a texture bound by
        // both sides names one sampler.
        RHISamplerRef sampler = RHIGraphicsRole::IsCurrent()
            ? std::make_shared<OpenGLRHISampler>(desc)
            : std::make_shared<OpenGLRHISampler>(desc, DeferredNativeCreate{});
        sampler = ScheduleNativeCreate(std::move(sampler), "openGL sampler");
        m_SamplerCache.emplace(desc, sampler);
        return sampler;
    }

}