#include "AssetSerializeHelper.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"

namespace minEngine
{
    bool AssetSerializeHelper::WriteObjectToFile(
        const std::string& absolutePath,
        const MEObject& object,
        std::string* outError)
    {
        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            absolutePath,
            &object,
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

    bool AssetSerializeHelper::ReadObjectFromFile(
        const std::string& absolutePath,
        MEObject& object,
        std::string* outError)
    {
        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            absolutePath,
            &object,
            archive,
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
                "AssetSerializeHelper: deserialize failed for '{}'. Error: {}. Field path: {}",
                absolutePath,
                result.message,
                result.fieldPath);
            return false;
        }
        return true;
    }
}
