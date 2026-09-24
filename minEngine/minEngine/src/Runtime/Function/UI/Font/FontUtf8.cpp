#include "FontUtf8.h"

namespace minEngine
{
    bool FontUtf8::DecodeNext(const char*& cursor, const char* end, uint32_t& outCodepoint)
    {
        if (cursor >= end)
        {
            return false;
        }

        const unsigned char lead = static_cast<unsigned char>(*cursor++);
        if (lead < 0x80)
        {
            outCodepoint = lead;
            return true;
        }

        int remaining = 0;
        uint32_t codepoint = 0;
        if ((lead & 0xE0) == 0xC0)
        {
            remaining = 1;
            codepoint = lead & 0x1F;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
            remaining = 2;
            codepoint = lead & 0x0F;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
            remaining = 3;
            codepoint = lead & 0x07;
        }
        else
        {
            outCodepoint = 0xFFFD;
            return true;
        }

        for (int i = 0; i < remaining; ++i)
        {
            if (cursor >= end)
            {
                outCodepoint = 0xFFFD;
                return true;
            }
            const unsigned char cont = static_cast<unsigned char>(*cursor++);
            if ((cont & 0xC0) != 0x80)
            {
                outCodepoint = 0xFFFD;
                return true;
            }
            codepoint = (codepoint << 6) | (cont & 0x3F);
        }

        outCodepoint = codepoint;
        return true;
    }
}
