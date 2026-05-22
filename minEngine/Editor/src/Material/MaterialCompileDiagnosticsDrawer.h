#pragma once

namespace minEngine
{
    class Material;

    class MaterialCompileDiagnosticsDrawer
    {
    public:
        static void Draw(const Material& material, bool defaultOpen = true);
    };
}
