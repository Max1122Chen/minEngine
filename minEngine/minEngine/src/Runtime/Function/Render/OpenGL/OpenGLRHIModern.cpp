#include "OpenGLRHIModern.h"

#include "OpenGLBuffers.h"
#include "OpenGLShader.h"
#include "OpenGLTexture.h"
#include "OpenGLVertexArrayObject.h"
#include "glad/glad.h"

namespace minEngine
{
    namespace
    {
        RHITextureDesc ToLegacyTextureDesc(const RHITextureCreateDesc& desc)
        {
            RHITextureDesc legacy;
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
        , m_Target(GL_TEXTURE_2D)
    {
        RHITextureDesc legacy = ToLegacyTextureDesc(desc);
        if (desc.Format == TextureFormat::RGB16F || desc.Format == TextureFormat::RGBA16F)
        {
            m_OwningLegacyTexture =
                std::make_shared<OpenGLTexture2D>(static_cast<const float*>(initialData), legacy);
        }
        else
        {
            m_OwningLegacyTexture =
                std::make_shared<OpenGLTexture2D>(static_cast<const unsigned char*>(initialData), legacy);
        }
        m_TextureId = m_OwningLegacyTexture ? m_OwningLegacyTexture->GetID() : 0;
        m_OwnsGlTexture = m_TextureId != 0;
    }

    std::shared_ptr<OpenGLRHITexture> OpenGLRHITexture::WrapLegacy2D(const std::shared_ptr<RHITexture2D>& legacy)
    {
        if (!legacy || legacy->GetID() == 0)
        {
            return nullptr;
        }
        RHITextureCreateDesc desc;
        desc.Dimension = RHITextureDimension::Texture2D;
        desc.Width = legacy->GetWidth();
        desc.Height = legacy->GetHeight();
        desc.DepthOrArrayLayers = 1;
        desc.Format = legacy->GetFormat();
        desc.Flags = RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource;

        auto wrapped = std::shared_ptr<OpenGLRHITexture>(new OpenGLRHITexture(desc, nullptr));
        wrapped->m_OwningLegacyTexture.reset();
        wrapped->m_OwnsGlTexture = false;
        wrapped->m_TextureId = legacy->GetID();
        return wrapped;
    }

    void* OpenGLRHITexture::GetNativeResource() const
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(m_TextureId));
    }

    std::shared_ptr<OpenGLRHITexture> OpenGLRHITexture::WrapLegacy2DArray(
        const std::shared_ptr<RHITexture2DArray>& legacy,
        uint32_t arrayLayer)
    {
        if (!legacy || legacy->GetID() == 0)
        {
            return nullptr;
        }
        RHITextureCreateDesc desc;
        desc.Dimension = RHITextureDimension::Texture2DArray;
        desc.Width = legacy->GetWidth();
        desc.Height = legacy->GetHeight();
        desc.DepthOrArrayLayers = legacy->GetLayers();
        desc.Format = legacy->GetFormat();
        desc.Flags = RHITextureCreateFlags::RenderTarget;

        auto wrapped = std::shared_ptr<OpenGLRHITexture>(new OpenGLRHITexture(desc, nullptr));
        wrapped->m_OwningLegacyTexture.reset();
        wrapped->m_OwnsGlTexture = false;
        wrapped->m_TextureId = legacy->GetID();
        wrapped->m_Target = GL_TEXTURE_2D_ARRAY;
        wrapped->m_ArrayLayer = static_cast<int32_t>(arrayLayer);
        return wrapped;
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

    std::shared_ptr<OpenGLRHIBuffer> OpenGLRHIBuffer::WrapLegacyVertexBuffer(VertexBuffer* legacy)
    {
        if (!legacy)
        {
            return nullptr;
        }
        return WrapLegacyVertexBuffer(std::shared_ptr<VertexBuffer>(legacy, [](VertexBuffer*) {}));
    }

    std::shared_ptr<OpenGLRHIBuffer> OpenGLRHIBuffer::WrapLegacyVertexBuffer(const std::shared_ptr<VertexBuffer>& legacy)
    {
        auto* glBuffer = dynamic_cast<OpenGLVertexBuffer*>(legacy.get());
        if (!glBuffer)
        {
            return nullptr;
        }
        RHIBufferCreateDesc desc;
        desc.Usage = RHIBufferUsage::Vertex;
        desc.ElementCount = glBuffer->GetNumVertices();
        auto wrapped = std::shared_ptr<OpenGLRHIBuffer>(new OpenGLRHIBuffer(desc, nullptr));
        wrapped->m_BufferId = glBuffer->GetVBO();
        wrapped->m_Target = GL_ARRAY_BUFFER;
        return wrapped;
    }

    std::shared_ptr<OpenGLRHIBuffer> OpenGLRHIBuffer::WrapLegacyIndexBuffer(IndexBuffer* legacy)
    {
        if (!legacy)
        {
            return nullptr;
        }
        return WrapLegacyIndexBuffer(std::shared_ptr<IndexBuffer>(legacy, [](IndexBuffer*) {}));
    }

    std::shared_ptr<OpenGLRHIBuffer> OpenGLRHIBuffer::WrapLegacyIndexBuffer(const std::shared_ptr<IndexBuffer>& legacy)
    {
        auto* glBuffer = dynamic_cast<OpenGLIndexBuffer*>(legacy.get());
        if (!glBuffer)
        {
            return nullptr;
        }
        RHIBufferCreateDesc desc;
        desc.Usage = RHIBufferUsage::Index;
        desc.ElementCount = glBuffer->GetNumIndices();
        auto wrapped = std::shared_ptr<OpenGLRHIBuffer>(new OpenGLRHIBuffer(desc, nullptr));
        wrapped->m_BufferId = glBuffer->GetEBO();
        wrapped->m_Target = GL_ELEMENT_ARRAY_BUFFER;
        return wrapped;
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

    std::shared_ptr<OpenGLRHIVertexInputLayout> OpenGLRHIVertexInputLayout::WrapLegacyVertexDefinition(
        VertexDefinition* legacy)
    {
        if (!legacy)
        {
            return nullptr;
        }
        return WrapLegacyVertexDefinition(std::shared_ptr<VertexDefinition>(legacy, [](VertexDefinition*) {}));
    }

    std::shared_ptr<OpenGLRHIVertexInputLayout> OpenGLRHIVertexInputLayout::WrapLegacyVertexDefinition(
        const std::shared_ptr<VertexDefinition>& legacy)
    {
        auto* vao = dynamic_cast<OpenGLVertexArrayObject*>(legacy.get());
        if (!vao)
        {
            return nullptr;
        }
        return FromLegacyVAO(vao->GetVAO(), legacy->GetElements(), legacy->GetStride());
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
