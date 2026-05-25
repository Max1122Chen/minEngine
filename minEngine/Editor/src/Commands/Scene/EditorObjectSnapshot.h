#pragma once

#include "Core.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Serialization/SerializationTypes.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace minEngine
{
    class Component;
    class GameObject;

    constexpr uint32_t kEditorObjectSnapshotMagic = 0x4E53454Du; // 'MESN'
    constexpr uint16_t kEditorObjectSnapshotVersion = 1u;

    enum class EditorSnapshotKind : uint16_t
    {
        GameObject = 0,
        Component = 1,
    };

    struct EditorObjectSnapshot
    {
        uint32_t magic = kEditorObjectSnapshotMagic;
        uint16_t version = kEditorObjectSnapshotVersion;
        EditorSnapshotKind kind = EditorSnapshotKind::GameObject;

        uint64_t sourceRuntimeId = 0;
        GUID sourceRootGuid{};

        std::string rootClassName;
        std::vector<uint8_t> payload;

        uint64_t ownerGameObjectId = 0;
        GUID ownerGameObjectGuid{};
        int32_t componentIndexInOwner = -1;
    };

    class EditorObjectSnapshotUtil
    {
    public:
        static bool WriteEnvelope(const EditorObjectSnapshot& snapshot, std::vector<uint8_t>& outEnvelope);

        static bool ReadEnvelope(const std::vector<uint8_t>& envelope, EditorObjectSnapshot& outSnapshot);
    };
}
