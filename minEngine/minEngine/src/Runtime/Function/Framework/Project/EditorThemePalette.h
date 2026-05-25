#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"

namespace minEngine
{
    /** Semantic editor theme tokens (stored as LinearColor; applied in Editor via sRGB display). */
    ME_STRUCT()
    struct EditorThemePalette
    {
        ME_GENERATED_BODY(EditorThemePalette)

        ME_PROPERTY()
        LinearColor WindowBackground{};

        ME_PROPERTY()
        LinearColor PanelBackground{};

        ME_PROPERTY()
        LinearColor PopupBackground{};

        ME_PROPERTY()
        LinearColor FieldBackground{};

        ME_PROPERTY()
        LinearColor FieldBackgroundHovered{};

        ME_PROPERTY()
        LinearColor FieldBackgroundActive{};

        ME_PROPERTY()
        LinearColor Accent{};

        ME_PROPERTY()
        LinearColor AccentHovered{};

        ME_PROPERTY()
        LinearColor AccentActive{};

        ME_PROPERTY()
        LinearColor Button{};

        ME_PROPERTY()
        LinearColor ButtonHovered{};

        ME_PROPERTY()
        LinearColor ButtonActive{};

        ME_PROPERTY()
        LinearColor TextPrimary{};

        ME_PROPERTY()
        LinearColor TextMuted{};

        ME_PROPERTY()
        LinearColor Border{};

        ME_PROPERTY()
        LinearColor Separator{};

        ME_PROPERTY()
        LinearColor Tab{};

        ME_PROPERTY()
        LinearColor TabHovered{};

        ME_PROPERTY()
        LinearColor TabActive{};

        ME_PROPERTY()
        LinearColor TabUnfocused{};

        ME_PROPERTY()
        LinearColor TabUnfocusedActive{};

        ME_PROPERTY()
        LinearColor Selection{};
    };
}

#include "EditorThemePalette.gen.h"
