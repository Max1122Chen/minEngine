#include "BinaryArchive.h"

#include "Reflection/MEClass.h"
#include "Reflection/Reflection.h"

#include <cstring>
#include <fstream>
#include <limits>

namespace minEngine::Serialization
{
    namespace
    {
        constexpr uint32_t kBinarySchemaVersion = 1u;

        bool WriteTypeName(std::vector<uint8_t>& buffer, const std::string& typeName)
        {
            if (typeName.size() > static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
            {
                return false;
            }

            const uint16_t nameLength = static_cast<uint16_t>(typeName.size());
            const uint8_t lengthBytes[2] = {
                static_cast<uint8_t>(nameLength & 0xFFu),
                static_cast<uint8_t>((nameLength >> 8) & 0xFFu),
            };
            buffer.insert(buffer.end(), lengthBytes, lengthBytes + 2);
            buffer.insert(buffer.end(), typeName.begin(), typeName.end());
            return true;
        }
    }

    bool BinaryWriterArchive::AppendBytes(const void* data, size_t size)
    {
        if (size == 0)
        {
            return true;
        }

        if (data == nullptr)
        {
            m_LastArchiveError = "append bytes failed: data is null";
            return false;
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        m_Buffer.insert(m_Buffer.end(), bytes, bytes + size);
        return true;
    }

    bool BinaryWriterArchive::AppendU8(uint8_t value)
    {
        return AppendBytes(&value, 1);
    }

    bool BinaryWriterArchive::AppendU16(uint16_t value)
    {
        const uint8_t bytes[2] = {
            static_cast<uint8_t>(value & 0xFFu),
            static_cast<uint8_t>((value >> 8) & 0xFFu),
        };
        return AppendBytes(bytes, 2);
    }

    bool BinaryWriterArchive::AppendU32(uint32_t value)
    {
        const uint8_t bytes[4] = {
            static_cast<uint8_t>(value & 0xFFu),
            static_cast<uint8_t>((value >> 8) & 0xFFu),
            static_cast<uint8_t>((value >> 16) & 0xFFu),
            static_cast<uint8_t>((value >> 24) & 0xFFu),
        };
        return AppendBytes(bytes, 4);
    }

    bool BinaryWriterArchive::AppendU64(uint64_t value)
    {
        const uint8_t bytes[8] = {
            static_cast<uint8_t>(value & 0xFFu),
            static_cast<uint8_t>((value >> 8) & 0xFFu),
            static_cast<uint8_t>((value >> 16) & 0xFFu),
            static_cast<uint8_t>((value >> 24) & 0xFFu),
            static_cast<uint8_t>((value >> 32) & 0xFFu),
            static_cast<uint8_t>((value >> 40) & 0xFFu),
            static_cast<uint8_t>((value >> 48) & 0xFFu),
            static_cast<uint8_t>((value >> 56) & 0xFFu),
        };
        return AppendBytes(bytes, 8);
    }

    bool BinaryWriterArchive::AppendStringBytes(const std::string& value)
    {
        if (value.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
        {
            m_LastArchiveError = "string too large for binary archive";
            return false;
        }

        if (!AppendU32(static_cast<uint32_t>(value.size())))
        {
            return false;
        }

        return AppendBytes(value.data(), value.size());
    }

    bool BinaryWriterArchive::CommitTaggedNull()
    {
        return CommitTaggedPayload(BinaryWireTag::Null, nullptr, 0);
    }

    bool BinaryWriterArchive::CommitTaggedPayload(BinaryWireTag tag, const void* payload, size_t payloadSize)
    {
        std::vector<uint8_t> tagged;
        tagged.reserve(1 + payloadSize);
        tagged.push_back(static_cast<uint8_t>(tag));
        if (payloadSize > 0)
        {
            const uint8_t* bytes = static_cast<const uint8_t*>(payload);
            tagged.insert(tagged.end(), bytes, bytes + payloadSize);
        }

        if (m_Stack.empty())
        {
            m_Buffer.insert(m_Buffer.end(), tagged.begin(), tagged.end());
            return true;
        }

        WriteFrame& frame = m_Stack.back();
        if (frame.kind == WriteFrameKind::Array)
        {
            m_Buffer.insert(m_Buffer.end(), tagged.begin(), tagged.end());
            ++frame.arrayWrittenCount;
            return true;
        }

        if (frame.pendingFieldName.empty())
        {
            m_LastArchiveError = "binary write failed: object field name is missing";
            return false;
        }

        if (frame.pendingFieldName.size() > static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
        {
            m_LastArchiveError = "binary write failed: field name too long";
            return false;
        }

        const uint16_t fieldNameLength = static_cast<uint16_t>(frame.pendingFieldName.size());
        if (!AppendU16(fieldNameLength))
        {
            return false;
        }

        if (!AppendBytes(frame.pendingFieldName.data(), frame.pendingFieldName.size()))
        {
            return false;
        }

        m_Buffer.insert(m_Buffer.end(), tagged.begin(), tagged.end());
        frame.pendingFieldName.clear();
        return true;
    }

    bool BinaryWriterArchive::CommitTaggedString(const std::string& value)
    {
        std::vector<uint8_t> tagged;
        tagged.push_back(static_cast<uint8_t>(BinaryWireTag::String));
        if (value.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
        {
            m_LastArchiveError = "string too large for binary archive";
            return false;
        }

        const uint32_t length = static_cast<uint32_t>(value.size());
        const uint8_t lengthBytes[4] = {
            static_cast<uint8_t>(length & 0xFFu),
            static_cast<uint8_t>((length >> 8) & 0xFFu),
            static_cast<uint8_t>((length >> 16) & 0xFFu),
            static_cast<uint8_t>((length >> 24) & 0xFFu),
        };
        tagged.insert(tagged.end(), lengthBytes, lengthBytes + 4);
        tagged.insert(tagged.end(), value.begin(), value.end());

        if (m_Stack.empty())
        {
            m_Buffer.insert(m_Buffer.end(), tagged.begin(), tagged.end());
            return true;
        }

        WriteFrame& frame = m_Stack.back();
        if (frame.kind == WriteFrameKind::Array)
        {
            m_Buffer.insert(m_Buffer.end(), tagged.begin(), tagged.end());
            ++frame.arrayWrittenCount;
            return true;
        }

        if (frame.pendingFieldName.empty())
        {
            m_LastArchiveError = "binary write failed: object field name is missing";
            return false;
        }

        const uint16_t fieldNameLength = static_cast<uint16_t>(frame.pendingFieldName.size());
        if (!AppendU16(fieldNameLength))
        {
            return false;
        }

        if (!AppendBytes(frame.pendingFieldName.data(), frame.pendingFieldName.size()))
        {
            return false;
        }

        m_Buffer.insert(m_Buffer.end(), tagged.begin(), tagged.end());
        frame.pendingFieldName.clear();
        return true;
    }

    bool BinaryWriterArchive::BeginObject(const std::string& typeName)
    {
        if (!AppendU8(static_cast<uint8_t>(BinaryWireTag::Object)))
        {
            return false;
        }

        if (!WriteTypeName(m_Buffer, typeName))
        {
            m_LastArchiveError = "BeginObject failed: type name write failed";
            return false;
        }

        WriteFrame frame;
        frame.kind = WriteFrameKind::Object;
        m_Stack.push_back(std::move(frame));
        return true;
    }

    bool BinaryWriterArchive::EndObject()
    {
        if (m_Stack.empty() || m_Stack.back().kind != WriteFrameKind::Object)
        {
            m_LastArchiveError = "EndObject failed: invalid writer stack";
            return false;
        }

        if (!m_Stack.back().pendingFieldName.empty())
        {
            m_LastArchiveError = "EndObject failed: pending field was not written";
            return false;
        }

        m_Stack.pop_back();
        return AppendU8(static_cast<uint8_t>(BinaryWireTag::EndObject));
    }

    bool BinaryWriterArchive::BeginObjectPtr(const std::string& typeName)
    {
        if (!AppendU8(static_cast<uint8_t>(BinaryWireTag::ObjectPtr)))
        {
            return false;
        }

        if (!WriteTypeName(m_Buffer, typeName))
        {
            m_LastArchiveError = "BeginObjectPtr failed: type name write failed";
            return false;
        }

        WriteFrame frame;
        frame.kind = WriteFrameKind::Object;
        m_Stack.push_back(std::move(frame));
        return true;
    }

    bool BinaryWriterArchive::EndObjectPtr()
    {
        return EndObject();
    }

    bool BinaryWriterArchive::BeginGuidRef(const GUID& guid)
    {
        if (!AppendU8(static_cast<uint8_t>(BinaryWireTag::GuidRef)))
        {
            return false;
        }

        if (!AppendU64(guid.High))
        {
            return false;
        }

        return AppendU64(guid.Low);
    }

    bool BinaryWriterArchive::EndGuidRef()
    {
        return true;
    }

    bool BinaryWriterArchive::BeginField(const std::string& fieldName)
    {
        if (m_Stack.empty() || m_Stack.back().kind != WriteFrameKind::Object)
        {
            m_LastArchiveError = "BeginField failed: not inside an object";
            return false;
        }

        m_Stack.back().pendingFieldName = fieldName;
        return true;
    }

    bool BinaryWriterArchive::EndField()
    {
        if (m_Stack.empty() || m_Stack.back().kind != WriteFrameKind::Object)
        {
            m_LastArchiveError = "EndField failed: not inside an object";
            return false;
        }

        if (!m_Stack.back().pendingFieldName.empty())
        {
            m_LastArchiveError = "EndField failed: field value was not written";
            return false;
        }

        return true;
    }

    bool BinaryWriterArchive::BeginArray(size_t count)
    {
        if (count > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
        {
            m_LastArchiveError = "BeginArray failed: count too large";
            return false;
        }

        if (!AppendU8(static_cast<uint8_t>(BinaryWireTag::Array)))
        {
            return false;
        }

        if (!AppendU32(static_cast<uint32_t>(count)))
        {
            return false;
        }

        WriteFrame frame;
        frame.kind = WriteFrameKind::Array;
        frame.arrayExpectedCount = count;
        frame.arrayWrittenCount = 0;
        m_Stack.push_back(std::move(frame));
        return true;
    }

    bool BinaryWriterArchive::EndArray()
    {
        if (m_Stack.empty() || m_Stack.back().kind != WriteFrameKind::Array)
        {
            m_LastArchiveError = "EndArray failed: invalid writer stack";
            return false;
        }

        const WriteFrame& frame = m_Stack.back();
        if (frame.arrayWrittenCount != frame.arrayExpectedCount)
        {
            m_LastArchiveError = "EndArray failed: element count mismatch";
            return false;
        }

        m_Stack.pop_back();
        return true;
    }

    bool BinaryWriterArchive::WriteNull()
    {
        return CommitTaggedNull();
    }

    bool BinaryWriterArchive::WriteBool(bool value)
    {
        const uint8_t payload = value ? 1u : 0u;
        return CommitTaggedPayload(BinaryWireTag::Bool, &payload, 1);
    }

    bool BinaryWriterArchive::WriteInt64(int64_t value)
    {
        return CommitTaggedPayload(BinaryWireTag::Int64, &value, sizeof(value));
    }

    bool BinaryWriterArchive::WriteUInt64(uint64_t value)
    {
        return CommitTaggedPayload(BinaryWireTag::UInt64, &value, sizeof(value));
    }

    bool BinaryWriterArchive::WriteDouble(double value)
    {
        return CommitTaggedPayload(BinaryWireTag::Double, &value, sizeof(value));
    }

    bool BinaryWriterArchive::WriteString(const std::string& value)
    {
        return CommitTaggedString(value);
    }

    void BinaryWriterArchive::ResetWriteState()
    {
        m_Buffer.clear();
        m_Stack.clear();
        m_LastArchiveError.clear();
    }

    bool BinaryWriterArchive::WriteToFile(const std::string& filePath)
    {
        m_LastArchiveError.clear();

        std::ofstream output(filePath, std::ios::binary);
        if (!output.is_open())
        {
            m_LastArchiveError = "failed to open output file";
            return false;
        }

        const uint8_t header[8] = {'M', 'E', 0x01, 'B', 'I', 'N', 0, 0};
        output.write(reinterpret_cast<const char*>(header), sizeof(header));
        const uint32_t version = kBinarySchemaVersion;
        output.write(reinterpret_cast<const char*>(&version), sizeof(version));
        output.write(reinterpret_cast<const char*>(m_Buffer.data()), static_cast<std::streamsize>(m_Buffer.size()));
        return output.good();
    }

    BinaryReaderArchive::BinaryReaderArchive(std::vector<uint8_t> buffer)
    {
        BindBuffer(std::move(buffer));
    }

    void BinaryReaderArchive::BindBuffer(std::vector<uint8_t> buffer)
    {
        m_Buffer = std::move(buffer);
        ResetReadState();
    }

    bool BinaryReaderArchive::ReadBytes(void* outData, size_t size)
    {
        if (size == 0)
        {
            return true;
        }

        if (outData == nullptr)
        {
            m_LastArchiveError = "read bytes failed: out data is null";
            return false;
        }

        if (m_ReadPos + size > m_Buffer.size())
        {
            m_LastArchiveError = "read bytes failed: buffer overrun";
            return false;
        }

        std::memcpy(outData, m_Buffer.data() + m_ReadPos, size);
        m_ReadPos += size;
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

    bool BinaryReaderArchive::ReadU16(uint16_t& outValue)
    {
        uint8_t bytes[2] = {};
        if (!ReadBytes(bytes, 2))
        {
            return false;
        }

        outValue = static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
        return true;
    }

    bool BinaryReaderArchive::ReadU32(uint32_t& outValue)
    {
        uint8_t bytes[4] = {};
        if (!ReadBytes(bytes, 4))
        {
            return false;
        }

        outValue = static_cast<uint32_t>(bytes[0])
            | (static_cast<uint32_t>(bytes[1]) << 8)
            | (static_cast<uint32_t>(bytes[2]) << 16)
            | (static_cast<uint32_t>(bytes[3]) << 24);
        return true;
    }

    bool BinaryReaderArchive::ReadU64(uint64_t& outValue)
    {
        return ReadBytes(&outValue, sizeof(outValue));
    }

    bool BinaryReaderArchive::ReadStringPayload(std::string& outValue)
    {
        uint32_t length = 0;
        if (!ReadU32(length))
        {
            return false;
        }

        if (m_ReadPos + length > m_Buffer.size())
        {
            m_LastArchiveError = "read string failed: buffer overrun";
            return false;
        }

        outValue.assign(reinterpret_cast<const char*>(m_Buffer.data() + m_ReadPos), length);
        m_ReadPos += length;
        return true;
    }

    bool BinaryReaderArchive::ConsumeFromActiveSlice(void* outData, size_t size)
    {
        if (size == 0)
        {
            return true;
        }

        if (outData == nullptr)
        {
            m_LastArchiveError = "consume slice failed: out data is null";
            return false;
        }

        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            m_LastArchiveError = "consume slice failed: no active value slice";
            return false;
        }

        ReadFrame& frame = m_Stack.back();
        if (frame.sliceReadPos + size > frame.valueSlice.size())
        {
            m_LastArchiveError = "consume slice failed: slice overrun";
            return false;
        }

        std::memcpy(outData, frame.valueSlice.data() + frame.sliceReadPos, size);
        frame.sliceReadPos += size;
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

    bool BinaryReaderArchive::ReadTaggedValueFromSlice(std::vector<uint8_t>& buffer,
                                                       size_t& readPos,
                                                       std::vector<uint8_t>& outSlice)
    {
        const size_t startPos = readPos;
        if (readPos >= buffer.size())
        {
            m_LastArchiveError = "read tagged value failed: empty buffer";
            return false;
        }

        const BinaryWireTag tag = static_cast<BinaryWireTag>(buffer[readPos++]);
        switch (tag)
        {
        case BinaryWireTag::Null:
            break;
        case BinaryWireTag::Bool:
        {
            if (readPos + 1 > buffer.size())
            {
                return false;
            }
            readPos += 1;
            break;
        }
        case BinaryWireTag::Int64:
        case BinaryWireTag::UInt64:
        case BinaryWireTag::Double:
        {
            if (readPos + 8 > buffer.size())
            {
                return false;
            }
            readPos += 8;
            break;
        }
        case BinaryWireTag::String:
        {
            if (readPos + 4 > buffer.size())
            {
                return false;
            }

            const uint32_t length = static_cast<uint32_t>(buffer[readPos])
                | (static_cast<uint32_t>(buffer[readPos + 1]) << 8)
                | (static_cast<uint32_t>(buffer[readPos + 2]) << 16)
                | (static_cast<uint32_t>(buffer[readPos + 3]) << 24);
            readPos += 4;
            if (readPos + length > buffer.size())
            {
                return false;
            }
            readPos += length;
            break;
        }
        case BinaryWireTag::Array:
        {
            if (readPos + 4 > buffer.size())
            {
                return false;
            }

            const uint32_t count = static_cast<uint32_t>(buffer[readPos])
                | (static_cast<uint32_t>(buffer[readPos + 1]) << 8)
                | (static_cast<uint32_t>(buffer[readPos + 2]) << 16)
                | (static_cast<uint32_t>(buffer[readPos + 3]) << 24);
            readPos += 4;

            for (uint32_t index = 0; index < count; ++index)
            {
                std::vector<uint8_t> ignored;
                if (!ReadTaggedValueFromSlice(buffer, readPos, ignored))
                {
                    return false;
                }
            }
            break;
        }
        case BinaryWireTag::GuidRef:
        {
            if (readPos + 16 > buffer.size())
            {
                return false;
            }
            readPos += 16;
            break;
        }
        case BinaryWireTag::Object:
        case BinaryWireTag::ObjectPtr:
        {
            if (readPos + 2 > buffer.size())
            {
                return false;
            }

            const uint16_t typeNameLength = static_cast<uint16_t>(buffer[readPos])
                | (static_cast<uint16_t>(buffer[readPos + 1]) << 8);
            readPos += 2;
            if (readPos + typeNameLength > buffer.size())
            {
                return false;
            }
            readPos += typeNameLength;

            while (readPos < buffer.size())
            {
                if (static_cast<BinaryWireTag>(buffer[readPos]) == BinaryWireTag::EndObject)
                {
                    ++readPos;
                    break;
                }

                if (readPos + 2 > buffer.size())
                {
                    return false;
                }

                const uint16_t fieldNameLength = static_cast<uint16_t>(buffer[readPos])
                    | (static_cast<uint16_t>(buffer[readPos + 1]) << 8);
                readPos += 2;
                if (readPos + fieldNameLength > buffer.size())
                {
                    return false;
                }
                readPos += fieldNameLength;

                std::vector<uint8_t> ignored;
                if (!ReadTaggedValueFromSlice(buffer, readPos, ignored))
                {
                    return false;
                }
            }
            break;
        }
        default:
            m_LastArchiveError = "read tagged value failed: unknown tag";
            return false;
        }

        outSlice.assign(buffer.begin() + static_cast<std::ptrdiff_t>(startPos), buffer.begin() + static_cast<std::ptrdiff_t>(readPos));
        return true;
    }

    bool BinaryReaderArchive::ParseObjectFields(std::unordered_map<std::string, std::vector<uint8_t>>& outFields,
                                                std::string& outTypeName,
                                                std::vector<uint8_t>& buffer,
                                                size_t& readPos)
    {
        if (readPos >= buffer.size())
        {
            m_LastArchiveError = "parse object failed: empty buffer";
            return false;
        }

        const BinaryWireTag objectTag = static_cast<BinaryWireTag>(buffer[readPos++]);
        if (objectTag != BinaryWireTag::Object && objectTag != BinaryWireTag::ObjectPtr)
        {
            m_LastArchiveError = "parse object failed: expected object tag";
            return false;
        }

        if (readPos + 2 > buffer.size())
        {
            m_LastArchiveError = "parse object failed: type name length overrun";
            return false;
        }

        const uint16_t typeNameLength = static_cast<uint16_t>(buffer[readPos])
            | (static_cast<uint16_t>(buffer[readPos + 1]) << 8);
        readPos += 2;

        if (readPos + typeNameLength > buffer.size())
        {
            m_LastArchiveError = "parse object failed: type name overrun";
            return false;
        }

        outTypeName.assign(reinterpret_cast<const char*>(buffer.data() + readPos), typeNameLength);
        readPos += typeNameLength;

        while (readPos < buffer.size())
        {
            if (static_cast<BinaryWireTag>(buffer[readPos]) == BinaryWireTag::EndObject)
            {
                ++readPos;
                break;
            }

            if (readPos + 2 > buffer.size())
            {
                m_LastArchiveError = "parse object failed: field name length overrun";
                return false;
            }

            const uint16_t fieldNameLength = static_cast<uint16_t>(buffer[readPos])
                | (static_cast<uint16_t>(buffer[readPos + 1]) << 8);
            readPos += 2;

            if (readPos + fieldNameLength > buffer.size())
            {
                m_LastArchiveError = "parse object failed: field name overrun";
                return false;
            }

            std::string fieldName(reinterpret_cast<const char*>(buffer.data() + readPos), fieldNameLength);
            readPos += fieldNameLength;

            std::vector<uint8_t> fieldSlice;
            if (!ReadTaggedValueFromSlice(buffer, readPos, fieldSlice))
            {
                return false;
            }

            outFields.emplace(std::move(fieldName), std::move(fieldSlice));
        }

        return true;
    }

    bool BinaryReaderArchive::BeginObjectFromFields(const std::string& expectedTypeName,
                                                    const minEngine::Reflection::MEClass* baseClassInfo,
                                                    std::unordered_map<std::string, std::vector<uint8_t>> fields,
                                                    BinaryWireTag objectTag)
    {
        (void)objectTag;

        if (!expectedTypeName.empty()
            && baseClassInfo != nullptr
            && !Reflection::ReflectionSystem::Get().IsClassNameSameOrDerived(expectedTypeName, baseClassInfo))
        {
            m_LastArchiveError = "BeginObject failed: type mismatch";
            return false;
        }

        ReadFrame frame;
        frame.kind = ReadFrameKind::ObjectMap;
        frame.objectFields = std::move(fields);
        m_Stack.push_back(std::move(frame));
        return true;
    }

    bool BinaryReaderArchive::BeginObject(const Reflection::MEClass* baseClassInfo)
    {
        std::unordered_map<std::string, std::vector<uint8_t>> fields;
        std::string typeName;

        if (!m_Stack.empty() && m_Stack.back().kind == ReadFrameKind::ValueSlice)
        {
            ReadFrame& sliceFrame = m_Stack.back();
            if (!ParseObjectFields(fields, typeName, sliceFrame.valueSlice, sliceFrame.sliceReadPos))
            {
                return false;
            }
        }
        else if (!ParseObjectFields(fields, typeName, m_Buffer, m_ReadPos))
        {
            return false;
        }

        if (baseClassInfo != nullptr
            && !Reflection::ReflectionSystem::Get().IsClassNameSameOrDerived(typeName, baseClassInfo))
        {
            m_LastArchiveError = "BeginObject failed: type mismatch";
            return false;
        }

        return BeginObjectFromFields(typeName, baseClassInfo, std::move(fields), BinaryWireTag::Object);
    }

    bool BinaryReaderArchive::BeginObject(const std::string& expectedTypeName)
    {
        std::unordered_map<std::string, std::vector<uint8_t>> fields;
        std::string typeName;

        if (!m_Stack.empty() && m_Stack.back().kind == ReadFrameKind::ValueSlice)
        {
            ReadFrame& sliceFrame = m_Stack.back();
            if (!ParseObjectFields(fields, typeName, sliceFrame.valueSlice, sliceFrame.sliceReadPos))
            {
                return false;
            }
        }
        else if (!ParseObjectFields(fields, typeName, m_Buffer, m_ReadPos))
        {
            return false;
        }

        if (!expectedTypeName.empty() && typeName != expectedTypeName)
        {
            m_LastArchiveError = "BeginObject failed: type name mismatch";
            return false;
        }

        return BeginObjectFromFields(typeName, nullptr, std::move(fields), BinaryWireTag::Object);
    }

    bool BinaryReaderArchive::EndObject()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ObjectMap)
        {
            m_LastArchiveError = "EndObject failed: invalid reader stack";
            return false;
        }

        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::BeginObjectPtr(const Reflection::MEClass* baseClassInfo, std::string& outClassName)
    {
        std::unordered_map<std::string, std::vector<uint8_t>> fields;
        if (!m_Stack.empty() && m_Stack.back().kind == ReadFrameKind::ValueSlice)
        {
            ReadFrame& sliceFrame = m_Stack.back();
            if (!ParseObjectFields(fields, outClassName, sliceFrame.valueSlice, sliceFrame.sliceReadPos))
            {
                return false;
            }
        }
        else if (!ParseObjectFields(fields, outClassName, m_Buffer, m_ReadPos))
        {
            return false;
        }

        if (baseClassInfo != nullptr
            && !Reflection::ReflectionSystem::Get().IsClassNameSameOrDerived(outClassName, baseClassInfo))
        {
            m_LastArchiveError = "BeginObjectPtr failed: type mismatch";
            return false;
        }

        return BeginObjectFromFields(outClassName, baseClassInfo, std::move(fields), BinaryWireTag::ObjectPtr);
    }

    bool BinaryReaderArchive::EndObjectPtr()
    {
        return EndObject();
    }

    bool BinaryReaderArchive::BeginGuidRef(GUID& outGuid)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag))
        {
            return false;
        }

        if (tag != BinaryWireTag::GuidRef)
        {
            m_LastArchiveError = "BeginGuidRef failed: unexpected tag";
            return false;
        }

        uint8_t rawTag = 0;
        if (!ConsumeFromActiveSlice(&rawTag, 1))
        {
            return false;
        }

        if (!ConsumeFromActiveSlice(&outGuid.High, sizeof(outGuid.High)))
        {
            return false;
        }

        return ConsumeFromActiveSlice(&outGuid.Low, sizeof(outGuid.Low));
    }

    bool BinaryReaderArchive::EndGuidRef()
    {
        return true;
    }

    bool BinaryReaderArchive::EnterField(const std::string& fieldName)
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ObjectMap)
        {
            m_LastArchiveError = "EnterField failed: not inside an object";
            return false;
        }

        const auto iter = m_Stack.back().objectFields.find(fieldName);
        if (iter == m_Stack.back().objectFields.end())
        {
            return false;
        }

        ReadFrame sliceFrame;
        sliceFrame.kind = ReadFrameKind::ValueSlice;
        sliceFrame.valueSlice = iter->second;
        sliceFrame.sliceReadPos = 0;
        m_Stack.push_back(std::move(sliceFrame));
        return true;
    }

    bool BinaryReaderArchive::LeaveField()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            m_LastArchiveError = "LeaveField failed: not inside a field slice";
            return false;
        }

        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::BeginArray(size_t& outCount)
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            m_LastArchiveError = "BeginArray failed: no active value slice";
            return false;
        }

        ReadFrame& sliceFrame = m_Stack.back();
        size_t readPos = sliceFrame.sliceReadPos;
        if (readPos >= sliceFrame.valueSlice.size())
        {
            m_LastArchiveError = "BeginArray failed: empty slice";
            return false;
        }

        if (static_cast<BinaryWireTag>(sliceFrame.valueSlice[readPos]) != BinaryWireTag::Array)
        {
            m_LastArchiveError = "BeginArray failed: unexpected tag";
            return false;
        }

        ++readPos;
        if (readPos + 4 > sliceFrame.valueSlice.size())
        {
            m_LastArchiveError = "BeginArray failed: count overrun";
            return false;
        }

        const uint32_t count = static_cast<uint32_t>(sliceFrame.valueSlice[readPos])
            | (static_cast<uint32_t>(sliceFrame.valueSlice[readPos + 1]) << 8)
            | (static_cast<uint32_t>(sliceFrame.valueSlice[readPos + 2]) << 16)
            | (static_cast<uint32_t>(sliceFrame.valueSlice[readPos + 3]) << 24);
        readPos += 4;

        ReadFrame arrayFrame;
        arrayFrame.kind = ReadFrameKind::ArrayElements;
        arrayFrame.arrayElements.reserve(count);
        for (uint32_t index = 0; index < count; ++index)
        {
            std::vector<uint8_t> elementSlice;
            if (!ReadTaggedValueFromSlice(sliceFrame.valueSlice, readPos, elementSlice))
            {
                return false;
            }
            arrayFrame.arrayElements.push_back(std::move(elementSlice));
        }

        sliceFrame.sliceReadPos = readPos;
        outCount = arrayFrame.arrayElements.size();
        m_Stack.push_back(std::move(arrayFrame));
        return true;
    }

    bool BinaryReaderArchive::EnterArrayElement(size_t index)
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ArrayElements)
        {
            m_LastArchiveError = "EnterArrayElement failed: not inside an array";
            return false;
        }

        if (index >= m_Stack.back().arrayElements.size())
        {
            m_LastArchiveError = "EnterArrayElement failed: index out of range";
            return false;
        }

        ReadFrame sliceFrame;
        sliceFrame.kind = ReadFrameKind::ValueSlice;
        sliceFrame.valueSlice = m_Stack.back().arrayElements[index];
        sliceFrame.sliceReadPos = 0;
        m_Stack.push_back(std::move(sliceFrame));
        return true;
    }

    bool BinaryReaderArchive::LeaveArrayElement()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            m_LastArchiveError = "LeaveArrayElement failed: not inside an array element";
            return false;
        }

        m_Stack.pop_back();
        return true;
    }

    bool BinaryReaderArchive::EndArray()
    {
        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ArrayElements)
        {
            m_LastArchiveError = "EndArray failed: invalid reader stack";
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
            return false;
        }

        uint8_t rawTag = 0;
        return ConsumeFromActiveSlice(&rawTag, 1);
    }

    bool BinaryReaderArchive::ReadBool(bool& outValue)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::Bool)
        {
            return false;
        }

        uint8_t rawTag = 0;
        uint8_t payload = 0;
        if (!ConsumeFromActiveSlice(&rawTag, 1) || !ConsumeFromActiveSlice(&payload, 1))
        {
            return false;
        }

        outValue = payload != 0;
        return true;
    }

    bool BinaryReaderArchive::ReadInt64(int64_t& outValue)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::Int64)
        {
            return false;
        }

        uint8_t rawTag = 0;
        if (!ConsumeFromActiveSlice(&rawTag, 1))
        {
            return false;
        }

        return ConsumeFromActiveSlice(&outValue, sizeof(outValue));
    }

    bool BinaryReaderArchive::ReadUInt64(uint64_t& outValue)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::UInt64)
        {
            return false;
        }

        uint8_t rawTag = 0;
        if (!ConsumeFromActiveSlice(&rawTag, 1))
        {
            return false;
        }

        return ConsumeFromActiveSlice(&outValue, sizeof(outValue));
    }

    bool BinaryReaderArchive::ReadDouble(double& outValue)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::Double)
        {
            return false;
        }

        uint8_t rawTag = 0;
        uint64_t rawBits = 0;
        if (!ConsumeFromActiveSlice(&rawTag, 1) || !ConsumeFromActiveSlice(&rawBits, sizeof(rawBits)))
        {
            return false;
        }

        std::memcpy(&outValue, &rawBits, sizeof(outValue));
        return true;
    }

    bool BinaryReaderArchive::ReadString(std::string& outValue)
    {
        BinaryWireTag tag = BinaryWireTag::Null;
        if (!PeekActiveSliceTag(tag) || tag != BinaryWireTag::String)
        {
            return false;
        }

        uint8_t rawTag = 0;
        if (!ConsumeFromActiveSlice(&rawTag, 1))
        {
            return false;
        }

        if (m_Stack.empty() || m_Stack.back().kind != ReadFrameKind::ValueSlice)
        {
            return false;
        }

        ReadFrame& frame = m_Stack.back();
        if (frame.sliceReadPos + 4 > frame.valueSlice.size())
        {
            m_LastArchiveError = "read string failed: length overrun";
            return false;
        }

        const uint32_t length = static_cast<uint32_t>(frame.valueSlice[frame.sliceReadPos])
            | (static_cast<uint32_t>(frame.valueSlice[frame.sliceReadPos + 1]) << 8)
            | (static_cast<uint32_t>(frame.valueSlice[frame.sliceReadPos + 2]) << 16)
            | (static_cast<uint32_t>(frame.valueSlice[frame.sliceReadPos + 3]) << 24);
        frame.sliceReadPos += 4;

        if (frame.sliceReadPos + length > frame.valueSlice.size())
        {
            m_LastArchiveError = "read string failed: payload overrun";
            return false;
        }

        outValue.assign(reinterpret_cast<const char*>(frame.valueSlice.data() + frame.sliceReadPos), length);
        frame.sliceReadPos += length;
        return true;
    }

    void BinaryReaderArchive::ResetReadState()
    {
        m_ReadPos = 0;
        m_Stack.clear();
        m_LastArchiveError.clear();

        if (!m_Buffer.empty())
        {
            ReadFrame rootSlice;
            rootSlice.kind = ReadFrameKind::ValueSlice;
            rootSlice.valueSlice = m_Buffer;
            rootSlice.sliceReadPos = 0;
            m_Stack.push_back(std::move(rootSlice));
        }
    }

    bool BinaryReaderArchive::ReadFromFile(const std::string& filePath)
    {
        m_LastArchiveError.clear();

        std::ifstream input(filePath, std::ios::binary);
        if (!input.is_open())
        {
            m_LastArchiveError = "failed to open input file";
            return false;
        }

        input.seekg(0, std::ios::end);
        const std::streamsize fileSize = input.tellg();
        input.seekg(0, std::ios::beg);
        if (fileSize < static_cast<std::streamsize>(sizeof(uint32_t) + 8))
        {
            m_LastArchiveError = "binary file too small";
            return false;
        }

        std::vector<uint8_t> fileBytes(static_cast<size_t>(fileSize));
        input.read(reinterpret_cast<char*>(fileBytes.data()), fileSize);
        if (!input.good())
        {
            m_LastArchiveError = "failed to read input file";
            return false;
        }

        const uint32_t version = *reinterpret_cast<const uint32_t*>(fileBytes.data() + 8);
        if (version != kBinarySchemaVersion)
        {
            m_LastArchiveError = "unsupported binary schema version";
            return false;
        }

        BindBuffer(std::vector<uint8_t>(fileBytes.begin() + 12, fileBytes.end()));
        return true;
    }
}
