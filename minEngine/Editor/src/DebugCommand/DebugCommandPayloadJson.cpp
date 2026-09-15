#include "DebugCommand/DebugCommandPayloadJson.h"

namespace minEngine::DebugCommand
{
    std::string DebugCommandPayloadJson::EscapeString(std::string_view text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (const char character : text)
        {
            switch (character)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(character);
                break;
            }
        }
        return escaped;
    }

    std::string DebugCommandPayloadJson::Quote(std::string_view text)
    {
        return "\"" + EscapeString(text) + "\"";
    }
}
