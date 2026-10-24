#include "AssetSerializeHelper.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"

namespace minEngine
{
    bool AssetSerializeHelper::WriteObjectToFile(
        const std::string& absolutePath,
        const MEObject& object,
        std::string* outError)
    {
        const Reflection::MEClass* rootClass = object.GetClass();
        if (rootClass == nullptr)
        {
            if (outError != nullptr)
            {
                *outError = "WriteObjectToFile: object has no runtime class";
            }
            ME_LOG(LogAsset, Error, "AssetSerializeHelper: '{}' has null GetClass().", absolutePath);
            return false;
        }

        Serialization::JsonWriterArchive archive;
        // Must use runtime class — template ToFile<MEObject> would serialize only MEObject fields.
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absolutePath,
            rootClass,
            static_cast<const void*>(&object),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = false});
        if (!result.ok)
        {
            if (outError != nullptr)
            {
                *outError = result.message;
            }
            ME_LOG(
                LogAsset,
                Error,
                "AssetSerializeHelper: serialize failed for '{}'. Error: {}. Field path: {}",
                absolutePath,
                result.message,
                result.fieldPath);
            return false;
        }
        return true;
    }

    bool AssetSerializeHelper::ReadJsonFromFile(
        const std::string& absolutePath,
        Json& outRoot,
        std::string* outError)
    {
        const Serialization::SerializeResult result =
            Serialization::Serializer::LoadJsonFromFile(absolutePath, outRoot);
        if (!result.ok)
        {
            if (outError != nullptr)
            {
                *outError = result.message;
            }
            ME_LOG(
                LogAsset,
                Error,
                "AssetSerializeHelper: LoadJsonFromFile failed for '{}'. Error: {}. Field path: {}",
                absolutePath,
                result.message,
                result.fieldPath);
            return false;
        }
        return true;
    }

    bool AssetSerializeHelper::ReadObjectFromJson(
        const Json& root,
        MEObject& object,
        std::string* outError)
    {
        const Reflection::MEClass* rootClass = object.GetClass();
        if (rootClass == nullptr)
        {
            if (outError != nullptr)
            {
                *outError = "ReadObjectFromJson: object has no runtime class";
            }
            ME_LOG(LogAsset, Error, "AssetSerializeHelper: ReadObjectFromJson has null GetClass().");
            return false;
        }

        const Serialization::SerializeResult result = Serialization::Serializer::FromJson(
            rootClass,
            static_cast<void*>(&object),
            root,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = true});
        if (!result.ok)
        {
            if (outError != nullptr)
            {
                *outError = result.message;
            }
            ME_LOG(
                LogAsset,
                Error,
                "AssetSerializeHelper: FromJson failed. Error: {}. Field path: {}",
                result.message,
                result.fieldPath);
            return false;
        }
        return true;
    }

    bool AssetSerializeHelper::ReadObjectFromFile(
        const std::string& absolutePath,
        MEObject& object,
        std::string* outError)
    {
        Json root;
        if (!ReadJsonFromFile(absolutePath, root, outError))
        {
            return false;
        }
        return ReadObjectFromJson(root, object, outError);
    }
}
