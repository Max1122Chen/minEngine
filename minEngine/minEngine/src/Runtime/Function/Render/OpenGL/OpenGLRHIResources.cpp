#include "OpenGLRHIResources.h"

#include "OpenGLFunctionTable.h"
#include "Log/LogSystem.h"
#include "Runtime/Core/Threading/ThreadIdentity.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <atomic>

namespace minEngine
{
    namespace
    {
        /**
         * Deleting a GL name needs a context that is current on the calling thread; a context is
         * current on at most one thread, and frame roles hand it over around their frame work.
         * There is no GL call that answers this without a context, so the question goes to the
         * windowing layer that owns the context.
         */
        bool HasCurrentGraphicsContext()
        {
            return glfwGetCurrentContext() != nullptr;
        }

        /**
         * Names the caller and its thread. The size and count in the message below say an object is
         * broken but never who broke it; this line is the part that answers "which thread ran which
         * code". Frames are written as module+offset because a bare address cannot be symbolised
         * without the load base of the run it came from.
         */
        std::string DescribeCreatingThreadAndSite()
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

        /**
         * Reports a native object whose creation had no context to run in.
         *
         * A GL name only exists inside the context that generated it: driving the entry points with no
         * context current leaves the name at zero and silently drops whatever data was uploaded, which
         * is indistinguishable downstream from an object that was created empty. The calling thread is
         * therefore wrong, not the object, and the report says so at the point of failure.
         */
        void ReportMissingGraphicsContext(const char* kind, uint32_t byteSize, const void* object)
        {
            static std::atomic<uint32_t> s_reported{ 0 };
            const uint32_t reported = s_reported.fetch_add(1, std::memory_order_relaxed) + 1;
            if (reported == 1 || reported % 100 == 0)
            {
                ME_LOG(
                    LogRHI,
                    Error,
                    "OpenGL {} ({} bytes, object {:p}) was created on a thread with no current "
                    "graphics context: the driver never creates it, its name stays 0, and every draw "
                    "that binds it names no data. Native objects belong to the thread that holds the "
                    "context. Creating thread and call site:{} {} such object(s) so far.",
                    kind,
                    byteSize,
                    object,
                    DescribeCreatingThreadAndSite(),
                    reported);
            }
        }

        /**
         * Reports a write that arrived before the resource had a native object.
         *
         * The write cannot be honoured: binding name 0 would send the data into whatever object
         * happens to be bound instead of into this one. Dropping it silently would make the
         * resource keep its creation-time contents while every caller believes the write landed,
         * so the drop is counted and named.
         */
        void ReportUploadBeforeNativeObject(const char* kind, uint32_t byteSize)
        {
            static std::atomic<uint32_t> s_reported{ 0 };
            const uint32_t reported = s_reported.fetch_add(1, std::memory_order_relaxed) + 1;
            if (reported == 1 || reported % 100 == 0)
            {
                ME_LOG(
                    LogRHI,
                    Warn,
                    "OpenGL {} update of {} byte(s) dropped: the resource has no native object yet. "
                    "It is created at the start of the next render frame; re-issue the update after "
                    "that. {} such update(s) so far.{}",
                    kind,
                    byteSize,
                    reported,
                    DescribeCreatingThreadAndSite());
            }
        }

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
            EnsureOpenGLFunctionTableLoaded();
            // author sampling lives on RHISampler (glBindSampler). Texture object
            // keeps only attachment-safe defaults; depth still needs NEAREST for FBO completeness.
            (void)generateMipmaps;
            if (IsDepthLikeTexture(format, usage))
            {
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
            }
            else
            {
                glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        GLuint UploadTexture2D(const RHITextureCreateDesc& desc, const void* initialData)
        {
            EnsureOpenGLFunctionTableLoaded();
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
            EnsureOpenGLFunctionTableLoaded();
            if (desc.Width == 0 || desc.Height == 0)
            {
                ME_LOG(LogRHI, Error, "OpenGLRHITexture cube: Width/Height must be > 0.");
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
                ME_LOG(LogRHI, Error, "OpenGLRHITexture cube: expected 6 face pointers, got {}.", faceData.size());
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
                ME_LOG(LogRHI, Error, "OpenGLRHITexture cube: unsupported color format.");
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
                    ME_LOG(LogRHI, Error, "OpenGLRHITexture cube: color face {} is null.", faceIndex);
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
            EnsureOpenGLFunctionTableLoaded();
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
            EnsureOpenGLFunctionTableLoaded();
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
            EnsureOpenGLFunctionTableLoaded();
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
            EnsureOpenGLFunctionTableLoaded();
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

        bool SpecializeSpirvStage(
            GLuint shaderObject,
            GLenum stage,
            const std::vector<uint32_t>& spirvWords,
            std::string& outLog)
        {
            EnsureOpenGLFunctionTableLoaded();
            if (glShaderBinary == nullptr || glSpecializeShader == nullptr)
            {
                outLog = "OpenGL SPIR-V entry points unavailable (need GL 4.6 / ARB_gl_spirv).";
                return false;
            }

            if (spirvWords.empty())
            {
                outLog = "Empty SPIR-V module.";
                return false;
            }

            const GLsizei binarySize = static_cast<GLsizei>(spirvWords.size() * sizeof(uint32_t));
            glShaderBinary(
                1,
                &shaderObject,
                GL_SHADER_BINARY_FORMAT_SPIR_V,
                spirvWords.data(),
                binarySize);
            glSpecializeShader(shaderObject, "main", 0, nullptr, nullptr);

            int compileStatus = GL_FALSE;
            glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &compileStatus);
            if (compileStatus == GL_TRUE)
            {
                return true;
            }

            outLog = ReadShaderInfoLog(shaderObject);
            if (outLog.empty())
            {
                outLog = stage == GL_VERTEX_SHADER ? "Vertex SPIR-V specialize failed."
                                                  : "Fragment SPIR-V specialize failed.";
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
        EnsureOpenGLFunctionTableLoaded();
        if (m_OwnsGlTexture && m_TextureId != 0)
        {
            glDeleteTextures(1, &m_TextureId);
            m_TextureId = 0;
        }
    }

    OpenGLRHITexture::OpenGLRHITexture(const RHITextureCreateDesc& desc, const void* initialData)
        : m_Desc(desc)
    {
        ConfigureTargetFromDescription();
        UploadNow(initialData);
    }

    OpenGLRHITexture::OpenGLRHITexture(
        const RHITextureCreateDesc& desc,
        const void* initialData,
        DeferredNativeCreate)
        : m_Desc(desc)
    {
        ConfigureTargetFromDescription();
        CapturePendingPixels(initialData);
    }

    void OpenGLRHITexture::ConfigureTargetFromDescription()
    {
        switch (m_Desc.Dimension)
        {
        case RHITextureDimension::Texture2DArray:
            m_Target = GL_TEXTURE_2D_ARRAY;
            break;
        case RHITextureDimension::TextureCube:
            m_Target = GL_TEXTURE_CUBE_MAP;
            break;
        case RHITextureDimension::Texture2D:
        default:
            m_Target = GL_TEXTURE_2D;
            break;
        }
    }

    void OpenGLRHITexture::CapturePendingPixels(const void* initialData)
    {
        if (initialData == nullptr)
        {
            return;
        }

        // Only mip 0 is ever uploaded from creation data, so only mip 0 is copied.
        const uint32_t imageBytes =
            m_Desc.Width * m_Desc.Height * GetTextureFormatByteSize(m_Desc.Format);
        if (imageBytes == 0)
        {
            return;
        }

        switch (m_Desc.Dimension)
        {
        case RHITextureDimension::TextureCube:
        {
            // Cube creation data is an array of face pointers, not one contiguous image.
            const uint32_t faceCount = m_Desc.DepthOrArrayLayers;
            const auto* facePointers = static_cast<const unsigned char* const*>(initialData);
            m_PendingPixels.Bytes.resize(static_cast<size_t>(imageBytes) * faceCount);
            // Null faces stay null so the upload still reports them as it does today.
            m_PendingPixels.CubeFaces.assign(faceCount, nullptr);
            // The copy is complete before any pointer into it is taken, so it never reallocates
            // underneath the face table.
            for (uint32_t faceIndex = 0; faceIndex < faceCount; ++faceIndex)
            {
                const unsigned char* face = facePointers[faceIndex];
                if (face == nullptr)
                {
                    continue;
                }
                unsigned char* slot =
                    m_PendingPixels.Bytes.data() + static_cast<size_t>(imageBytes) * faceIndex;
                std::memcpy(slot, face, imageBytes);
                m_PendingPixels.CubeFaces[faceIndex] = slot;
            }
            break;
        }
        case RHITextureDimension::Texture2DArray:
        {
            const uint32_t layerCount = m_Desc.DepthOrArrayLayers == 0 ? 1u : m_Desc.DepthOrArrayLayers;
            const size_t totalBytes = static_cast<size_t>(imageBytes) * layerCount;
            m_PendingPixels.Bytes.resize(totalBytes);
            std::memcpy(m_PendingPixels.Bytes.data(), initialData, totalBytes);
            break;
        }
        case RHITextureDimension::Texture2D:
        default:
        {
            m_PendingPixels.Bytes.resize(imageBytes);
            std::memcpy(m_PendingPixels.Bytes.data(), initialData, imageBytes);
            break;
        }
        }
    }

    void OpenGLRHITexture::UploadNow(const void* initialData)
    {
        switch (m_Desc.Dimension)
        {
        case RHITextureDimension::Texture2DArray:
            m_TextureId = UploadTexture2DArray(m_Desc, initialData);
            break;
        case RHITextureDimension::TextureCube:
            m_TextureId = UploadTextureCube(m_Desc, initialData);
            break;
        case RHITextureDimension::Texture2D:
        default:
            m_TextureId = UploadTexture2D(m_Desc, initialData);
            break;
        }
        m_OwnsGlTexture = m_TextureId != 0;
    }

    bool OpenGLRHITexture::CreateNativeResources()
    {
        if (m_TextureId != 0)
        {
            return true;
        }

        EnsureOpenGLFunctionTableLoaded();
        if (!HasCurrentGraphicsContext())
        {
            ReportMissingGraphicsContext(
                "texture",
                static_cast<uint32_t>(m_PendingPixels.Bytes.size()),
                this);
            return false;
        }

        const void* pixels = nullptr;
        if (!m_PendingPixels.Bytes.empty())
        {
            pixels = m_Desc.Dimension == RHITextureDimension::TextureCube
                ? static_cast<const void*>(m_PendingPixels.CubeFaces.data())
                : static_cast<const void*>(m_PendingPixels.Bytes.data());
        }

        UploadNow(pixels);
        if (m_TextureId == 0)
        {
            return false;
        }

        std::vector<unsigned char>().swap(m_PendingPixels.Bytes);
        m_PendingPixels.CubeFaces.clear();
        return true;
    }

    void* OpenGLRHITexture::GetNativeResource() const
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_TextureId));
    }

    OpenGLRHIBuffer::OpenGLRHIBuffer(
        const RHIBufferCreateDesc& desc,
        const void* initialData)
        : m_Desc(desc)
    {
        EnsureOpenGLFunctionTableLoaded();
        ConfigureBindingTarget();
        CreateNativeBuffer(initialData);
    }

    OpenGLRHIBuffer::OpenGLRHIBuffer(
        const RHIBufferCreateDesc& desc,
        const void* initialData,
        DeferredNativeCreate)
        : m_Desc(desc)
    {
        EnsureOpenGLFunctionTableLoaded();
        ConfigureBindingTarget();
        if (initialData != nullptr && desc.ByteSize > 0)
        {
            const auto* bytes = static_cast<const unsigned char*>(initialData);
            m_PendingUpload.assign(bytes, bytes + desc.ByteSize);
        }
    }

    void OpenGLRHIBuffer::ConfigureBindingTarget()
    {
        switch (m_Desc.Usage)
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
    }

    bool OpenGLRHIBuffer::CreateNativeBuffer(const void* data)
    {
        // Per-function guard, not inherited from the caller: the function-table load has to happen
        // on whichever path reaches a driver entry point first, and a null table pointer is a crash
        // rather than an error return.
        EnsureOpenGLFunctionTableLoaded();
        if (!HasCurrentGraphicsContext())
        {
            ReportMissingGraphicsContext("buffer", m_Desc.ByteSize, this);
            return false;
        }

        glGenBuffers(1, &m_BufferId);
        glBindBuffer(m_Target, m_BufferId);
        glBufferData(m_Target, m_Desc.ByteSize, data, GL_STATIC_DRAW);
        glBindBuffer(m_Target, 0);
        return m_BufferId != 0;
    }

    bool OpenGLRHIBuffer::CreateNativeResources()
    {
        if (m_BufferId != 0)
        {
            return true;
        }

        EnsureOpenGLFunctionTableLoaded();
        const void* data = m_PendingUpload.empty() ? nullptr : m_PendingUpload.data();
        if (!CreateNativeBuffer(data))
        {
            return false;
        }

        std::vector<unsigned char>().swap(m_PendingUpload);
        return true;
    }

    OpenGLRHIBuffer::~OpenGLRHIBuffer()
    {
        if (m_BufferId == 0)
        {
            return;
        }

        // The GL name outlives this wrapper. Whoever dropped the last reference did so on a thread
        // that may hold no context and is still recording commands that bind this name, so the
        // delete belongs to the RHI role, once the batch recorded against it has been submitted.
        const GLuint bufferId = m_BufferId;
        m_BufferId = 0;
        PostNativeDestroy(
            [bufferId]() {
                // The queue may drain before any other GL call of this session loaded the entry
                // points into this module's table.
                EnsureOpenGLFunctionTableLoaded();
                if (!HasCurrentGraphicsContext())
                {
                    return false;
                }
                glDeleteBuffers(1, &bufferId);
                return true;
            },
            "openGL buffer");
    }

    void OpenGLRHIBuffer::UpdateSubresource(const void* data, uint32_t offset, uint32_t size)
    {
        EnsureOpenGLFunctionTableLoaded();
        if (!data || size == 0)
        {
            return;
        }

        if (m_BufferId == 0)
        {
            ReportUploadBeforeNativeObject("buffer", size);
            return;
        }

        glBindBuffer(m_Target, m_BufferId);
        glBufferSubData(m_Target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
        glBindBuffer(m_Target, 0);
    }

    OpenGLRHIShader::~OpenGLRHIShader()
    {
        EnsureOpenGLFunctionTableLoaded();
        if (m_ProgramId != 0)
        {
            glDeleteProgram(m_ProgramId);
            m_ProgramId = 0;
        }
    }

    OpenGLRHIShader::OpenGLRHIShader(std::string_view vertexSource, std::string_view fragmentSource)
    {
        // Direct shader construction is the second structural GL entry. It is
        // reached WITHOUT OpenGLRHI::Initialize() by EngineShaderUtils::TryCompileSourcesOnGpu
        // (the material smoke path in minEngineTests), which constructs this class directly.
        EnsureOpenGLFunctionTableLoaded();
        CompileFromSources(vertexSource, fragmentSource);
    }

    OpenGLRHIShader::OpenGLRHIShader(
        std::string_view vertexSource,
        std::string_view fragmentSource,
        DeferredNativeCreate)
    {
        // The sources are copied; compiling them drives the driver, which the graphics role does.
        m_PendingVertexSource.assign(vertexSource);
        m_PendingFragmentSource.assign(fragmentSource);
    }

    bool OpenGLRHIShader::CompileFromSources(
        std::string_view vertexSource,
        std::string_view fragmentSource)
    {
        EnsureOpenGLFunctionTableLoaded();
        const unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        std::string stageLog;
        if (!CompileShaderStage(vertexShader, GL_VERTEX_SHADER, vertexSource, stageLog))
        {
            ReportCreateFailure("Vertex shader compile error:\n" + stageLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }

        if (!CompileShaderStage(fragmentShader, GL_FRAGMENT_SHADER, fragmentSource, stageLog))
        {
            ReportCreateFailure("Fragment shader compile error:\n" + stageLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }

        if (!LinkProgram(vertexShader, fragmentShader))
        {
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return true;
    }

    OpenGLRHIShader::OpenGLRHIShader(const RHIShaderCreateDesc& desc)
    {
        EnsureOpenGLFunctionTableLoaded();

        const std::vector<uint32_t>* vertexSpirv = nullptr;
        const std::vector<uint32_t>* fragmentSpirv = nullptr;
        for (const RHIShaderStageBytecode& stage : desc.Stages)
        {
            if (stage.Stage == RHIGraphicsShaderStage::Vertex)
            {
                vertexSpirv = &stage.SpirvWords;
            }
            else if (stage.Stage == RHIGraphicsShaderStage::Pixel)
            {
                fragmentSpirv = &stage.SpirvWords;
            }
        }

        if (vertexSpirv == nullptr || fragmentSpirv == nullptr)
        {
            ReportCreateFailure("RHIShaderCreateDesc requires Vertex and Pixel SPIR-V stages.");
            return;
        }

        if (SpecializeFromSpirv(*vertexSpirv, *fragmentSpirv) && !desc.DebugName.empty())
        {
            ME_LOG(LogRHI, Info, "OpenGLRHIShader: loaded SPIR-V program '{}'", desc.DebugName);
        }
    }

    OpenGLRHIShader::OpenGLRHIShader(const RHIShaderCreateDesc& desc, DeferredNativeCreate)
        : m_PendingDesc(desc)
    {
        // Nothing runs here: the bytecode is copied and both stages are specialized by the
        // graphics role, where the context is the one this program will live on.
    }

    bool OpenGLRHIShader::CreateNativeResources()
    {
        if (m_ProgramId != 0)
        {
            return true;
        }
        if (m_bCreateFailed.load(std::memory_order_acquire))
        {
            return false;
        }

        EnsureOpenGLFunctionTableLoaded();

        if (!m_PendingDesc.Stages.empty())
        {
            const std::vector<uint32_t>* vertexSpirv = nullptr;
            const std::vector<uint32_t>* fragmentSpirv = nullptr;
            for (const RHIShaderStageBytecode& stage : m_PendingDesc.Stages)
            {
                if (stage.Stage == RHIGraphicsShaderStage::Vertex)
                {
                    vertexSpirv = &stage.SpirvWords;
                }
                else if (stage.Stage == RHIGraphicsShaderStage::Pixel)
                {
                    fragmentSpirv = &stage.SpirvWords;
                }
            }

            const std::string debugName = m_PendingDesc.DebugName;
            if (vertexSpirv == nullptr || fragmentSpirv == nullptr)
            {
                DropPendingData();
                ReportCreateFailure("RHIShaderCreateDesc requires Vertex and Pixel SPIR-V stages.");
                return false;
            }

            const bool bCreated = SpecializeFromSpirv(*vertexSpirv, *fragmentSpirv);
            DropPendingData();
            if (bCreated && !debugName.empty())
            {
                ME_LOG(LogRHI, Info, "OpenGLRHIShader: loaded SPIR-V program '{}'", debugName);
            }
            return bCreated;
        }

        if (!m_PendingVertexSource.empty() || !m_PendingFragmentSource.empty())
        {
            const std::string vertexSource = std::move(m_PendingVertexSource);
            const std::string fragmentSource = std::move(m_PendingFragmentSource);
            DropPendingData();
            return CompileFromSources(vertexSource, fragmentSource);
        }

        ReportCreateFailure("OpenGLRHIShader: nothing to create the program from.");
        return false;
    }

    bool OpenGLRHIShader::SpecializeFromSpirv(
        const std::vector<uint32_t>& vertexSpirv,
        const std::vector<uint32_t>& fragmentSpirv)
    {
        EnsureOpenGLFunctionTableLoaded();
        const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        std::string stageLog;
        if (!SpecializeSpirvStage(vertexShader, GL_VERTEX_SHADER, vertexSpirv, stageLog))
        {
            ReportCreateFailure("Vertex SPIR-V error:\n" + stageLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }

        if (!SpecializeSpirvStage(fragmentShader, GL_FRAGMENT_SHADER, fragmentSpirv, stageLog))
        {
            ReportCreateFailure("Fragment SPIR-V error:\n" + stageLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }

        if (!LinkProgram(vertexShader, fragmentShader))
        {
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return false;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return true;
    }

    void OpenGLRHIShader::DropPendingData()
    {
        // Whatever was captured for this creation has been used: keeping the bytecode of a
        // program that exists would only hold a copy of the engine's shader assets.
        m_PendingDesc = RHIShaderCreateDesc{};
        m_PendingVertexSource.clear();
        m_PendingFragmentSource.clear();
        // A string that is already empty keeps its allocation; release it with the data.
        std::string().swap(m_PendingVertexSource);
        std::string().swap(m_PendingFragmentSource);
    }

    void OpenGLRHIShader::ReportCreateFailure(std::string message)
    {
        if (m_bCreateFailed.load(std::memory_order_acquire))
        {
            return;
        }

        m_CompileLog = std::move(message);
        m_bCreateFailed.store(true, std::memory_order_release);
        // The one report this failure gets: it is made on the thread that asked the driver, and
        // the caller on the game role can only read the handle afterwards.
        ME_LOG(LogRHI, Error, "{}", m_CompileLog);
    }

    bool OpenGLRHIShader::LinkProgram(GLuint vertexShader, GLuint fragmentShader)
    {
        EnsureOpenGLFunctionTableLoaded();
        m_ProgramId = glCreateProgram();
        glAttachShader(m_ProgramId, vertexShader);
        glAttachShader(m_ProgramId, fragmentShader);
        glLinkProgram(m_ProgramId);

        int linkStatus = GL_FALSE;
        glGetProgramiv(m_ProgramId, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE)
        {
            const std::string programLog = "Shader program link error:\n" + ReadProgramInfoLog(m_ProgramId);
            glDeleteProgram(m_ProgramId);
            m_ProgramId = 0;
            ReportCreateFailure(programLog);
            return false;
        }

        m_bIsValid.store(true, std::memory_order_release);
        return true;
    }

    OpenGLRHIVertexInputLayout::OpenGLRHIVertexInputLayout(
        std::initializer_list<RHIVertexElement> elements)
    {
        EnsureOpenGLFunctionTableLoaded();
        BuildElements(elements);
        CreateVertexArray();
    }

    OpenGLRHIVertexInputLayout::OpenGLRHIVertexInputLayout(
        std::initializer_list<RHIVertexElement> elements,
        DeferredNativeCreate)
    {
        EnsureOpenGLFunctionTableLoaded();
        // The attribute table is CPU-side data and is complete before this returns; only the
        // vertex array object needs the graphics role.
        BuildElements(elements);
    }

    void OpenGLRHIVertexInputLayout::BuildElements(std::initializer_list<RHIVertexElement> elements)
    {
        uint32_t offset = 0;
        uint32_t denseFallback = 0;
        for (const auto& element : elements)
        {
            RHIVertexElement copy = element;
            copy.Offset = offset;
            offset += VertexElementTypeSize(element.Type);
            if (copy.Location == VertexAttr::kUnsetLocation)
            {
                copy.Location = denseFallback;
            }
            ++denseFallback;
            m_Elements.push_back(copy);
        }
        m_Stride = offset;
    }

    bool OpenGLRHIVertexInputLayout::CreateVertexArray()
    {
        if (m_VAO != 0)
        {
            return true;
        }

        EnsureOpenGLFunctionTableLoaded();
        if (!HasCurrentGraphicsContext())
        {
            ReportMissingGraphicsContext("vertex input layout", m_Stride, this);
            return false;
        }

        glGenVertexArrays(1, &m_VAO);
        return m_VAO != 0;
    }

    bool OpenGLRHIVertexInputLayout::CreateNativeResources()
    {
        return CreateVertexArray();
    }

    OpenGLRHIVertexInputLayout::~OpenGLRHIVertexInputLayout()
    {
        if (m_VAO == 0)
        {
            return;
        }

        // Same hand-off as the buffer: deleting a vertex array object needs the drawing context,
        // and the layout may be released by a thread that never had it current, so the queue of
        // the live device takes it and the RHI role deletes it after the batch is submitted.
        const GLuint vertexArrayId = m_VAO;
        m_VAO = 0;
        PostNativeDestroy(
            [vertexArrayId]() {
                EnsureOpenGLFunctionTableLoaded();
                if (!HasCurrentGraphicsContext())
                {
                    return false;
                }
                glDeleteVertexArrays(1, &vertexArrayId);
                return true;
            },
            "openGL vertex input layout");
    }

    void OpenGLRHIVertexInputLayout::BindVertexBuffer(GLuint bufferId)
    {
        EnsureOpenGLFunctionTableLoaded();
        if (m_VAO == 0)
        {
            return;
        }

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, bufferId);

        for (const auto& element : m_Elements)
        {
            const GLuint location = element.Location;
            glEnableVertexAttribArray(location);
            const void* offsetPtr =
                reinterpret_cast<const void*>(static_cast<uintptr_t>(element.Offset));
            // ivec* shader inputs require IPointer; Pointer + GL_INT converts to float and
            // breaks bone indices (bind pose still looks fine because every skin matrix is I).
            if (IsIntegerVertexElementType(element.Type))
            {
                glVertexAttribIPointer(
                    location,
                    element.Size,
                    VertexElementTypeToGL(element.Type),
                    m_Stride,
                    offsetPtr);
            }
            else
            {
                glVertexAttribPointer(
                    location,
                    element.Size,
                    VertexElementTypeToGL(element.Type),
                    element.bNormalized ? GL_TRUE : GL_FALSE,
                    m_Stride,
                    offsetPtr);
            }
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

    namespace
    {
        GLint ToGLWrap(RHISamplerAddress address)
        {
            switch (address)
            {
            case RHISamplerAddress::MirroredRepeat:
                return GL_MIRRORED_REPEAT;
            case RHISamplerAddress::ClampToEdge:
                return GL_CLAMP_TO_EDGE;
            case RHISamplerAddress::ClampToBorder:
                return GL_CLAMP_TO_BORDER;
            case RHISamplerAddress::Repeat:
            default:
                return GL_REPEAT;
            }
        }
    }

    OpenGLRHISampler::OpenGLRHISampler(const RHISamplerDesc& desc)
        : m_Desc(desc)
    {
        EnsureOpenGLFunctionTableLoaded();
        ConfigureSampler();
    }

    OpenGLRHISampler::OpenGLRHISampler(const RHISamplerDesc& desc, DeferredNativeCreate)
        : m_Desc(desc)
    {
        // glGenSamplers and the parameter calls need the context, so the graphics role runs them.
    }

    bool OpenGLRHISampler::CreateNativeResources()
    {
        if (m_SamplerId != 0)
        {
            return true;
        }

        EnsureOpenGLFunctionTableLoaded();
        ConfigureSampler();
        return m_SamplerId != 0;
    }

    void OpenGLRHISampler::ConfigureSampler()
    {
        const RHISamplerDesc& desc = m_Desc;
        EnsureOpenGLFunctionTableLoaded();
        glGenSamplers(1, &m_SamplerId);
        const GLint mag = (desc.MagFilter == RHISamplerFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
        // Mip mapping is selected at bind time via texture NumMips; sampler stores base Min.
        // When the bound texture has mips, Material/Proxy should request LinearMipmap via desc
        // expansion — MVP uses Linear/Nearest here; GenerateMips no longer bakes tex params.
        const GLint min = (desc.MinFilter == RHISamplerFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
        glSamplerParameteri(m_SamplerId, GL_TEXTURE_MAG_FILTER, mag);
        glSamplerParameteri(m_SamplerId, GL_TEXTURE_MIN_FILTER, min);
        glSamplerParameteri(m_SamplerId, GL_TEXTURE_WRAP_S, ToGLWrap(desc.AddressU));
        glSamplerParameteri(m_SamplerId, GL_TEXTURE_WRAP_T, ToGLWrap(desc.AddressV));
        glSamplerParameteri(m_SamplerId, GL_TEXTURE_WRAP_R, ToGLWrap(desc.AddressW));
        if (desc.AddressU == RHISamplerAddress::ClampToBorder
            || desc.AddressV == RHISamplerAddress::ClampToBorder
            || desc.AddressW == RHISamplerAddress::ClampToBorder)
        {
            const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
            glSamplerParameterfv(m_SamplerId, GL_TEXTURE_BORDER_COLOR, borderColor);
        }
    }

    OpenGLRHISampler::~OpenGLRHISampler()
    {
        EnsureOpenGLFunctionTableLoaded();
        if (m_SamplerId != 0)
        {
            glDeleteSamplers(1, &m_SamplerId);
            m_SamplerId = 0;
        }
    }
}