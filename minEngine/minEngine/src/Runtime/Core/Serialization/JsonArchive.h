#pragma once

#include "Archive.h"
#include "Json.h"

#include <string>
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

        const Json& GetRoot() const { return m_Root; };
        void ResetRoot() { m_Root = Json(); m_HasRoot = false; m_Stack.clear(); };
        Json&& MoveRoot() { return std::move(m_Root); };

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
        std::vector<WriteContext> m_Stack;
    };

    class MINENGINE_API JsonReaderArchive final : public ReaderArchive
    {
    public:
        explicit JsonReaderArchive(const Json& root)
            : m_Root(root)
        {
        }

        bool BeginObject(const minEngine::Reflection::MEClass* baseClassInfo) override;
        bool BeginObject(const std::string& expectedTypeName) override;
        bool EndObject() override;

        bool BeginObjectPtr(const minEngine::Reflection::MEClass* baseClassInfo, std::string& outClassName) override;
        bool EndObjectPtr() override;

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

    private:
        const Json* CurrentValue() const;

        const Json& m_Root;
        std::vector<const Json*> m_ContextStack;
        std::vector<const Json*> m_ValueStack;
    };
}
