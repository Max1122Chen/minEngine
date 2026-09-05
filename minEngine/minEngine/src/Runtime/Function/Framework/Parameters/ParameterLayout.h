#pragma once

#include "Runtime/Function/Framework/Parameters/ParameterSchema.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    struct ParameterLayoutEntry
    {
        ParameterKeyId KeyId = kInvalidParameterKeyId;
        ParameterValueType Type = ParameterValueType::Float;
        uint16_t Offset = 0;
        uint16_t Size = 0;
        std::string Name;
    };

    // Immutable after Compile. KeyId == Schema declaration order (0..n-1).
    // Memory offsets may be packed by size; ValueOffsets[KeyId] via GetEntry.
    class ParameterLayout
    {
    public:
        static bool Compile(
            const ParameterSchema& schema,
            ParameterLayout& outLayout,
            std::string* outError = nullptr);

        uint16_t GetTotalBytes() const { return m_TotalBytes; }
        size_t GetEntryCount() const { return m_Entries.size(); }
        const std::vector<uint8_t>& GetDefaultBlob() const { return m_DefaultBlob; }

        const ParameterLayoutEntry* GetEntry(ParameterKeyId keyId) const;
        ParameterKeyId FindKeyId(std::string_view name) const;

        bool HasKey(ParameterKeyId keyId) const;

    private:
        static uint16_t AlignUp(uint16_t value, uint16_t alignment);

        std::vector<ParameterLayoutEntry> m_Entries;
        std::vector<uint8_t> m_DefaultBlob;
        uint16_t m_TotalBytes = 0;
    };
}
