#pragma once

#include "Core.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class FontUtf8
    {
    public:
        /** Decode one UTF-8 codepoint; advances cursor. Returns false at end. */
        static bool DecodeNext(const char*& cursor, const char* end, uint32_t& outCodepoint);

        /** Invoke visitor(codepoint) for each codepoint in utf8. */
        template <typename TVisitor>
        static void ForEachCodepoint(const std::string& utf8Text, TVisitor&& visitor)
        {
            const char* cursor = utf8Text.data();
            const char* end = cursor + utf8Text.size();
            uint32_t codepoint = 0;
            while (DecodeNext(cursor, end, codepoint))
            {
                visitor(codepoint);
            }
        }
    };
}
