#include "Services/ComponentTypeUiCatalog.h"

#include "UI/Appearance/EditorAppearance.h"

#include "Runtime/Core/Reflection/ReflectionDisplayNames.h"

#include "IconFontCppHeaders/IconsFontAwesome7.h"
#include "imgui.h"

#include <algorithm>

namespace minEngine
{
    const std::unordered_map<std::string, const char*>& ComponentTypeUiCatalog::GetIconTable()
    {
        static const std::unordered_map<std::string, const char*> s_IconByShortName = {
            {"DirectionalLightComponent", ICON_FA_SUN},
            {"PointLightComponent", ICON_FA_LIGHTBULB},
            {"SpotLightComponent", ICON_FA_LIGHTBULB},
            {"LightComponent", ICON_FA_LIGHTBULB},
            {"SkyBoxComponent", ICON_FA_CLOUD_SUN},
            {"AudioComponent", ICON_FA_VOLUME_HIGH},
            {"AudioListenerComponent", ICON_FA_HEADPHONES},
            {"StaticMeshComponent", ICON_FA_CUBE},
            {"PrimitiveComponent", ICON_FA_CUBE},
            {"BoxColliderComponent", ICON_FA_SQUARE},
            {"SphereColliderComponent", ICON_FA_CIRCLE},
            {"CapsuleColliderComponent", ICON_FA_CAPSULES},
            {"ColliderComponent", ICON_FA_SQUARE},
            {"RigidBodyComponent", ICON_FA_WEIGHT_HANGING},
            {"LuaComponent", ICON_FA_CODE},
            {"CameraComponent", ICON_FA_VIDEO},
            {"InputComponent", ICON_FA_KEYBOARD},
            {"MovementComponent", ICON_FA_PERSON},
            {"SceneComponent", ICON_FA_LOCATION_DOT},
        };
        return s_IconByShortName;
    }

    const char* ComponentTypeUiCatalog::LookupIconExact(std::string_view shortName)
    {
        const auto& table = GetIconTable();
        const auto found = table.find(std::string(shortName));
        if (found == table.end())
        {
            return nullptr;
        }
        return found->second;
    }

    std::string ComponentTypeUiCatalog::GetShortTypeName(std::string_view reflectedOrShortTypeName)
    {
        const size_t scopePos = reflectedOrShortTypeName.rfind("::");
        if (scopePos == std::string_view::npos)
        {
            return std::string(reflectedOrShortTypeName);
        }
        return std::string(reflectedOrShortTypeName.substr(scopePos + 2));
    }

    const char* ComponentTypeUiCatalog::ResolveIconGlyph(std::string_view reflectedOrShortTypeName)
    {
        const std::string shortName = GetShortTypeName(reflectedOrShortTypeName);
        if (const char* exact = LookupIconExact(shortName))
        {
            return exact;
        }
        return ICON_FA_PUZZLE_PIECE;
    }

    const char* ComponentTypeUiCatalog::ResolveIconGlyph(const Reflection::MEClass* componentClass)
    {
        if (componentClass == nullptr)
        {
            return ICON_FA_PUZZLE_PIECE;
        }

        for (const Reflection::MEClass* current = componentClass; current != nullptr;
             current = current->GetSuperClass())
        {
            const std::string shortName = GetShortTypeName(current->GetName());
            if (const char* glyph = LookupIconExact(shortName))
            {
                return glyph;
            }
        }

        return ICON_FA_PUZZLE_PIECE;
    }

    void ComponentTypeUiCatalog::DrawIconGlyph(EditorAppearance& appearance, const char* glyph, float fontSizePx)
    {
        ImFont* iconFont = appearance.GetAssetIconSolidImFont();
        if (iconFont == nullptr)
        {
            iconFont = appearance.GetAssetIconRegularImFont();
        }

        const float resolvedSize = fontSizePx > 0.0f ? fontSizePx : ImGui::GetFontSize();
        // Slightly under body size so FA glyphs don't dominate adjacent labels.
        const float drawSize = std::max(11.0f, resolvedSize * 0.92f);

        if (iconFont != nullptr)
        {
            ImGui::PushFont(iconFont, drawSize);
        }
        ImGui::TextUnformatted(glyph != nullptr ? glyph : ICON_FA_PUZZLE_PIECE);
        if (iconFont != nullptr)
        {
            ImGui::PopFont();
        }
    }

    void ComponentTypeUiCatalog::DrawIcon(EditorAppearance& appearance,
                                          const Reflection::MEClass* componentClass,
                                          float fontSizePx)
    {
        DrawIconGlyph(appearance, ResolveIconGlyph(componentClass), fontSizePx);
    }

    void ComponentTypeUiCatalog::DrawIcon(EditorAppearance& appearance,
                                          std::string_view reflectedOrShortTypeName,
                                          float fontSizePx)
    {
        DrawIconGlyph(appearance, ResolveIconGlyph(reflectedOrShortTypeName), fontSizePx);
    }

    std::string ComponentTypeUiCatalog::MakeTypeDisplayName(std::string_view reflectedOrShortTypeName)
    {
        return Reflection::FormatTypeDisplayName(reflectedOrShortTypeName);
    }

    std::string ComponentTypeUiCatalog::MakeTypeDisplayName(const Reflection::MEClass* componentClass)
    {
        if (componentClass == nullptr)
        {
            return {};
        }
        return MakeTypeDisplayName(componentClass->GetName());
    }

    std::string ComponentTypeUiCatalog::MakeDefaultInstanceName(std::string_view reflectedOrShortTypeName)
    {
        return Reflection::FormatDefaultComponentInstanceName(reflectedOrShortTypeName);
    }

    std::string ComponentTypeUiCatalog::MakeDefaultInstanceName(const Reflection::MEClass* componentClass)
    {
        if (componentClass == nullptr)
        {
            return {};
        }
        return MakeDefaultInstanceName(componentClass->GetName());
    }
}
