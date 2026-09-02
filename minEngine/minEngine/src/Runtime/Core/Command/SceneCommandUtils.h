#pragma once

#include "Core.h"

#include "Runtime/Core/PropertyPath/PropertyPath.h"
#include "Runtime/Core/PropertyPath/PropertyPathTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class Component;
    class GameObject;
    class Scene;
}

namespace minEngine::Command
{
    struct SceneGameObjectMatch
    {
        std::string Name;
        std::string ClassName;
        std::string GuidText;
    };

    class SceneCommandUtils
    {
    public:
        static bool ContainsIgnoreCase(std::string_view haystack, std::string_view needle);

        static std::vector<SceneGameObjectMatch> FindGameObjects(const Scene* scene, std::string_view query);
        static std::vector<std::string> ListGameObjectNames(const Scene* scene, std::string_view prefix);
        static std::vector<std::string> ListPropertyPathPrefixes(
            const Scene* scene,
            std::string_view objectRef,
            std::string_view memberPrefix);
        static std::vector<std::string> ListAttachedComponentNames(
            const Scene* scene,
            std::string_view gameObjectName,
            std::string_view prefix);
        static std::vector<PropertyPathSuggestion> ListPropertyPathSuggestions(
            const Scene* scene,
            std::string_view gameObjectName,
            std::string_view explicitComponentName,
            std::string_view memberPrefix);
        static std::vector<std::string> ListComponentTypeNames(std::string_view prefix);
    };
}
