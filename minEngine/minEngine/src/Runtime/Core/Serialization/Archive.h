#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace minEngine::Serialization
{
    // Archive interfaces for serialization and deserialization.

    class WriterArchive
    {
    public:
        virtual ~WriterArchive() = default;

        virtual bool BeginObject(const std::string& typeName) = 0;
        virtual bool EndObject() = 0;

        virtual bool BeginField(const std::string& fieldName) = 0;
        virtual bool EndField() = 0;

        virtual bool BeginArray(size_t count) = 0;
        virtual bool EndArray() = 0;

        virtual bool WriteNull() = 0;
        virtual bool WriteBool(bool value) = 0;
        virtual bool WriteInt64(int64_t value) = 0;
        virtual bool WriteUInt64(uint64_t value) = 0;
        virtual bool WriteDouble(double value) = 0;
        virtual bool WriteString(const std::string& value) = 0;
    };

    class ReaderArchive
    {
    public:
        virtual ~ReaderArchive() = default;

        virtual bool BeginObject(const std::string& expectedTypeName) = 0;
        virtual bool EndObject() = 0;

        virtual bool EnterField(const std::string& fieldName) = 0;
        virtual bool LeaveField() = 0;

        virtual bool BeginArray(size_t& outCount) = 0;
        virtual bool EnterArrayElement(size_t index) = 0;
        virtual bool LeaveArrayElement() = 0;
        virtual bool EndArray() = 0;

        virtual bool ReadNull() = 0;
        virtual bool ReadBool(bool& outValue) = 0;
        virtual bool ReadInt64(int64_t& outValue) = 0;
        virtual bool ReadUInt64(uint64_t& outValue) = 0;
        virtual bool ReadDouble(double& outValue) = 0;
        virtual bool ReadString(std::string& outValue) = 0;
    };
}
