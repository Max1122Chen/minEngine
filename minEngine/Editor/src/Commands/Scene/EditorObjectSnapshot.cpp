#include "Commands/Scene/EditorObjectSnapshot.h"

#include <cstring>
#include <limits>

namespace minEngine
{
    namespace
    {
        void AppendU16(std::vector<uint8_t>& buffer, uint16_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value & 0xFFu));
            buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
        }

        void AppendU32(std::vector<uint8_t>& buffer, uint32_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value & 0xFFu));
            buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
            buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
            buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
        }

        void AppendU64(std::vector<uint8_t>& buffer, uint64_t value)
        {
            for (int shift = 0; shift < 64; shift += 8)
            {
                buffer.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
            }
        }

        void AppendGuid(std::vector<uint8_t>& buffer, const GUID& guid)
        {
            AppendU64(buffer, guid.High);
            AppendU64(buffer, guid.Low);
        }

        void AppendString(std::vector<uint8_t>& buffer, const std::string& value)
        {
            if (value.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
            {
                return;
            }

            AppendU32(buffer, static_cast<uint32_t>(value.size()));
            buffer.insert(buffer.end(), value.begin(), value.end());
        }

        bool ReadU16(const std::vector<uint8_t>& buffer, size_t& offset, uint16_t& outValue)
        {
            if (offset + 2 > buffer.size())
            {
                return false;
            }

            outValue = static_cast<uint16_t>(buffer[offset]) | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
            offset += 2;
            return true;
        }

        bool ReadU32(const std::vector<uint8_t>& buffer, size_t& offset, uint32_t& outValue)
        {
            if (offset + 4 > buffer.size())
            {
                return false;
            }

            outValue = static_cast<uint32_t>(buffer[offset])
                | (static_cast<uint32_t>(buffer[offset + 1]) << 8)
                | (static_cast<uint32_t>(buffer[offset + 2]) << 16)
                | (static_cast<uint32_t>(buffer[offset + 3]) << 24);
            offset += 4;
            return true;
        }

        bool ReadU64(const std::vector<uint8_t>& buffer, size_t& offset, uint64_t& outValue)
        {
            if (offset + 8 > buffer.size())
            {
                return false;
            }

            outValue = 0;
            for (int shift = 0; shift < 64; shift += 8)
            {
                outValue |= static_cast<uint64_t>(buffer[offset++]) << shift;
            }
            return true;
        }

        bool ReadGuid(const std::vector<uint8_t>& buffer, size_t& offset, GUID& outGuid)
        {
            return ReadU64(buffer, offset, outGuid.High) && ReadU64(buffer, offset, outGuid.Low);
        }

        bool ReadString(const std::vector<uint8_t>& buffer, size_t& offset, std::string& outValue)
        {
            uint32_t length = 0;
            if (!ReadU32(buffer, offset, length))
            {
                return false;
            }

            if (offset + length > buffer.size())
            {
                return false;
            }

            outValue.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
            offset += length;
            return true;
        }
    }

    bool EditorObjectSnapshotUtil::WriteEnvelope(const EditorObjectSnapshot& snapshot, std::vector<uint8_t>& outEnvelope)
    {
        outEnvelope.clear();
        AppendU32(outEnvelope, snapshot.magic);
        AppendU16(outEnvelope, snapshot.version);
        AppendU16(outEnvelope, static_cast<uint16_t>(snapshot.kind));
        AppendU64(outEnvelope, snapshot.sourceRuntimeId);
        AppendGuid(outEnvelope, snapshot.sourceRootGuid);
        AppendString(outEnvelope, snapshot.rootClassName);
        AppendU64(outEnvelope, snapshot.ownerGameObjectId);
        AppendGuid(outEnvelope, snapshot.ownerGameObjectGuid);
        AppendU32(outEnvelope, static_cast<uint32_t>(snapshot.componentIndexInOwner));
        AppendU32(outEnvelope, static_cast<uint32_t>(snapshot.payload.size()));
        outEnvelope.insert(outEnvelope.end(), snapshot.payload.begin(), snapshot.payload.end());
        return true;
    }

    bool EditorObjectSnapshotUtil::ReadEnvelope(const std::vector<uint8_t>& envelope, EditorObjectSnapshot& outSnapshot)
    {
        size_t offset = 0;
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t kindValue = 0;
        uint32_t componentIndex = 0;
        uint32_t payloadSize = 0;

        if (!ReadU32(envelope, offset, magic)
            || magic != kEditorObjectSnapshotMagic
            || !ReadU16(envelope, offset, version)
            || version != kEditorObjectSnapshotVersion
            || !ReadU16(envelope, offset, kindValue)
            || !ReadU64(envelope, offset, outSnapshot.sourceRuntimeId)
            || !ReadGuid(envelope, offset, outSnapshot.sourceRootGuid)
            || !ReadString(envelope, offset, outSnapshot.rootClassName)
            || !ReadU64(envelope, offset, outSnapshot.ownerGameObjectId)
            || !ReadGuid(envelope, offset, outSnapshot.ownerGameObjectGuid)
            || !ReadU32(envelope, offset, componentIndex)
            || !ReadU32(envelope, offset, payloadSize))
        {
            return false;
        }

        outSnapshot.magic = magic;
        outSnapshot.version = version;
        outSnapshot.kind = static_cast<EditorSnapshotKind>(kindValue);
        outSnapshot.componentIndexInOwner = static_cast<int32_t>(componentIndex);

        if (offset + payloadSize > envelope.size())
        {
            return false;
        }

        outSnapshot.payload.assign(envelope.begin() + static_cast<std::ptrdiff_t>(offset),
                                   envelope.begin() + static_cast<std::ptrdiff_t>(offset + payloadSize));
        return true;
    }
}
