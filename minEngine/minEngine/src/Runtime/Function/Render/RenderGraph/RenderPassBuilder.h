#pragma once

#include "Core.h"
#include "Render/RenderGraph/RDGTexture.h"

namespace minEngine
{
    class RenderGraph;
    class RenderPass;

    enum class RDGPassResourceAccessType : uint8_t
    {
        ColorOutput,
        DepthStencilOutput,
        TextureInput,
        DepthStencilInput,
    };

    struct PassResourceAccess
    {
        std::string TextureName;
        RDGPassResourceAccessType AccessType = RDGPassResourceAccessType::TextureInput;
        RDGTextureUsage UsageHint = RDGTextureUsage::Unknown;
    };

    /** Setup-phase IO declaration (Granite add_* verbs). */
    class RenderPassBuilder
    {
    public:
        RenderPassBuilder(RenderGraph& graph, RenderPass& pass);

        RDGTextureRef AddColorOutput(const char* name, const RDGTextureDesc& desc);
        RDGTextureRef AddTextureInput(const char* name);
        RDGTextureRef SetDepthStencilOutput(const char* name, const RDGTextureDesc& desc);
        RDGTextureRef SetDepthStencilInput(const char* name);
        RDGTextureRef UseTexture(const char* name);

    private:
        RDGTextureRef DeclareAccess(
            const char* name,
            RDGPassResourceAccessType accessType,
            const RDGTextureDesc* desc);

        RenderGraph& m_Graph;
        RenderPass& m_Pass;
    };
}
