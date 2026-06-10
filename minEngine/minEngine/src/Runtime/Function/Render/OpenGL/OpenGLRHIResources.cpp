#include "OpenGLRHIResources.h"

#include "OpenGLShader.h"
#include "OpenGLTexture.h"
#include "glad/glad.h"

namespace minEngine
{
    namespace
    {
        OpenGLTextureUploadDesc ToLegacyTextureDesc(const RHITextureCreateDesc& desc)
        {
            OpenGLTextureUploadDesc legacy;
            legacy.Width = desc.Width;
            legacy.Height = desc.Height;
            legacy.Layers = desc.DepthOrArrayLayers;
            legacy.Format = desc.Format;
            if ((desc.Flags & RHITextureCreateFlags::RenderTarget) != RHITextureCreateFlags::None)
            {
                if (desc.Format == TextureFormat::DEPTH24STENCIL8 ||
                    desc.Format == TextureFormat::DEPTH16 ||
                    desc.Format == TextureFormat::DEPTH24 ||
                    desc.Format == TextureFormat::DEPTH32)
                {
                    legacy.Usage = TextureUsage::DepthStencil;
                }
                else
                {
                    legacy.Usage = TextureUsage::Color;
                }
            }
            else
            {
                legacy.Usage = TextureUsage::TextureBinding;
            }
            return legacy;
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

    OpenGLRHITexture::~OpenGLRHITexture() = default;

    OpenGLRHITexture::OpenGLRHITexture(const RHITextureCreateDesc& desc, const void* initialData)
        : m_Desc(desc)
    {
        OpenGLTextureUploadDesc legacy = ToLegacyTextureDesc(desc);
        switch (desc.Dimension)
        {
        case RHITextureDimension::Texture2DArray:
            m_Target = GL_TEXTURE_2D_ARRAY;
            m_OwningUploadTexture2DArray =
                std::make_shared<OpenGLTexture2DArray>(static_cast<const unsigned char*>(initialData), legacy);
            m_TextureId = m_OwningUploadTexture2DArray ? m_OwningUploadTexture2DArray->GetID() : 0;
            break;
        case RHITextureDimension::TextureCube:
        {
            m_Target = GL_TEXTURE_CUBE_MAP;
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
            m_OwningUploadTextureCube =
                std::make_shared<OpenGLTextureCube>(faceData, legacy, false);
            m_TextureId = m_OwningUploadTextureCube ? m_OwningUploadTextureCube->GetID() : 0;
            break;
        }
        case RHITextureDimension::Texture2D:
        default:
            m_Target = GL_TEXTURE_2D;
            if (desc.Format == TextureFormat::RGB16F || desc.Format == TextureFormat::RGBA16F)
            {
                m_OwningUploadTexture2D =
                    std::make_shared<OpenGLTexture2D>(static_cast<const float*>(initialData), legacy);
            }
            else
            {
                m_OwningUploadTexture2D =
                    std::make_shared<OpenGLTexture2D>(static_cast<const unsigned char*>(initialData), legacy);
            }
            m_TextureId = m_OwningUploadTexture2D ? m_OwningUploadTexture2D->GetID() : 0;
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

    OpenGLRHIShader::OpenGLRHIShader(std::shared_ptr<OpenGLShader> shader)
        : m_Shader(std::move(shader))
    {
    }

    bool OpenGLRHIShader::IsValid() const
    {
        return m_Shader && m_Shader->IsValid();
    }

    const std::string& OpenGLRHIShader::GetCompileLog() const
    {
        static const std::string kEmpty;
        return m_Shader ? m_Shader->GetCompileLog() : kEmpty;
    }

    GLuint OpenGLRHIShader::GetProgramId() const
    {
        return m_Shader ? m_Shader->m_ID : 0;
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

    std::shared_ptr<OpenGLRHIVertexInputLayout> OpenGLRHIVertexInputLayout::FromLegacyVAO(
        GLuint vao,
        const std::vector<RHIVertexElement>& elements,
        uint32_t stride)
    {
        auto wrapped = std::make_shared<OpenGLRHIVertexInputLayout>();
        wrapped->m_Elements = elements;
        wrapped->m_Stride = stride;
        wrapped->m_VAO = vao;
        return wrapped;
    }

    OpenGLRHIShaderResourceView::OpenGLRHIShaderResourceView(const RHITextureSRVDesc& desc)
        : m_Desc(desc)
    {
    }

    OpenGLRHIBindingLayout::OpenGLRHIBindingLayout(std::vector<RHIBindingLayoutEntry> entries)
        : m_Entries(std::move(entries))
    {
    }

    OpenGLRHIBindingSet::OpenGLRHIBindingSet(RHIBindingLayout* layout, std::vector<RHIBindingResource> resources)
        : m_Layout(layout)
        , m_Resources(std::move(resources))
    {
    }
}
