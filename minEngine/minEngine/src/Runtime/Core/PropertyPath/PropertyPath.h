#pragma once

#include "Runtime/Core/Command/CommandContext.h"
#include "Runtime/Core/Command/CommandResult.h"
#include "Runtime/Core/PropertyPath/PropertyPathTypes.h"

#include <optional>
#include <string>
#include <string_view>

namespace minEngine
{
    class GameObject;
    class Scene;
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

        bool Resolve(const CommandContext& context, ResolvedPropertyTarget& outTarget) const;

        CommandResult GetValue(const CommandContext& context) const;
        CommandResult SetValue(const CommandContext& context, std::string_view literal) const;
        CommandResult Inspect(const CommandContext& context) const;

        bool TryResolveLeafProperty(const CommandContext& context, const Reflection::MEProperty*& outProperty) const;

        static bool IsPropertyWritable(const Reflection::MEProperty& property);

        const std::string& GetObjectRef() const { return m_ObjectRef; }
        const std::string& GetPropertySubPath() const { return m_PropertySubPath; }

    private:
        PropertyPath(std::string objectRef, std::string propertySubPath);

        std::string m_ObjectRef;
        std::string m_PropertySubPath;

        static GameObject* FindGameObjectByName(Scene* scene, std::string_view objectRef);
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
