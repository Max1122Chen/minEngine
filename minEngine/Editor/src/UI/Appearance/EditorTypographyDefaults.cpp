#include "UI/Appearance/EditorTypographyDefaults.h"

#include "Core.h"
#include "Runtime/Function/Framework/Project/EditorTypographySettings.h"

#include <array>

namespace minEngine
{
    namespace
    {
        constexpr size_t RoleCount()
        {
            return static_cast<size_t>(EditorTypographyRole::Count);
        }

        /** Built-in defaults (2026-05-26): 3/4 of original M5b sizes. */
        constexpr float kDefaultTypographyScale = 0.75f;
        constexpr std::array<float, RoleCount()> kBaseSizePixels = {
            28.0f, // Body
            32.0f, // Heading
            30.0f, // Subheading
            24.0f, // Caption
            26.0f, // MenuBar
        };

        constexpr float ScaledDefaultSizePixels(const size_t roleIndex)
        {
            return kBaseSizePixels[roleIndex] * kDefaultTypographyScale;
        }

        // MyMEProject/Assets/Fonts — from AssetManager scan (2026-05-26).
        const GUID kInterRegularGuid{12411318281514927934ULL, 10351155873599353985ULL};
        const GUID kInterSemiBoldGuid{11519423272931444815ULL, 13211526748840255211ULL};
    }

    float EditorTypographyDefaults::GetDefaultSizePixels(EditorTypographyRole role)
    {
        const size_t roleIndex = static_cast<size_t>(role);
        if (roleIndex >= RoleCount())
        {
            return ScaledDefaultSizePixels(0);
        }

        return ScaledDefaultSizePixels(roleIndex);
    }

    const char* EditorTypographyDefaults::GetDefaultFontProjectPath(EditorTypographyRole role)
    {
        switch (role)
        {
        case EditorTypographyRole::Heading:
            return kHeadingFontProjectPath;
        case EditorTypographyRole::Body:
        case EditorTypographyRole::Subheading:
        case EditorTypographyRole::Caption:
        case EditorTypographyRole::MenuBar:
        default:
            return kBodyFontProjectPath;
        }
    }

    GUID EditorTypographyDefaults::GetDefaultFontAssetGuid(EditorTypographyRole role)
    {
        switch (role)
        {
        case EditorTypographyRole::Heading:
            return kInterSemiBoldGuid;
        case EditorTypographyRole::Body:
        case EditorTypographyRole::Subheading:
        case EditorTypographyRole::Caption:
        case EditorTypographyRole::MenuBar:
        default:
            return kInterRegularGuid;
        }
    }

    void EditorTypographyDefaults::EnsureSlotCount(EditorTypographySettings& typography)
    {
        const size_t requiredCount = RoleCount();
        if (typography.Slots.size() < requiredCount)
        {
            typography.Slots.resize(requiredCount);
        }
    }

    void EditorTypographyDefaults::ApplyBuiltinDefaults(EditorTypographySettings& typography)
    {
        EnsureSlotCount(typography);

        for (size_t roleIndex = 0; roleIndex < RoleCount(); ++roleIndex)
        {
            const auto role = static_cast<EditorTypographyRole>(roleIndex);
            EditorTypographySlot& slot = typography.Slots[roleIndex];

            if (slot.FontAssetGuid.IsZero())
            {
                slot.FontAssetGuid = GetDefaultFontAssetGuid(role);
            }

            if (slot.SizePixels <= 0.0f)
            {
                slot.SizePixels = GetDefaultSizePixels(role);
            }
        }
    }
}
