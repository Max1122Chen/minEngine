#pragma once

#include "Core.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

namespace minEngine
{
    struct EditorTypographySettings;
}

namespace minEngine
{
    class EditorTypographyDefaults
    {
    public:
        static constexpr const char* kBodyFontProjectPath = "Fonts/Inter_18pt-Regular.ttf";
        static constexpr const char* kHeadingFontProjectPath = "Fonts/Inter_18pt-SemiBold.ttf";

        static float GetDefaultSizePixels(EditorTypographyRole role);
        static const char* GetDefaultFontProjectPath(EditorTypographyRole role);
        static GUID GetDefaultFontAssetGuid(EditorTypographyRole role);

        static void ApplyBuiltinDefaults(EditorTypographySettings& typography);
        static void EnsureSlotCount(EditorTypographySettings& typography);
    };
}
