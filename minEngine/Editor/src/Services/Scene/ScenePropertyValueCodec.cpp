#include "Services/Scene/ScenePropertyValueCodec.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Resource/AssetManager.h"

#include <cstdio>

namespace minEngine
{
    namespace
    {
        constexpr const char* kAssetRefKey = "$assetRef";
        constexpr const char* kGuidRefKey = "$guid";
        constexpr const char* kObjectRefKey = "$objectRef";

        bool IsReferenceNode(const Json& value)
        {
            return value.is_object()
                && (value.contains(kAssetRefKey) || value.contains(kGuidRefKey)
                    || value.contains(kObjectRefKey));
        }

        Json MakeGuidNode(const GUID& guid)
        {
            return Json{{kGuidRefKey, Json{{"high", guid.High}, {"low", guid.Low}}}};
        }

        struct RefNormalizeState
        {
            ScenePropertyValueCodec::RefError ErrorKind = ScenePropertyValueCodec::RefError::None;
            std::string Error;

            bool Fail(ScenePropertyValueCodec::RefError kind, std::string message)
            {
                ErrorKind = kind;
                Error = std::move(message);
                return false;
            }
        };

        bool NormalizeReferenceNode(const Json& node, Json& outNode, RefNormalizeState& state);

        bool NormalizeAssetReference(const Json& node, Json& outNode, RefNormalizeState& state)
        {
            const Json& target = node.at(kAssetRefKey);
            if (!target.is_string() || target.get<std::string>().empty())
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Invalid,
                    std::string(kAssetRefKey) + " must be a non-empty project-relative asset path.");
            }

            const std::string path = target.get<std::string>();
            if (!AssetManager::HasInstance())
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Unresolved,
                    "AssetManager is not available; cannot resolve asset reference '" + path + "'.");
            }

            std::string loadError;
            const std::shared_ptr<Asset> asset = AssetManager::Get().LoadAssetByPath(path, loadError);
            const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(path);
            if (asset == nullptr || meta == nullptr)
            {
                std::string message = "Unresolved asset reference '" + path + "'.";
                if (!loadError.empty())
                {
                    message += " " + loadError;
                }
                return state.Fail(ScenePropertyValueCodec::RefError::Unresolved, std::move(message));
            }

            outNode = MakeGuidNode(meta->Guid);
            return true;
        }

        bool NormalizeObjectReference(const Json& node, Json& outNode, RefNormalizeState& state)
        {
            const Json& target = node.at(kObjectRefKey);
            if (!target.is_string())
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Invalid,
                    std::string(kObjectRefKey) + " must be a GUID string.");
            }

            const std::string text = target.get<std::string>();
            GUID guid;
            if (!ScenePropertyValueCodec::TryParseGuid(text, guid))
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Invalid,
                    std::string(kObjectRefKey) + " is not a valid GUID: '" + text + "'.");
            }

            if (!ObjectManager::Get().FindObject(guid))
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Unresolved,
                    "Unresolved object reference '" + guid.ToString() + "': no live object has this GUID.");
            }

            outNode = MakeGuidNode(guid);
            return true;
        }

        bool NormalizeGuidReference(const Json& node, Json& outNode, RefNormalizeState& state)
        {
            const Json& guidNode = node.at(kGuidRefKey);
            if (!guidNode.is_object() || !guidNode.contains("high") || !guidNode.contains("low"))
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Invalid,
                    std::string(kGuidRefKey) + " must be an object with 'high' and 'low'.");
            }

            const Json& highNode = guidNode["high"];
            const Json& lowNode = guidNode["low"];
            const bool highOk = highNode.is_number_unsigned() || highNode.is_number_integer();
            const bool lowOk = lowNode.is_number_unsigned() || lowNode.is_number_integer();
            if (!highOk || !lowOk)
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Invalid,
                    std::string(kGuidRefKey) + " 'high' and 'low' must be integers.");
            }

            outNode = MakeGuidNode(
                GUID{highNode.get<uint64_t>(), lowNode.get<uint64_t>()});
            return true;
        }

        bool NormalizeReferenceNode(const Json& node, Json& outNode, RefNormalizeState& state)
        {
            size_t presentKeyCount = 0;
            if (node.contains(kAssetRefKey))
            {
                ++presentKeyCount;
            }
            if (node.contains(kGuidRefKey))
            {
                ++presentKeyCount;
            }
            if (node.contains(kObjectRefKey))
            {
                ++presentKeyCount;
            }

            if (presentKeyCount != 1)
            {
                return state.Fail(
                    ScenePropertyValueCodec::RefError::Invalid,
                    "Reference value must use exactly one of $assetRef, $guid, or $objectRef.");
            }

            if (node.contains(kAssetRefKey))
            {
                return NormalizeAssetReference(node, outNode, state);
            }
            if (node.contains(kObjectRefKey))
            {
                return NormalizeObjectReference(node, outNode, state);
            }
            return NormalizeGuidReference(node, outNode, state);
        }

        bool NormalizeValueNode(const Json& node, bool insideArray, Json& outNode, RefNormalizeState& state)
        {
            if (node.is_array())
            {
                outNode = Json::array();
                for (const Json& element : node)
                {
                    if (IsReferenceNode(element))
                    {
                        return state.Fail(
                            ScenePropertyValueCodec::RefError::Unsupported,
                            "Object references inside arrays or collections are not supported yet.");
                    }

                    Json normalizedElement;
                    if (!NormalizeValueNode(element, true, normalizedElement, state))
                    {
                        return false;
                    }
                    outNode.push_back(std::move(normalizedElement));
                }
                return true;
            }

            if (!node.is_object())
            {
                outNode = node;
                return true;
            }

            if (IsReferenceNode(node))
            {
                if (insideArray)
                {
                    return state.Fail(
                        ScenePropertyValueCodec::RefError::Unsupported,
                        "Object references inside arrays or collections are not supported yet.");
                }
                return NormalizeReferenceNode(node, outNode, state);
            }

            outNode = Json::object();
            for (auto it = node.begin(); it != node.end(); ++it)
            {
                Json normalizedChild;
                if (!NormalizeValueNode(it.value(), false, normalizedChild, state))
                {
                    return false;
                }
                outNode[it.key()] = std::move(normalizedChild);
            }
            return true;
        }
    }

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

    bool ScenePropertyValueCodec::TryNormalizeValueReferences(
        const Json& value,
        Json& outValue,
        RefError& outErrorKind,
        std::string& outError)
    {
        RefNormalizeState state;
        Json normalized;
        if (!NormalizeValueNode(value, false, normalized, state))
        {
            outErrorKind = state.ErrorKind;
            outError = state.Error;
            outValue = Json();
            return false;
        }

        outErrorKind = RefError::None;
        outError.clear();
        outValue = std::move(normalized);
        return true;
    }

    bool ScenePropertyValueCodec::TryWriteJsonToLive(
        MEObject& owner,
        const Reflection::MEClass& ownerClass,
        const std::string& propertyPath,
        const Json& value,
        std::string& outError,
        const Serialization::SerializerOptions& options,
        RefError* outErrorKind)
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
            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(pendingRefs);
            if (!resolveResult.ok)
            {
                if (outErrorKind != nullptr)
                {
                    *outErrorKind = RefError::Unresolved;
                }
                outError = "Property value has unresolvable object references";
                if (!resolveResult.fieldPath.empty())
                {
                    outError += " at '" + resolveResult.fieldPath + "'";
                }
                outError += ": " + resolveResult.message;
                return false;
            }
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
            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(pendingRefs);
            if (!resolveResult.ok)
            {
                outError = "Restored property has unresolvable object references: "
                    + resolveResult.message;
                return false;
            }
        }
        return true;
    }
}
