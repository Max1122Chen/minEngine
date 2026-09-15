#pragma once

#include <string_view>

struct ImFont;

namespace minEngine
{
    class EditorAppearance;

    /** Shared Font Awesome glyphs for asset / document types (Content Browser + Document tabs). */
    class EditorAssetTypeIcons
    {
    public:
        enum class FontStyle
        {
            Regular,
            Solid,
        };

        static const char* GlyphForAssetType(std::string_view assetType);
        static FontStyle FontStyleForAssetType(std::string_view assetType);
        static ImFont* ResolveFont(const EditorAppearance& appearance, std::string_view assetType);

        /** Folder tile glyph (Content Browser directories). */
        static const char* GlyphForFolder();
        static ImFont* ResolveFolderFont(const EditorAppearance& appearance);

        /** Map EditorDocumentSession TypeId → same glyphs as AssetType where possible. */
        static const char* GlyphForDocumentTypeId(std::string_view documentTypeId);
        static ImFont* ResolveFontForDocumentTypeId(const EditorAppearance& appearance,
                                                     std::string_view documentTypeId);
    };
}
