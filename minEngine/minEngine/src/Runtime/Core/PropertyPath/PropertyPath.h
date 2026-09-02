#pragma once

#include "Runtime/Core/Command/CommandContext.h"
#include "Runtime/Core/Command/CommandResult.h"
#include "Runtime/Core/PropertyPath/PropertyPathTypes.h"

#include <optional>
#include <string>
#include <string_view>

namespace minEngine
{
    class Component;
    class GameObject;
    class Scene;
}

namespace minEngine::Serialization
{
    struct SerializerOptions;
}

namespace minEngine::Reflection
{
    class MEClass;
    class MEProperty;
}

namespace minEngine::Command
{
    class PropertyPath
    {
    public:
        static std::optional<PropertyPath> Parse(std::string_view text);

        PropertyPathResolveStatus TryResolve(
            const CommandContext& context,
            ResolvedPropertyTarget& outTarget,
            std::vector<std::string>* outAmbiguousCandidates = nullptr) const;

        bool Resolve(const CommandContext& context, ResolvedPropertyTarget& outTarget) const;

        CommandResult GetValue(const CommandContext& context) const;
        CommandResult SetValue(const CommandContext& context, std::string_view literal) const;
        bool TryBuildSetTransaction(
            const CommandContext& context,
            std::string_view literal,
            PropertySetTransaction& outTransaction,
            CommandResult& outError,
            const Serialization::SerializerOptions* serializerOptions = nullptr) const;
        CommandResult BuildSetValueSuccessResult(const CommandContext& context) const;
        CommandResult Inspect(const CommandContext& context) const;

        bool TryResolveLeafProperty(const CommandContext& context, const Reflection::MEProperty*& outProperty) const;

        static bool IsPropertyWritable(const Reflection::MEProperty& property);
        static std::string FormatPropertyTypeName(const Reflection::MEProperty& property);
        static bool ComponentTypeMatches(const Reflection::MEClass* componentClass, std::string_view typeQuery);
        static std::string FormatComponentTypeName(const Reflection::MEClass* componentClass);

        const std::string& GetGameObjectName() const { return m_GameObjectName; }
        const std::string& GetExplicitComponentName() const { return m_ExplicitComponentName; }
        const std::string& GetPropertySubPath() const { return m_PropertySubPath; }
        bool HasExplicitComponent() const { return !m_ExplicitComponentName.empty(); }

        std::string GetObjectRef() const;
        std::string GetCanonicalPath() const;

    private:
        PropertyPath(std::string gameObjectName, std::string explicitComponentName, std::string propertySubPath);

        std::string m_GameObjectName;
        std::string m_ExplicitComponentName;
        std::string m_PropertySubPath;

        CommandResult BuildResolveErrorResult(
            PropertyPathResolveStatus status,
            const std::vector<std::string>* ambiguousCandidates) const;

        static GameObject* FindGameObjectByName(Scene* scene, std::string_view objectRef);
        static Component* FindComponentByTypeName(GameObject& gameObject, std::string_view componentTypeName);
        static void CollectShortPathMatches(
            GameObject& gameObject,
            const std::string& propertySubPath,
            std::vector<ResolvedPropertyTarget>& outMatches);
        static bool TryResolvePropertySubPath(
            void* ownerObject,
            const Reflection::MEClass* ownerClass,
            const std::string& propertySubPath);
        static bool ResolvePropertySubPathOnGameObject(
            GameObject& gameObject,
            const std::string& propertySubPath,
            ResolvedPropertyTarget& outTarget);
        static bool WalkToLeafOwner(
            void* ownerObject,
            const Reflection::MEClass* ownerClass,
            const std::string& propertySubPath,
            void*& outLeafOwner,
            const Reflection::MEClass*& outLeafOwnerClass,
            std::string& outLeafPropertyName,
            std::string& outError);
        static const Reflection::MEProperty* FindPropertyInHierarchy(
            const Reflection::MEClass* ownerClass,
            std::string_view propertyName);
        static bool IsPropertyVisible(const Reflection::MEProperty& property);
        static std::string FormatPropertyValueAsText(
            void* ownerObject,
            const Reflection::MEClass* ownerClass,
            const std::string& propertySubPath);
        static bool WritePrimitiveLiteralToBuffer(
            const Reflection::MEProperty& property,
            std::string_view literal,
            std::vector<uint8_t>& outBuffer,
            std::string& outError);
        static void AppendInspectProperties(
            CommandOutputBuilder& builder,
            void* ownerObject,
            const Reflection::MEClass* ownerClass,
            int indentLevel);
    };
}
