#pragma once

#include "glad/glad.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

#include "OpenGLTexture.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

namespace minEngine
{
    class OpenGLShader;

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
        std::shared_ptr<OpenGLTexture2D> m_OwningUploadTexture2D;
        std::shared_ptr<OpenGLTexture2DArray> m_OwningUploadTexture2DArray;
        std::shared_ptr<OpenGLTextureCube> m_OwningUploadTextureCube;
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
        explicit OpenGLRHIShader(std::shared_ptr<OpenGLShader> shader);

        virtual bool IsValid() const override;
        virtual const std::string& GetCompileLog() const override;

        OpenGLShader* GetGLShader() const { return m_Shader.get(); }
        GLuint GetProgramId() const;

    private:
        std::shared_ptr<OpenGLShader> m_Shader;
    };

    class OpenGLRHIVertexInputLayout final : public RHIVertexInputLayout
    {
    public:
        OpenGLRHIVertexInputLayout() = default;
        explicit OpenGLRHIVertexInputLayout(std::initializer_list<RHIVertexElement> elements);

        static std::shared_ptr<OpenGLRHIVertexInputLayout> FromLegacyVAO(
            GLuint vao,
            const std::vector<RHIVertexElement>& elements,
            uint32_t stride);

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

    class OpenGLRHIBindingLayout final : public RHIBindingLayout
    {
    public:
        explicit OpenGLRHIBindingLayout(std::vector<RHIBindingLayoutEntry> entries);

        virtual const std::vector<RHIBindingLayoutEntry>& GetEntries() const override { return m_Entries; }

    private:
        std::vector<RHIBindingLayoutEntry> m_Entries;
    };

    class OpenGLRHIBindingSet final : public RHIBindingSet
    {
    public:
        OpenGLRHIBindingSet(RHIBindingLayout* layout, std::vector<RHIBindingResource> resources);

        virtual const RHIBindingLayout* GetLayout() const override { return m_Layout; }
        virtual const std::vector<RHIBindingResource>& GetResources() const override { return m_Resources; }

    private:
        RHIBindingLayout* m_Layout = nullptr;
        std::vector<RHIBindingResource> m_Resources;
    };

    GLuint GetOpenGLTextureId(RHITexture* texture);
}
