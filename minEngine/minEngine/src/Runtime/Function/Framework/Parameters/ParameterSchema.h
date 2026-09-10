#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    ME_STRUCT()
    struct ParameterSchemaEntry
    {
        ME_GENERATED_BODY()

        ParameterSchemaEntry() = default;
        ParameterSchemaEntry(std::string name, ParameterValueType type, std::vector<uint8_t> defaultBytes = {})
            : Name(std::move(name))
            , Type(type)
            , DefaultBytes(std::move(defaultBytes))
        {
        }

        ME_PROPERTY()
        std::string Name;

        ME_PROPERTY()
        ParameterValueType Type = ParameterValueType::Float;

        // Interpreted by Type; empty => zero default at Compile.
        ME_PROPERTY()
        std::vector<uint8_t> DefaultBytes;
    };

    // Ordered, closed declaration set. No runtime AddKey after Compile.
    ME_STRUCT()
    class ParameterSchema
    {
        ME_GENERATED_BODY()
    public:
        bool AddEntry(ParameterSchemaEntry entry, std::string* outError = nullptr);
        void Clear();

        bool Validate(std::string* outError = nullptr) const;

        const std::vector<ParameterSchemaEntry>& GetEntries() const { return m_Entries; }
        std::vector<ParameterSchemaEntry>& GetEntriesMutable() { return m_Entries; }
        bool RemoveEntryAt(size_t index);
        size_t GetEntryCount() const { return m_Entries.size(); }

        // Returns index into GetEntries(), or SIZE_MAX if missing.
        size_t FindEntryIndex(std::string_view name) const;

    private:
        static bool ValidateEntry(const ParameterSchemaEntry& entry, std::string* outError);

        ME_PROPERTY()
        std::vector<ParameterSchemaEntry> m_Entries;
    };
}

#include "Generated/Reflection/ParameterSchema.gen.h"
