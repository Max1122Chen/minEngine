#pragma once

namespace minEngine
{
    class MaterialEditor;
    class MaterialGraphNodeDef;
    struct PropertyEditSession;

    /** Reflection-driven property UI for selected MaterialGraphNodeDef (Details panel). */
    class MaterialNodeDefPropertyDrawer
    {
    public:
        /** Width for value widgets; use -1 inside a width-bounded parent (Details scroll child). */
        static constexpr float kFieldWidth = -1.0f;

        static bool DrawProperties(
            MaterialGraphNodeDef* nodeDef,
            MaterialEditor& materialEditor,
            const PropertyEditSession& editSession);
    };
}
