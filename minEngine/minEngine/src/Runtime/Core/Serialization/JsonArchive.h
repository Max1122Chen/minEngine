#pragma once

#include "Archive.h"
#include "Json.h"

#include <string>
#include <utility>
#include <vector>

namespace minEngine::Serialization
{
    class JsonWriterArchive final : public WriterArchive
    {
    public:
        JsonWriterArchive() = default;

        bool BeginObject(const std::string& typeName) override;
        bool EndObject() override;

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

        const Json& GetRoot() const;
        Json&& MoveRoot();

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

    class JsonReaderArchive final : public ReaderArchive
    {
    public:
        explicit JsonReaderArchive(const Json& root)
            : m_Root(root)
        {
        }

        bool BeginObject(const std::string& expectedTypeName) override;
        bool EndObject() override;

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
