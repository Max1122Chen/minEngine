#pragma once

#include "Core.h"

#include <string>

namespace minEngine
{
    class MEObject;

    class AssetSerializeHelper
    {
    public:
        static bool WriteObjectToFile(
            const std::string& absolutePath,
            const MEObject& object,
            std::string* outError = nullptr);

        static bool ReadObjectFromFile(
            const std::string& absolutePath,
            MEObject& object,
            std::string* outError = nullptr);
    };
}
