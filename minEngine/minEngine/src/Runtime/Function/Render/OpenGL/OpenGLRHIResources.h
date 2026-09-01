#pragma once

#include "glad/glad.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIPipelineLayout.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class OpenGLRHITexture final : public RHITexture
    {
    public:
        ~OpenGLRHITexture() override;

        explicit OpenGLRHITexture(const RHITextureCreateDesc& desc, const void* initialData);

        virtual const RHITextureCreateDesc& GetDesc() const override { return m_Desc; }
        virtual void* GetNativeResource() const override;
        virtual uint32_t GetNativeHandle() const override { return m_TextureId; }

        GLuint GetTextureId() const { return m_TextureId; }
        GLenum GetTextureTarget() const { return m_Target; }
        int32_t GetArrayLayer() const { return m_ArrayLayer; }

    private:
        RHITextureCreateDesc m_Desc;
        GLuint m_TextureId = 0;
        GLenum m_Target = GL_TEXTURE_2D;
        int32_t m_ArrayLayer = -1;
        bool m_OwnsGlTexture = false;
    };

    class OpenGLRHIBuffer final : public RHIBuffer
    {
    public:
        OpenGLRHIBuffer(const RHIBufferCreateDesc& desc, const void* initialData);

        virtual const RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        virtual void UpdateSubresource(const void* data, uint32_t offset, uint32_t size) override;

        GLuint GetBufferId() const { return m_BufferId; }
        GLenum GetBindingTarget() const { return m_Target; }

    private:
        RHIBufferCreateDesc m_Desc;
        GLuint m_BufferId = 0;
        GLenum m_Target = 0;
    };

    class OpenGLRHIShader final : public RHIShader
    {
    public:
        OpenGLRHIShader(std::string_view vertexSource, std::string_view fragmentSource);
        explicit OpenGLRHIShader(const RHIShaderCreateDesc& desc);
        ~OpenGLRHIShader() override;

        OpenGLRHIShader(const OpenGLRHIShader&) = delete;
        OpenGLRHIShader& operator=(const OpenGLRHIShader&) = delete;

        virtual bool IsValid() const override { return m_IsValid; }
        virtual const std::string& GetCompileLog() const override { return m_CompileLog; }

        GLuint GetProgramId() const { return m_ProgramId; }

    private:
        bool LinkProgram(GLuint vertexShader, GLuint fragmentShader);

        GLuint m_ProgramId = 0;
        bool m_IsValid = false;
        std::string m_CompileLog;
    };

    class OpenGLRHIVertexInputLayout final : public RHIVertexInputLayout
    {
    public:
        OpenGLRHIVertexInputLayout() = default;
        explicit OpenGLRHIVertexInputLayout(std::initializer_list<RHIVertexElement> elements);

        virtual const std::vector<RHIVertexElement>& GetElements() const override { return m_Elements; }
        virtual uint32_t GetStride() const override { return m_Stride; }

        GLuint GetVertexArrayId() const { return m_VAO; }

        void BindVertexBuffer(GLuint bufferId);

    private:
        std::vector<RHIVertexElement> m_Elements;
        uint32_t m_Stride = 0;
        GLuint m_VAO = 0;
    };

    class OpenGLRHIShaderResourceView final : public RHIShaderResourceView
    {
    public:
        explicit OpenGLRHIShaderResourceView(const RHITextureSRVDesc& desc);

        virtual const RHITextureSRVDesc& GetCreateDesc() const override { return m_Desc; }

    private:
        RHITextureSRVDesc m_Desc;
    };

    class OpenGLRHIShaderBindingSetLayout final : public RHIShaderBindingSetLayout
    {
    public:
        explicit OpenGLRHIShaderBindingSetLayout(std::vector<RHIShaderBindingSetLayoutEntry> entries);

        virtual const std::vector<RHIShaderBindingSetLayoutEntry>& GetEntries() const override { return m_Entries; }

    private:
        std::vector<RHIShaderBindingSetLayoutEntry> m_Entries;
    };

    class OpenGLRHIPipelineLayout final : public RHIPipelineLayout
    {
    public:
        explicit OpenGLRHIPipelineLayout(std::vector<RHIShaderBindingSetLayout*> setLayouts);

        virtual uint32_t GetShaderBindingSetLayoutCount() const override { return static_cast<uint32_t>(m_SetLayouts.size()); }
        virtual RHIShaderBindingSetLayout* GetShaderBindingSetLayout(uint32_t setIndex) const override;

    private:
        std::vector<RHIShaderBindingSetLayout*> m_SetLayouts;
    };

    class OpenGLRHIShaderBindingSet final : public RHIShaderBindingSet
    {
    public:
        OpenGLRHIShaderBindingSet(RHIShaderBindingSetLayout* layout, std::vector<RHIShaderBinding> resources);

        virtual const RHIShaderBindingSetLayout* GetLayout() const override { return m_Layout; }
        virtual const std::vector<RHIShaderBinding>& GetBindings() const override { return m_Resources; }

    private:
        RHIShaderBindingSetLayout* m_Layout = nullptr;
        std::vector<RHIShaderBinding> m_Resources;
    };

    GLuint GetOpenGLTextureId(RHITexture* texture);
}
