#include "Runtime/Function/Framework/Parameters/ParameterSchema.h"

#include <cstddef>
#include <unordered_set>

namespace minEngine
{
    bool ParameterSchema::ValidateEntry(const ParameterSchemaEntry& entry, std::string* outError)
    {
        if (entry.Name.empty())
        {
            if (outError)
            {
                *outError = "ParameterSchemaEntry.Name must be non-empty";
            }
            return false;
        }

        if (!ParameterValueTypeUtil::IsKnown(entry.Type))
        {
            if (outError)
            {
                *outError = "ParameterSchemaEntry.Type is unknown: " + entry.Name;
            }
            return false;
        }

        const uint16_t expectedSize = ParameterValueTypeUtil::SizeOf(entry.Type);
        if (!entry.DefaultBytes.empty() && entry.DefaultBytes.size() != expectedSize)
        {
            if (outError)
            {
                *outError = "ParameterSchemaEntry.DefaultBytes size mismatch: " + entry.Name;
            }
            return false;
        }

        return true;
    }

    bool ParameterSchema::AddEntry(ParameterSchemaEntry entry, std::string* outError)
    {
        if (!ValidateEntry(entry, outError))
        {
            return false;
        }

        if (FindEntryIndex(entry.Name) != SIZE_MAX)
        {
            if (outError)
            {
                *outError = "Duplicate ParameterSchemaEntry.Name: " + entry.Name;
            }
            return false;
        }

        if (m_Entries.size() >= static_cast<size_t>(kInvalidParameterKeyId))
        {
            if (outError)
            {
                *outError = "ParameterSchema exceeds ParameterKeyId capacity";
            }
            return false;
        }

        m_Entries.push_back(std::move(entry));
        return true;
    }

    void ParameterSchema::Clear()
    {
        m_Entries.clear();
    }

    bool ParameterSchema::Validate(std::string* outError) const
    {
        std::unordered_set<std::string> seenNames;
        seenNames.reserve(m_Entries.size());

        for (const ParameterSchemaEntry& entry : m_Entries)
        {
            if (!ValidateEntry(entry, outError))
            {
                return false;
            }

            if (!seenNames.insert(entry.Name).second)
            {
                if (outError)
                {
                    *outError = "Duplicate ParameterSchemaEntry.Name: " + entry.Name;
                }
                return false;
            }
        }

        return true;
    }

    bool ParameterSchema::RemoveEntryAt(size_t index)
{
        if (index >= m_Entries.size())
        {
            return false;
        }
        m_Entries.erase(m_Entries.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    size_t ParameterSchema::FindEntryIndex(std::string_view name) const
    {
        for (size_t i = 0; i < m_Entries.size(); ++i)
        {
            if (m_Entries[i].Name == name)
            {
                return i;
            }
        }
        return SIZE_MAX;
    }
}
