#include "TransientSchemaTable.h"

#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include <algorithm>

namespace minEngine::Serialization
{
    using Reflection::MEClass;
    using Reflection::MEProperty;
    using Reflection::PropertySpecifier;
    using Reflection::ReflectionSystem;

    namespace
    {
        uint64_t HashMix(uint64_t hash, uint64_t value)
        {
            hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
            return hash;
        }

        uint64_t HashBytes(uint64_t hash, const void* data, size_t size)
        {
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                hash = HashMix(hash, bytes[i]);
            }
            return hash;
        }

        uint64_t HashString(uint64_t hash, const std::string& text)
        {
            return HashBytes(hash, text.data(), text.size());
        }
    }

    TransientSchemaTable& TransientSchemaTable::Get()
    {
        static TransientSchemaTable table;
        return table;
    }

    void TransientSchemaTable::Clear()
    {
        m_Ready = false;
        m_Fingerprint = 0;
        m_ClassesById.clear();
        m_ClassIdByPointer.clear();
    }

    bool TransientSchemaTable::BuildFromReflection()
    {
        Clear();

        ReflectionSystem& reflection = ReflectionSystem::Get();
        if (!reflection.IsReady())
        {
            return false;
        }

        std::vector<const MEClass*> classes = reflection.GetAllClasses();

        std::sort(
            classes.begin(),
            classes.end(),
            [](const MEClass* left, const MEClass* right)
            {
                return left->GetName() < right->GetName();
            });

        m_ClassesById.resize(1); // id 0 invalid

        for (const MEClass* classInfo : classes)
        {
            ClassEntry entry;
            entry.classInfo = classInfo;
            entry.classId = static_cast<uint32_t>(m_ClassesById.size());

            uint32_t nextFieldId = 1;
            reflection.ForEachPropertyInHierarchy(
                classInfo,
                [&](const MEProperty& property) -> bool
                {
                    if (property.HasSpecifier(PropertySpecifier::Transient))
                    {
                        return true;
                    }

                    const uint32_t fieldId = nextFieldId++;
                    entry.fieldById[fieldId] = &property;
                    entry.fieldIdByName[property.GetName()] = fieldId;
                    entry.fieldIdByProperty[&property] = fieldId;
                    return true;
                });

            m_ClassIdByPointer[classInfo] = entry.classId;
            m_ClassesById.push_back(std::move(entry));
        }

        ComputeFingerprint();
        m_Ready = true;
        return true;
    }

    void TransientSchemaTable::ComputeFingerprint()
    {
        uint64_t hash = 0xcbf29ce484222325ull;
        hash = HashMix(hash, kBinarySchemaVersionV2);

        for (size_t classIndex = 1; classIndex < m_ClassesById.size(); ++classIndex)
        {
            const ClassEntry& entry = m_ClassesById[classIndex];
            hash = HashMix(hash, entry.classId);
            hash = HashString(hash, entry.classInfo->GetName());

            std::vector<uint32_t> fieldIds;
            fieldIds.reserve(entry.fieldById.size());
            for (const auto& pair : entry.fieldById)
            {
                fieldIds.push_back(pair.first);
            }
            std::sort(fieldIds.begin(), fieldIds.end());

            for (uint32_t fieldId : fieldIds)
            {
                const MEProperty* property = entry.fieldById.at(fieldId);
                hash = HashMix(hash, fieldId);
                hash = HashString(hash, property->GetName());
                hash = HashMix(hash, static_cast<uint64_t>(property->GetCategory()));
            }
        }

        m_Fingerprint = hash;
    }

    const MEClass* TransientSchemaTable::FindClass(uint32_t classId) const
    {
        if (!m_Ready || classId == 0 || classId >= m_ClassesById.size())
        {
            return nullptr;
        }
        return m_ClassesById[classId].classInfo;
    }

    uint32_t TransientSchemaTable::GetClassId(const MEClass* classInfo) const
    {
        if (!m_Ready || classInfo == nullptr)
        {
            return 0;
        }

        const auto iter = m_ClassIdByPointer.find(classInfo);
        if (iter == m_ClassIdByPointer.end())
        {
            return 0;
        }
        return iter->second;
    }

    const MEProperty* TransientSchemaTable::FindProperty(uint32_t classId, uint32_t fieldId) const
    {
        if (!m_Ready || classId == 0 || classId >= m_ClassesById.size() || fieldId == 0)
        {
            return nullptr;
        }

        const ClassEntry& entry = m_ClassesById[classId];
        const auto iter = entry.fieldById.find(fieldId);
        if (iter == entry.fieldById.end())
        {
            return nullptr;
        }
        return iter->second;
    }

    uint32_t TransientSchemaTable::GetFieldId(const MEClass* classInfo, const MEProperty& property) const
    {
        if (!m_Ready || classInfo == nullptr)
        {
            return 0;
        }

        const uint32_t classId = GetClassId(classInfo);
        if (classId == 0)
        {
            return 0;
        }

        const ClassEntry& entry = m_ClassesById[classId];
        const auto iter = entry.fieldIdByProperty.find(&property);
        if (iter != entry.fieldIdByProperty.end())
        {
            return iter->second;
        }

        // Property pointer from hierarchy walk should match; fall back to name.
        return GetFieldId(classId, property.GetName());
    }

    uint32_t TransientSchemaTable::GetFieldId(uint32_t classId, const std::string& propertyName) const
    {
        if (!m_Ready || classId == 0 || classId >= m_ClassesById.size() || propertyName.empty())
        {
            return 0;
        }

        const ClassEntry& entry = m_ClassesById[classId];
        const auto iter = entry.fieldIdByName.find(propertyName);
        if (iter == entry.fieldIdByName.end())
        {
            return 0;
        }
        return iter->second;
    }

    uint32_t TransientSchemaTable::GetFieldCount(uint32_t classId) const
    {
        if (!m_Ready || classId == 0 || classId >= m_ClassesById.size())
        {
            return 0;
        }
        return static_cast<uint32_t>(m_ClassesById[classId].fieldById.size());
    }
}
