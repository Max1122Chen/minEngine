#pragma once

#include "Archive.h"
#include "Json.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace minEngine::Serialization
{
    class MINENGINE_API JsonWriterArchive final : public WriterArchive
    {
    public:
        JsonWriterArchive() = default;

        bool BeginObject(const std::string& typeName) override;
        bool EndObject() override;

        bool BeginObjectPtr(const std::string& typeName) override;
        bool EndObjectPtr() override;

        bool BeginGuidRef(const GUID& guid) override;
        bool EndGuidRef() override;

        bool BeginField(const std::string& fieldName) override;
        bool EndField() override;

        bool BeginArray(size_t count) override;
        bool EndArray() override;

        bool WriteNull() override;
        bool WriteBool(bool value) override;
        bool WriteInt64(int64_t value) override;
        bool WriteUInt64(uint64_t value) override;
        bool WriteDouble(double value) override;
        bool WriteString(const std::string& value) override;

        void ResetWriteState() override;
        bool WriteToFile(const std::string& filePath) override;
        const std::string& GetLastArchiveError() const override { return m_LastArchiveError; }

        const Json& GetRoot() const { return m_Root; };
        void ResetRoot()
        {
            m_Root = Json();
            m_HasRoot = false;
            m_Stack.clear();
            m_SchemaVersionApplied = false;
            m_EngineVersionApplied = false;
        };
        Json&& MoveRoot() { return std::move(m_Root); };

        // Inject "$schemaVersion" on the root object if present (CORE-F10).
        void ApplyRootSchemaVersion(uint32_t schemaVersion);
        // Inject "$engineVersion" string stamp on the root (CORE-F18).
        void ApplyRootEngineVersion(const std::string& engineVersion);

    private:
        struct WriteContext
        {
            Json* node = nullptr;
            bool isObject = false;
            std::string pendingFieldName;
        };

        Json* AttachValue(Json&& value);

        Json m_Root;
        bool m_HasRoot = false;
        bool m_SchemaVersionApplied = false;
        bool m_EngineVersionApplied = false;
        std::vector<WriteContext> m_Stack;
        std::string m_LastArchiveError;
    };

    class MINENGINE_API JsonReaderArchive final : public ReaderArchive
    {
    public:
        JsonReaderArchive() = default;

        explicit JsonReaderArchive(const Json& root)
        {
            BindRoot(root);
        }

        void BindRoot(const Json& root);

        bool BeginObject(const minEngine::Reflection::MEClass* baseClassInfo) override;
        bool BeginObject(const std::string& expectedTypeName) override;
        bool EndObject() override;

        bool BeginObjectPtr(const minEngine::Reflection::MEClass* baseClassInfo, std::string& outClassName) override;
        bool EndObjectPtr() override;

        bool BeginGuidRef(GUID& outGuid) override;
        bool EndGuidRef() override;

        bool EnterField(const std::string& fieldName) override;
        bool LeaveField() override;

        bool BeginArray(size_t& outCount) override;
        bool EnterArrayElement(size_t index) override;
        bool LeaveArrayElement() override;
        bool EndArray() override;

        bool ReadNull() override;
        bool ReadBool(bool& outValue) override;
        bool ReadInt64(int64_t& outValue) override;
        bool ReadUInt64(uint64_t& outValue) override;
        bool ReadDouble(double& outValue) override;
        bool ReadString(std::string& outValue) override;

        void ResetReadState() override;
        bool ReadFromFile(const std::string& filePath) override;
        const std::string& GetLastArchiveError() const override { return m_LastArchiveError; }

        // Parsed from root "$schemaVersion"; missing => 0 (CORE-F10).
        uint32_t GetReadSchemaVersion() const { return m_ReadSchemaVersion; }
        // Parsed from root "$engineVersion"; missing => empty (CORE-F18).
        const std::string& GetReadEngineVersion() const { return m_ReadEngineVersion; }

    private:
        const Json* CurrentValue() const;
        void PushObjectContext(const Json* object);
        void WarnUnreadObjectKeys(const Json& object, const std::unordered_set<std::string>& consumedKeys) const;
        void ParseRootDiskMeta(const Json& root);
        static bool IsKnownMetaKey(const std::string& key);

        const Json* m_Root = nullptr;
        Json m_OwnedRoot;
        std::vector<const Json*> m_ContextStack;
        std::vector<const Json*> m_ValueStack;
        // Parallel to object frames only: pushed/popped with BeginObject/EndObject (incl. ObjectPtr/GuidRef shells).
        std::vector<std::unordered_set<std::string>> m_ObjectConsumedKeys;
        uint32_t m_ReadSchemaVersion = 0;
        std::string m_ReadEngineVersion;
        std::string m_LastArchiveError;
    };
}
