#include "BinaryArchive.h"

#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include <cstring>
#include <fstream>
#include <limits>

namespace minEngine::Serialization
{
    namespace
    {
        constexpr size_t kMaxStringBytes = 16u * 1024u * 1024u;
        constexpr size_t kMaxArrayCount = 1u << 20;
        constexpr size_t kMaxFieldCount = 1u << 20;

        bool AppendRaw(std::vector<uint8_t>& out, const void* data, size_t size)
        {
            if (size == 0)
            {
                return true;
            }
            if (data == nullptr)
            {
                return false;
            }
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            out.insert(out.end(), bytes, bytes + size);
            return true;
        }

        void WriteU32LE(std::vector<uint8_t>& out, uint32_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFFu));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
            out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
            out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
        }

        void WriteU64LE(std::vector<uint8_t>& out, uint64_t value)
        {
            for (int shift = 0; shift < 64; shift += 8)
            {
                out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
            }
        }

        bool ReadExact(const std::vector<uint8_t>& buffer, size_t& pos, void* outData, size_t size)
        {
            if (pos + size > buffer.size())
            {
                return false;
            }
            if (size > 0 && outData != nullptr)
            {
                std::memcpy(outData, buffer.data() + pos, size);
            }
            pos += size;
            return true;
        }

        bool ReadU32LE(const std::vector<uint8_t>& buffer, size_t& pos, uint32_t& outValue)
        {
            uint8_t bytes[4];
            if (!ReadExact(buffer, pos, bytes, 4))
            {
                return false;
            }
            outValue = static_cast<uint32_t>(bytes[0])
                | (static_cast<uint32_t>(bytes[1]) << 8)
                | (static_cast<uint32_t>(bytes[2]) << 16)
                | (static_cast<uint32_t>(bytes[3]) << 24);
            return true;
        }

        bool ReadU64LE(const std::vector<uint8_t>& buffer, size_t& pos, uint64_t& outValue)
        {
            uint8_t bytes[8];
            if (!ReadExact(buffer, pos, bytes, 8))
            {
                return false;
            }
            outValue = 0;
            for (int i = 0; i < 8; ++i)
            {
                outValue |= static_cast<uint64_t>(bytes[i]) << (8 * i);
            }
            return true;
        }
    }

    bool BinaryWriterArchive::AppendBytes(std::vector<uint8_t>& out, const void* data, size_t size)
    {
        if (!AppendRaw(out, data, size))
        {
            m_LastArchiveError = "append bytes failed";
            return false;
        }
        return true;
    }

    bool BinaryWriterArchive::AppendU8(std::vector<uint8_t>& out, uint8_t value)
    {
        out.push_back(value);
        return true;
    }

    bool BinaryWriterArchive::AppendU32(std::vector<uint8_t>& out, uint32_t value)
    {
        WriteU32LE(out, value);
        return true;
    }

    bool BinaryWriterArchive::AppendU64(std::vector<uint8_t>& out, uint64_t value)
    {
        WriteU64LE(out, value);
        return true;
    }

    std::vector<uint8_t>& BinaryWriterArchive::ActiveValueBuffer()
    {
        if (!m_Stack.empty())
        {
            return m_Stack.back().pendingValue;
        }
        return m_Buffer;
    }

    bool BinaryWriterArchive::EnsureHeader()
    {
        if (m_HeaderWritten)
        {
            return true;
        }

        const TransientSchemaTable& schema = TransientSchemaTable::Get();
        if (!schema.IsReady())
        {
            m_LastArchiveError = "transient schema table is not ready";
            return false;
        }

        m_Buffer.clear();
        AppendRaw(m_Buffer, kBinaryMagicV2, 4);
        const uint16_t version = schema.GetSchemaVersion();
        m_Buffer.push_back(static_cast<uint8_t>(version & 0xFFu));
        m_Buffer.push_back(static_cast<uint8_t>((version >> 8) & 0xFFu));
        m_Buffer.push_back(0); // HeaderFlags low
        m_Buffer.push_back(0); // HeaderFlags high
        WriteU64LE(m_Buffer, schema.GetFingerprint());
        m_HeaderWritten = true;
        return true;
    }

    bool BinaryWriterArchive::CommitValueToParent(std::vector<uint8_t> valueBytes)
    {
        if (valueBytes.empty())
        {
            m_LastArchiveError = "commit value failed: empty";
            return false;
        }

        if (m_Stack.empty())
        {
            return AppendBytes(m_Buffer, valueBytes.data(), valueBytes.size());
        }

        WriteFrame& parent = m_Stack.back();
        if (parent.kind == WriteFrameKind::Array)
        {
            parent.arrayElements.push_back(std::move(valueBytes));
            return true;
        }

        if ((parent.kind == WriteFrameKind::Object || parent.kind == WriteFrameKind::ObjectPtr)
            && parent.pendingFieldId != 0)
        {
            if (!parent.pendingValue.empty())
            {
                m_LastArchiveError = "commit value failed: field already has value";
                return false;
            }
            parent.pendingValue = std::move(valueBytes);
            return true;
        }

        m_LastArchiveError = "commit value failed: invalid parent";
        return false;
    }

    bool BinaryWriterArchive::AppendTaggedValueTo(std::vector<uint8_t>& out,
                                                  BinaryWireTag tag,
                                                  const void* payload,
                                                  size_t payloadSize)
    {
        out.push_back(static_cast<uint8_t>(tag));
        if (payloadSize > 0)
        {
            return AppendBytes(out, payload, payloadSize);
        }
        return true;
    }

    bool BinaryWriterArchive::WriteTaggedValue(BinaryWireTag tag, const void* payload, size_t payloadSize)
    {
        if (!EnsureHeader())
        {
            return false;
        }

        std::vector<uint8_t> valueBytes;
        if (!AppendTaggedValueTo(valueBytes, tag, payload, payloadSize))
        {
            return false;
        }
        return CommitValueToParent(std::move(valueBytes));
    }

    bool BinaryWriterArchive::BeginObjectWithClass(const Reflection::MEClass* classInfo, BinaryWireTag objectTag)
    {
        if (!EnsureHeader())
        {
            return false;
        }
        if (classInfo == nullptr)
        {
            m_LastArchiveError = "BeginObject failed: classInfo is null";
            return false;
        }

        const uint32_t classId = TransientSchemaTable::Get().GetClassId(classInfo);
        if (classId == 0)
        {
            m_LastArchiveError = "BeginObject failed: class is not in transient schema";
            return false;
        }

        WriteFrame frame;
        frame.kind = (objectTag == BinaryWireTag::ObjectPtr) ? WriteFrameKind::ObjectPtr : WriteFrameKind::Object;
        frame.classId = classId;
        m_Stack.push_back(std::move(frame));
        return true;
    }

    bool BinaryWriterArchive::BeginObject(const Reflection::MEClass* classInfo, bool /*writeTypeName*/)
    {
        return BeginObjectWithClass(classInfo, BinaryWireTag::Object);
    }

    bool BinaryWriterArchive::BeginObject(const std::string& typeName)
    {
        if (typeName.empty())
        {
            m_LastArchiveError = "BeginObject failed: empty typeName";
            return false;
        }
        return BeginObjectWithClass(Reflection::ReflectionSystem::Get().FindClass(typeName), BinaryWireTag::Object);
    }

    bool BinaryWriterArchive::BeginObjectPtr(const Reflection::MEClass* classInfo)
    {
        return BeginObjectWithClass(classInfo, BinaryWireTag::ObjectPtr);
    }

    bool BinaryWriterArchive::BeginObjectPtr(const std::string& typeName)
    {
        if (typeName.empty())
        {
            m_LastArchiveError = "BeginObjectPtr failed: empty typeName";
            return false;
        }
        return BeginObjectWithClass(Reflection::ReflectionSystem::Get().FindClass(typeName), BinaryWireTag::ObjectPtr);
    }

    bool BinaryWriterArchive::CommitPendingField()
    {
        if (m_Stack.empty())
        {
            return false;
        }
        WriteFrame& frame = m_Stack.back();
        if (frame.kind != WriteFrameKind::Object && frame.kind != WriteFrameKind::ObjectPtr)
        {
            return false;
        }
        if (frame.pendingFieldId == 0)
        {
            m_LastArchiveError = "EndField failed: no pending field";
            return false;
        }
        if (frame.pendingValue.empty())
        {
            m_LastArchiveError = "EndField failed: field value was not written";
            return false;
        }

        FieldRecord record;
        record.fieldId = frame.pendingFieldId;
        record.valueBytes = std::move(frame.pendingValue);
        frame.fields.push_back(std::move(record));
        frame.pendingFieldId = 0;
        return true;
    }

    bool BinaryWriterArchive::EndObjectWithTag(BinaryWireTag objectTag)
    {
        if (m_Stack.empty())
        {
            m_LastArchiveError = "EndObject failed: empty stack";
            return false;
        }

        WriteFrame frame = std::move(m_Stack.back());
        m_Stack.pop_back();

        const bool expectPtr = objectTag == BinaryWireTag::ObjectPtr;
        if (expectPtr != (frame.kind == WriteFrameKind::ObjectPtr))
        {
            m_LastArchiveError = "EndObject failed: frame kind mismatch";
            return false;
        }
        if (frame.pendingFieldId != 0)
        {
            m_LastArchiveError = "EndObject failed: pending field not closed";
            return false;
        }

        std::vector<uint8_t> body;
        for (const FieldRecord& field : frame.fields)
        {
            WriteU32LE(body, field.fieldId);
            AppendRaw(body, field.valueBytes.data(), field.valueBytes.size());
        }
        if (body.size() > std::numeric_limits<uint32_t>::max())
        {
            m_LastArchiveError = "EndObject failed: body too large";
            return false;
        }

        std::vector<uint8_t> objectBytes;
        objectBytes.push_back(static_cast<uint8_t>(objectTag));
        WriteU32LE(objectBytes, frame.classId);
        WriteU32LE(objectBytes, static_cast<uint32_t>(frame.fields.size()));
        WriteU32LE(objectBytes, static_cast<uint32_t>(body.size()));
        AppendRaw(objectBytes, body.data(), body.size());

        return CommitValueToParent(std::move(objectBytes));
    }

    bool BinaryWriterArchive::EndObject()
    {
        return EndObjectWithTag(BinaryWireTag::Object);
    }

    bool BinaryWriterArchive::EndObjectPtr()
    {
        return EndObjectWithTag(BinaryWireTag::ObjectPtr);
    }

    bool BinaryWriterArchive::BeginField(const std::string& fieldName)
    {
        if (m_Stack.empty())
        {
            m_LastArchiveError = "BeginField failed: empty stack";
            return false;
        }
        WriteFrame& frame = m_Stack.back();
        if (frame.kind != WriteFrameKind::Object && frame.kind != WriteFrameKind::ObjectPtr)
        {
            m_LastArchiveError = "BeginField failed: not in object";
            return false;
        }
        if (frame.pendingFieldId != 0)
        {
            m_LastArchiveError = "BeginField failed: previous field not ended";
            return false;
        }

        const uint32_t fieldId = TransientSchemaTable::Get().GetFieldId(frame.classId, fieldName);
        if (fieldId == 0)
        {
            m_LastArchiveError = "BeginField failed: unknown field " + fieldName;
            return false;
        }
        frame.pendingFieldId = fieldId;
        frame.pendingValue.clear();
        return true;
    }

    bool BinaryWriterArchive::EndField()
    {
        return CommitPendingField();
    }

    bool BinaryWriterArchive::BeginArray(size_t count)
    {
        if (!EnsureHeader())
        {
            return false;
        }
        if (count > kMaxArrayCount)
        {
            m_LastArchiveError = "BeginArray failed: count too large";
            return false;
        }

        WriteFrame frame;
        frame.kind = WriteFrameKind::Array;
        frame.arrayExpectedCount = count;
        m_Stack.push_back(std::move(frame));
        return true;
    }

    bool BinaryWriterArchive::EndArray()
    {
        if (m_Stack.empty() || m_Stack.back().kind != WriteFrameKind::Array)
        {
            m_LastArchiveError = "EndArray failed: invalid stack";
            return false;
        }

        WriteFrame frame = std::move(m_Stack.back());
        m_Stack.pop_back();

        if (frame.arrayElements.size() != frame.arrayExpectedCount)
        {
            m_LastArchiveError = "EndArray failed: element count mismatch";
            return false;
        }

        std::vector<uint8_t> arrayBytes;
        arrayBytes.push_back(static_cast<uint8_t>(BinaryWireTag::Array));
        WriteU32LE(arrayBytes, static_cast<uint32_t>(frame.arrayExpectedCount));
        for (const std::vector<uint8_t>& element : frame.arrayElements)
        {
            AppendRaw(arrayBytes, element.data(), element.size());
        }
        return CommitValueToParent(std::move(arrayBytes));
    }

    bool BinaryWriterArchive::BeginGuidRef(const GUID& guid)
    {
        uint8_t payload[16];
        const uint64_t high = guid.High;
        const uint64_t low = guid.Low;
        std::memcpy(payload, &high, 8);
        std::memcpy(payload + 8, &low, 8);
        return WriteTaggedValue(BinaryWireTag::GuidRef, payload, sizeof(payload));
    }

    bool BinaryWriterArchive::EndGuidRef()
    {
        return true;
    }

    bool BinaryWriterArchive::WriteNull()
    {
        return WriteTaggedValue(BinaryWireTag::Null, nullptr, 0);
    }

    bool BinaryWriterArchive::WriteBool(bool value)
    {
        const uint8_t payload = value ? 1u : 0u;
        return WriteTaggedValue(BinaryWireTag::Bool, &payload, 1);
    }

    bool BinaryWriterArchive::WriteInt64(int64_t value)
    {
        return WriteTaggedValue(BinaryWireTag::Int64, &value, sizeof(value));
    }

    bool BinaryWriterArchive::WriteUInt64(uint64_t value)
    {
        return WriteTaggedValue(BinaryWireTag::UInt64, &value, sizeof(value));
    }

    bool BinaryWriterArchive::WriteDouble(double value)
    {
        return WriteTaggedValue(BinaryWireTag::Double, &value, sizeof(value));
    }

    bool BinaryWriterArchive::WriteString(const std::string& value)
    {
        if (value.size() > kMaxStringBytes)
        {
            m_LastArchiveError = "WriteString failed: string too large";
            return false;
        }
        std::vector<uint8_t> payload;
        WriteU32LE(payload, static_cast<uint32_t>(value.size()));
        AppendRaw(payload, value.data(), value.size());
        return WriteTaggedValue(BinaryWireTag::String, payload.data(), payload.size());
    }

    void BinaryWriterArchive::ResetWriteState()
    {
        m_Buffer.clear();
        m_HeaderWritten = false;
        m_Stack.clear();
        m_LastArchiveError.clear();
    }

    bool BinaryWriterArchive::WriteToFile(const std::string& filePath)
    {
        std::ofstream stream(filePath, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            m_LastArchiveError = "WriteToFile failed";
            return false;
        }
        if (!m_Buffer.empty())
        {
            stream.write(reinterpret_cast<const char*>(m_Buffer.data()), static_cast<std::streamsize>(m_Buffer.size()));
        }
        return static_cast<bool>(stream);
    }

    // ----------------- Reader -----------------

    BinaryReaderArchive::BinaryReaderArchive(std::vector<uint8_t> buffer)
    {
        BindBuffer(std::move(buffer));
    }

    void BinaryReaderArchive::BindBuffer(std::vector<uint8_t> buffer)
    {
        ResetReadState();
        m_Buffer = std::move(buffer);
        ParseHeader();
    }

    bool BinaryReaderArchive::ParseHeader()
    {
        m_HeaderParsed = false;
        m_ReadPos = 0;
        if (m_Buffer.size() < 16)
        {
            m_LastArchiveError = "binary header too small";
            return false;
        }
        if (std::memcmp(m_Buffer.data(), kBinaryMagicV2, 4) != 0)
        {
            m_LastArchiveError = "binary magic mismatch (expected MEB2)";
            return false;
        }
        m_ReadPos = 4;
        uint32_t versionAndFlags = 0;
        if (!ReadU32LE(m_Buffer, m_ReadPos, versionAndFlags))
        {
            return false;
        }
        const uint16_t version = static_cast<uint16_t>(versionAndFlags & 0xFFFFu);
        if (version != kBinarySchemaVersionV2)
        {
            m_LastArchiveError = "unsupported binary schema version";
            return false;
        }

        uint64_t fingerprint = 0;
        if (!ReadU64LE(m_Buffer, m_ReadPos, fingerprint))
        {
            return false;
        }

        const TransientSchemaTable& schema = TransientSchemaTable::Get();
        if (!schema.IsReady())
        {
            m_LastArchiveError = "transient schema table is not ready";
            return false;
        }
        if (fingerprint != schema.GetFingerprint())
        {
            m_LastArchiveError = "schema fingerprint mismatch";
            return false;
        }

        m_HeaderParsed = true;
        return true;
    }

    bool BinaryReaderArchive::ReadBytes(void* outData, size_t size)
    {
        if (!ReadExact(m_Buffer, m_ReadPos, outData, size))
        {
            m_LastArchiveError = "read past end of buffer";
            return false;
        }
        return true;
    }

    bool BinaryReaderArchive::PeekU8(uint8_t& outValue) const
    {
        if (m_ReadPos >= m_Buffer.size())
        {
            return false;
        }
        outValue = m_Buffer[m_ReadPos];
        return true;
    }

    bool BinaryReaderArchive::ReadU8(uint8_t& outValue)
    {
        return ReadBytes(&outValue, 1);
    }

    bool BinaryReaderArchive::ReadU32(uint32_t& outValue)
    {
        return ReadU32LE(m_Buffer, m_ReadPos, outValue);
    }

    bool BinaryReaderArchive::ReadU64(uint64_t& outValue)
    {
        return ReadU64LE(m_Buffer, m_ReadPos, outValue);
    }

    bool BinaryReaderArchive::ReadTaggedValueFromBuffer(const std::vector<uint8_t>& buffer,
                                                        size_t& readPos,
                                                        std::vector<uint8_t>& outSlice)
    {
        const size_t start = readPos;
        if (readPos >= buffer.size())
        {
            m_LastArchiveError = "empty tagged value";
            return false;
        }

        const BinaryWireTag tag = static_cast<BinaryWireTag>(buffer[readPos++]);
        switch (tag)
        {
        case BinaryWireTag::Null:
            break;
        case BinaryWireTag::Bool:
            if (!ReadExact(buffer, readPos, nullptr, 1))
            {
                return false;
            }
            break;
        case BinaryWireTag::Int64:
        case BinaryWireTag::UInt64:
        case BinaryWireTag::Double:
            if (!ReadExact(buffer, readPos, nullptr, 8))
            {
                return false;
            }
            break;
        case BinaryWireTag::String:
        {
            uint32_t length = 0;
            if (!ReadU32LE(buffer, readPos, length) || length > kMaxStringBytes)
            {
                return false;
            }
            if (!ReadExact(buffer, readPos, nullptr, length))
            {
                return false;
            }
            break;
        }
        case BinaryWireTag::Array:
        {
            uint32_t count = 0;
            if (!ReadU32LE(buffer, readPos, count) || count > kMaxArrayCount)
            {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i)
            {
                std::vector<uint8_t> element;
                if (!ReadTaggedValueFromBuffer(buffer, readPos, element))
                {
                    return false;
                }
            }
            break;
        }
        case BinaryWireTag::GuidRef:
            if (!ReadExact(buffer, readPos, nullptr, 16))
            {
                return false;
            }
            break;
        case BinaryWireTag::Object:
        case BinaryWireTag::ObjectPtr:
        {
            uint32_t classId = 0;
            uint32_t fieldCount = 0;
            uint32_t bodyLength = 0;
            if (!ReadU32LE(buffer, readPos, classId)
                || !ReadU32LE(buffer, readPos, fieldCount)
                || !ReadU32LE(buffer, readPos, bodyLength))
            {
                return false;
            }
            if (fieldCount > kMaxFieldCount || readPos + bodyLength > buffer.size())
            {
                return false;
            }
            readPos += bodyLength;
            break;
        }
        default:
            m_LastArchiveError = "unknown wire tag";
            return false;
        }

        outSlice.assign(buffer.begin() + static_cast<std::ptrdiff_t>(start),
                       buffer.begin() + static_cast<std::ptrdiff_t>(readPos));
        return true;
    }

    bool BinaryReaderArchive::ReadTaggedValueIntoSlice(std::vector<uint8_t>& outSlice)
    {
        return ReadTaggedValueFromBuffer(m_Buffer, m_ReadPos, outSlice);
    }

    bool BinaryReaderArchive::HasActiveValueSlice() const
    {
        return !m_Stack.empty() && m_Stack.back().kind == ReadFrameKind::ValueSlice;
    }

    bool BinaryReaderArchive::PeekNextTag(BinaryWireTag& outTag) const
    {
        if (HasActiveValueSlice())
        {
            return PeekActiveSliceTag(outTag);
        }
        uint8_t tagByte = 0;
        if (!PeekU8(tagByte))
        {
            return false;
        }
        outTag = static_cast<BinaryWireTag>(tagByte);
        return true;
    }

    bool BinaryReaderArchive::BeginObjectCommon(const Reflection::MEClass* expectedClass, std::string* outDynamicClassName)
    {
        if (!m_HeaderParsed && !ParseHeader())
        {
            return false;
        }

        const bool fromSlice = HasActiveValueSlice();
        const size_t parentSliceIndex = fromSlice ? (m_Stack.size() - 1) : 0;
        const std::vector<uint8_t>& sourceBuffer = fromSlice ? m_Stack[parentSliceIndex].valueSlice : m_Buffer;
        size_t readPos = fromSlice ? m_Stack[parentSliceIndex].sliceReadPos : m_ReadPos;

        if (readPos >= sourceBuffer.size())
        {
            m_LastArchiveError = "BeginObject failed: no tag";
            return false;
        }

        const BinaryWireTag tag = static_cast<BinaryWireTag>(sourceBuffer[readPos++]);
        if (outDynamicClassName != nullptr)
        {
            if (tag != BinaryWireTag::ObjectPtr)
            {
                m_LastArchiveError = "BeginObjectPtr failed: not an ObjectPtr tag";
                return false;
            }
        }
        else if (tag != BinaryWireTag::Object)
        {
            m_LastArchiveError = "BeginObject failed: not an Object tag";
            return false;
        }

        uint32_t classId = 0;
        uint32_t fieldCount = 0;
        uint32_t bodyLength = 0;
        if (!ReadU32LE(sourceBuffer, readPos, classId)
            || !ReadU32LE(sourceBuffer, readPos, fieldCount)
            || !ReadU32LE(sourceBuffer, readPos, bodyLength))
        {
            m_LastArchiveError = "BeginObject failed: truncated header";
            return false;
        }
        if (fieldCount > kMaxFieldCount)
        {
            m_LastArchiveError = "BeginObject failed: fieldCount too large";
            return false;
        }

        const TransientSchemaTable& schema = TransientSchemaTable::Get();
        const Reflection::MEClass* dynamicClass = schema.FindClass(classId);
        if (dynamicClass == nullptr)
        {
            m_LastArchiveError = "BeginObject failed: unknown ClassId";
            return false;
        }
        if (expectedClass != nullptr
            && !Reflection::ReflectionSystem::Get().IsClassSameOrDerived(dynamicClass, expectedClass))
        {
            m_LastArchiveError = "BeginObject failed: class type mismatch";
            return false;
        }
        if (outDynamicClassName != nullptr)
        {
            *outDynamicClassName = dynamicClass->GetName();
        }

        const size_t bodyStart = readPos;
        if (bodyStart + bodyLength > sourceBuffer.size())
        {
            m_LastArchiveError = "BeginObject failed: bodyLength out of range";
            return false;
        }

        ReadFrame frame;
        frame.kind = ReadFrameKind::ObjectMap;
        frame.classId = classId;

        size_t bodyPos = bodyStart;
        const size_t bodyEnd = bodyStart + bodyLength;
        for (uint32_t i = 0; i < fieldCount; ++i)
        {
            uint32_t fieldId = 0;
            if (!ReadU32LE(sourceBuffer, bodyPos, fieldId))
            {
                m_LastArchiveError = "BeginObject failed: bad field id";
                return false;
            }
            if (schema.FindProperty(classId, fieldId) == nullptr)
            {
                m_LastArchiveError = "BeginObject failed: unknown FieldId";
                return false;
            }
            std::vector<uint8_t> valueSlice;
            if (!ReadTaggedValueFromBuffer(sourceBuffer, bodyPos, valueSlice))
            {
                return false;
            }
            if (!frame.objectFields.emplace(fieldId, std::move(valueSlice)).second)
            {
                m_LastArchiveError = "BeginObject failed: duplicate FieldId";
                return false;
            }
        }
        if (bodyPos != bodyEnd)
        {
            m_LastArchiveError = "BeginObject failed: bodyLength mismatch";
            return false;
        }

        if (fromSlice)
        {
            m_Stack[parentSliceIndex].sliceReadPos = bodyEnd;
        }
        else
        {
            m_ReadPos = bodyEnd;
        }
        m_Stack.push_back(std::move(frame));
        return true;
    }

    bool BinaryReaderArchive::BeginObject(const Reflection::MEClass* baseClassInfo)
    {
        return BeginObjectCommon(baseClassInfo, nullptr);
    }

    bool BinaryReaderArchive::BeginObject(const std::string& expectedTypeName)
    {
        const Reflection::MEClass* expected = nullptr;
        if (!expectedTypeName.empty())
        {
            expected = Reflection::ReflectionSystem::Get().FindClass(expectedTypeName);
            if (expected == nullptr)
            {
                m_LastArchiveError = "BeginObject failed: expected class not found";
                return false;
            }
        }
        return BeginObjectCommon(expected, nullptr);
    }

    bool BinaryReaderArchive::BeginObjectPtr(const Reflection::MEClass* baseClassInfo, std::string& outClassName)
    {
        outClassName.clear();
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekNextTag(tag) || tag != BinaryWireTag::ObjectPtr)
        {
            return false;
        }
        return BeginObjectCommon(baseClassInfo, &outClassName);
    }

    bool BinaryReaderArchive::EndObject()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ObjectMap)
        {
            m_LastArchiveError = "EndObject failed: invalid stack";
            return false;
        }

        const ReadFrame& frame = m_Stack.back();
        if (frame.consumedFieldIds.size() != frame.objectFields.size())
        {
            m_LastArchiveError = "EndObject failed: unread fields remain (strict transient)";
            return false;
        }

        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::EndObjectPtr()
    {
        return EndObject();
    }

    bool BinaryReaderArchive::EnterField(const std::string& fieldName)
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ObjectMap)
        {
            return false;
        }

        ReadFrame& frame = m_Stack.back();
        const uint32_t fieldId = TransientSchemaTable::Get().GetFieldId(frame.classId, fieldName);
        if (fieldId == 0)
        {
            return false;
        }

        const auto iter = frame.objectFields.find(fieldId);
        if (iter == frame.objectFields.end())
        {
            return false;
        }

        frame.consumedFieldIds.insert(fieldId);

        ReadFrame valueFrame;
        valueFrame.kind = ReadFrameKind::ValueSlice;
        valueFrame.valueSlice = iter->second;
        valueFrame.sliceReadPos = 0;
        m_Stack.push_back(std::move(valueFrame));
        return true;
    }

    bool BinaryReaderArchive::LeaveField()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            m_LastArchiveError = "LeaveField failed";
            return false;
        }
        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::PeekActiveSliceTag(BinaryWireTag& outTag) const
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            return false;
        }
        const ReadFrame& frame = m_Stack.back();
        if (frame.sliceReadPos >= frame.valueSlice.size())
        {
            return false;
        }
        outTag = static_cast<BinaryWireTag>(frame.valueSlice[frame.sliceReadPos]);
        return true;
    }

    bool BinaryReaderArchive::EnsureSliceBytes(size_t size)
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            return false;
        }
        ReadFrame& frame = m_Stack.back();
        return frame.sliceReadPos + size <= frame.valueSlice.size();
    }

    bool BinaryReaderArchive::ConsumeFromActiveSlice(void* outData, size_t size)
    {
        if (!EnsureSliceBytes(size))
        {
            m_LastArchiveError = "slice underflow";
            return false;
        }
        ReadFrame& frame = m_Stack.back();
        if (size > 0 && outData != nullptr)
        {
            std::memcpy(outData, frame.valueSlice.data() + frame.sliceReadPos, size);
        }
        frame.sliceReadPos += size;
        return true;
    }

    bool BinaryReaderArchive::BeginGuidRef(GUID& outGuid)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekNextTag(tag) || tag != BinaryWireTag::GuidRef)
        {
            return false;
        }

        uint64_t high = 0;
        uint64_t low = 0;
        if (HasActiveValueSlice())
        {
            if (!ConsumeFromActiveSlice(nullptr, 1)
                || !ConsumeFromActiveSlice(&high, 8)
                || !ConsumeFromActiveSlice(&low, 8))
            {
                return false;
            }
        }
        else
        {
            uint8_t tagByte = 0;
            if (!ReadU8(tagByte) || !ReadBytes(&high, 8) || !ReadBytes(&low, 8))
            {
                return false;
            }
        }
        outGuid = GUID(high, low);

        ReadFrame placeholder;
        placeholder.kind = ReadFrameKind::GuidRefPlaceholder;
        m_Stack.push_back(std::move(placeholder));
        return true;
    }

    bool BinaryReaderArchive::EndGuidRef()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::GuidRefPlaceholder)
        {
            return false;
        }
        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::BeginArray(size_t& outCount)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekNextTag(tag) || tag != BinaryWireTag::Array)
        {
            m_LastArchiveError = "BeginArray failed: not array tag";
            return false;
        }

        const bool fromSlice = HasActiveValueSlice();
        const size_t parentSliceIndex = fromSlice ? (m_Stack.size() - 1) : 0;
        const std::vector<uint8_t>& sourceBuffer = fromSlice ? m_Stack[parentSliceIndex].valueSlice : m_Buffer;
        size_t readPos = fromSlice ? m_Stack[parentSliceIndex].sliceReadPos : m_ReadPos;

        ++readPos; // consume Array tag
        uint32_t count = 0;
        if (!ReadU32LE(sourceBuffer, readPos, count) || count > kMaxArrayCount)
        {
            m_LastArchiveError = "BeginArray failed: bad count";
            return false;
        }

        ReadFrame arrayFrame;
        arrayFrame.kind = ReadFrameKind::ArrayElements;
        for (uint32_t i = 0; i < count; ++i)
        {
            std::vector<uint8_t> element;
            if (!ReadTaggedValueFromBuffer(sourceBuffer, readPos, element))
            {
                return false;
            }
            arrayFrame.arrayElements.push_back(std::move(element));
        }

        if (fromSlice)
        {
            m_Stack[parentSliceIndex].sliceReadPos = readPos;
        }
        else
        {
            m_ReadPos = readPos;
        }

        outCount = count;
        m_Stack.push_back(std::move(arrayFrame));
        return true;
    }

    bool BinaryReaderArchive::EnterArrayElement(size_t index)
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ArrayElements)
        {
            return false;
        }
        ReadFrame& arrayFrame = m_Stack.back();
        if (index >= arrayFrame.arrayElements.size())
        {
            return false;
        }

        ReadFrame valueFrame;
        valueFrame.kind = ReadFrameKind::ValueSlice;
        valueFrame.valueSlice = arrayFrame.arrayElements[index];
        valueFrame.sliceReadPos = 0;
        m_Stack.push_back(std::move(valueFrame));
        return true;
    }

    bool BinaryReaderArchive::LeaveArrayElement()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            return false;
        }
        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::EndArray()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ArrayElements)
        {
            return false;
        }
        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::ReadNull()
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::Null)
        {
            // Also allow root-level: if stack empty, peek buffer
            if (m_Stack.empty())
            {
                uint8_t tagByte = 0;
                if (!PeekU8(tagByte) || static_cast<BinaryWireTag>(tagByte) != BinaryWireTag::Null)
                {
                    return false;
                }
                ++m_ReadPos;
                return true;
            }
            return false;
        }
        return ConsumeFromActiveSlice(nullptr, 1);
    }

    bool BinaryReaderArchive::ReadBool(bool& outValue)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (m_Stack.empty())
        {
            std::vector<uint8_t> slice;
            if (!ReadTaggedValueIntoSlice(slice) || slice.empty()
                || static_cast<BinaryWireTag>(slice[0]) != BinaryWireTag::Bool || slice.size() < 2)
            {
                return false;
            }
            outValue = slice[1] != 0;
            return true;
        }

        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::Bool)
        {
            return false;
        }
        if (!ConsumeFromActiveSlice(nullptr, 1))
        {
            return false;
        }
        uint8_t value = 0;
        if (!ConsumeFromActiveSlice(&value, 1))
        {
            return false;
        }
        outValue = value != 0;
        return true;
    }

    bool BinaryReaderArchive::ReadInt64(int64_t& outValue)
    {
        if (m_Stack.empty())
        {
            std::vector<uint8_t> slice;
            if (!ReadTaggedValueIntoSlice(slice) || slice.size() < 9
                || static_cast<BinaryWireTag>(slice[0]) != BinaryWireTag::Int64)
            {
                return false;
            }
            std::memcpy(&outValue, slice.data() + 1, 8);
            return true;
        }
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::Int64)
        {
            return false;
        }
        return ConsumeFromActiveSlice(nullptr, 1) && ConsumeFromActiveSlice(&outValue, 8);
    }

    bool BinaryReaderArchive::ReadUInt64(uint64_t& outValue)
    {
        if (m_Stack.empty())
        {
            std::vector<uint8_t> slice;
            if (!ReadTaggedValueIntoSlice(slice) || slice.size() < 9
                || static_cast<BinaryWireTag>(slice[0]) != BinaryWireTag::UInt64)
            {
                return false;
            }
            std::memcpy(&outValue, slice.data() + 1, 8);
            return true;
        }
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::UInt64)
        {
            return false;
        }
        return ConsumeFromActiveSlice(nullptr, 1) && ConsumeFromActiveSlice(&outValue, 8);
    }

    bool BinaryReaderArchive::ReadDouble(double& outValue)
    {
        if (m_Stack.empty())
        {
            std::vector<uint8_t> slice;
            if (!ReadTaggedValueIntoSlice(slice) || slice.size() < 9
                || static_cast<BinaryWireTag>(slice[0]) != BinaryWireTag::Double)
            {
                return false;
            }
            std::memcpy(&outValue, slice.data() + 1, 8);
            return true;
        }
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::Double)
        {
            return false;
        }
        return ConsumeFromActiveSlice(nullptr, 1) && ConsumeFromActiveSlice(&outValue, 8);
    }

    bool BinaryReaderArchive::ReadString(std::string& outValue)
    {
        auto parseFromSlice = [&](const std::vector<uint8_t>& slice, size_t pos) -> bool
        {
            if (pos >= slice.size() || static_cast<BinaryWireTag>(slice[pos]) != BinaryWireTag::String)
            {
                return false;
            }
            ++pos;
            uint32_t length = 0;
            if (!ReadU32LE(slice, pos, length) || length > kMaxStringBytes || pos + length > slice.size())
            {
                return false;
            }
            outValue.assign(reinterpret_cast<const char*>(slice.data() + pos), length);
            return true;
        };

        if (m_Stack.empty())
        {
            std::vector<uint8_t> slice;
            if (!ReadTaggedValueIntoSlice(slice))
            {
                return false;
            }
            return parseFromSlice(slice, 0);
        }

        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::String)
        {
            return false;
        }
        ReadFrame& frame = m_Stack.back();
        const size_t start = frame.sliceReadPos;
        if (!parseFromSlice(frame.valueSlice, start))
        {
            return false;
        }
        // advance slice pos: 1 + 4 + length
        uint32_t length = 0;
        size_t pos = start + 1;
        ReadU32LE(frame.valueSlice, pos, length);
        frame.sliceReadPos = pos + length;
        return true;
    }

    void BinaryReaderArchive::ResetReadState()
    {
        m_Buffer.clear();
        m_ReadPos = 0;
        m_HeaderParsed = false;
        m_Stack.clear();
        m_LastArchiveError.clear();
    }

    bool BinaryReaderArchive::ReadFromFile(const std::string& filePath)
    {
        std::ifstream stream(filePath, std::ios::binary);
        if (!stream)
        {
            m_LastArchiveError = "ReadFromFile failed";
            return false;
        }
        stream.seekg(0, std::ios::end);
        const std::streamoff endPos = stream.tellg();
        if (endPos < 0)
        {
            return false;
        }
        stream.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(static_cast<size_t>(endPos));
        if (!buffer.empty())
        {
            stream.read(reinterpret_cast<char*>(buffer.data()), endPos);
        }
        BindBuffer(std::move(buffer));
        return m_HeaderParsed;
    }
}
