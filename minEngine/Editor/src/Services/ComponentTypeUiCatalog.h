#pragma once

#include "Runtime/Core/Reflection/MEClass.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace minEngine
{
    class EditorAppearance;

    /**
     * Editor-only mapping from Component types to FA icons and display labels.
     * Not a Runtime reflection registry — static table + inheritance fallback.
     */
    class ComponentTypeUiCatalog
    {
    public:
        /** FA glyph literal (ICON_FA_*), never null. */
        static const char* ResolveIconGlyph(const Reflection::MEClass* componentClass);
        static const char* ResolveIconGlyph(std::string_view reflectedOrShortTypeName);

        /**
         * Draw FA glyph sized to match surrounding text (not full icon-font face size).
         * @param fontSizePx 0 = current ImGui::GetFontSize().
         */
        static void DrawIcon(EditorAppearance& appearance,
                             const Reflection::MEClass* componentClass,
                             float fontSizePx = 0.0f);
        static void DrawIcon(EditorAppearance& appearance,
                             std::string_view reflectedOrShortTypeName,
                             float fontSizePx = 0.0f);

        static std::string MakeTypeDisplayName(const Reflection::MEClass* componentClass);
        static std::string MakeTypeDisplayName(std::string_view reflectedOrShortTypeName);

        /** Default m_Name when adding: no word breaks, no "Component" suffix. */
        static std::string MakeDefaultInstanceName(const Reflection::MEClass* componentClass);
        static std::string MakeDefaultInstanceName(std::string_view reflectedOrShortTypeName);

        static std::string GetShortTypeName(std::string_view reflectedOrShortTypeName);

    private:
        static const std::unordered_map<std::string, const char*>& GetIconTable();
        static const char* LookupIconExact(std::string_view shortName);
        static void DrawIconGlyph(EditorAppearance& appearance, const char* glyph, float fontSizePx);
    };
}
