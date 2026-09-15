#include "UI/Appearance/EditorAssetTypeIcons.h"

#include "UI/Appearance/EditorAppearance.h"

#include "IconFontCppHeaders/IconsFontAwesome7.h"

namespace minEngine
{
    const char* EditorAssetTypeIcons::GlyphForAssetType(std::string_view assetType)
    {
        if (assetType == "Texture2D")
        {
            return ICON_FA_IMAGE;
        }
        if (assetType == "StaticMesh")
        {
            return ICON_FA_CUBE;
        }
        if (assetType == "Material")
        {
            return ICON_FA_PALETTE;
        }
        if (assetType == "EnvironmentMap")
        {
            return ICON_FA_GLOBE;
        }
        if (assetType == "Scene")
        {
            return ICON_FA_MAP;
        }
        if (assetType == "Font")
        {
            return ICON_FA_FONT;
        }
        if (assetType == "AnimationGraph" || assetType == "AnimationClip" || assetType == "Skeleton"
            || assetType == "SkeletalMesh")
        {
            return ICON_FA_DIAGRAM_PROJECT;
        }
        return ICON_FA_FILE;
    }

    EditorAssetTypeIcons::FontStyle EditorAssetTypeIcons::FontStyleForAssetType(std::string_view assetType)
    {
        if (assetType == "StaticMesh" || assetType == "Material" || assetType == "Font"
            || assetType == "EnvironmentMap" || assetType == "AnimationGraph"
            || assetType == "AnimationClip" || assetType == "Skeleton" || assetType == "SkeletalMesh")
        {
            return FontStyle::Solid;
        }
        return FontStyle::Regular;
    }

    ImFont* EditorAssetTypeIcons::ResolveFont(const EditorAppearance& appearance, std::string_view assetType)
    {
        if (FontStyleForAssetType(assetType) == FontStyle::Solid)
        {
            if (ImFont* solid = appearance.GetAssetIconSolidImFont())
            {
                return solid;
            }
            return appearance.GetAssetIconRegularImFont();
        }

        if (ImFont* regular = appearance.GetAssetIconRegularImFont())
        {
            return regular;
        }
        return appearance.GetAssetIconSolidImFont();
    }

    const char* EditorAssetTypeIcons::GlyphForDocumentTypeId(std::string_view documentTypeId)
    {
        return GlyphForAssetType(documentTypeId);
    }

    ImFont* EditorAssetTypeIcons::ResolveFontForDocumentTypeId(const EditorAppearance& appearance,
                                                               std::string_view documentTypeId)
    {
        return ResolveFont(appearance, documentTypeId);
    }

    const char* EditorAssetTypeIcons::GlyphForFolder()
    {
        return ICON_FA_FOLDER;
    }

    ImFont* EditorAssetTypeIcons::ResolveFolderFont(const EditorAppearance& appearance)
    {
        if (ImFont* solid = appearance.GetAssetIconSolidImFont())
        {
            return solid;
        }
        return appearance.GetAssetIconRegularImFont();
    }
}
