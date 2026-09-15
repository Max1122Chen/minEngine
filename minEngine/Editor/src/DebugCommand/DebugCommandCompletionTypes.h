#pragma once

#include "Core.h"

#include <string>

namespace minEngine::DebugCommand
{
    enum class CompletionKind : uint8_t
    {
        Command,
        ObjectRef,
        Property,
        ComponentType,
        ValueLiteral,
        EnumValue,
        Plain,
    };

    struct CompletionItem
    {
        std::string Label;
        std::string InsertText;
        std::string Description;
        CompletionKind Kind = CompletionKind::Plain;
    };
}
