#pragma once

#include "Archive.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine::Serialization
{
    enum class BinaryWireTag : uint8_t
    {
        Null = 0,
        Bool = 1,
        Int64 = 2,
        UInt64 = 3,
        Double = 4,
        String = 5,
        Array = 6,
        GuidRef = 7,
        Object = 8,
        ObjectPtr = 9,
        EndObject = 10,
    };

    class MINENGINE_API BinaryWriterArchive final : public WriterArchive
    {
    public:
        BinaryWriterArchive() = default;

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

        const std::vector<uint8_t>& GetBuffer() const { return m_Buffer; }
        std::vector<uint8_t>&& TakeBuffer() { return std::move(m_Buffer); }

    private:
        enum class WriteFrameKind
        {
            Object,
            Array,
        };

        struct WriteFrame
        {
            WriteFrameKind kind = WriteFrameKind::Object;
            std::string pendingFieldName;
            std::string committingFieldName;
            bool isFieldValueObject = false;
            std::vector<uint8_t> fieldValueBody;
            size_t arrayExpectedCount = 0;
            size_t arrayWrittenCount = 0;
        };

        bool ShouldWriteNextObjectAsFieldValue() const;
        bool ShouldWriteTaggedValueInSubBuffer() const;
        std::vector<uint8_t>& GetActiveWriteBuffer();
        std::vector<uint8_t>& GetPayloadWriteBuffer();
        bool AppendToActiveBuffer(const void* data, size_t size);
        bool AppendU8ToActive(uint8_t value);
        bool AppendU16ToActive(uint16_t value);
        bool AppendU32ToActive(uint32_t value);
        bool AppendU64ToActive(uint64_t value);
        bool AppendStringBytesToActive(const std::string& value);
        bool CommitFieldValue(std::vector<uint8_t> fieldValueBytes);
        bool CommitArrayElement(std::vector<uint8_t> elementBytes);
        bool CommitTaggedValueToParent(std::vector<uint8_t> valueBytes);
        bool BeginObjectBody(BinaryWireTag objectTag, const std::string& typeName);
        bool EndObjectBody(BinaryWireTag objectTag);
        bool CommitTaggedPayload(BinaryWireTag tag, const void* payload, size_t payloadSize);
        bool CommitTaggedString(const std::string& value);
        bool CommitTaggedNull();
        bool AppendBytes(const void* data, size_t size);
        bool AppendU8(uint8_t value);
        bool AppendU16(uint16_t value);
        bool AppendU32(uint32_t value);
        bool AppendU64(uint64_t value);
        bool AppendStringBytes(const std::string& value);

        std::vector<uint8_t> m_Buffer;
        std::vector<WriteFrame> m_Stack;
        std::string m_LastArchiveError;
    };

    class MINENGINE_API BinaryReaderArchive final : public ReaderArchive
    {
    public:
        BinaryReaderArchive() = default;

        explicit BinaryReaderArchive(std::vector<uint8_t> buffer);

        void BindBuffer(std::vector<uint8_t> buffer);

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

    private:
        enum class ReadFrameKind
        {
            ObjectMap,
            ArrayElements,
            ValueSlice,
            GuidRefPlaceholder,
        };

        struct ReadFrame
        {
            ReadFrameKind kind = ReadFrameKind::ValueSlice;
            std::unordered_map<std::string, std::vector<uint8_t>> objectFields;
            std::vector<std::vector<uint8_t>> arrayElements;
            std::vector<uint8_t> valueSlice;
            size_t sliceReadPos = 0;
        };

        bool ReadBytes(void* outData, size_t size);
        bool PeekU8(uint8_t& outValue) const;
        bool ReadU8(uint8_t& outValue);
        bool ReadU16(uint16_t& outValue);
        bool ReadU32(uint32_t& outValue);
        bool ReadU64(uint64_t& outValue);
        bool ReadStringPayload(std::string& outValue);
        bool ReadTaggedValueIntoSlice(std::vector<uint8_t>& outSlice);
        bool ParseObjectFields(std::unordered_map<std::string, std::vector<uint8_t>>& outFields,
                               std::string& outTypeName,
                               std::vector<uint8_t>& buffer,
                               size_t& readPos);
        bool BeginObjectFromFields(const std::string& expectedTypeName,
                                   const minEngine::Reflection::MEClass* baseClassInfo,
                                   std::unordered_map<std::string, std::vector<uint8_t>> fields,
                                   BinaryWireTag objectTag);
        bool EnsureSliceBytes(size_t size);
        bool ConsumeFromActiveSlice(void* outData, size_t size);
        bool PeekActiveSliceTag(BinaryWireTag& outTag) const;
        bool ReadTaggedValueFromSlice(std::vector<uint8_t>& buffer, size_t& readPos, std::vector<uint8_t>& outSlice);
        static bool IsObjectFieldStreamEnd(const std::vector<uint8_t>& buffer, size_t readPos);

        std::vector<uint8_t> m_Buffer;
        size_t m_ReadPos = 0;
        std::vector<ReadFrame> m_Stack;
        std::string m_LastArchiveError;
    };
}
