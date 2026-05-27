#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    ME_CLASS()
    class Font : public Asset
    {
        ME_GENERATED_BODY(Font)

    public:
        Font() = default;
        ~Font() override = default;

        const std::vector<uint8_t>& GetFontFileBytes() const { return m_FontFileBytes; }
        const std::string& GetSourceExtension() const { return m_SourceExtension; }
        bool IsValid() const { return !m_FontFileBytes.empty(); }

    protected:
        friend class FontLoader;

        std::vector<uint8_t> m_FontFileBytes;
        std::string m_SourceExtension;
    };
}

#include "Font.gen.h"
