#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine
{
    ME_ENUM()
    enum class EditorTypographyRole : uint8_t
    {
        Body = 0,
        Heading,
        Subheading,
        Caption,
        MenuBar,
        Count
    };
}

#include "EditorTypographyRole.gen.h"
