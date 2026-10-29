#pragma once

#include "glad/glad.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIPipelineLayout.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHISampler.h"

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class OpenGLRHITexture final : public RHITexture
    {
    public:
        ~OpenGLRHITexture() override;

        /** Uploads the driver object now, from the caller's memory. */
        explicit OpenGLRHITexture(const RHITextureCreateDesc& desc, const void* initialData);

        /**
         * Takes a copy of the initial pixels and creates the driver object when the graphics role
         * asks for it. The copy exists because the caller's memory is not reachable from the
         * thread that runs the upload.
         */
        OpenGLRHITexture(const RHITextureCreateDesc& desc, const void* initialData, DeferredNativeCreate);

        virtual const RHITextureCreateDesc& GetDesc() const override { return m_Desc; }
        virtual void* GetNativeResource() const override;
        virtual uint32_t GetNativeHandle() const override { return m_TextureId; }
        virtual bool CreateNativeResources() override;
        virtual bool IsReady() const override { return m_TextureId != 0; }

        GLuint GetTextureId() const { return m_TextureId; }
        GLenum GetTextureTarget() const { return m_Target; }
        int32_t GetArrayLayer() const { return m_ArrayLayer; }

    private:
        /** The pixels a deferred create uploads, copied out of the caller's memory. */
        struct PendingPixelData
        {
            /** Contiguous copy: one 2D image, every layer, or every non-null cube face. */
            std::vector<unsigned char> Bytes;
            /** Cube only: one pointer per face, null where the caller passed null. */
            std::vector<const unsigned char*> CubeFaces;
        };

        void ConfigureTargetFromDescription();
        void CapturePendingPixels(const void* initialData);
        void UploadNow(const void* initialData);

        RHITextureCreateDesc m_Desc;
        GLuint m_TextureId = 0;
        GLenum m_Target = GL_TEXTURE_2D;
        int32_t m_ArrayLayer = -1;
        bool m_OwnsGlTexture = false;
        PendingPixelData m_PendingPixels;
    };

    class OpenGLRHIBuffer final : public RHIBuffer
    {
    public:
        /** Uploads the driver object now, from the caller's memory. */
        OpenGLRHIBuffer(const RHIBufferCreateDesc& desc, const void* initialData);

        /**
         * Takes a copy of the initial contents and creates the driver object when the graphics
         * role asks for it.
         */
        OpenGLRHIBuffer(
            const RHIBufferCreateDesc& desc,
            const void* initialData,
            DeferredNativeCreate);
        ~OpenGLRHIBuffer() override;

        virtual const RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        virtual void UpdateSubresource(const void* data, uint32_t offset, uint32_t size) override;
        virtual bool CreateNativeResources() override;
        virtual bool IsReady() const override { return m_BufferId != 0; }

        GLuint GetBufferId() const { return m_BufferId; }
        GLenum GetBindingTarget() const { return m_Target; }

    private:
        void ConfigureBindingTarget();
        bool CreateNativeBuffer(const void* data);

        RHIBufferCreateDesc m_Desc;
        GLuint m_BufferId = 0;
        GLenum m_Target = 0;
        /** Initial contents captured for a deferred create; empty once uploaded. */
        std::vector<unsigned char> m_PendingUpload;
    };

    class OpenGLRHIShader final : public RHIShader
    {
    public:
        OpenGLRHIShader(std::string_view vertexSource, std::string_view fragmentSource);
        explicit OpenGLRHIShader(const RHIShaderCreateDesc& desc);

        /**
         * Keeps what it needs and leaves the driver alone: glShaderBinary/glSpecializeShader and
         * glLinkProgram only work where the context is current, which is the graphics role.
         */
        OpenGLRHIShader(std::string_view vertexSource, std::string_view fragmentSource, DeferredNativeCreate);
        OpenGLRHIShader(const RHIShaderCreateDesc& desc, DeferredNativeCreate);
        ~OpenGLRHIShader() override;

        OpenGLRHIShader(const OpenGLRHIShader&) = delete;
        OpenGLRHIShader& operator=(const OpenGLRHIShader&) = delete;

        virtual bool IsValid() const override { return m_bIsValid.load(std::memory_order_acquire); }
        virtual const std::string& GetCompileLog() const override { return m_CompileLog; }
        virtual bool CreateNativeResources() override;
        virtual bool IsReady() const override { return m_ProgramId != 0; }
        virtual bool HasNativeCreateFailed() const override
        {
            return m_bCreateFailed.load(std::memory_order_acquire);
        }

        GLuint GetProgramId() const { return m_ProgramId; }

    private:
        bool SpecializeFromSpirv(const std::vector<uint32_t>& vertexSpirv, const std::vector<uint32_t>& fragmentSpirv);
        bool CompileFromSources(std::string_view vertexSource, std::string_view fragmentSource);
        bool LinkProgram(GLuint vertexShader, GLuint fragmentShader);
        /**
         * Records why the program does not exist and reports it once, where it happened. The
         * caller on the game role cannot see this thread's driver message, so this line is the
         * only report the failure gets.
         */
        void ReportCreateFailure(std::string message);
        void DropPendingData();

        /** Bytecode or sources captured for a deferred create; empty once it has run. */
        RHIShaderCreateDesc m_PendingDesc;
        std::string m_PendingVertexSource;
        std::string m_PendingFragmentSource;
        GLuint m_ProgramId = 0;
        std::atomic<bool> m_bIsValid{ false };
        std::atomic<bool> m_bCreateFailed{ false };
        std::string m_CompileLog;
    };

    class OpenGLRHIVertexInputLayout final : public RHIVertexInputLayout
    {
    public:
        /** Owns no vertex array object: nothing to destroy. */
        OpenGLRHIVertexInputLayout() = default;
        /** Builds the attribute table and the vertex array object now. */
        OpenGLRHIVertexInputLayout(std::initializer_list<RHIVertexElement> elements);
        /**
         * Builds the attribute table now - it is CPU-side data - and leaves the vertex array
         * object to the graphics role.
         */
        OpenGLRHIVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements,
            DeferredNativeCreate);
        ~OpenGLRHIVertexInputLayout() override;

        virtual const std::vector<RHIVertexElement>& GetElements() const override { return m_Elements; }
        virtual uint32_t GetStride() const override { return m_Stride; }
        virtual bool CreateNativeResources() override;
        virtual bool IsReady() const override { return m_VAO != 0; }

        GLuint GetVertexArrayId() const { return m_VAO; }

        void BindVertexBuffer(GLuint bufferId);

    private:
        void BuildElements(std::initializer_list<RHIVertexElement> elements);
        bool CreateVertexArray();

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

    class OpenGLRHISampler final : public RHISampler
    {
    public:
        explicit OpenGLRHISampler(const RHISamplerDesc& desc);
        /** Keeps the description; glGenSamplers and its parameters run on the graphics role. */
        OpenGLRHISampler(const RHISamplerDesc& desc, DeferredNativeCreate);
        ~OpenGLRHISampler() override;

        OpenGLRHISampler(const OpenGLRHISampler&) = delete;
        OpenGLRHISampler& operator=(const OpenGLRHISampler&) = delete;

        const RHISamplerDesc& GetDesc() const override { return m_Desc; }
        virtual bool CreateNativeResources() override;
        virtual bool IsReady() const override { return m_SamplerId != 0; }

        GLuint GetSamplerId() const { return m_SamplerId; }

    private:
        void ConfigureSampler();

        RHISamplerDesc m_Desc;
        GLuint m_SamplerId = 0;
    };

    GLuint GetOpenGLTextureId(RHITexture* texture);
}
