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
        /** Why a property value could not be turned into a live value. */
        enum class RefError
        {
            None,
            Invalid,
            Unsupported,
            Unresolved
        };

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

        /**
         * Rewrite reference sugar ($assetRef path / $objectRef scene object) into the canonical
         * $guid node understood by the serializer, and verify that every reference resolves.
         * Nothing is written to any live object, so a whole request can be validated first.
         * Arrays and collections holding references are rejected as Unsupported.
         */
        static bool TryNormalizeValueReferences(
            const Json& value,
            Json& outValue,
            RefError& outErrorKind,
            std::string& outError);

        static bool TryWriteJsonToLive(
            MEObject& owner,
            const Reflection::MEClass& ownerClass,
            const std::string& propertyPath,
            const Json& value,
            std::string& outError,
            const Serialization::SerializerOptions& options,
            RefError* outErrorKind = nullptr);

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
