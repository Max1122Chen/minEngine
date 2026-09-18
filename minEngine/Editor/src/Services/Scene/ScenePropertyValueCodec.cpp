#include "Services/Scene/ScenePropertyValueCodec.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include <cstdio>

namespace minEngine
{
    bool ScenePropertyValueCodec::TryParseGuid(std::string_view text, GUID& outGuid)
    {
        unsigned int a = 0;
        unsigned int b = 0;
        unsigned int c = 0;
        unsigned int d = 0;
        unsigned long long e = 0;
        const std::string normalized(text);
        if (std::sscanf(
                normalized.c_str(),
                "%08x-%04x-%04x-%04x-%012llx",
                &a,
                &b,
                &c,
                &d,
                &e)
            != 5)
        {
            return false;
        }

        outGuid.High = (static_cast<uint64_t>(a) << 32) | (static_cast<uint64_t>(b) << 16)
            | static_cast<uint64_t>(c);
        outGuid.Low = (static_cast<uint64_t>(d) << 48) | e;
        return true;
    }

    std::string ScenePropertyValueCodec::NormalizePropertyPath(std::string_view propertyPath)
    {
        std::string path(propertyPath);
        constexpr std::string_view kTransformAlias = "Transform.";
        if (path.rfind(kTransformAlias, 0) == 0)
        {
            path.replace(0, kTransformAlias.size(), "m_Transform.");
        }
        return path;
    }

    bool ScenePropertyValueCodec::WalkToLeafOwner(
        void* ownerObject,
        const Reflection::MEClass* ownerClass,
        const std::string& propertyPath,
        void*& outLeafOwner,
        const Reflection::MEClass*& outLeafOwnerClass,
        std::string& outLeafPropertyName,
        std::string& outError)
    {
        outLeafOwner = ownerObject;
        outLeafOwnerClass = ownerClass;
        outLeafPropertyName.clear();
        outError.clear();

        if (ownerObject == nullptr || ownerClass == nullptr)
        {
            outError = "Property path owner is null.";
            return false;
        }

        if (propertyPath.empty())
        {
            outError = "propertyPath is empty.";
            return false;
        }

        size_t segmentStart = 0;
        std::vector<std::string> segments;
        while (segmentStart < propertyPath.size())
        {
            const size_t dotIndex = propertyPath.find('.', segmentStart);
            const size_t segmentEnd = dotIndex == std::string::npos ? propertyPath.size() : dotIndex;
            const std::string segment = propertyPath.substr(segmentStart, segmentEnd - segmentStart);
            if (segment.empty())
            {
                outError = "propertyPath has an empty segment.";
                return false;
            }
            segments.push_back(segment);
            if (dotIndex == std::string::npos)
            {
                break;
            }
            segmentStart = dotIndex + 1;
        }

        if (segments.empty())
        {
            outError = "propertyPath is empty.";
            return false;
        }

        void* currentOwner = ownerObject;
        const Reflection::MEClass* currentClass = ownerClass;
        for (size_t index = 0; index + 1 < segments.size(); ++index)
        {
            const Reflection::MEProperty* property = nullptr;
            Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
                currentClass->GetName(),
                [&](const Reflection::MEProperty& candidate) -> bool
                {
                    if (candidate.GetName() == segments[index])
                    {
                        property = &candidate;
                        return false;
                    }
                    return true;
                });
            if (property == nullptr)
            {
                outError = "Unknown property segment '" + segments[index] + "'.";
                return false;
            }

            if (property->GetCategory() != Reflection::MEPropertyCategory::Object)
            {
                outError = "Property segment '" + segments[index] + "' is not a nested object.";
                return false;
            }

            const Reflection::MEObjectProperty& objectProperty =
                static_cast<const Reflection::MEObjectProperty&>(*property);
            const Reflection::MEClass* nestedClass = objectProperty.GetValueClass();
            if (nestedClass == nullptr)
            {
                outError = "Nested property '" + segments[index] + "' has no class info.";
                return false;
            }

            if (property->GetConstAccessor() == nullptr)
            {
                outError = "Nested property '" + segments[index] + "' is not readable.";
                return false;
            }

            void* nested = const_cast<void*>(property->GetConst(currentOwner));
            if (nested == nullptr)
            {
                outError = "Failed to access nested property '" + segments[index] + "'.";
                return false;
            }

            currentOwner = nested;
            currentClass = nestedClass;
        }

        outLeafOwner = currentOwner;
        outLeafOwnerClass = currentClass;
        outLeafPropertyName = segments.back();
        return true;
    }

    bool ScenePropertyValueCodec::TryReadJson(
        MEObject& owner,
        const Reflection::MEClass& ownerClass,
        const std::string& propertyPath,
        Json& outValue,
        std::string& outError,
        const Serialization::SerializerOptions& options)
    {
        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafName;
        if (!WalkToLeafOwner(
                &owner,
                &ownerClass,
                propertyPath,
                leafOwner,
                leafOwnerClass,
                leafName,
                outError))
        {
            return false;
        }

        Serialization::JsonWriterArchive writer;
        const Serialization::SerializeResult result = Serialization::Serializer::SerializeProperty(
            leafOwner,
            leafOwnerClass,
            leafName,
            writer,
            options);
        if (!result.ok)
        {
            outError = result.message;
            return false;
        }

        outValue = writer.GetRoot();
        return true;
    }

    bool ScenePropertyValueCodec::TryWriteJsonToLive(
        MEObject& owner,
        const Reflection::MEClass& ownerClass,
        const std::string& propertyPath,
        const Json& value,
        std::string& outError,
        const Serialization::SerializerOptions& options)
    {
        void* leafOwner = nullptr;
        const Reflection::MEClass* leafOwnerClass = nullptr;
        std::string leafName;
        if (!WalkToLeafOwner(
                &owner,
                &ownerClass,
                propertyPath,
                leafOwner,
                leafOwnerClass,
                leafName,
                outError))
        {
            return false;
        }

        Serialization::JsonReaderArchive reader(value);
        std::vector<Serialization::PendingObjectRef> pendingRefs;
        const Serialization::SerializeResult result = Serialization::Serializer::DeserializeProperty(
            leafOwner,
            leafOwnerClass,
            leafName,
            reader,
            pendingRefs,
            options);
        if (!result.ok)
        {
            outError = result.message;
            return false;
        }
        if (!pendingRefs.empty())
        {
            outError = "Property value still has unresolved object references.";
            return false;
        }
        return true;
    }

    bool ScenePropertyValueCodec::TryCaptureBlob(
        MEObject& owner,
        const Reflection::MEClass& ownerClass,
        const std::string& propertyPath,
        std::vector<uint8_t>& outBlob,
        std::string& outError,
        const Serialization::SerializerOptions& options)
    {
        const Serialization::SerializeResult result = Serialization::Serializer::SerializePropertyByPathToBuffer(
            &owner,
            &ownerClass,
            propertyPath,
            outBlob,
            options);
        if (!result.ok)
        {
            outError = result.message;
            return false;
        }
        return true;
    }

    bool ScenePropertyValueCodec::TryRestoreBlob(
        MEObject& owner,
        const Reflection::MEClass& ownerClass,
        const std::string& propertyPath,
        const std::vector<uint8_t>& blob,
        std::string& outError,
        const Serialization::SerializerOptions& options)
    {
        std::vector<Serialization::PendingObjectRef> pendingRefs;
        const Serialization::SerializeResult result =
            Serialization::Serializer::DeserializePropertyByPathFromBuffer(
                &owner,
                &ownerClass,
                propertyPath,
                blob,
                pendingRefs,
                options);
        if (!result.ok)
        {
            outError = result.message;
            return false;
        }
        if (!pendingRefs.empty())
        {
            outError = "Restored property still has unresolved object references.";
            return false;
        }
        return true;
    }
}
