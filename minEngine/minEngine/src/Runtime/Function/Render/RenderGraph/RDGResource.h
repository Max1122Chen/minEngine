#pragma once

#include "Core.h"
#include "Render/RenderGraph/RDGTypes.h"

#include <string>
#include <unordered_set>

namespace minEngine
{
    class RDGResource
    {
    public:
        enum class Type : uint8_t
        {
            Texture = 0,
            Buffer,
            Proxy,
        };

        static constexpr uint32_t kUnused = ~0u;

        RDGResource(Type type, uint32_t logicalIndex, std::string name);
        virtual ~RDGResource() = default;

        Type GetType() const { return m_Type; }
        uint32_t GetLogicalIndex() const { return m_LogicalIndex; }
        const std::string& GetName() const { return m_Name; }

        uint32_t GetPhysicalIndex() const { return m_PhysicalIndex; }
        void SetPhysicalIndex(uint32_t index) { m_PhysicalIndex = index; }

        void MarkWrittenInPass(uint32_t passIndex);
        void MarkReadInPass(uint32_t passIndex);
        void ClearPassUsage();

        const std::unordered_set<uint32_t>& GetReadPasses() const { return m_ReadPasses; }
        const std::unordered_set<uint32_t>& GetWritePasses() const { return m_WritePasses; }

    private:
        Type m_Type = Type::Texture;
        uint32_t m_LogicalIndex = kUnused;
        uint32_t m_PhysicalIndex = kUnused;
        std::string m_Name;
        std::unordered_set<uint32_t> m_ReadPasses;
        std::unordered_set<uint32_t> m_WritePasses;
    };

    class RDGTextureResource : public RDGResource
    {
    public:
        explicit RDGTextureResource(uint32_t logicalIndex, std::string name);

        void SetAttachmentInfo(const RDGAttachmentInfo& info);
        const RDGAttachmentInfo& GetAttachmentInfo() const { return m_AttachmentInfo; }
        bool HasAttachmentInfo() const { return m_HasAttachmentInfo; }

        void AddUsage(RHITextureCreateFlags flags);
        RHITextureCreateFlags GetUsage() const { return m_Usage; }

        bool IsTransient() const { return m_Transient; }
        void SetTransient(bool enable) { m_Transient = enable; }

        /** Non-empty when AddColorOutput(..., colorInput) requested rename-alias. */
        const std::string& GetColorInputAlias() const { return m_ColorInputAlias; }
        void SetColorInputAlias(std::string name) { m_ColorInputAlias = std::move(name); }

    private:
        RDGAttachmentInfo m_AttachmentInfo{};
        bool m_HasAttachmentInfo = false;
        RHITextureCreateFlags m_Usage = RHITextureCreateFlags::None;
        bool m_Transient = false;
        std::string m_ColorInputAlias;
    };
}
