#pragma once

namespace minEngine
{
    class EditorAppearance;
    class Material;

    class MaterialCompileDiagnosticsDrawer
    {
    public:
        static void Draw(const Material& material, const EditorAppearance& appearance, bool defaultOpen = true);
    };
}
