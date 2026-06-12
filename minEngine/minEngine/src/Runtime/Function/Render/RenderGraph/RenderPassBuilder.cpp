#include "Render/RenderGraph/RenderPassBuilder.h"

#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"

namespace minEngine
{
    RenderPassBuilder::RenderPassBuilder(RenderGraph& graph, RenderPass& pass)
        : m_Graph(graph)
        , m_Pass(pass)
    {
    }

    RDGTextureRef RenderPassBuilder::DeclareAccess(
        const char* name,
        RDGPassResourceAccessType accessType,
        const RDGTextureDesc* desc)
    {
        RDGTextureDesc textureDesc{};
        if (desc != nullptr)
        {
            textureDesc = *desc;
        }

        RDGTextureRef textureRef = m_Graph.FindOrRegisterTexture(name, textureDesc, false);

        PassResourceAccess access;
        access.TextureName = name != nullptr ? name : "";
        access.AccessType = accessType;
        switch (accessType)
        {
        case RDGPassResourceAccessType::ColorOutput:
            access.UsageHint = RDGTextureUsage::RenderTarget;
            break;
        case RDGPassResourceAccessType::DepthStencilOutput:
            access.UsageHint = RDGTextureUsage::DepthWrite;
            break;
        case RDGPassResourceAccessType::TextureInput:
            access.UsageHint = RDGTextureUsage::ShaderResource;
            break;
        case RDGPassResourceAccessType::DepthStencilInput:
            access.UsageHint = RDGTextureUsage::DepthRead;
            break;
        }

        m_Pass.AddDeclaredAccess(std::move(access));
        return textureRef;
    }

    RDGTextureRef RenderPassBuilder::AddColorOutput(const char* name, const RDGTextureDesc& desc)
    {
        return DeclareAccess(name, RDGPassResourceAccessType::ColorOutput, &desc);
    }

    RDGTextureRef RenderPassBuilder::AddTextureInput(const char* name)
    {
        return DeclareAccess(name, RDGPassResourceAccessType::TextureInput, nullptr);
    }

    RDGTextureRef RenderPassBuilder::SetDepthStencilOutput(const char* name, const RDGTextureDesc& desc)
    {
        return DeclareAccess(name, RDGPassResourceAccessType::DepthStencilOutput, &desc);
    }

    RDGTextureRef RenderPassBuilder::SetDepthStencilInput(const char* name)
    {
        return DeclareAccess(name, RDGPassResourceAccessType::DepthStencilInput, nullptr);
    }

    RDGTextureRef RenderPassBuilder::UseTexture(const char* name)
    {
        return AddTextureInput(name);
    }
}
