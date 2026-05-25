#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "EngineAPI.h"

namespace minEngine::Reflection
{
    struct MEEnumEntry
    {
        std::string name;
        int64_t value = 0;
    };

    class MINENGINE_API MEEnum
    {
    public:
        explicit MEEnum(std::string inName)
            : m_Name(std::move(inName))
        {
        }

        const std::string& GetName() const
        {
            return m_Name;
        }

        void AddEntry(std::string name, int64_t value)
        {
            m_Entries.push_back(MEEnumEntry{std::move(name), value});
        }

        const MEEnumEntry* FindByName(const std::string& enumName) const
        {
            for (const MEEnumEntry& entry : m_Entries)
            {
                if (entry.name == enumName)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const MEEnumEntry* FindByValue(int64_t enumValue) const
        {
            for (const MEEnumEntry& entry : m_Entries)
            {
                if (entry.value == enumValue)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const std::vector<MEEnumEntry>& GetEntries() const
        {
            return m_Entries;
        }

        void SetSize(size_t size)
        {
            m_Size = size;
        }

        size_t GetSize() const
        {
            return m_Size;
        }

    private:
        std::string m_Name;
        std::vector<MEEnumEntry> m_Entries;
        size_t m_Size = 0;
    };
}