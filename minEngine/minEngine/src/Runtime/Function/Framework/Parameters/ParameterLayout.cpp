#include "Runtime/Function/Framework/Parameters/ParameterLayout.h"

#include <algorithm>
#include <cstring>

namespace minEngine
{
    uint16_t ParameterLayout::AlignUp(uint16_t value, uint16_t alignment)
    {
        if (alignment <= 1)
        {
            return value;
        }
        const uint16_t mask = static_cast<uint16_t>(alignment - 1);
        return static_cast<uint16_t>((value + mask) & ~mask);
    }

    bool ParameterLayout::Compile(
        const ParameterSchema& schema,
        ParameterLayout& outLayout,
        std::string* outError)
    {
        outLayout = ParameterLayout{};

        if (!schema.Validate(outError))
        {
            return false;
        }

        const std::vector<ParameterSchemaEntry>& schemaEntries = schema.GetEntries();
        const size_t entryCount = schemaEntries.size();
        if (entryCount == 0)
        {
            return true;
        }

        outLayout.m_Entries.resize(entryCount);
        for (size_t i = 0; i < entryCount; ++i)
        {
            ParameterLayoutEntry& layoutEntry = outLayout.m_Entries[i];
            layoutEntry.KeyId = static_cast<ParameterKeyId>(i);
            layoutEntry.Type = schemaEntries[i].Type;
            layoutEntry.Size = ParameterValueTypeUtil::SizeOf(schemaEntries[i].Type);
            layoutEntry.Name = schemaEntries[i].Name;
            layoutEntry.Offset = 0;
        }

        // Pack by descending size (then declaration order) to reduce padding.
        std::vector<ParameterKeyId> packOrder(entryCount);
        for (size_t i = 0; i < entryCount; ++i)
        {
            packOrder[i] = static_cast<ParameterKeyId>(i);
        }

        std::stable_sort(
            packOrder.begin(),
            packOrder.end(),
            [&](ParameterKeyId a, ParameterKeyId b)
            {
                const uint16_t sizeA = outLayout.m_Entries[a].Size;
                const uint16_t sizeB = outLayout.m_Entries[b].Size;
                if (sizeA != sizeB)
                {
                    return sizeA > sizeB;
                }
                return a < b;
            });

        uint16_t cursor = 0;
        for (ParameterKeyId keyId : packOrder)
        {
            ParameterLayoutEntry& layoutEntry = outLayout.m_Entries[keyId];
            const uint16_t alignment = ParameterValueTypeUtil::AlignOf(layoutEntry.Type);
            cursor = AlignUp(cursor, alignment);
            layoutEntry.Offset = cursor;
            cursor = static_cast<uint16_t>(cursor + layoutEntry.Size);
        }

        outLayout.m_TotalBytes = cursor;
        outLayout.m_DefaultBlob.assign(outLayout.m_TotalBytes, 0);

        for (size_t i = 0; i < entryCount; ++i)
        {
            const ParameterSchemaEntry& schemaEntry = schemaEntries[i];
            const ParameterLayoutEntry& layoutEntry = outLayout.m_Entries[i];
            uint8_t* dest = outLayout.m_DefaultBlob.data() + layoutEntry.Offset;

            if (schemaEntry.DefaultBytes.empty())
            {
                std::memset(dest, 0, layoutEntry.Size);
            }
            else
            {
                std::memcpy(dest, schemaEntry.DefaultBytes.data(), layoutEntry.Size);
            }
        }

        return true;
    }

    const ParameterLayoutEntry* ParameterLayout::GetEntry(ParameterKeyId keyId) const
    {
        if (!HasKey(keyId))
        {
            return nullptr;
        }
        return &m_Entries[keyId];
    }

    ParameterKeyId ParameterLayout::FindKeyId(std::string_view name) const
    {
        for (const ParameterLayoutEntry& entry : m_Entries)
        {
            if (entry.Name == name)
            {
                return entry.KeyId;
            }
        }
        return kInvalidParameterKeyId;
    }

    bool ParameterLayout::HasKey(ParameterKeyId keyId) const
    {
        return keyId < m_Entries.size();
    }
}
