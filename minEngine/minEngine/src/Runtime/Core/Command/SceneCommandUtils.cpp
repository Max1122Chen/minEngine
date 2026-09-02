#include "Runtime/Core/Command/SceneCommandUtils.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <algorithm>
#include <cctype>

namespace minEngine::Command
{
    bool SceneCommandUtils::ContainsIgnoreCase(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty())
        {
            return true;
        }

        if (haystack.size() < needle.size())
        {
            return false;
        }

        const auto it = std::search(
            haystack.begin(),
            haystack.end(),
            needle.begin(),
            needle.end(),
            [](const char lhs, const char rhs)
            {
                return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
            });
        return it != haystack.end();
    }

    namespace
    {
        SceneGameObjectMatch MakeMatch(const GameObject& gameObject)
        {
            SceneGameObjectMatch match;
            match.Name = gameObject.GetName();
            if (const Reflection::MEClass* gameObjectClass = gameObject.GetClass())
            {
                match.ClassName = gameObjectClass->GetName();
            }
            match.GuidText = gameObject.GetGuid().ToString();
            return match;
        }

        bool MatchesTypeQuery(GameObject& gameObject, std::string_view typeQuery)
        {
            for (const std::shared_ptr<Component>& component : gameObject.GetAllComponents())
            {
                if (!component)
                {
                    continue;
                }

                const Reflection::MEClass* componentClass = component->GetClass();
                if (componentClass == nullptr)
                {
                    continue;
                }

                const std::string_view className = componentClass->GetName();
                if (SceneCommandUtils::ContainsIgnoreCase(className, typeQuery)
                    || className.size() >= typeQuery.size()
                        && className.substr(className.size() - typeQuery.size()) == typeQuery)
                {
                    return true;
                }
            }

            return false;
        }

        bool MatchesFreeTextQuery(GameObject& gameObject, std::string_view query)
        {
            if (SceneCommandUtils::ContainsIgnoreCase(gameObject.GetName(), query))
            {
                return true;
            }

            if (const Reflection::MEClass* gameObjectClass = gameObject.GetClass())
            {
                if (SceneCommandUtils::ContainsIgnoreCase(gameObjectClass->GetName(), query))
                {
                    return true;
                }
            }

            return MatchesTypeQuery(gameObject, query);
        }

        void AppendVisiblePropertyNames(
            void* ownerObject,
            const Reflection::MEClass* ownerClass,
            std::string_view memberPrefix,
            std::vector<std::string>& inOutNames)
        {
            if (ownerObject == nullptr || ownerClass == nullptr)
            {
                return;
            }

            Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
                ownerClass->GetName(),
                [&](const Reflection::MEProperty& property) -> bool
                {
                    if (property.HasSpecifier(Reflection::PropertySpecifier::Invisible))
                    {
                        return true;
                    }

                    const std::string_view propertyName = property.GetName();
                    if (!memberPrefix.empty() && !SceneCommandUtils::ContainsIgnoreCase(propertyName, memberPrefix))
                    {
                        return true;
                    }

                    inOutNames.push_back(std::string(propertyName));
                    return true;
                });
        }
    }

    std::vector<SceneGameObjectMatch> SceneCommandUtils::FindGameObjects(const Scene* scene, std::string_view query)
    {
        std::vector<SceneGameObjectMatch> matches;
        if (scene == nullptr || query.empty())
        {
            return matches;
        }

        constexpr std::string_view kTypePrefix = "type=";
        constexpr std::string_view kNamePrefix = "name=";

        const bool isTypeQuery = query.rfind(kTypePrefix, 0) == 0;
        const bool isNameQuery = query.rfind(kNamePrefix, 0) == 0;
        const std::string_view lookup = isTypeQuery ? query.substr(kTypePrefix.size())
            : isNameQuery                              ? query.substr(kNamePrefix.size())
                                                       : query;

        if (lookup.empty())
        {
            return matches;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            bool matched = false;
            if (isTypeQuery)
            {
                matched = MatchesTypeQuery(*gameObject, lookup);
            }
            else if (isNameQuery)
            {
                matched = gameObject->GetName() == lookup;
            }
            else
            {
                matched = MatchesFreeTextQuery(*gameObject, lookup);
            }

            if (matched)
            {
                matches.push_back(MakeMatch(*gameObject));
            }
        }

        return matches;
    }

    std::vector<std::string> SceneCommandUtils::ListGameObjectNames(const Scene* scene, std::string_view prefix)
    {
        std::vector<std::string> names;
        if (scene == nullptr)
        {
            return names;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject || gameObject->GetName().empty())
            {
                continue;
            }

            if (prefix.empty() || SceneCommandUtils::ContainsIgnoreCase(gameObject->GetName(), prefix))
            {
                names.push_back(gameObject->GetName());
            }
        }

        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        return names;
    }

    std::vector<std::string> SceneCommandUtils::ListPropertyPathPrefixes(
        const Scene* scene,
        std::string_view objectRef,
        std::string_view memberPrefix)
    {
        std::vector<std::string> names;
        if (scene == nullptr || objectRef.empty())
        {
            return names;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject || gameObject->GetName() != objectRef)
            {
                continue;
            }

            AppendVisiblePropertyNames(gameObject.get(), gameObject->GetClass(), memberPrefix, names);
            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (!component)
                {
                    continue;
                }

                AppendVisiblePropertyNames(component.get(), component->GetClass(), memberPrefix, names);
            }
            break;
        }

        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        return names;
    }

    std::vector<std::string> SceneCommandUtils::ListComponentTypeNames(std::string_view prefix)
    {
        std::vector<std::string> names;
        const Reflection::MEClass* componentBase =
            Reflection::ReflectionSystem::Get().FindClass("minEngine::Component");
        if (componentBase == nullptr)
        {
            return names;
        }

        for (const Reflection::MEClass* classInfo : Reflection::ReflectionSystem::Get().GetAllClasses())
        {
            if (classInfo == nullptr || !classInfo->IsA(componentBase))
            {
                continue;
            }

            const std::string shortName = classInfo->GetName();
            if (!prefix.empty() && !SceneCommandUtils::ContainsIgnoreCase(shortName, prefix))
            {
                continue;
            }

            names.push_back(shortName);
        }

        std::sort(names.begin(), names.end());
        return names;
    }
}
