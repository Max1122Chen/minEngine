#pragma once

#include "Core.h"
#include "Runtime/Core/Serialization/Json.h"

#include <string>

namespace minEngine
{
    class MEObject;

    class MINENGINE_API AssetSerializeHelper
    {
    public:
        static bool WriteObjectToFile(
            const std::string& absolutePath,
            const MEObject& object,
            std::string* outError = nullptr);

        // File → Json (disk meta included). Does not construct MEObject.
        static bool ReadJsonFromFile(
            const std::string& absolutePath,
            Json& outRoot,
            std::string* outError = nullptr);

        // Json → Object + sync pending-ref resolve.
        static bool ReadObjectFromJson(
            const Json& root,
            MEObject& object,
            std::string* outError = nullptr);

        // File → Object (= ReadJsonFromFile + ReadObjectFromJson).
        static bool ReadObjectFromFile(
            const std::string& absolutePath,
            MEObject& object,
            std::string* outError = nullptr);
    };
}
