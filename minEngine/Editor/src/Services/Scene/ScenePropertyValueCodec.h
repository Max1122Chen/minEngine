#pragma once

#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Serialization/Json.h"
#include "Runtime/Core/Serialization/Serializer.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class MEObject;

    namespace Reflection
    {
        class MEClass;
    }

    struct ScenePropertyValueCodec
    {
        static bool TryParseGuid(std::string_view text, GUID& outGuid);

        /** Map agent-friendly aliases (e.g. Transform.Position) to reflection paths. */
        static std::string NormalizePropertyPath(std::string_view propertyPath);

        static bool WalkToLeafOwner(
            void* ownerObject,
            const Reflection::MEClass* ownerClass,
            const std::string& propertyPath,
            void*& outLeafOwner,
            const Reflection::MEClass*& outLeafOwnerClass,
            std::string& outLeafPropertyName,
            std::string& outError);

        static bool TryReadJson(
            MEObject& owner,
            const Reflection::MEClass& ownerClass,
            const std::string& propertyPath,
            Json& outValue,
            std::string& outError,
            const Serialization::SerializerOptions& options);

        static bool TryWriteJsonToLive(
            MEObject& owner,
            const Reflection::MEClass& ownerClass,
            const std::string& propertyPath,
            const Json& value,
            std::string& outError,
            const Serialization::SerializerOptions& options);

        static bool TryCaptureBlob(
            MEObject& owner,
            const Reflection::MEClass& ownerClass,
            const std::string& propertyPath,
            std::vector<uint8_t>& outBlob,
            std::string& outError,
            const Serialization::SerializerOptions& options);

        static bool TryRestoreBlob(
            MEObject& owner,
            const Reflection::MEClass& ownerClass,
            const std::string& propertyPath,
            const std::vector<uint8_t>& blob,
            std::string& outError,
            const Serialization::SerializerOptions& options);
    };
}
