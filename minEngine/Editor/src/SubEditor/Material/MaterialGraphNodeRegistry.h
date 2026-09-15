#pragma once

#include <imgui.h>

#include <vector>

namespace minEngine::Reflection
{
    class MEClass;
}

namespace minEngine
{
    class MaterialEdGraphNode;
    class MaterialEditor;
    class MaterialGraphNodeDef;
    class MaterialGraphNodeDef_Constant;
    class MaterialGraphNodeDef_Constant3;
    class MaterialGraphNodeDef_ScalarParameter;
    class MaterialGraphNodeDef_TextureObject;

    struct MaterialGraphNodeStyle
    {
        ImU32 HeaderColor = IM_COL32(90, 90, 90, 255);
        const char* DisplayName = "Node";
    };

    struct MaterialGraphNodeRegistryEntry
    {
        const Reflection::MEClass* NodeDefClass = nullptr;
        const char* DisplayName = "Node";
        ImU32 HeaderColor = IM_COL32(90, 90, 90, 255);
    };

    /** Display metadata + creatable NodeDef whitelist for the material graph editor. */
    class MaterialGraphNodeRegistry
    {
    public:
        static constexpr float kNodeContentWidth = 168.0f;

        static void EnsureRegistered();
        static const std::vector<MaterialGraphNodeRegistryEntry>& GetCreatableNodes();

        static MaterialGraphNodeStyle GetStyle(const MaterialGraphNodeDef* nodeDef);
        static const char* GetDisplayName(const MaterialGraphNodeDef* nodeDef);
        static const MaterialGraphNodeRegistryEntry* FindEntry(const Reflection::MEClass* nodeDefClass);

        /** In-node subtitle widgets; returns true if a value changed (live preview). */
        static bool DrawNode(MaterialEdGraphNode& node, MaterialEditor& materialEditor);

    private:
        static bool DrawConstant(MaterialGraphNodeDef_Constant* constant, MaterialEditor& materialEditor);
        static bool DrawConstant3(MaterialGraphNodeDef_Constant3* constant3, MaterialEditor& materialEditor);
        static bool DrawScalarParameter(MaterialGraphNodeDef_ScalarParameter* scalar, MaterialEditor& materialEditor);
        static bool DrawTextureObject(MaterialGraphNodeDef_TextureObject* textureObject);
        static bool DrawDefault(MaterialGraphNodeDef* nodeDef);
    };
}
