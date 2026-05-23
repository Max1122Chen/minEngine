#include "MaterialGraphNodeRegistry.h"

#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include "imgui.h"

#include <cstring>

namespace minEngine
{
    namespace
    {
        uint32_t HashString(const char* text)
        {
            uint32_t hash = 2166136261u;
            if (!text)
            {
                return hash;
            }

            for (const char* cursor = text; *cursor != '\0'; ++cursor)
            {
                hash ^= static_cast<uint32_t>(*cursor);
                hash *= 16777619u;
            }

            return hash;
        }

        ImU32 ColorFromHash(uint32_t hash)
        {
            const float hue = static_cast<float>(hash % 360) / 360.0f;
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            const float s = 0.55f;
            const float v = 0.85f;

            const int region = static_cast<int>(hue * 6.0f);
            const float fraction = hue * 6.0f - static_cast<float>(region);
            const float p = v * (1.0f - s);
            const float q = v * (1.0f - s * fraction);
            const float t = v * (1.0f - s * (1.0f - fraction));

            switch (region % 6)
            {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                default: r = v; g = p; b = q; break;
            }

            return IM_COL32(
                static_cast<int>(r * 255.0f),
                static_cast<int>(g * 255.0f),
                static_cast<int>(b * 255.0f),
                255);
        }

        const char* StripNodeDefPrefix(const char* className)
        {
            constexpr const char kPrefix[] = "minEngine::MaterialGraphNodeDef_";
            if (className && std::strncmp(className, kPrefix, sizeof(kPrefix) - 1) == 0)
            {
                return className + sizeof(kPrefix) - 1;
            }

            return className ? className : "Unknown";
        }

        MaterialGraphNodeRegistryEntry MakeEntry(const Reflection::MEClass* nodeDefClass)
        {
            MaterialGraphNodeRegistryEntry entry;
            entry.NodeDefClass = nodeDefClass;
            if (nodeDefClass != nullptr)
            {
                entry.DisplayName = StripNodeDefPrefix(nodeDefClass->GetName().c_str());
                entry.HeaderColor = ColorFromHash(HashString(entry.DisplayName));
            }

            return entry;
        }

        std::vector<MaterialGraphNodeRegistryEntry> s_CreatableNodes;
        bool s_Registered = false;

        void RegisterCreatableNodes()
        {
            s_CreatableNodes.clear();
            s_CreatableNodes.reserve(12);

            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Constant::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Constant3::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_MakeFloat3::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Multiply::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Lerp::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Subtract::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Divide::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Min::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_Max::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_TextureCoordinate::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_TextureObject::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_TextureSample::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_NormalUnpack::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_ComponentMask::StaticClass()));
            s_CreatableNodes.push_back(MakeEntry(MaterialGraphNodeDef_ScalarParameter::StaticClass()));
        }
    }

    void MaterialGraphNodeRegistry::EnsureRegistered()
    {
        if (!s_Registered)
        {
            RegisterCreatableNodes();
            s_Registered = true;
        }
    }

    const std::vector<MaterialGraphNodeRegistryEntry>& MaterialGraphNodeRegistry::GetCreatableNodes()
    {
        EnsureRegistered();
        return s_CreatableNodes;
    }

    const MaterialGraphNodeRegistryEntry* MaterialGraphNodeRegistry::FindEntry(const Reflection::MEClass* nodeDefClass)
    {
        if (nodeDefClass == nullptr)
        {
            return nullptr;
        }

        EnsureRegistered();
        for (const MaterialGraphNodeRegistryEntry& entry : s_CreatableNodes)
        {
            if (entry.NodeDefClass == nodeDefClass)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    const char* MaterialGraphNodeRegistry::GetDisplayName(const MaterialGraphNodeDef* nodeDef)
    {
        if (!nodeDef || !nodeDef->GetClass())
        {
            return "Unknown";
        }

        if (const MaterialGraphNodeRegistryEntry* entry = FindEntry(nodeDef->GetClass()))
        {
            return entry->DisplayName;
        }

        return StripNodeDefPrefix(nodeDef->GetClass()->GetName().c_str());
    }

    MaterialGraphNodeStyle MaterialGraphNodeRegistry::GetStyle(const MaterialGraphNodeDef* nodeDef)
    {
        MaterialGraphNodeStyle style;
        if (!nodeDef || !nodeDef->GetClass())
        {
            return style;
        }

        if (const MaterialGraphNodeRegistryEntry* entry = FindEntry(nodeDef->GetClass()))
        {
            style.DisplayName = entry->DisplayName;
            style.HeaderColor = entry->HeaderColor;
            return style;
        }

        style.DisplayName = StripNodeDefPrefix(nodeDef->GetClass()->GetName().c_str());
        style.HeaderColor = ColorFromHash(HashString(style.DisplayName));
        return style;
    }

    bool MaterialGraphNodeRegistry::DrawConstant(MaterialGraphNodeDef_Constant* constant)
    {
        if (!constant)
        {
            return false;
        }

        ImGui::SetNextItemWidth(kNodeContentWidth);
        return ImGui::DragFloat("##Value", &constant->Value, 0.01f, 0.0f, 0.0f, "%.3f");
    }

    bool MaterialGraphNodeRegistry::DrawConstant3(MaterialGraphNodeDef_Constant3* constant3)
    {
        if (!constant3)
        {
            return false;
        }

        float rgb[3] = {constant3->R, constant3->G, constant3->B};
        ImGui::SetNextItemWidth(kNodeContentWidth);
        if (ImGui::DragFloat3("##RGB", rgb, 0.01f, 0.0f, 0.0f, "%.3f"))
        {
            constant3->R = rgb[0];
            constant3->G = rgb[1];
            constant3->B = rgb[2];
            return true;
        }

        return false;
    }

    bool MaterialGraphNodeRegistry::DrawScalarParameter(MaterialGraphNodeDef_ScalarParameter* scalar)
    {
        if (!scalar)
        {
            return false;
        }

        ImGui::SetNextItemWidth(kNodeContentWidth);
        return ImGui::DragFloat("##Default", &scalar->DefaultValue, 0.01f, 0.0f, 0.0f, "%.3f");
    }

    bool MaterialGraphNodeRegistry::DrawTextureObject(MaterialGraphNodeDef_TextureObject* textureObject)
    {
        if (!textureObject)
        {
            return false;
        }

        const char* name =
            textureObject->ParameterName.empty() ? "Texture" : textureObject->ParameterName.c_str();
        ImGui::TextDisabled("%s", name);
        return false;
    }

    bool MaterialGraphNodeRegistry::DrawDefault(MaterialGraphNodeDef* nodeDef)
    {
        if (!nodeDef || !nodeDef->GetClass())
        {
            return false;
        }

        ImGui::TextDisabled("%s", StripNodeDefPrefix(nodeDef->GetClass()->GetName().c_str()));
        return false;
    }

    bool MaterialGraphNodeRegistry::DrawNode(MaterialEdGraphNode& node)
    {
        MaterialGraphNodeDef* nodeDef = node.GetNodeDef();
        if (!nodeDef || !nodeDef->GetClass())
        {
            return false;
        }

        const Reflection::MEClass* nodeClass = nodeDef->GetClass();
        if (nodeClass->IsA(MaterialGraphNodeDef_Constant::StaticClass()))
        {
            return DrawConstant(static_cast<MaterialGraphNodeDef_Constant*>(nodeDef));
        }

        if (nodeClass->IsA(MaterialGraphNodeDef_Constant3::StaticClass()))
        {
            return DrawConstant3(static_cast<MaterialGraphNodeDef_Constant3*>(nodeDef));
        }

        if (nodeClass->IsA(MaterialGraphNodeDef_ScalarParameter::StaticClass()))
        {
            return DrawScalarParameter(static_cast<MaterialGraphNodeDef_ScalarParameter*>(nodeDef));
        }

        if (nodeClass->IsA(MaterialGraphNodeDef_TextureObject::StaticClass()))
        {
            return DrawTextureObject(static_cast<MaterialGraphNodeDef_TextureObject*>(nodeDef));
        }

        return DrawDefault(nodeDef);
    }
}
