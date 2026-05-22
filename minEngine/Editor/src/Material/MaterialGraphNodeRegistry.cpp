#include "MaterialGraphNodeRegistry.h"

#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <cstring>

namespace minEngine
{
    namespace
    {
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

        const char* StripNodeDefPrefix(const char* className)
        {
            constexpr const char kPrefix[] = "MaterialGraphNodeDef_";
            if (className && std::strncmp(className, kPrefix, sizeof(kPrefix) - 1) == 0)
            {
                return className + sizeof(kPrefix) - 1;
            }

            return className ? className : "Unknown";
        }
    }

    const char* MaterialGraphNodeRegistry::GetDisplayName(const MaterialGraphNodeDef* nodeDef)
    {
        if (!nodeDef || !nodeDef->GetClass())
        {
            return "Unknown";
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

        style.DisplayName = StripNodeDefPrefix(nodeDef->GetClass()->GetName().c_str());
        style.HeaderColor = ColorFromHash(HashString(style.DisplayName));
        return style;
    }
}
