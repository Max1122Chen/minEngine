#pragma once

#include <imgui.h>

namespace minEngine
{
    class MaterialGraphNodeDef;

    struct MaterialGraphNodeStyle
    {
        ImU32 HeaderColor = IM_COL32(90, 90, 90, 255);
        const char* DisplayName = "Node";
    };

    /** Display names and colors for MaterialGraphNodeDef types (MVP lookup). */
    class MaterialGraphNodeRegistry
    {
    public:
        static MaterialGraphNodeStyle GetStyle(const MaterialGraphNodeDef* nodeDef);
        static const char* GetDisplayName(const MaterialGraphNodeDef* nodeDef);
    };
}
