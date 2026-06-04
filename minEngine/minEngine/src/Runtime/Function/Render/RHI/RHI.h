#pragma once
#include "Core.h"
#include "Math/Math.h"

namespace minEngine
{
    struct RHIVertexElement;
    class VertexDefinition;
    class VertexBuffer;
    class IndexBuffer;
    class FrameBuffer;
    class UniformBuffer;
    class RHITexture2D;
    class RHITextureCube;
    class RHITexture2DArray;
    class RHITextureDesc;
    class RHIShaderLegacy;

    class RHI
    {
    public:
        RHI() = default;
        virtual ~RHI() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        // TODO: No more immediate mode commands, we will use command lists and pipeline states instead.
        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

        virtual void SetClearColor(Vector4 clearColor) = 0;
        virtual void Clear() = 0;

        virtual void SetDrawBuffer(uint32_t index) = 0;
        virtual void SetReadBuffer(uint32_t index) = 0;

        virtual void EnableDepthTest() = 0;
        virtual void DisableDepthTest() = 0;
        virtual void SetDepthMask(bool bEnable) = 0;

        virtual void EnableStencilTest() = 0;
        virtual void DisableStencilTest() = 0;
        virtual void SetStencilMask(uint32_t mask) = 0;

        virtual void EnableBlend() = 0;
        virtual void DisableBlend() = 0;

        virtual void EnableCullFace() = 0;
        virtual void DisableCullFace() = 0;

        // TODO: we need new resource creation functions here to replace this OpenGL style resource creation functions.
        virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size, uint32_t numVertices) = 0;
        virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(uint32_t* indices, uint32_t numIndices) = 0;
        virtual std::shared_ptr<VertexDefinition> CreateVertexDefinition(std::initializer_list<RHIVertexElement> elements) = 0;
        virtual std::shared_ptr<FrameBuffer> CreateFrameBuffer(uint32_t width, uint32_t height) = 0;
        virtual std::shared_ptr<UniformBuffer> CreateUniformBuffer(uint32_t size, uint32_t bindingPoint = 0) = 0;
        virtual std::shared_ptr<RHITexture2D> CreateRHITexture2D(const unsigned char* data, RHITextureDesc desc) = 0;
        virtual std::shared_ptr<RHITexture2D> CreateRHITexture2DFloat(const float* data, RHITextureDesc desc) = 0;
        virtual std::shared_ptr<RHITextureCube> CreateRHITextureCube(
            const std::vector<unsigned char*>& faceData,
            RHITextureDesc desc,
            bool generateMipmaps = false) = 0;
        virtual std::shared_ptr<RHITexture2DArray> CreateRHITexture2DArray(const unsigned char* data, RHITextureDesc desc) = 0;
        virtual std::shared_ptr<RHIShaderLegacy> CreateRHIShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr) = 0;
    };
}