#pragma once

#include "DebugCommand/DebugCommandContext.h"
#include "DebugCommand/DebugCommandResult.h"
#include "PropertyPath/PropertyPathTypes.h"

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

namespace minEngine::DebugCommand
{
    class PropertyPath
    {
    public:
        static std::optional<PropertyPath> Parse(std::string_view text);

        PropertyPathResolveStatus TryResolve(
            const DebugCommandContext& context,
            ResolvedPropertyTarget& outTarget,
            std::vector<std::string>* outAmbiguousCandidates = nullptr) const;

        bool Resolve(const DebugCommandContext& context, ResolvedPropertyTarget& outTarget) const;

        DebugCommandResult GetValue(const DebugCommandContext& context) const;
        DebugCommandResult SetValue(
            const DebugCommandContext& context,
            std::string_view literal,
            PropertyWriteMode writeMode = PropertyWriteMode::Force) const;
        bool TryBuildSetTransaction(
            const DebugCommandContext& context,
            std::string_view literal,
            PropertySetTransaction& outTransaction,
            DebugCommandResult& outError,
            const Serialization::SerializerOptions* serializerOptions = nullptr,
            PropertyWriteMode writeMode = PropertyWriteMode::Force,
            minEngine::EditorPropertyEditContextKind editContextKind =
                minEngine::EditorPropertyEditContextKind::SceneInstance) const;
        DebugCommandResult BuildSetValueSuccessResult(const DebugCommandContext& context) const;
        DebugCommandResult Inspect(const DebugCommandContext& context) const;

        bool TryResolveLeafProperty(const DebugCommandContext& context, const Reflection::MEProperty*& outProperty) const;

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

        DebugCommandResult BuildResolveErrorResult(
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
            DebugCommandOutputBuilder& builder,
            void* ownerObject,
            const Reflection::MEClass* ownerClass,
            int indentLevel);
    };
}
