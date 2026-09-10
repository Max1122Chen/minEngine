#pragma once

#include "EngineAPI.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine::Reflection
{
    class MEClass;
    class MEProperty;
}

namespace minEngine::Serialization
{
    inline constexpr uint16_t kBinarySchemaVersionV2 = 2u;
    inline constexpr char kBinaryMagicV2[4] = {'M', 'E', 'B', '2'};

    class MINENGINE_API TransientSchemaTable
    {
    public:
        static TransientSchemaTable& Get();

        void Clear();
        bool BuildFromReflection();

        bool IsReady() const { return m_Ready; }
        uint16_t GetSchemaVersion() const { return kBinarySchemaVersionV2; }
        uint64_t GetFingerprint() const { return m_Fingerprint; }

        const Reflection::MEClass* FindClass(uint32_t classId) const;
        uint32_t GetClassId(const Reflection::MEClass* classInfo) const;

        const Reflection::MEProperty* FindProperty(uint32_t classId, uint32_t fieldId) const;
        uint32_t GetFieldId(const Reflection::MEClass* classInfo, const Reflection::MEProperty& property) const;
        uint32_t GetFieldId(uint32_t classId, const std::string& propertyName) const;

        uint32_t GetFieldCount(uint32_t classId) const;

    private:
        struct ClassEntry
        {
            const Reflection::MEClass* classInfo = nullptr;
            uint32_t classId = 0;
            std::unordered_map<uint32_t, const Reflection::MEProperty*> fieldById;
            std::unordered_map<std::string, uint32_t> fieldIdByName;
            std::unordered_map<const Reflection::MEProperty*, uint32_t> fieldIdByProperty;
        };

        void ComputeFingerprint();

        bool m_Ready = false;
        uint64_t m_Fingerprint = 0;
        std::vector<ClassEntry> m_ClassesById; // index 0 unused; id == index
        std::unordered_map<const Reflection::MEClass*, uint32_t> m_ClassIdByPointer;
    };
}
