#include "OpenGLRHI.h"
#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"

#include "OpenGLBuffers.h"
#include "OpenGLVertexArrayObject.h"
#include "OpenGLTexture.h"
#include "OpenGLShader.h"

#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"

#include "glad/glad.h"

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

        void ApplyCullMode(const RHIRasterizerStateDesc& rasterizer)
        {
            if (!rasterizer.bCullEnabled || rasterizer.CullMode == RHICullMode::None)
            {
                glDisable(GL_CULL_FACE);
                return;
            }

            glEnable(GL_CULL_FACE);
            switch (rasterizer.CullMode)
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

    void OpenGLRHI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(x, y, width, height);
    }

    void OpenGLRHI::SetClearColor(Vector4 clearColor)
    {
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    }

    void OpenGLRHI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    void OpenGLRHI::SetDrawBuffer(uint32_t index)
    {
        if(index == -1) // GL_NONE
        {
            glDrawBuffer(GL_NONE);
        }
        else
        {
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + index);
        }
    }

    void OpenGLRHI::SetReadBuffer(uint32_t index)
    {
        if(index == -1) // GL_NONE
        {
            glReadBuffer(GL_NONE);
        }
        else
        {
            glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
        }
    }

    void OpenGLRHI::EnableDepthTest()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRHI::DisableDepthTest()
    {
        glDisable(GL_DEPTH_TEST);
    }

    void OpenGLRHI::SetDepthMask(bool bEnable)
    {
        glDepthMask(bEnable ? GL_TRUE : GL_FALSE);
    }

    void OpenGLRHI::EnableStencilTest()
    {
        glEnable(GL_STENCIL_TEST);
    }

    void OpenGLRHI::DisableStencilTest()
    {
        glDisable(GL_STENCIL_TEST);
    }

    void OpenGLRHI::SetStencilMask(uint32_t mask)
    {
        glStencilMask(mask);
    }

    void OpenGLRHI::EnableBlend()
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Set default blend function for alpha blending
    }

    void OpenGLRHI::DisableBlend()
    {
        glDisable(GL_BLEND);

    }

    void OpenGLRHI::EnableCullFace()
    {
        glEnable(GL_CULL_FACE);
    }

    void OpenGLRHI::DisableCullFace()
    {
        glDisable(GL_CULL_FACE);
    }

    std::shared_ptr<VertexBuffer> OpenGLRHI::CreateVertexBuffer(float *vertices, uint32_t size, uint32_t numVertices)
    {
        return std::make_shared<OpenGLVertexBuffer>(vertices, size, numVertices);
    }

    std::shared_ptr<IndexBuffer> OpenGLRHI::CreateIndexBuffer(uint32_t *indices, uint32_t numIndices)
    {
        return std::make_shared<OpenGLIndexBuffer>(indices, numIndices);
    }

    std::shared_ptr<VertexDefinition> OpenGLRHI::CreateVertexDefinition(std::initializer_list<RHIVertexElement> elements)
    {
        return std::make_shared<OpenGLVertexArrayObject>(elements);
    }

    std::shared_ptr<FrameBuffer> OpenGLRHI::CreateFrameBuffer(uint32_t width, uint32_t height)
    {
        return std::make_shared<OpenGLFrameBuffer>(width, height);
    }

    std::shared_ptr<UniformBuffer> OpenGLRHI::CreateUniformBuffer(uint32_t size, uint32_t bindingPoint)
    {
        return std::make_shared<OpenGLUniformBuffer>(size, bindingPoint);
    }

    std::shared_ptr<RHITexture2D> OpenGLRHI::CreateRHITexture2D(const unsigned char *data, RHITextureDesc desc)
    {
        return std::make_shared<OpenGLTexture2D>(data, desc);
    }

    std::shared_ptr<RHITexture2D> OpenGLRHI::CreateRHITexture2DFloat(const float* data, RHITextureDesc desc)
    {
        auto texture = std::make_shared<OpenGLTexture2D>(data, desc);
        if (texture->GetID() == 0)
        {
            return nullptr;
        }
        return texture;
    }

    std::shared_ptr<RHITextureCube> OpenGLRHI::CreateRHITextureCube(
        const std::vector<unsigned char*>& faceData,
        RHITextureDesc desc,
        bool generateMipmaps)
    {
        auto texture = std::make_shared<OpenGLTextureCube>(faceData, desc, generateMipmaps);
        if (texture->GetID() == 0)
        {
            return nullptr;
        }
        return texture;
    }

    std::shared_ptr<RHITexture2DArray> OpenGLRHI::CreateRHITexture2DArray(const unsigned char *data, RHITextureDesc desc)
    {
        return std::make_shared<OpenGLTexture2DArray>(data, desc);
    }

    std::shared_ptr<RHIShaderLegacy> OpenGLRHI::CreateRHIShader(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outCompileLog)
    {
        std::shared_ptr<OpenGLShader> shader = std::make_shared<OpenGLShader>(vertexSource, fragmentSource);
        if (outCompileLog != nullptr)
        {
            *outCompileLog = shader->GetCompileLog();
        }

        if (!shader->IsValid())
        {
            return nullptr;
        }

        return shader;
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

    std::shared_ptr<RHIBuffer> OpenGLRHI::RHICreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData)
    {
        return std::make_shared<OpenGLRHIBuffer>(desc, initialData);
    }

    std::shared_ptr<RHIShader> OpenGLRHI::RHICreateShader(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outCompileLog)
    {
        auto shader = std::make_shared<OpenGLShader>(vertexSource, fragmentSource);
        if (outCompileLog)
        {
            *outCompileLog = shader->GetCompileLog();
        }
        if (!shader->IsValid())
        {
            return nullptr;
        }
        return std::make_shared<OpenGLRHIShader>(shader);
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
        if (auto* vs = dynamic_cast<OpenGLRHIShader*>(desc.VertexShader))
        {
            glUseProgram(vs->GetProgramId());
        }

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

        if (desc.BlendState.bBlendEnabled)
        {
            glEnable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
        }

        // Culling deferred: winding/mode not validated yet; keep off for all modern passes.
        glDisable(GL_CULL_FACE);

        if (desc.VertexInputLayout)
        {
            RHICmdSetVertexInputLayout(desc.VertexInputLayout);
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
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    colorTex->GetTextureTarget(),
                    colorTex->GetTextureId(),
                    color0.MipIndex);
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
    }

    void OpenGLRHI::RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        ApplyGraphicsPipelineState(pipelineState);
    }

    void OpenGLRHI::RHICmdSetBindingSet(uint32_t setIndex, RHIBindingSet* bindingSet)
    {
        (void)setIndex;
        auto* glSet = dynamic_cast<OpenGLRHIBindingSet*>(bindingSet);
        if (!glSet)
        {
            return;
        }

        for (const RHIBindingResource& resource : glSet->GetResources())
        {
            if (resource.Type == RHIBindingType::TextureSRV && resource.TextureSRV)
            {
                const RHITextureSRVDesc& srvDesc = resource.TextureSRV->GetCreateDesc();
                GLuint texId = GetOpenGLTextureId(srvDesc.Texture);
                const RHIBindingLayout* layout = glSet->GetLayout();
                uint32_t unit = 0;
                if (layout && !layout->GetEntries().empty())
                {
                    unit = layout->GetEntries()[0].ShaderBinding;
                }
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, texId);
            }
            else if (resource.Type == RHIBindingType::UniformBuffer && resource.Buffer)
            {
                auto* ubo = dynamic_cast<OpenGLRHIBuffer*>(resource.Buffer);
                if (ubo)
                {
                    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo->GetBufferId());
                }
            }
        }
    }

    void OpenGLRHI::RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }

    void OpenGLRHI::RHICmdSetVertexInputLayout(RHIVertexInputLayout* layout)
    {
        m_BoundVertexLayout = static_cast<OpenGLRHIVertexInputLayout*>(layout);
        if (m_BoundVertexLayout)
        {
            glBindVertexArray(m_BoundVertexLayout->GetVertexArrayId());
        }
    }

    void OpenGLRHI::RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot)
    {
        (void)slot;
        m_BoundVertexBuffer = static_cast<OpenGLRHIBuffer*>(vertexBuffer);
        if (m_BoundVertexBuffer)
        {
            glBindBuffer(m_BoundVertexBuffer->GetBindingTarget(), m_BoundVertexBuffer->GetBufferId());
        }
    }

    void OpenGLRHI::RHICmdSetIndexBuffer(RHIBuffer* indexBuffer)
    {
        m_BoundIndexBuffer = static_cast<OpenGLRHIBuffer*>(indexBuffer);
        if (m_BoundIndexBuffer)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_BoundIndexBuffer->GetBufferId());
        }
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
