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

namespace minEngine
{
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
        void ModernRHIStubNotImplemented(const char* apiName)
        {
            ME_ASSERT(false, apiName);
        }
    }

    std::shared_ptr<RHITexture> OpenGLRHI::RHICreateTexture2D(const RHITextureCreateDesc& desc, const void* initialData)
    {
        (void)desc;
        (void)initialData;
        ModernRHIStubNotImplemented("RHICreateTexture2D");
        return nullptr;
    }

    std::shared_ptr<RHIBuffer> OpenGLRHI::RHICreateBuffer(const RHIBufferCreateDesc& desc, const void* initialData)
    {
        (void)desc;
        (void)initialData;
        ModernRHIStubNotImplemented("RHICreateBuffer");
        return nullptr;
    }

    std::shared_ptr<RHIShader> OpenGLRHI::RHICreateShader(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string* outCompileLog)
    {
        (void)vertexSource;
        (void)fragmentSource;
        (void)outCompileLog;
        ModernRHIStubNotImplemented("RHICreateShader");
        return nullptr;
    }

    std::shared_ptr<RHIGraphicsPipelineState> OpenGLRHI::RHICreateGraphicsPipelineState(const RHIGraphicsPSODesc& desc)
    {
        (void)desc;
        ModernRHIStubNotImplemented("RHICreateGraphicsPipelineState");
        return nullptr;
    }

    std::shared_ptr<RHIBindingLayout> OpenGLRHI::RHICreateBindingLayout(const std::vector<RHIBindingLayoutEntry>& entries)
    {
        (void)entries;
        ModernRHIStubNotImplemented("RHICreateBindingLayout");
        return nullptr;
    }

    std::shared_ptr<RHIBindingSet> OpenGLRHI::RHICreateBindingSet(
        RHIBindingLayout* layout,
        const std::vector<RHIBindingResource>& resources)
    {
        (void)layout;
        (void)resources;
        ModernRHIStubNotImplemented("RHICreateBindingSet");
        return nullptr;
    }

    void OpenGLRHI::RHICmdBeginRenderPass(const RHIRenderPassInfo& info)
    {
        (void)info;
        ModernRHIStubNotImplemented("RHICmdBeginRenderPass");
    }

    void OpenGLRHI::RHICmdEndRenderPass()
    {
        ModernRHIStubNotImplemented("RHICmdEndRenderPass");
    }

    void OpenGLRHI::RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState)
    {
        (void)pipelineState;
        ModernRHIStubNotImplemented("RHICmdSetGraphicsPipelineState");
    }

    void OpenGLRHI::RHICmdSetBindingSet(uint32_t setIndex, RHIBindingSet* bindingSet)
    {
        (void)setIndex;
        (void)bindingSet;
        ModernRHIStubNotImplemented("RHICmdSetBindingSet");
    }

    void OpenGLRHI::RHICmdSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        ModernRHIStubNotImplemented("RHICmdSetViewport");
    }

    void OpenGLRHI::RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot)
    {
        (void)vertexBuffer;
        (void)slot;
        ModernRHIStubNotImplemented("RHICmdSetVertexBuffer");
    }

    void OpenGLRHI::RHICmdSetIndexBuffer(RHIBuffer* indexBuffer)
    {
        (void)indexBuffer;
        ModernRHIStubNotImplemented("RHICmdSetIndexBuffer");
    }

    void OpenGLRHI::RHICmdDrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
    {
        (void)indexCount;
        (void)firstIndex;
        (void)vertexOffset;
        ModernRHIStubNotImplemented("RHICmdDrawIndexed");
    }

    void OpenGLRHI::RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex)
    {
        (void)vertexCount;
        (void)firstVertex;
        ModernRHIStubNotImplemented("RHICmdDraw");
    }

}
