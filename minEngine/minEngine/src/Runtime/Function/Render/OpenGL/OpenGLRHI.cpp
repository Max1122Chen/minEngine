#include "OpenGLRHI.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"

#include "OpenGLRHIResources.h"

#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"

#include "glad/glad.h"

#include <algorithm>

namespace minEngine
{
    namespace
    {
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

    }

    void OpenGLRHI::Initialize()
    {
        // Initialize OpenGL specific resources here
        m_WindowSystem = &WindowSystem::Get();
        
        ME_CORE_INFO("OpenGLRHI Initialized"); 

    }

    void OpenGLRHI::Shutdown()
    {
        m_WindowSystem = nullptr;
        ME_CORE_INFO("OpenGLRHI Shutdown");
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
        if (m_OwnsTransientFramebuffer && m_TransientFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_TransientFramebuffer);
        }
        m_TransientFramebuffer = 0;
        m_OwnsTransientFramebuffer = false;
    }

    std::shared_ptr<RHITexture> OpenGLRHI::RHICreateTexture2D(const RHITextureCreateDesc& desc, const void* initialData)
    {
        return std::make_shared<OpenGLRHITexture>(desc, initialData);
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
        return std::make_shared<OpenGLRHIBuffer>(desc, initialData);
    }

    std::shared_ptr<RHIShader> OpenGLRHI::RHICreateShader(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outCompileLog)
    {
        auto shader = std::make_shared<OpenGLRHIShader>(vertexSource, fragmentSource);
        if (outCompileLog)
        {
            *outCompileLog = shader->GetCompileLog();
        }
        if (!shader->IsValid())
        {
            return nullptr;
        }
        return shader;
    }

    std::shared_ptr<RHIGraphicsPipelineState> OpenGLRHI::RHICreateGraphicsPipelineState(const RHIGraphicsPSODesc& desc)
    {
        return std::make_shared<RHIGraphicsPSOStateFallback>(desc);
    }

    std::shared_ptr<RHIBindingLayout> OpenGLRHI::RHICreateBindingLayout(const std::vector<RHIBindingLayoutEntry>& entries)
    {
        return std::make_shared<OpenGLRHIBindingLayout>(entries);
    }

    std::shared_ptr<RHIBindingSet> OpenGLRHI::RHICreateBindingSet(
        RHIBindingLayout* layout,
        const std::vector<RHIBindingResource>& resources)
    {
        return std::make_shared<OpenGLRHIBindingSet>(layout, resources);
    }

    std::shared_ptr<RHIVertexInputLayout> OpenGLRHI::RHICreateVertexInputLayout(
        std::initializer_list<RHIVertexElement> elements)
    {
        return std::make_shared<OpenGLRHIVertexInputLayout>(elements);
    }

    void OpenGLRHI::ApplyGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        m_BoundPipeline = pipelineState;
        auto* fallback = dynamic_cast<RHIGraphicsPSOStateFallback*>(pipelineState);
        if (!fallback)
        {
            return;
        }

        const RHIGraphicsPSODesc& desc = fallback->GetDesc();

        // Set shader program
        if (auto* vs = dynamic_cast<OpenGLRHIShader*>(desc.VertexShader))
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
    }

    void OpenGLRHI::RHICmdBeginRenderPass(const RHIRenderPassInfo& info)
    {
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
        DestroyTransientFramebuffer();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDepthMask(GL_FALSE);
    }

    void OpenGLRHI::RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        ApplyGraphicsPipelineState(pipelineState);
    }

    void OpenGLRHI::RHICmdSetBindingSet(uint32_t setIndex, RHIBindingSet* bindingSet)
    {
        (void)setIndex;
        auto* glSet = static_cast<OpenGLRHIBindingSet*>(bindingSet);
        if (!glSet)
        {
            return;
        }

        const RHIBindingLayout* layout = glSet->GetLayout();
        const std::vector<RHIBindingResource>& resources = glSet->GetResources();
        if (!layout)
        {
            return;
        }

        const std::vector<RHIBindingLayoutEntry>& entries = layout->GetEntries();
        const size_t bindCount = std::min(resources.size(), entries.size());
        for (size_t i = 0; i < bindCount; ++i)
        {
            const RHIBindingResource& resource = resources[i];
            const RHIBindingLayoutEntry& entry = entries[i];

            if (resource.Type == RHIBindingType::TextureSRV && resource.TextureSRV)
            {
                const RHITextureSRVDesc& srvDesc = resource.TextureSRV->GetCreateDesc();
                if (!srvDesc.Texture)
                {
                    continue;
                }

                GLuint texId = GetOpenGLTextureId(srvDesc.Texture);
                if (texId == 0)
                {
                    continue;
                }

                GLenum target = GL_TEXTURE_2D;
                if (auto* glTexture = dynamic_cast<OpenGLRHITexture*>(srvDesc.Texture))
                {
                    target = glTexture->GetTextureTarget();
                }

                glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(entry.ShaderBinding));
                glBindTexture(target, texId);
            }
            else if (resource.Type == RHIBindingType::UniformBuffer && resource.Buffer)
            {
                auto* ubo = dynamic_cast<OpenGLRHIBuffer*>(resource.Buffer);
                if (ubo)
                {
                    glBindBufferBase(
                        GL_UNIFORM_BUFFER,
                        entry.ShaderBinding,
                        ubo->GetBufferId());
                }
            }
        }
    }

    void OpenGLRHI::RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }


    void OpenGLRHI::RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot)
    {
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
        const void* indices = reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex * sizeof(uint32_t)));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, indices);
        (void)vertexOffset;
    }

    void OpenGLRHI::RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex)
    {
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
    }

}
