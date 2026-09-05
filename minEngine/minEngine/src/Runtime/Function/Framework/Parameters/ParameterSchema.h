#pragma once

#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    struct ParameterSchemaEntry
    {
        std::string Name;
        ParameterValueType Type = ParameterValueType::Float;
        // Interpreted by Type; empty => zero default at Compile.
        std::vector<uint8_t> DefaultBytes;
    };

    // Ordered, closed declaration set. No runtime AddKey after Compile.
    class ParameterSchema
    {
    public:
        bool AddEntry(ParameterSchemaEntry entry, std::string* outError = nullptr);
        void Clear();

        bool Validate(std::string* outError = nullptr) const;

        const std::vector<ParameterSchemaEntry>& GetEntries() const { return m_Entries; }
        size_t GetEntryCount() const { return m_Entries.size(); }

        // Returns index into GetEntries(), or SIZE_MAX if missing.
        size_t FindEntryIndex(std::string_view name) const;

    private:
        static bool ValidateEntry(const ParameterSchemaEntry& entry, std::string* outError);

        std::vector<ParameterSchemaEntry> m_Entries;
    };
}
