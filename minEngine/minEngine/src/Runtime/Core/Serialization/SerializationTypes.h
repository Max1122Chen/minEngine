#pragma once

#include <string>
#include <utility>

namespace minEngine::Serialization
{
    struct SerializerOptions
    {
        bool enumAsString = true;
        bool strictTypeCheck = true;
        bool skipUnknownField = true;
        bool allowObjectPtrSerialization = false;
    };

    struct SerializeResult
    {
        bool ok = false;
        std::string message;
        std::string fieldPath;

        static SerializeResult Success()
        {
            return SerializeResult{true, "", ""};
        }

        static SerializeResult Failure(std::string inMessage, std::string inFieldPath = "")
        {
            return SerializeResult{false, std::move(inMessage), std::move(inFieldPath)};
        }
    };
}
