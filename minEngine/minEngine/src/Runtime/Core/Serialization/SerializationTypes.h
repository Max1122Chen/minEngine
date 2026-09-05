#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace minEngine::Serialization
{
    // Disk JSON (CORE-F10) recommended: skipUnknownField=true, strictTypeCheck=false, writeSchemaVersion=true.
    // Transient Binary buffer / PIE: Serializer forces skipUnknownField=false and strictTypeCheck=true.
    struct MINENGINE_API SerializerOptions
    {
        bool enumAsString = true;
        // true = type / codec mismatch fails; false = skip field + warn (leave defaults).
        bool strictTypeCheck = true;
        // true = missing reflected fields keep defaults; false = fail on missing field.
        // Does NOT mean "ignore extra JSON keys" — extras are always skipped (with warn).
        bool skipUnknownField = true;
        bool writeObjectTypeName = false;
        // Root JSON object writes "$schemaVersion" when true (Json path).
        bool writeSchemaVersion = true;
        // Algebra written to "$schemaVersion". Missing on load is treated as 0.
        uint32_t schemaVersion = 1u;
    };

    struct MINENGINE_API SerializeResult
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
