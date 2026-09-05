#pragma once

#include "Archive.h"
#include "TransientSchemaTable.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    };

    class MINENGINE_API BinaryWriterArchive final : public WriterArchive
    {
    public:
        BinaryWriterArchive() = default;

        bool BeginObject(const std::string& typeName) override;
        bool BeginObject(const Reflection::MEClass* classInfo, bool writeTypeName) override;
        bool EndObject() override;

        bool BeginObjectPtr(const std::string& typeName) override;
        bool BeginObjectPtr(const Reflection::MEClass* classInfo) override;
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
            ObjectPtr,
            Array,
        };

        struct FieldRecord
        {
            uint32_t fieldId = 0;
            std::vector<uint8_t> valueBytes;
        };

        struct WriteFrame
        {
            WriteFrameKind kind = WriteFrameKind::Object;
            uint32_t classId = 0;
            uint32_t pendingFieldId = 0;
            std::vector<uint8_t> pendingValue;
            std::vector<FieldRecord> fields;
            size_t arrayExpectedCount = 0;
            std::vector<std::vector<uint8_t>> arrayElements;
        };

        bool EnsureHeader();
        bool BeginObjectWithClass(const Reflection::MEClass* classInfo, BinaryWireTag objectTag);
        bool EndObjectWithTag(BinaryWireTag objectTag);
        bool CommitPendingField();
        bool CommitValueToParent(std::vector<uint8_t> valueBytes);
        bool AppendTaggedValueTo(std::vector<uint8_t>& out, BinaryWireTag tag, const void* payload, size_t payloadSize);
        bool WriteTaggedValue(BinaryWireTag tag, const void* payload, size_t payloadSize);
        bool AppendBytes(std::vector<uint8_t>& out, const void* data, size_t size);
        bool AppendU8(std::vector<uint8_t>& out, uint8_t value);
        bool AppendU32(std::vector<uint8_t>& out, uint32_t value);
        bool AppendU64(std::vector<uint8_t>& out, uint64_t value);
        std::vector<uint8_t>& ActiveValueBuffer();

        std::vector<uint8_t> m_Buffer;
        bool m_HeaderWritten = false;
        std::vector<WriteFrame> m_Stack;
        std::string m_LastArchiveError;
    };

    class MINENGINE_API BinaryReaderArchive final : public ReaderArchive
    {
    public:
        BinaryReaderArchive() = default;
        explicit BinaryReaderArchive(std::vector<uint8_t> buffer);

        void BindBuffer(std::vector<uint8_t> buffer);

        bool BeginObject(const Reflection::MEClass* baseClassInfo) override;
        bool BeginObject(const std::string& expectedTypeName) override;
        bool EndObject() override;

        bool BeginObjectPtr(const Reflection::MEClass* baseClassInfo, std::string& outClassName) override;
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
            uint32_t classId = 0;
            std::unordered_map<uint32_t, std::vector<uint8_t>> objectFields;
            std::unordered_set<uint32_t> consumedFieldIds;
            std::vector<std::vector<uint8_t>> arrayElements;
            std::vector<uint8_t> valueSlice;
            size_t sliceReadPos = 0;
        };

        bool ParseHeader();
        bool BeginObjectCommon(const Reflection::MEClass* expectedClass, std::string* outDynamicClassName);
        bool ReadTaggedValueIntoSlice(std::vector<uint8_t>& outSlice);
        bool ReadTaggedValueFromBuffer(const std::vector<uint8_t>& buffer, size_t& readPos, std::vector<uint8_t>& outSlice);
        bool PeekNextTag(BinaryWireTag& outTag) const;
        bool PeekActiveSliceTag(BinaryWireTag& outTag) const;
        bool ConsumeFromActiveSlice(void* outData, size_t size);
        bool EnsureSliceBytes(size_t size);
        bool HasActiveValueSlice() const;
        bool ReadBytes(void* outData, size_t size);
        bool PeekU8(uint8_t& outValue) const;
        bool ReadU8(uint8_t& outValue);
        bool ReadU32(uint32_t& outValue);
        bool ReadU64(uint64_t& outValue);

        std::vector<uint8_t> m_Buffer;
        size_t m_ReadPos = 0;
        bool m_HeaderParsed = false;
        std::vector<ReadFrame> m_Stack;
        std::string m_LastArchiveError;
    };
}
