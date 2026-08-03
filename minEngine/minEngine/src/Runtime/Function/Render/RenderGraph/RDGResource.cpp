#include "Render/RenderGraph/RDGResource.h"

namespace minEngine
{
    RDGResource::RDGResource(Type type, uint32_t logicalIndex, std::string name)
        : m_Type(type)
        , m_LogicalIndex(logicalIndex)
        , m_Name(std::move(name))
    {
    }

    void RDGResource::MarkWrittenInPass(uint32_t passIndex)
    {
        m_WritePasses.insert(passIndex);
    }

    void RDGResource::MarkReadInPass(uint32_t passIndex)
    {
        m_ReadPasses.insert(passIndex);
    }

    void RDGResource::ClearPassUsage()
    {
        m_ReadPasses.clear();
        m_WritePasses.clear();
    }

    RDGTextureResource::RDGTextureResource(uint32_t logicalIndex, std::string name)
        : RDGResource(Type::Texture, logicalIndex, std::move(name))
    {
    }

    void RDGTextureResource::SetAttachmentInfo(const RDGAttachmentInfo& info)
    {
        m_AttachmentInfo = info;
        m_HasAttachmentInfo = true;
    }

    void RDGTextureResource::AddUsage(RHITextureCreateFlags flags)
    {
        m_Usage = m_Usage | flags;
    }
}
