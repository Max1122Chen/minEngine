#include "OpenGLRHIResources.h"

#include "Log/LogSystem.h"
#include "glad/glad.h"

#include <algorithm>
#include <vector>

namespace minEngine
{
    namespace
    {
        TextureUsage GetTextureUsage(const RHITextureCreateDesc& desc)
        {
            if ((desc.Flags & RHITextureCreateFlags::RenderTarget) != RHITextureCreateFlags::None)
            {
                if (desc.Format == TextureFormat::DEPTH16 || desc.Format == TextureFormat::DEPTH24 ||
                    desc.Format == TextureFormat::DEPTH32 || desc.Format == TextureFormat::DEPTH24STENCIL8)
                {
                    return TextureUsage::DepthStencil;
                }
                return TextureUsage::Color;
            }
            return TextureUsage::TextureBinding;
        }

        void ResolveOpenGLTextureFormat(
            TextureFormat format,
            TextureUsage usage,
            GLint& internalFormat,
            GLenum& dataFormat,
            GLenum& dataType)
        {
            internalFormat = 0;
            dataFormat = 0;
            dataType = GL_UNSIGNED_BYTE;

            if (format == TextureFormat::RED)
            {
                internalFormat = GL_R8;
                dataFormat = GL_RED;
            }
            else if (format == TextureFormat::RGB8)
            {
                internalFormat = GL_RGB8;
                dataFormat = GL_RGB;
            }
            else if (format == TextureFormat::RGBA8)
            {
                internalFormat = GL_RGBA8;
                dataFormat = GL_RGBA;
            }
            else if (format == TextureFormat::RGB16F)
            {
                internalFormat = GL_RGB16F;
                dataFormat = GL_RGB;
                dataType = GL_FLOAT;
            }
            else if (format == TextureFormat::RGBA16F)
            {
                internalFormat = GL_RGBA16F;
                dataFormat = GL_RGBA;
                dataType = GL_FLOAT;
            }
            else if (format == TextureFormat::DEPTH16)
            {
                internalFormat = GL_DEPTH_COMPONENT16;
                dataFormat = GL_DEPTH_COMPONENT;
                dataType = GL_UNSIGNED_SHORT;
            }
            else if (format == TextureFormat::DEPTH24)
            {
                internalFormat = GL_DEPTH_COMPONENT24;
                dataFormat = GL_DEPTH_COMPONENT;
                dataType = GL_UNSIGNED_INT;
            }
            else if (format == TextureFormat::DEPTH32)
            {
                // Prefer 32F: GL_DEPTH_COMPONENT32 is not reliably framebuffer-complete on all drivers
                // (point-light cube shadow FBOs were SIGSEGV'ing after attach/clear).
                internalFormat = GL_DEPTH_COMPONENT32F;
                dataFormat = GL_DEPTH_COMPONENT;
                dataType = GL_FLOAT;
            }
            else if (format == TextureFormat::DEPTH24STENCIL8)
            {
                internalFormat = GL_DEPTH24_STENCIL8;
                dataFormat = GL_DEPTH_STENCIL;
                dataType = GL_UNSIGNED_INT_24_8;
            }

            if (internalFormat == 0)
            {
                if (usage == TextureUsage::Depth)
                {
                    internalFormat = GL_DEPTH_COMPONENT;
                    dataFormat = GL_DEPTH_COMPONENT;
                }
                else if (usage == TextureUsage::Stencil)
                {
                    internalFormat = GL_STENCIL_INDEX;
                    dataFormat = GL_STENCIL_INDEX;
                }
                else if (usage == TextureUsage::DepthStencil)
                {
                    internalFormat = GL_DEPTH_STENCIL;
                    dataFormat = GL_DEPTH_STENCIL;
                    dataType = GL_UNSIGNED_INT_24_8;
                }
            }
        }

        bool IsDepthLikeTexture(TextureFormat format, TextureUsage usage)
        {
            return format == TextureFormat::DEPTH16 || format == TextureFormat::DEPTH24 ||
                   format == TextureFormat::DEPTH32 || format == TextureFormat::DEPTH24STENCIL8 ||
                   usage == TextureUsage::Depth || usage == TextureUsage::DepthStencil;
        }

        bool IsFloatColorTexture(TextureFormat format)
        {
            return format == TextureFormat::RGB16F || format == TextureFormat::RGBA16F;
        }

        void Configure2DTextureSampling(GLenum target, TextureFormat format, TextureUsage usage, bool generateMipmaps)
        {
            if (IsDepthLikeTexture(format, usage))
            {
                // NEAREST: LINEAR on depth attachments can make FBOs incomplete and crash drivers.
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor);
                return;
            }

            if (IsFloatColorTexture(format))
            {
                glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                return;
            }

            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            (void)generateMipmaps;
        }

        GLuint UploadTexture2D(const RHITextureCreateDesc& desc, const void* initialData)
        {
            const TextureUsage usage = GetTextureUsage(desc);
            GLuint textureId = 0;
            glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_2D, textureId);

            GLint internalFormat = 0;
            GLenum dataFormat = 0;
            GLenum dataType = GL_UNSIGNED_BYTE;
            ResolveOpenGLTextureFormat(desc.Format, usage, internalFormat, dataFormat, dataType);

            const bool isDepthStencil = IsDepthLikeTexture(desc.Format, usage);
            Configure2DTextureSampling(GL_TEXTURE_2D, desc.Format, usage, true);

            if (internalFormat != 0 && desc.Width > 0 && desc.Height > 0)
            {
                GLint previousUnpackAlignment = 4;
                glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                if (IsFloatColorTexture(desc.Format))
                {
                    glTexImage2D(
                        GL_TEXTURE_2D,
                        0,
                        internalFormat,
                        static_cast<GLsizei>(desc.Width),
                        static_cast<GLsizei>(desc.Height),
                        0,
                        dataFormat,
                        dataType,
                        initialData);
                }
                else
                {
                    glTexImage2D(
                        GL_TEXTURE_2D,
                        0,
                        internalFormat,
                        static_cast<GLsizei>(desc.Width),
                        static_cast<GLsizei>(desc.Height),
                        0,
                        dataFormat,
                        dataType,
                        initialData);
                }

                glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);

                if (!isDepthStencil)
                {
                    glGenerateMipmap(GL_TEXTURE_2D);
                }
            }

            glBindTexture(GL_TEXTURE_2D, 0);
            return textureId;
        }

        GLuint UploadTextureCube(const RHITextureCreateDesc& desc, const void* initialData)
        {
            if (desc.Width == 0 || desc.Height == 0)
            {
                ME_CORE_ERROR("OpenGLRHITexture cube: Width/Height must be > 0.");
                return 0;
            }

            const TextureUsage usage = GetTextureUsage(desc);
            std::vector<unsigned char*> faceData;
            if (initialData != nullptr)
            {
                auto* faces = static_cast<const unsigned char* const*>(initialData);
                for (uint32_t i = 0; i < desc.DepthOrArrayLayers; ++i)
                {
                    faceData.push_back(const_cast<unsigned char*>(faces[i]));
                }
            }
            else
            {
                faceData.assign(desc.DepthOrArrayLayers, nullptr);
            }

            if (faceData.size() < 6)
            {
                ME_CORE_ERROR("OpenGLRHITexture cube: expected 6 face pointers, got {}.", faceData.size());
                return 0;
            }

            GLuint textureId = 0;
            glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            GLint internalFormat = 0;
            GLenum dataFormat = 0;
            GLenum dataType = GL_UNSIGNED_BYTE;
            ResolveOpenGLTextureFormat(desc.Format, usage, internalFormat, dataFormat, dataType);

            const bool isDepthLike = IsDepthLikeTexture(desc.Format, usage);
            if (!isDepthLike && internalFormat == 0)
            {
                ME_CORE_ERROR("OpenGLRHITexture cube: unsupported color format.");
                glDeleteTextures(1, &textureId);
                return 0;
            }

            const uint32_t numMips = desc.NumMips == 0 ? 1u : desc.NumMips;
            const bool wantGenerateMips =
                HasTextureCreateFlag(desc.Flags, RHITextureCreateFlags::GenerateMips);
            const bool useMipFilter = numMips > 1 || wantGenerateMips;

            if (isDepthLike)
            {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            else if (useMipFilter)
            {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }

            GLint previousUnpackAlignment = 4;
            if (!isDepthLike)
            {
                glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            }

            bool hasBaseLevelData = false;
            for (unsigned int faceIndex = 0; faceIndex < 6; faceIndex++)
            {
                const unsigned char* facePixels = faceData[faceIndex];
                if (!isDepthLike && facePixels == nullptr && !IsFloatColorTexture(desc.Format))
                {
                    ME_CORE_ERROR("OpenGLRHITexture cube: color face {} is null.", faceIndex);
                    continue;
                }
                if (facePixels != nullptr)
                {
                    hasBaseLevelData = true;
                }

                for (uint32_t mip = 0; mip < numMips; ++mip)
                {
                    const uint32_t mipWidth = std::max(1u, desc.Width >> mip);
                    const uint32_t mipHeight = std::max(1u, desc.Height >> mip);
                    const void* mipPixels = (mip == 0) ? facePixels : nullptr;

                    glTexImage2D(
                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                        static_cast<GLint>(mip),
                        internalFormat,
                        static_cast<GLsizei>(mipWidth),
                        static_cast<GLsizei>(mipHeight),
                        0,
                        dataFormat,
                        dataType,
                        mipPixels);
                }
            }

            if (numMips > 1)
            {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(numMips - 1));
            }

            if (!isDepthLike)
            {
                glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
                if (wantGenerateMips && hasBaseLevelData && textureId != 0)
                {
                    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
                }
            }

            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            return textureId;
        }

        GLuint UploadTexture2DArray(const RHITextureCreateDesc& desc, const void* initialData)
        {
            const uint32_t layerCount = (desc.DepthOrArrayLayers == 0) ? 1u : desc.DepthOrArrayLayers;
            const TextureUsage usage = GetTextureUsage(desc);

            GLuint textureId = 0;
            glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_2D_ARRAY, textureId);

            GLint internalFormat = 0;
            GLenum dataFormat = 0;
            GLenum dataType = GL_UNSIGNED_BYTE;
            ResolveOpenGLTextureFormat(desc.Format, usage, internalFormat, dataFormat, dataType);
            const bool isDepthLike = IsDepthLikeTexture(desc.Format, usage);

            if (isDepthLike)
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }

            if (internalFormat != 0 && desc.Width > 0 && desc.Height > 0)
            {
                glTexImage3D(
                    GL_TEXTURE_2D_ARRAY,
                    0,
                    internalFormat,
                    static_cast<GLsizei>(desc.Width),
                    static_cast<GLsizei>(desc.Height),
                    static_cast<GLsizei>(layerCount),
                    0,
                    dataFormat,
                    dataType,
                    initialData);

                if (!isDepthLike)
                {
                    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
                }
            }

            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            return textureId;
        }

        std::string ReadShaderInfoLog(unsigned int shaderObject)
        {
            int logLength = 0;
            glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength <= 1)
            {
                return {};
            }

            std::vector<char> logBuffer(static_cast<size_t>(logLength));
            glGetShaderInfoLog(shaderObject, logLength, nullptr, logBuffer.data());
            return std::string(logBuffer.data());
        }

        std::string ReadProgramInfoLog(unsigned int programObject)
        {
            int logLength = 0;
            glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength <= 1)
            {
                return {};
            }

            std::vector<char> logBuffer(static_cast<size_t>(logLength));
            glGetProgramInfoLog(programObject, logLength, nullptr, logBuffer.data());
            return std::string(logBuffer.data());
        }

        bool CompileShaderStage(unsigned int shaderObject, GLenum stage, std::string_view source, std::string& outLog)
        {
            const char* sourcePtr = source.data();
            const int sourceLength = static_cast<int>(source.size());
            glShaderSource(shaderObject, 1, &sourcePtr, &sourceLength);
            glCompileShader(shaderObject);

            int compileStatus = GL_FALSE;
            glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &compileStatus);
            if (compileStatus == GL_TRUE)
            {
                return true;
            }

            outLog = ReadShaderInfoLog(shaderObject);
            if (outLog.empty())
            {
                outLog = stage == GL_VERTEX_SHADER ? "Vertex shader compilation failed." : "Fragment shader compilation failed.";
            }
            return false;
        }

        GLenum VertexElementTypeToGL(VertexElementType type)
        {
            switch (type)
            {
            case VertexElementType::Float:
            case VertexElementType::Float2:
            case VertexElementType::Float3:
            case VertexElementType::Float4:
            case VertexElementType::Mat3:
            case VertexElementType::Mat4:
                return GL_FLOAT;
            case VertexElementType::Int:
            case VertexElementType::Int2:
            case VertexElementType::Int3:
            case VertexElementType::Int4:
                return GL_INT;
            case VertexElementType::Bool:
                return GL_BOOL;
            default:
                break;
            }
            return GL_FLOAT;
        }
    }

    GLuint GetOpenGLTextureId(RHITexture* texture)
    {
        if (!texture)
        {
            return 0;
        }
        return static_cast<OpenGLRHITexture*>(texture)->GetTextureId();
    }

    OpenGLRHITexture::~OpenGLRHITexture()
    {
        if (m_OwnsGlTexture && m_TextureId != 0)
        {
            glDeleteTextures(1, &m_TextureId);
            m_TextureId = 0;
        }
    }

    OpenGLRHITexture::OpenGLRHITexture(const RHITextureCreateDesc& desc, const void* initialData)
        : m_Desc(desc)
    {
        switch (desc.Dimension)
        {
        case RHITextureDimension::Texture2DArray:
            m_Target = GL_TEXTURE_2D_ARRAY;
            m_TextureId = UploadTexture2DArray(desc, initialData);
            break;
        case RHITextureDimension::TextureCube:
            m_Target = GL_TEXTURE_CUBE_MAP;
            m_TextureId = UploadTextureCube(desc, initialData);
            break;
        case RHITextureDimension::Texture2D:
        default:
            m_Target = GL_TEXTURE_2D;
            m_TextureId = UploadTexture2D(desc, initialData);
            break;
        }
        m_OwnsGlTexture = m_TextureId != 0;
    }

    void* OpenGLRHITexture::GetNativeResource() const
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_TextureId));
    }

    OpenGLRHIBuffer::OpenGLRHIBuffer(const RHIBufferCreateDesc& desc, const void* initialData)
        : m_Desc(desc)
    {
        switch (desc.Usage)
        {
        case RHIBufferUsage::Vertex:
            m_Target = GL_ARRAY_BUFFER;
            break;
        case RHIBufferUsage::Index:
            m_Target = GL_ELEMENT_ARRAY_BUFFER;
            break;
        case RHIBufferUsage::Uniform:
            m_Target = GL_UNIFORM_BUFFER;
            break;
        default:
            m_Target = GL_ARRAY_BUFFER;
            break;
        }

        glGenBuffers(1, &m_BufferId);
        glBindBuffer(m_Target, m_BufferId);
        glBufferData(m_Target, desc.ByteSize, initialData, GL_STATIC_DRAW);
        glBindBuffer(m_Target, 0);
    }

    void OpenGLRHIBuffer::UpdateSubresource(const void* data, uint32_t offset, uint32_t size)
    {
        if (!data || size == 0)
        {
            return;
        }
        glBindBuffer(m_Target, m_BufferId);
        glBufferSubData(m_Target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
        glBindBuffer(m_Target, 0);
    }

    OpenGLRHIShader::~OpenGLRHIShader()
    {
        if (m_ProgramId != 0)
        {
            glDeleteProgram(m_ProgramId);
            m_ProgramId = 0;
        }
    }

    OpenGLRHIShader::OpenGLRHIShader(std::string_view vertexSource, std::string_view fragmentSource)
    {
        const unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        std::string stageLog;
        if (!CompileShaderStage(vertexShader, GL_VERTEX_SHADER, vertexSource, stageLog))
        {
            m_CompileLog = "Vertex shader compile error:\n" + stageLog;
            ME_CORE_ERROR("{}", m_CompileLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return;
        }

        if (!CompileShaderStage(fragmentShader, GL_FRAGMENT_SHADER, fragmentSource, stageLog))
        {
            m_CompileLog = "Fragment shader compile error:\n" + stageLog;
            ME_CORE_ERROR("{}", m_CompileLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return;
        }

        m_ProgramId = glCreateProgram();
        glAttachShader(m_ProgramId, vertexShader);
        glAttachShader(m_ProgramId, fragmentShader);
        glLinkProgram(m_ProgramId);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        int linkStatus = GL_FALSE;
        glGetProgramiv(m_ProgramId, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE)
        {
            m_CompileLog = "Shader program link error:\n" + ReadProgramInfoLog(m_ProgramId);
            ME_CORE_ERROR("{}", m_CompileLog);
            glDeleteProgram(m_ProgramId);
            m_ProgramId = 0;
            return;
        }

        m_IsValid = true;
    }

    OpenGLRHIVertexInputLayout::OpenGLRHIVertexInputLayout(std::initializer_list<RHIVertexElement> elements)
    {
        uint32_t offset = 0;
        for (const auto& element : elements)
        {
            RHIVertexElement copy = element;
            copy.Offset = offset;
            offset += VertexElementTypeSize(element.Type);
            m_Elements.push_back(copy);
        }
        m_Stride = offset;

        glGenVertexArrays(1, &m_VAO);
    }

    void OpenGLRHIVertexInputLayout::BindVertexBuffer(GLuint bufferId)
    {
        if (m_VAO == 0)
        {
            return;
        }

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);

        uint32_t index = 0;
        for (const auto& element : m_Elements)
        {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(
                index,
                element.Size,
                VertexElementTypeToGL(element.Type),
                element.bNormalized ? GL_TRUE : GL_FALSE,
                m_Stride,
                reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset)));
            ++index;
        }
    }

    OpenGLRHIShaderResourceView::OpenGLRHIShaderResourceView(const RHITextureSRVDesc& desc)
        : m_Desc(desc)
    {
    }

    OpenGLRHIShaderBindingSetLayout::OpenGLRHIShaderBindingSetLayout(std::vector<RHIShaderBindingSetLayoutEntry> entries)
        : m_Entries(std::move(entries))
    {
    }

    OpenGLRHIPipelineLayout::OpenGLRHIPipelineLayout(std::vector<RHIShaderBindingSetLayout*> setLayouts)
        : m_SetLayouts(std::move(setLayouts))
    {
    }

    RHIShaderBindingSetLayout* OpenGLRHIPipelineLayout::GetShaderBindingSetLayout(uint32_t setIndex) const
    {
        if (setIndex >= m_SetLayouts.size())
        {
            return nullptr;
        }
        return m_SetLayouts[setIndex];
    }

    OpenGLRHIShaderBindingSet::OpenGLRHIShaderBindingSet(RHIShaderBindingSetLayout* layout, std::vector<RHIShaderBinding> resources)
        : m_Layout(layout)
        , m_Resources(std::move(resources))
    {
    }
}
